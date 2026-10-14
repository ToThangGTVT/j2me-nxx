#include "backup.h"

#include <dirent.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <zlib.h>

#include "lang.h"
#include "platform.h"
#include "vm/zip.h"

#define META_NAME "backup.ini"

// ---- Đường dẫn ----

static void mkdirs(const char *path) {
    char buf[768];
    snprintf(buf, sizeof(buf), "%s", path);
    for (char *p = buf + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(buf, 0777);
            *p = '/';
        }
    }
    mkdir(buf, 0777);
}

// Tạo thư mục chứa file path
static void mkdirs_parent(const char *path) {
    char dir[768];
    snprintf(dir, sizeof(dir), "%s", path);
    char *slash = strrchr(dir, '/');
    if (slash) {
        *slash = '\0';
        mkdirs(dir);
    }
}

static bool is_dir(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static void rm_tree(const char *path) {
    DIR *d = opendir(path);
    if (d) {
        struct dirent *e;
        while ((e = readdir(d))) {
            if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
                continue;
            char sub[768];
            snprintf(sub, sizeof(sub), "%s/%s", path, e->d_name);
            if (is_dir(sub))
                rm_tree(sub);
            else
                remove(sub);
        }
        closedir(d);
    }
    rmdir(path);
}

bool backup_valid_id(const char *id) {
    size_t n = strlen(id);
    if (n == 0 || n >= sizeof(((BackupMeta *)0)->id) || strcmp(id, ".") == 0 || strcmp(id, "..") == 0)
        return false;
    for (const char *p = id; *p; p++) {
        if ((unsigned char)*p < 0x20 || strchr("/\\:*?\"<>|", *p))
            return false;
    }
    return true;
}

// "20261014-153000.zip" hoặc "20261014-153000-2.zip"
static bool valid_file(const char *f) {
    size_t n = strlen(f);
    if (n < 19 || n == 20 || n >= sizeof(((BackupInfo *)0)->file) || strcmp(f + n - 4, ".zip") != 0)
        return false;
    for (size_t i = 0; i < n - 4; i++) {
        bool digit = f[i] >= '0' && f[i] <= '9';
        if (i == 8 || (i == 15 && n > 19)) {
            if (f[i] != '-')
                return false;
        } else if (!digit) {
            return false;
        }
    }
    return true;
}

static void dir_of(const char *id, char *out, size_t size) {
    snprintf(out, size, "%s/backups/%s", platform_data_dir(), id);
}

bool backup_file_path(const char *id, const char *file, char *out, size_t size) {
    if (!backup_valid_id(id) || !valid_file(file))
        return false;
    snprintf(out, size, "%s/backups/%s/%s", platform_data_dir(), id, file);
    return true;
}

// Đường dẫn tương đối an toàn trong zip: không thoát ra ngoài thư mục đích
static bool safe_rel(const char *s) {
    if (!*s || *s == '/')
        return false;
    for (const char *p = s; *p; p++) {
        if ((unsigned char)*p < 0x20 || *p == '\\' || *p == ':')
            return false;
    }
    for (const char *seg = s; seg;) {
        const char *end = strchr(seg, '/');
        size_t len = end ? (size_t)(end - seg) : strlen(seg);
        if (len == 0 || (len == 1 && seg[0] == '.') || (len == 2 && seg[0] == '.' && seg[1] == '.'))
            return false;
        seg = end ? end + 1 : NULL;
    }
    return true;
}

// ---- Danh sách ----

static int cmp_newest(const void *a, const void *b) {
    const BackupInfo *x = a, *y = b;
    int c = strncmp(y->file, x->file, 15);
    if (c)
        return c;
    // Cùng giây: bản có hậu tố -N mới hơn
    return (int)strlen(y->file) - (int)strlen(x->file);
}

void backup_list(const char *id, BackupList *out) {
    out->items = NULL;
    out->count = 0;
    if (!backup_valid_id(id))
        return;
    char dir[600];
    dir_of(id, dir, sizeof(dir));
    DIR *d = opendir(dir);
    if (!d)
        return;
    int cap = 0;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (!valid_file(e->d_name))
            continue;
        char path[768];
        snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
        struct stat st;
        if (stat(path, &st) != 0 || !S_ISREG(st.st_mode))
            continue;
        if (out->count == cap) {
            cap = cap ? cap * 2 : 8;
            BackupInfo *n = realloc(out->items, (size_t)cap * sizeof(BackupInfo));
            if (!n)
                break;
            out->items = n;
        }
        BackupInfo *b = &out->items[out->count++];
        snprintf(b->file, sizeof(b->file), "%s", e->d_name);
        const char *f = e->d_name;
        snprintf(b->when, sizeof(b->when), "%.4s-%.2s-%.2s %.2s:%.2s:%.2s", f, f + 4, f + 6, f + 9, f + 11, f + 13);
        b->size = (long)st.st_size;
    }
    closedir(d);
    if (out->count > 1)
        qsort(out->items, (size_t)out->count, sizeof(BackupInfo), cmp_newest);
}

