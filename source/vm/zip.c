#include "zip.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

typedef struct {
    char *name;
    uint32_t local_offset;
    uint32_t comp_size;
    uint32_t size;
    uint16_t method;
} ZipEntry;

struct ZipFile {
    // Chế độ bộ nhớ: data chứa cả file. Chế độ lazy: fp mở file, đọc từng entry khi cần.
    uint8_t *data;
    size_t size;
    bool owned;
    FILE *fp;
    ZipEntry *entries;
    int count;
};

static uint16_t rd16(const uint8_t *p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

static uint32_t rd32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int cmp_entry(const void *a, const void *b) {
    return strcmp(((const ZipEntry *)a)->name, ((const ZipEntry *)b)->name);
}

// Tìm End Of Central Directory trong đoạn cuối file (comment tối đa 64KB)
static const uint8_t *find_eocd(const uint8_t *tail, size_t len) {
    if (len < 22)
        return NULL;
    for (size_t i = len - 22 + 1; i-- > 0;) {
        if (rd32(tail + i) == 0x06054b50)
            return tail + i;
    }
    return NULL;
}

// Đọc central directory (cd, cd_len byte), total = số entry khai báo
static bool parse_entries(ZipFile *z, const uint8_t *cd, size_t cd_len, int total) {
    z->entries = calloc(total ? total : 1, sizeof(ZipEntry));
    if (!z->entries)
        return false;

    const uint8_t *p = cd;
    const uint8_t *end = cd + cd_len;
    for (int i = 0; i < total; i++) {
        if (p + 46 > end || rd32(p) != 0x02014b50)
            break;
        uint16_t method = rd16(p + 10);
        uint32_t comp = rd32(p + 20);
        uint32_t size = rd32(p + 24);
        uint16_t name_len = rd16(p + 28);
        uint16_t extra_len = rd16(p + 30);
        uint16_t comment_len = rd16(p + 32);
        uint32_t local = rd32(p + 42);
        if (p + 46 + name_len > end)
            break;

        const char *name = (const char *)p + 46;
        // Bỏ '/' ở đầu (một số JAR lỗi có tên kiểu "/res/a.png")
        while (name_len > 0 && *name == '/') {
            name++;
            name_len--;
        }
        if (name_len > 0 && name[name_len - 1] != '/') {
            ZipEntry *e = &z->entries[z->count++];
            e->name = malloc(name_len + 1);
            memcpy(e->name, name, name_len);
            e->name[name_len] = '\0';
            e->method = method;
            e->comp_size = comp;
            e->size = size;
            e->local_offset = local;
        }
        p += 46 + name_len + extra_len + comment_len;
    }

    qsort(z->entries, z->count, sizeof(ZipEntry), cmp_entry);
    return true;
}

ZipFile *zip_open_mem(const uint8_t *data, size_t size, bool owned) {
    ZipFile *z = calloc(1, sizeof(ZipFile));
    if (!z)
        return NULL;
    z->data = (uint8_t *)data;
    z->size = size;
    z->owned = owned;

    size_t tail_len = size > 22 + 65535 ? 22 + 65535 : size;
    const uint8_t *eocd = find_eocd(data + size - tail_len, tail_len);
    uint32_t cd_off = eocd ? rd32(eocd + 16) : 0;
    if (!eocd || cd_off >= size || !parse_entries(z, data + cd_off, size - cd_off, rd16(eocd + 10))) {
        zip_close(z);
        return NULL;
    }
    return z;
}

ZipFile *zip_open_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size <= 0) {
        fclose(f);
        return NULL;
    }
    uint8_t *data = malloc(size);
    if (!data || fread(data, 1, size, f) != (size_t)size) {
        free(data);
        fclose(f);
        return NULL;
    }
    fclose(f);
    ZipFile *z = zip_open_mem(data, size, true);
    if (!z)
        free(data);
    return z;
}

