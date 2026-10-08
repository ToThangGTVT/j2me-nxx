#include "update.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <SDL.h>

#include "http.h"
#include "platform.h"

#ifdef __SWITCH__
#include <switch.h>
#endif

#define API_URL  "https://api.github.com/repos/" UPDATE_REPO "/releases/latest"
#define JSON_MAX (512 * 1024)

static SDL_mutex *lock;
static SDL_Thread *worker;
static volatile bool cancel_flag;
static UpdateState state = UPDATE_IDLE;
static char self_path[512];
static char latest[32];
static char asset_url[1024];
static char notes[2048];
static char error[192];
static bool has_new;
static int64_t done_bytes, total_bytes;
static Uint32 started_at;

static void set_state(UpdateState s) {
    SDL_LockMutex(lock);
    state = s;
    SDL_UnlockMutex(lock);
}

static void set_error(UpdateState s, const char *msg) {
    printf("[update] %s\n", msg);
    SDL_LockMutex(lock);
    snprintf(error, sizeof(error), "%s", msg);
    state = s;
    SDL_UnlockMutex(lock);
}

void update_init(const char *argv0) {
    if (!lock)
        lock = SDL_CreateMutex();
#ifdef __SWITCH__
    // hbmenu truyền đường dẫn .nro qua argv[0]
    if (argv0 && strncmp(argv0, "sdmc:/", 6) == 0 && strstr(argv0, ".nro"))
        snprintf(self_path, sizeof(self_path), "%s", argv0);
    else
        snprintf(self_path, sizeof(self_path), "sdmc:/switch/" UPDATE_ASSET);
#else
    // Desktop: không có .nro để thay, tải vào thư mục dữ liệu (để thử)
    (void)argv0;
    const char *p = SDL_getenv("J2ME_NX_UPDATE_PATH");
    if (p)
        snprintf(self_path, sizeof(self_path), "%s", p);
    else
        snprintf(self_path, sizeof(self_path), "%s/" UPDATE_ASSET, platform_data_dir());
#endif
}

bool update_supported(void) {
    return http_supported();
}

UpdateState update_state(void) {
    SDL_LockMutex(lock);
    UpdateState s = state;
    SDL_UnlockMutex(lock);
    return s;
}

const char *update_latest_version(void) {
    return latest;
}

bool update_available(void) {
    SDL_LockMutex(lock);
    bool r = has_new;
    SDL_UnlockMutex(lock);
    return r;
}

const char *update_notes(void) {
    return notes;
}

void update_error(char *out, size_t size) {
    SDL_LockMutex(lock);
    snprintf(out, size, "%s", error);
    SDL_UnlockMutex(lock);
}

void update_progress(int64_t *done, int64_t *total, int *bytes_per_sec) {
    SDL_LockMutex(lock);
    *done = done_bytes;
    *total = total_bytes;
    Uint32 ms = SDL_GetTicks() - started_at;
    *bytes_per_sec = ms > 500 ? (int)(done_bytes * 1000 / ms) : 0;
    SDL_UnlockMutex(lock);
}

// ---------------------------------------------------------------------------
// Phiên bản, JSON

// "v1.2.3" -> 10203... so sánh từng phần
static void parse_version(const char *s, int v[3]) {
    v[0] = v[1] = v[2] = 0;
    if (*s == 'v' || *s == 'V')
        s++;
    sscanf(s, "%d.%d.%d", &v[0], &v[1], &v[2]);
}

static bool newer(const char *remote, const char *local) {
    int a[3], b[3];
    parse_version(remote, a);
    parse_version(local, b);
    for (int i = 0; i < 3; i++) {
        if (a[i] != b[i])
            return a[i] > b[i];
    }
    return false;
}