void backup_list_free(BackupList *list) {
    free(list->items);
    list->items = NULL;
    list->count = 0;
}

static int cmp_str(const void *a, const void *b) {
    return strcasecmp(*(char *const *)a, *(char *const *)b);
}

int backup_ids(char ***out) {
    *out = NULL;
    char dir[600];
    snprintf(dir, sizeof(dir), "%s/backups", platform_data_dir());
    DIR *d = opendir(dir);
    if (!d)
        return 0;
    int count = 0, cap = 0;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (!backup_valid_id(e->d_name))
            continue;
        BackupList l;
        backup_list(e->d_name, &l);
        int n = l.count;
        backup_list_free(&l);
        if (n == 0)
            continue;
        if (count == cap) {
            cap = cap ? cap * 2 : 8;
            char **m = realloc(*out, (size_t)cap * sizeof(char *));
            if (!m)
                break;
            *out = m;
        }
        (*out)[count++] = strdup(e->d_name);
    }
    closedir(d);
    if (count > 1)
        qsort(*out, (size_t)count, sizeof(char *), cmp_str);
    return count;
}

void backup_ids_free(char **ids, int count) {
    for (int i = 0; i < count; i++)
        free(ids[i]);
    free(ids);
}

// ---- backup.ini ----

static void meta_value(const char *ini, const char *key, char *out, size_t size) {
    out[0] = '\0';
    size_t klen = strlen(key);
    for (const char *line = ini; line && *line;) {
        if (strncmp(line, key, klen) == 0 && line[klen] == '=') {
            const char *v = line + klen + 1;
            size_t n = strcspn(v, "\r\n");
            if (n >= size)
                n = size - 1;
            memcpy(out, v, n);
            out[n] = '\0';
            return;
        }
        line = strchr(line, '\n');
        if (line)
            line++;
    }
}

static bool meta_from_zip(ZipFile *z, BackupMeta *out) {
    memset(out, 0, sizeof(*out));
    size_t len;
    char *ini = (char *)zip_read(z, META_NAME, &len);
    if (!ini)
        return false;
    meta_value(ini, "id", out->id, sizeof(out->id));
    meta_value(ini, "title", out->title, sizeof(out->title));
    meta_value(ini, "game", out->game, sizeof(out->game));
    meta_value(ini, "created", out->created, sizeof(out->created));
    free(ini);
    if (!out->title[0])
        snprintf(out->title, sizeof(out->title), "%s", out->id);
    return backup_valid_id(out->id);
}

bool backup_read_meta(const char *zip_path, BackupMeta *out) {
    ZipFile *z = zip_open_file_lazy(zip_path);
    if (!z) {
        memset(out, 0, sizeof(*out));
        return false;
    }
    bool ok = meta_from_zip(z, out);
    zip_close(z);
    return ok;
}

// ---- Ghi zip ----

typedef struct {
    char *name;
    uint32_t crc, csize, size, offset;
    uint16_t method, time, date;
} ZwEntry;

typedef struct {
    FILE *f;
    ZwEntry *e;
    int n, cap;
    bool ok;
} ZipW;

static void wr16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void wr32(uint8_t *p, uint32_t v) {
    wr16(p, (uint16_t)v);
    wr16(p + 2, (uint16_t)(v >> 16));
}