ZipFile *zip_open_file_lazy(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    ZipFile *z = calloc(1, sizeof(ZipFile));
    uint8_t *tail = NULL, *cd = NULL;
    if (!z)
        goto fail;
    z->fp = f;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    if (size < 22)
        goto fail;
    z->size = (size_t)size;

    size_t tail_len = (size_t)size > 22 + 65535 ? 22 + 65535 : (size_t)size;
    tail = malloc(tail_len);
    fseek(f, size - (long)tail_len, SEEK_SET);
    if (!tail || fread(tail, 1, tail_len, f) != tail_len)
        goto fail;
    const uint8_t *eocd = find_eocd(tail, tail_len);
    if (!eocd)
        goto fail;
    int total = rd16(eocd + 10);
    uint32_t cd_size = rd32(eocd + 12);
    uint32_t cd_off = rd32(eocd + 16);
    if ((size_t)cd_off + cd_size > (size_t)size)
        goto fail;

    cd = malloc(cd_size ? cd_size : 1);
    fseek(f, (long)cd_off, SEEK_SET);
    if (!cd || fread(cd, 1, cd_size, f) != cd_size || !parse_entries(z, cd, cd_size, total))
        goto fail;
    free(tail);
    free(cd);
    return z;

fail:
    free(tail);
    free(cd);
    if (z)
        zip_close(z);
    else
        fclose(f);
    return NULL;
}

void zip_close(ZipFile *z) {
    if (!z)
        return;
    for (int i = 0; i < z->count; i++)
        free(z->entries[i].name);
    free(z->entries);
    if (z->owned)
        free(z->data);
    if (z->fp)
        fclose(z->fp);
    free(z);
}

static ZipEntry *find_entry(ZipFile *z, const char *name) {
    while (*name == '/')
        name++;
    ZipEntry key = { .name = (char *)name };
    return bsearch(&key, z->entries, z->count, sizeof(ZipEntry), cmp_entry);
}

bool zip_exists(ZipFile *z, const char *name) {
    return z && find_entry(z, name) != NULL;
}

static bool inflate_raw(const uint8_t *src, uint32_t comp, uint8_t *out, uint32_t size) {
    z_stream s;
    memset(&s, 0, sizeof(s));
    if (inflateInit2(&s, -MAX_WBITS) != Z_OK)
        return false;
    s.next_in = (Bytef *)src;
    s.avail_in = comp;
    s.next_out = out;
    s.avail_out = size;
    int rc = inflate(&s, Z_FINISH);
    inflateEnd(&s);
    return rc == Z_STREAM_END || (rc == Z_BUF_ERROR && s.avail_out == 0);
}

uint8_t *zip_read(ZipFile *z, const char *name, size_t *out_size) {
    if (!z)
        return NULL;
    ZipEntry *e = find_entry(z, name);
    if (!e || e->local_offset + 30 > z->size)
        return NULL;

    uint8_t lh[30];
    const uint8_t *src;
    uint8_t *file_buf = NULL;
    if (z->fp) {
        if (fseek(z->fp, (long)e->local_offset, SEEK_SET) != 0 || fread(lh, 1, 30, z->fp) != 30)
            return NULL;
    } else {
        memcpy(lh, z->data + e->local_offset, 30);
    }
    if (rd32(lh) != 0x04034b50)
        return NULL;
    size_t data_off = e->local_offset + 30 + rd16(lh + 26) + rd16(lh + 28);
    if (data_off + e->comp_size > z->size)
        return NULL;
    if (z->fp) {
        file_buf = malloc(e->comp_size ? e->comp_size : 1);
        if (!file_buf || fseek(z->fp, (long)data_off, SEEK_SET) != 0 ||
            fread(file_buf, 1, e->comp_size, z->fp) != e->comp_size) {
            free(file_buf);
            return NULL;
        }
        src = file_buf;
    } else {
        src = z->data + data_off;
    }

    uint8_t *out = malloc((size_t)e->size + 1);
    bool ok = out != NULL;
    if (ok && e->method == 0)
        memcpy(out, src, e->size);
    else if (ok && e->method == 8)
        ok = inflate_raw(src, e->comp_size, out, e->size);
    else
        ok = false;
    free(file_buf);
    if (!ok) {
        free(out);
        return NULL;
    }

    out[e->size] = '\0';
    if (out_size)
        *out_size = e->size;
    return out;
}