static void put_utf8(char **o, char *end, unsigned cp) {
    char tmp[4];
    int n;
    if (cp < 0x80) {
        tmp[0] = (char)cp;
        n = 1;
    } else if (cp < 0x800) {
        tmp[0] = (char)(0xC0 | (cp >> 6));
        tmp[1] = (char)(0x80 | (cp & 0x3F));
        n = 2;
    } else {
        tmp[0] = (char)(0xE0 | (cp >> 12));
        tmp[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        tmp[2] = (char)(0x80 | (cp & 0x3F));
        n = 3;
    }
    if (*o + n < end) {
        memcpy(*o, tmp, (size_t)n);
        *o += n;
    }
}

// Giá trị chuỗi của khoá key tính từ from; trả về vị trí sau giá trị, NULL nếu không thấy
static const char *json_string(const char *from, const char *key, char *out, size_t size) {
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char *p = strstr(from, pat);
    if (!p)
        return NULL;
    p += strlen(pat);
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == ':')
        p++;
    if (*p != '"')
        return NULL;
    p++;
    char *o = out, *end = out + size - 1;
    while (*p && *p != '"') {
        if (*p == '\\' && p[1]) {
            p++;
            switch (*p) {
            case 'n': if (o < end) *o++ = '\n'; break;
            case 't': if (o < end) *o++ = ' '; break;
            case 'r': break;
            case 'u': {
                unsigned cp = 0;
                if (sscanf(p + 1, "%4x", &cp) == 1)
                    p += 4;
                if (cp >= 0xD800 && cp <= 0xDFFF)
                    cp = 0;         // emoji (cặp surrogate): bỏ, font không có
                if (cp)
                    put_utf8(&o, end, cp);
                break;
            }
            default: if (o < end) *o++ = *p; break;
            }
            p++;
        } else {
            if (o < end)
                *o++ = *p;
            p++;
        }
    }
    *o = 0;
    return *p == '"' ? p + 1 : p;
}