static void dos_time(time_t t, uint16_t *time_out, uint16_t *date_out) {
    struct tm *tm = localtime(&t);
    if (!tm || tm->tm_year < 80) {
        *time_out = 0;
        *date_out = (1 << 5) | 1;   // 1980-01-01
        return;
    }
    *time_out = (uint16_t)((tm->tm_hour << 11) | (tm->tm_min << 5) | (tm->tm_sec / 2));
    *date_out = (uint16_t)(((tm->tm_year - 80) << 9) | ((tm->tm_mon + 1) << 5) | tm->tm_mday);
}

// Thêm 1 file từ bộ nhớ; nén deflate nếu pack và nhỏ đi được
static void zw_add(ZipW *z, const char *name, const uint8_t *data, size_t len, time_t mtime, bool pack) {
    if (!z->ok)
        return;
    if (z->n == z->cap) {
        int cap = z->cap ? z->cap * 2 : 32;
        ZwEntry *e = realloc(z->e, (size_t)cap * sizeof(ZwEntry));
        if (!e) {
            z->ok = false;
            return;
        }
        z->e = e;
        z->cap = cap;
    }
    uint8_t *packed = NULL;
    uLong packed_len = 0;
    if (pack && len > 0) {
        z_stream s;
        memset(&s, 0, sizeof(s));
        if (deflateInit2(&s, 6, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY) == Z_OK) {
            uLong bound = deflateBound(&s, (uLong)len);
            packed = malloc(bound);
            if (packed) {
                s.next_in = (Bytef *)data;
                s.avail_in = (uInt)len;
                s.next_out = packed;
                s.avail_out = (uInt)bound;
                if (deflate(&s, Z_FINISH) == Z_STREAM_END && s.total_out < len) {
                    packed_len = s.total_out;
                } else {
                    free(packed);
                    packed = NULL;
                }
            }
            deflateEnd(&s);
        }
    }

    ZwEntry *e = &z->e[z->n];
    e->name = strdup(name);
    e->crc = (uint32_t)crc32(0L, data, (uInt)len);
    e->size = (uint32_t)len;
    e->method = packed ? 8 : 0;
    e->csize = packed ? (uint32_t)packed_len : (uint32_t)len;
    e->offset = (uint32_t)ftell(z->f);
    dos_time(mtime, &e->time, &e->date);
    if (!e->name) {
        free(packed);
        z->ok = false;
        return;
    }
    z->n++;

    uint8_t h[30];
    size_t name_len = strlen(name);
    wr32(h, 0x04034b50);
    wr16(h + 4, 20);
    wr16(h + 6, 0x0800);    // tên file UTF-8
    wr16(h + 8, e->method);
    wr16(h + 10, e->time);
    wr16(h + 12, e->date);
    wr32(h + 14, e->crc);
    wr32(h + 18, e->csize);
    wr32(h + 22, e->size);
    wr16(h + 26, (uint16_t)name_len);
    wr16(h + 28, 0);
    const uint8_t *body = packed ? packed : data;
    if (fwrite(h, 1, 30, z->f) != 30 || fwrite(name, 1, name_len, z->f) != name_len ||
        (e->csize && fwrite(body, 1, e->csize, z->f) != e->csize))
        z->ok = false;
    free(packed);
}

static uint8_t *read_all(const char *path, size_t *len) {
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *buf = size >= 0 ? malloc((size_t)size + 1) : NULL;
    if (buf && fread(buf, 1, (size_t)size, f) != (size_t)size) {
        free(buf);
        buf = NULL;
    }
    fclose(f);
    if (buf)
        *len = (size_t)size;
    return buf;
}

static void zw_add_file(ZipW *z, const char *name, const char *path, bool pack) {
    struct stat st;
    if (!z->ok || stat(path, &st) != 0 || !S_ISREG(st.st_mode))
        return;
    size_t len = 0;
    uint8_t *data = read_all(path, &len);
    if (!data) {
        z->ok = false;
        return;
    }
    zw_add(z, name, data, len, st.st_mtime, pack);
    free(data);
}

static void zw_add_tree(ZipW *z, const char *dir, const char *prefix) {
    DIR *d = opendir(dir);
    if (!d)
        return;
    struct dirent *e;
    while (z->ok && (e = readdir(d))) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
            continue;
        char path[768], name[768];
        snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
        snprintf(name, sizeof(name), "%s/%s", prefix, e->d_name);
        if (is_dir(path))
            zw_add_tree(z, path, name);
        else
            zw_add_file(z, name, path, true);
    }
    closedir(d);
}

