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
    uint8_t *data;
    size_t size;
    bool owned;
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

static bool parse_central_dir(ZipFile *z) {
    if (z->size < 22)
        return false;

    // Tìm End Of Central Directory từ cuối file (comment tối đa 64KB)
    size_t min = z->size > 22 + 65535 ? z->size - 22 - 65535 : 0;
    const uint8_t *eocd = NULL;
    for (size_t i = z->size - 22 + 1; i-- > min;) {
        if (rd32(z->data + i) == 0x06054b50) {
            eocd = z->data + i;
            break;
        }
    }
    if (!eocd)
        return false;

    int total = rd16(eocd + 10);
    uint32_t cd_off = rd32(eocd + 16);
    if (cd_off >= z->size)
        return false;

    z->entries = calloc(total ? total : 1, sizeof(ZipEntry));
    if (!z->entries)
        return false;

    const uint8_t *p = z->data + cd_off;
    const uint8_t *end = z->data + z->size;
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
    if (!parse_central_dir(z)) {
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

void zip_close(ZipFile *z) {
    if (!z)
        return;
    for (int i = 0; i < z->count; i++)
        free(z->entries[i].name);
    free(z->entries);
    if (z->owned)
        free(z->data);
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

uint8_t *zip_read(ZipFile *z, const char *name, size_t *out_size) {
    if (!z)
        return NULL;
    ZipEntry *e = find_entry(z, name);
    if (!e)
        return NULL;

    const uint8_t *lh = z->data + e->local_offset;
    if (e->local_offset + 30 > z->size || rd32(lh) != 0x04034b50)
        return NULL;
    size_t data_off = e->local_offset + 30 + rd16(lh + 26) + rd16(lh + 28);
    if (data_off + e->comp_size > z->size)
        return NULL;
    const uint8_t *src = z->data + data_off;

    uint8_t *out = malloc((size_t)e->size + 1);
    if (!out)
        return NULL;

    if (e->method == 0) {
        memcpy(out, src, e->size);
    } else if (e->method == 8) {
        z_stream s;
        memset(&s, 0, sizeof(s));
        if (inflateInit2(&s, -MAX_WBITS) != Z_OK) {
            free(out);
            return NULL;
        }
        s.next_in = (Bytef *)src;
        s.avail_in = e->comp_size;
        s.next_out = out;
        s.avail_out = e->size;
        int rc = inflate(&s, Z_FINISH);
        inflateEnd(&s);
        if (rc != Z_STREAM_END && !(rc == Z_BUF_ERROR && s.avail_out == 0)) {
            free(out);
            return NULL;
        }
    } else {
        free(out);
        return NULL;
    }

    out[e->size] = '\0';
    if (out_size)
        *out_size = e->size;
    return out;
}