// Bỏ định dạng markdown và ký tự font không có (emoji...) để hiện ghi chú phát hành
static void clean_notes(const char *in, char *out, size_t size) {
    char *o = out, *end = out + size - 1;
    const char *line = in;
    while (*line && o < end) {
        const char *nl = strchr(line, '\n');
        size_t len = nl ? (size_t)(nl - line) : strlen(line);
        const char *s = line, *e = line + len;
        while (s < e && (*s == '#' || *s == ' ' || *s == '>'))
            s++;
        bool table = s < e && *s == '|';
        if (!table && s < e) {
            if (*s == '-' || *s == '*') {   // gạch đầu dòng
                s++;
                while (s < e && *s == ' ')
                    s++;
                if (o + 3 < end) {
                    memcpy(o, "\xE2\x80\xA2 ", 4);  // "• "
                    o += 4;
                }
            }
            for (const char *p = s; p < e && o < end;) {
                unsigned char ch = (unsigned char)*p;
                if (ch == '*' || ch == '`' || ch == '\r') {
                    p++;
                    continue;
                }
                int n = ch < 0x80 ? 1 : ch >= 0xF0 ? 4 : ch >= 0xE0 ? 3 : 2;
                if (n == 4) {   // ngoài BMP: emoji
                    p += 4;
                    continue;
                }
                if (n == 3) {
                    unsigned cp = ((ch & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
                    if (cp >= 0x2190 || (cp >= 0xFE00 && cp <= 0xFE0F)) {     // ký hiệu, emoji, variation selector
                        p += 3;
                        continue;
                    }
                }
                if (o + n > end)
                    break;
                if (ch == ' ' && (o == out || o[-1] == '\n' || o[-1] == ' ')) {  // khoảng trắng thừa (chỗ emoji đã bỏ)
                    p++;
                    continue;
                }
                memcpy(o, p, (size_t)n);
                o += n;
                p += n;
            }
            while (o > out && o[-1] == ' ')
                o--;
            if (o < end)
                *o++ = '\n';
        }
        line = nl ? nl + 1 : line + len;
    }
    while (o > out && o[-1] == '\n')
        o--;
    *o = 0;
}

// ---------------------------------------------------------------------------
// Kiểm tra bản mới

typedef struct {
    char *data;
    size_t len;
} Mem;

static bool mem_sink(void *ctx, const uint8_t *data, size_t len) {
    Mem *m = ctx;
    if (m->len + len >= JSON_MAX)
        return false;
    memcpy(m->data + m->len, data, len);
    m->len += len;
    m->data[m->len] = 0;
    return true;
}

static int check_thread(void *arg) {
    (void)arg;
    Mem m = { malloc(JSON_MAX), 0 };
    if (!m.data) {
        set_error(UPDATE_CHECK_FAILED, "out of memory");
        return 0;
    }
    m.data[0] = 0;
    char err[160];
    HttpOptions opt = { .accept = "application/vnd.github+json", .cancel = &cancel_flag, .sink = mem_sink, .ctx = &m };
    int status = http_get(API_URL, &opt, err, sizeof(err));
    if (status != 200) {
        if (status > 0)
            snprintf(err, sizeof(err), "GitHub HTTP %d", status);
        set_error(UPDATE_CHECK_FAILED, err);
        free(m.data);
        return 0;
    }

    char tag[32] = "", url[1024] = "";
    json_string(m.data, "tag_name", tag, sizeof(tag));
    // Tìm file .nro trong danh sách assets
    for (const char *p = m.data; (p = json_string(p, "browser_download_url", url, sizeof(url)));) {
        size_t n = strlen(url), an = strlen("/" UPDATE_ASSET);
        if (n > an && strcmp(url + n - an, "/" UPDATE_ASSET) == 0)
            break;
        url[0] = 0;
    }
    char *body = malloc(16384);
    if (body) {
        body[0] = 0;
        json_string(m.data, "body", body, 16384);
        SDL_LockMutex(lock);
        clean_notes(body, notes, sizeof(notes));
        SDL_UnlockMutex(lock);
        free(body);
    }
    free(m.data);

    if (!tag[0]) {
        set_error(UPDATE_CHECK_FAILED, "no release found");
        return 0;
    }
    const char *local = APP_VERSION_STR;
#ifndef __SWITCH__
    // Desktop: J2ME_NX_FAKE_VERSION=0.1.0 giả làm bản cũ để thử luồng cập nhật
    if (SDL_getenv("J2ME_NX_FAKE_VERSION"))
        local = SDL_getenv("J2ME_NX_FAKE_VERSION");
#endif
    SDL_LockMutex(lock);
    snprintf(latest, sizeof(latest), "%s", tag[0] == 'v' || tag[0] == 'V' ? tag + 1 : tag);
    snprintf(asset_url, sizeof(asset_url), "%s", url);
    has_new = newer(tag, local) && url[0];
    state = has_new ? UPDATE_AVAILABLE : UPDATE_LATEST;
    SDL_UnlockMutex(lock);
    printf("[update] latest %s, running %s%s\n", tag, local, has_new ? ": update available" : "");
    return 0;
}

static bool busy(void) {
    UpdateState s = update_state();
    return s == UPDATE_CHECKING || s == UPDATE_DOWNLOADING;
}

static void start_worker(int (*fn)(void *), const char *name) {
    if (worker) {
        SDL_WaitThread(worker, NULL);
        worker = NULL;
    }
    cancel_flag = false;
    worker = SDL_CreateThread(fn, name, NULL);
    if (!worker)
        set_error(UPDATE_FAILED, "cannot start thread");
}

void update_check(void) {
    if (!lock || !http_supported() || busy())
        return;
    set_state(UPDATE_CHECKING);
    start_worker(check_thread, "update-check");
}

// ---------------------------------------------------------------------------
// Tải và thay file

typedef struct {
    FILE *f;
    uint8_t head[32];
    size_t head_len;
} Dl;

static void dl_length(void *ctx, int64_t length) {
    (void)ctx;
    SDL_LockMutex(lock);
    total_bytes = length;
    SDL_UnlockMutex(lock);
}

static bool dl_sink(void *ctx, const uint8_t *data, size_t len) {
    Dl *d = ctx;
    if (d->head_len < sizeof(d->head)) {
        size_t n = sizeof(d->head) - d->head_len < len ? sizeof(d->head) - d->head_len : len;
        memcpy(d->head + d->head_len, data, n);
        d->head_len += n;
    }
    if (fwrite(data, 1, len, d->f) != len)
        return false;
    SDL_LockMutex(lock);
    done_bytes += (int64_t)len;
    SDL_UnlockMutex(lock);
    return !cancel_flag;
}

static int download_thread(void *arg) {
    (void)arg;
    char url[1024], tmp_path[600], old_path[600];
    SDL_LockMutex(lock);
    snprintf(url, sizeof(url), "%s", asset_url);
    SDL_UnlockMutex(lock);
    snprintf(tmp_path, sizeof(tmp_path), "%s.new", self_path);
    snprintf(old_path, sizeof(old_path), "%s.old", self_path);

    Dl d = { 0 };
    d.f = fopen(tmp_path, "wb");
    if (!d.f) {
        set_error(UPDATE_FAILED, "cannot write to SD card");
        return 0;
    }
    static char filebuf[256 * 1024];
    setvbuf(d.f, filebuf, _IOFBF, sizeof(filebuf));
    char err[160];
    HttpOptions opt = { .accept = "application/octet-stream", .cancel = &cancel_flag, .on_length = dl_length,
                        .sink = dl_sink, .ctx = &d };
    int status = http_get(url, &opt, err, sizeof(err));
    bool write_ok = fclose(d.f) == 0;
    if (cancel_flag) {
        remove(tmp_path);
        set_state(UPDATE_AVAILABLE);
        return 0;
    }

    int64_t got, want;
    int bps;
    update_progress(&got, &want, &bps);
    if (status != 200 || !write_ok) {
        remove(tmp_path);
        if (status > 0 && status != 200)
            snprintf(err, sizeof(err), "HTTP %d", status);
        else if (!write_ok)
            snprintf(err, sizeof(err), "SD card full?");
        set_error(UPDATE_FAILED, err);
        return 0;
    }
    // File .nro hợp lệ: đủ độ dài, có chữ "NRO0" ở byte 0x10
    if ((want > 0 && got != want) || d.head_len < 0x14 || memcmp(d.head + 0x10, "NRO0", 4) != 0) {
        remove(tmp_path);
        set_error(UPDATE_FAILED, "downloaded file is not a valid .nro");
        return 0;
    }
    // Thay file: cũ -> .old, mới -> tên chính, rồi xoá .old (lỗi thì trả lại file cũ)
    struct stat st;
    bool had_old = stat(self_path, &st) == 0;
    remove(old_path);
    if (had_old && rename(self_path, old_path) != 0) {
        remove(tmp_path);
        set_error(UPDATE_FAILED, "cannot replace the .nro file");
        return 0;
    }
    if (rename(tmp_path, self_path) != 0) {
        if (had_old)
            rename(old_path, self_path);
        remove(tmp_path);
        set_error(UPDATE_FAILED, "cannot replace the .nro file");
        return 0;
    }
    remove(old_path);
    SDL_LockMutex(lock);
    has_new = false;
    state = UPDATE_DONE;
    SDL_UnlockMutex(lock);
    return 0;
}

void update_download(void) {
    if (!lock || !http_supported() || busy() || !asset_url[0])
        return;
    SDL_LockMutex(lock);
    done_bytes = 0;
    total_bytes = -1;
    started_at = SDL_GetTicks();
    error[0] = 0;
    state = UPDATE_DOWNLOADING;
    SDL_UnlockMutex(lock);
    start_worker(download_thread, "update-download");
}

void update_cancel(void) {
    cancel_flag = true;
}

bool update_restart(void) {
#ifdef __SWITCH__
    // Chạy từ hbmenu: báo hbloader nạp file .nro mới khi app thoát
    if (!envHasNextLoad())
        return false;
    char args[600];
    snprintf(args, sizeof(args), "\"%s\"", self_path);
    return R_SUCCEEDED(envSetNextLoad(self_path, args));
#else
    return false;
#endif
}

void update_shutdown(void) {
    cancel_flag = true;
    if (worker) {
        SDL_WaitThread(worker, NULL);
        worker = NULL;
    }
}