static bool zw_finish(ZipW *z) {
    long cd_off = ftell(z->f);
    for (int i = 0; z->ok && i < z->n; i++) {
        ZwEntry *e = &z->e[i];
        uint8_t h[46];
        size_t name_len = strlen(e->name);
        memset(h, 0, sizeof(h));
        wr32(h, 0x02014b50);
        wr16(h + 4, 20);
        wr16(h + 6, 20);
        wr16(h + 8, 0x0800);
        wr16(h + 10, e->method);
        wr16(h + 12, e->time);
        wr16(h + 14, e->date);
        wr32(h + 16, e->crc);
        wr32(h + 20, e->csize);
        wr32(h + 24, e->size);
        wr16(h + 28, (uint16_t)name_len);
        wr32(h + 42, e->offset);
        if (fwrite(h, 1, 46, z->f) != 46 || fwrite(e->name, 1, name_len, z->f) != name_len)
            z->ok = false;
    }
    long cd_end = ftell(z->f);
    uint8_t eocd[22];
    memset(eocd, 0, sizeof(eocd));
    wr32(eocd, 0x06054b50);
    wr16(eocd + 8, (uint16_t)z->n);
    wr16(eocd + 10, (uint16_t)z->n);
    wr32(eocd + 12, (uint32_t)(cd_end - cd_off));
    wr32(eocd + 16, (uint32_t)cd_off);
    if (z->ok && fwrite(eocd, 1, sizeof(eocd), z->f) != sizeof(eocd))
        z->ok = false;
    for (int i = 0; i < z->n; i++)
        free(z->e[i].name);
    free(z->e);
    if (fclose(z->f) != 0)
        z->ok = false;
    return z->ok;
}

// ---- Tạo / xoá ----

static void options_path(const char *id, char *out, size_t size) {
    snprintf(out, size, "%s/options/%s.ini", platform_data_dir(), id);
}

static bool dir_has_files(const char *dir) {
    DIR *d = opendir(dir);
    if (!d)
        return false;
    bool found = false;
    struct dirent *e;
    while (!found && (e = readdir(d))) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
            continue;
        char sub[768];
        snprintf(sub, sizeof(sub), "%s/%s", dir, e->d_name);
        found = is_dir(sub) ? dir_has_files(sub) : true;
    }
    closedir(d);
    return found;
}

bool backup_has_data(const char *id) {
    char path[600];
    struct stat st;
    options_path(id, path, sizeof(path));
    if (stat(path, &st) == 0)
        return true;
    snprintf(path, sizeof(path), "%s/rms/%s", platform_data_dir(), id);
    if (dir_has_files(path))
        return true;
    snprintf(path, sizeof(path), "%s/files/%s", platform_data_dir(), id);
    return dir_has_files(path);
}

// Bỏ xuống dòng khỏi giá trị ghi vào backup.ini
static void one_line(char *out, size_t size, const char *s) {
    snprintf(out, size, "%s", s ? s : "");
    for (char *p = out; *p; p++) {
        if (*p == '\r' || *p == '\n')
            *p = ' ';
    }
}

// Tên file chưa có trong dir: <stamp>.zip, <stamp>-2.zip...
static void free_name(const char *dir, const char *stamp, char *out, size_t size) {
    for (int i = 1; i < 100; i++) {
        if (i == 1)
            snprintf(out, size, "%s.zip", stamp);
        else
            snprintf(out, size, "%s-%d.zip", stamp, i);
        char path[768];
        struct stat st;
        snprintf(path, sizeof(path), "%s/%s", dir, out);
        if (stat(path, &st) != 0)
            return;
    }
}

bool backup_create(const char *id, const char *game, const char *title, char *file_out, size_t file_size,
                   char *err, size_t err_size) {
    if (!backup_valid_id(id)) {
        snprintf(err, err_size, "%s", tr(S_BACKUP_ERR_WRITE));
        return false;
    }
    char dir[600];
    dir_of(id, dir, sizeof(dir));
    mkdirs(dir);

    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char stamp[24] = "19800101-000000", created[24] = "1980-01-01 00:00:00";
    if (tm) {
        strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", tm);
        strftime(created, sizeof(created), "%Y-%m-%d %H:%M:%S", tm);
    }
    char file[48];
    free_name(dir, stamp, file, sizeof(file));
    char path[768], tmp[800];
    snprintf(path, sizeof(path), "%s/%s", dir, file);
    snprintf(tmp, sizeof(tmp), "%s.part", path);

    ZipW z = { .f = fopen(tmp, "wb"), .ok = true };
    if (!z.f) {
        snprintf(err, err_size, "%s", tr(S_BACKUP_ERR_WRITE));
        return false;
    }

    // File game: .jar để nguyên (đã nén sẵn), .jad cùng tên nếu có
    char game_line[256] = "";
    if (game && *game && safe_rel(game)) {
        char jar[768], name[300];
        snprintf(jar, sizeof(jar), "%s/%s", platform_games_dir(), game);
        struct stat st;
        if (stat(jar, &st) == 0 && S_ISREG(st.st_mode)) {
            one_line(game_line, sizeof(game_line), game);
            snprintf(name, sizeof(name), "games/%s", game);
            zw_add_file(&z, name, jar, false);
            static const char *exts[] = { ".jad", ".JAD" };
            char *dot = strrchr(jar, '.');
            char *name_dot = strrchr(name, '.');
            for (size_t i = 0; dot && name_dot && i < 2; i++) {
                strcpy(dot, exts[i]);
                strcpy(name_dot, exts[i]);
                if (stat(jar, &st) == 0 && S_ISREG(st.st_mode)) {
                    zw_add_file(&z, name, jar, true);
                    break;
                }
            }
        }
    }

    char src[600], prefix[300];
    snprintf(src, sizeof(src), "%s/rms/%s", platform_data_dir(), id);
    snprintf(prefix, sizeof(prefix), "rms/%s", id);
    zw_add_tree(&z, src, prefix);
    snprintf(src, sizeof(src), "%s/files/%s", platform_data_dir(), id);
    snprintf(prefix, sizeof(prefix), "files/%s", id);
    zw_add_tree(&z, src, prefix);
    options_path(id, src, sizeof(src));
    snprintf(prefix, sizeof(prefix), "options/%s.ini", id);
    zw_add_file(&z, prefix, src, true);

    char title_line[128], ini[1024];
    one_line(title_line, sizeof(title_line), title && *title ? title : id);
    int n = snprintf(ini, sizeof(ini), "id=%s\ntitle=%s\ngame=%s\ncreated=%s\napp=" APP_VERSION_STR "\n", id,
                     title_line, game_line, created);
    zw_add(&z, META_NAME, (const uint8_t *)ini, (size_t)n, now, true);

    if (!zw_finish(&z) || rename(tmp, path) != 0) {
        remove(tmp);
        snprintf(err, err_size, "%s", tr(S_BACKUP_ERR_WRITE));
        return false;
    }
    snprintf(file_out, file_size, "%s", file);
    return true;
}

bool backup_delete(const char *id, const char *file) {
    char path[768];
    if (!backup_file_path(id, file, path, sizeof(path)) || remove(path) != 0)
        return false;
    // Hết backup thì bỏ luôn thư mục rỗng
    char dir[600];
    dir_of(id, dir, sizeof(dir));
    rmdir(dir);
    return true;
}

// ---- Áp dụng ----

static bool write_file(const char *path, const uint8_t *data, size_t len) {
    mkdirs_parent(path);
    FILE *f = fopen(path, "wb");
    if (!f)
        return false;
    bool ok = fwrite(data, 1, len, f) == len;
    if (fclose(f) != 0)
        ok = false;
    return ok;
}

bool backup_apply(const char *id, const char *file, char *err, size_t err_size) {
    char path[768];
    ZipFile *z = backup_file_path(id, file, path, sizeof(path)) ? zip_open_file_lazy(path) : NULL;
    if (!z) {
        snprintf(err, err_size, "%s", tr(S_BACKUP_ERR_READ));
        return false;
    }
    BackupMeta meta;
    if (!meta_from_zip(z, &meta)) {
        zip_close(z);
        snprintf(err, err_size, "%s", tr(S_BACKUP_ERR_NOT_BACKUP));
        return false;
    }

    // Thay toàn bộ dữ liệu hiện tại bằng bản trong backup
    char rms[600], files[600], opts[600];
    snprintf(rms, sizeof(rms), "%s/rms/%s", platform_data_dir(), id);
    snprintf(files, sizeof(files), "%s/files/%s", platform_data_dir(), id);
    options_path(id, opts, sizeof(opts));
    rm_tree(rms);
    rm_tree(files);
    remove(opts);

    char rms_prefix[256], files_prefix[256], opts_name[256];
    snprintf(rms_prefix, sizeof(rms_prefix), "rms/%s/", meta.id);
    snprintf(files_prefix, sizeof(files_prefix), "files/%s/", meta.id);
    snprintf(opts_name, sizeof(opts_name), "options/%s.ini", meta.id);
    size_t rms_len = strlen(rms_prefix), files_len = strlen(files_prefix);

    bool ok = true;
    for (int i = 0; ok && i < zip_count(z); i++) {
        const char *name = zip_name(z, i);
        char dst[1024];
        bool only_new = false;
        if (strncmp(name, rms_prefix, rms_len) == 0 && safe_rel(name + rms_len)) {
            snprintf(dst, sizeof(dst), "%s/%s", rms, name + rms_len);
        } else if (strncmp(name, files_prefix, files_len) == 0 && safe_rel(name + files_len)) {
            snprintf(dst, sizeof(dst), "%s/%s", files, name + files_len);
        } else if (strcmp(name, opts_name) == 0) {
            snprintf(dst, sizeof(dst), "%s", opts);
        } else if (strncmp(name, "games/", 6) == 0 && safe_rel(name + 6)) {
            // File game: không ghi đè bản đang có (có thể là phiên bản mới hơn)
            snprintf(dst, sizeof(dst), "%s/%s", platform_games_dir(), name + 6);
            only_new = true;
        } else {
            continue;
        }
        struct stat st;
        if (only_new && stat(dst, &st) == 0)
            continue;
        size_t len;
        uint8_t *data = zip_read(z, name, &len);
        ok = data && write_file(dst, data, len);
        free(data);
    }
    zip_close(z);
    if (!ok)
        snprintf(err, err_size, "%s", tr(S_BACKUP_ERR_RESTORE));
    return ok;
}

// ---- Nhận từ điện thoại ----

bool backup_import(const char *zip_path, char *id_out, size_t id_size, char *file_out, size_t file_size,
                   char *err, size_t err_size) {
    BackupMeta meta;
    if (!backup_read_meta(zip_path, &meta)) {
        remove(zip_path);
        snprintf(err, err_size, "%s", tr(S_BACKUP_ERR_NOT_BACKUP));
        return false;
    }
    // Giữ thời gian tạo của backup trong tên file
    char stamp[24];
    int y, mo, d, h, mi, s;
    if (sscanf(meta.created, "%d-%d-%d %d:%d:%d", &y, &mo, &d, &h, &mi, &s) == 6 && y >= 1980 && y <= 9999)
        snprintf(stamp, sizeof(stamp), "%04d%02d%02d-%02d%02d%02d", y, mo % 100, d % 100, h % 100, mi % 100, s % 100);
    else {
        time_t now = time(NULL);
        struct tm *tm = localtime(&now);
        if (!tm || !strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", tm))
            snprintf(stamp, sizeof(stamp), "19800101-000000");
    }

    char dir[600];
    dir_of(meta.id, dir, sizeof(dir));
    mkdirs(dir);
    // Đã có đúng file này (gửi lại lần nữa): bỏ bản mới nhận
    struct stat in_st, st;
    char path[768];
    snprintf(path, sizeof(path), "%s/%s.zip", dir, stamp);
    if (stat(zip_path, &in_st) == 0 && stat(path, &st) == 0 && st.st_size == in_st.st_size) {
        remove(zip_path);
        snprintf(id_out, id_size, "%s", meta.id);
        snprintf(file_out, file_size, "%s.zip", stamp);
        return true;
    }
    char file[48];
    free_name(dir, stamp, file, sizeof(file));
    snprintf(path, sizeof(path), "%s/%s", dir, file);
    if (rename(zip_path, path) != 0) {
        remove(zip_path);
        snprintf(err, err_size, "%s", tr(S_BACKUP_ERR_WRITE));
        return false;
    }
    snprintf(id_out, id_size, "%s", meta.id);
    snprintf(file_out, file_size, "%s", file);
    return true;
}
