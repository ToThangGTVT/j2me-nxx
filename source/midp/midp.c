// Native cho Display (sự kiện / màn hình), MIDlet, RecordStore, bàn phím ảo
#include "midp.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <SDL.h>

#include "../vm/vm.h"

#define QUEUE_SIZE 256

typedef struct {
    int type, a, b;
} Event;

static MidpConfig cfg;
static Event queue[QUEUE_SIZE];
static int q_head, q_tail;
static bool paint_pending, serial_pending;
static VMThread *event_waiter;
// Luồng chính gửi phím / đọc màn hình mà không phải chờ luồng VM: hàng đợi và framebuffer
// có khoá riêng, chỉ giữ trong tích tắc
static SDL_mutex *q_lock, *fb_lock;
static jlong last_flush_ms;
static int flush_gap_max;           // khoảng cách lớn nhất giữa 2 khung hình game (ms)

static uint32_t *framebuffer;
static int fb_w, fb_h;
static bool fb_dirty;
static double next_frame_ms;        // mốc được phép đẩy khung hình tiếp theo (giới hạn FPS)

// Video (VideoControl USE_DIRECT_VIDEO) vẽ đè lên màn hình game. Khi có lớp phủ đang hiện,
// fb_base giữ hình game gốc, framebuffer = fb_base + các lớp phủ.
#define MAX_OVERLAYS 4
typedef struct {
    bool used, visible;
    int x, y, w, h;
    const uint32_t *frame;          // do video.c giữ, sống tới khi gỡ lớp phủ
    int fw, fh;
} Overlay;
static Overlay overlays[MAX_OVERLAYS];
static uint32_t *fb_base;
static bool fb_has_overlay;         // framebuffer đang có lớp phủ (fb_base hợp lệ)
// Khung hình nét cao (chữ mịn vẽ ở độ phân giải màn hình), gấp cfg.text_hires lần
static uint32_t *fb_hi;
static bool fb_hi_valid;
static bool exit_requested;

// ---------------------------------------------------------------------------
// Hàng đợi sự kiện

static void wake_waiter(void) {
    if (event_waiter && event_waiter->state == TS_WAIT_EVENT)
        event_waiter->state = TS_RUNNABLE;
}

static void push_event(int type, int a, int b) {
    SDL_LockMutex(q_lock);
    if (type == MIDP_EV_PAINT) {
        paint_pending = true;
    } else if (type == MIDP_EV_SERIAL) {
        serial_pending = true;
    } else {
        int next = (q_tail + 1) % QUEUE_SIZE;
        if (next != q_head) {
            queue[q_tail] = (Event){ type, a, b };
            q_tail = next;
        }       // đầy: bỏ sự kiện
    }
    SDL_UnlockMutex(q_lock);
}

void midp_post_event(int type, int a, int b) {
    push_event(type, a, b);
    wake_waiter();
}

void midp_post_event_async(int type, int a, int b) {
    push_event(type, a, b);
}

void midp_post_key(int code, bool pressed) {
    push_event(pressed ? MIDP_EV_KEY_PRESSED : MIDP_EV_KEY_RELEASED, code, 0);
}

void midp_poll_events(void) {
    SDL_LockMutex(q_lock);
    bool any = q_head != q_tail || paint_pending || serial_pending;
    SDL_UnlockMutex(q_lock);
    if (any)
        wake_waiter();
}

const uint32_t *midp_framebuffer_lock(int *w, int *h, bool *dirty) {
    SDL_LockMutex(fb_lock);
    *w = fb_w;
    *h = fb_h;
    if (dirty) {
        *dirty = fb_dirty;
        fb_dirty = false;
    }
    return framebuffer;
}

const uint32_t *midp_framebuffer_hires(int *k) {
    *k = cfg.text_hires;
    return fb_hi_valid && !fb_has_overlay ? fb_hi : NULL;
}

void midp_framebuffer_unlock(void) {
    SDL_UnlockMutex(fb_lock);
}

int midp_take_frame_gap_max(void) {
    SDL_LockMutex(fb_lock);
    int v = flush_gap_max;
    flush_gap_max = 0;
    SDL_UnlockMutex(fb_lock);
    return v;
}

bool midp_exit_requested(void) {
    return exit_requested;
}

// ---------------------------------------------------------------------------
// Display

static NativeResult Display_screenWidth0(VMThread *t, Value *args, Value *ret) {
    (void)t;
    (void)args;
    ret->i = cfg.screen_w;
    return NATIVE_OK;
}

static NativeResult Display_screenHeight0(VMThread *t, Value *args, Value *ret) {
    (void)t;
    (void)args;
    ret->i = cfg.screen_h;
    return NATIVE_OK;
}

static NativeResult Display_waitEvent0(VMThread *t, Value *args, Value *ret) {
    (void)ret;
    Object *arr = args[0].l;
    jint *ev = ARRAY_DATA(arr, jint);
    bool got = true;
    SDL_LockMutex(q_lock);
    if (q_head != q_tail) {
        Event e = queue[q_head];
        q_head = (q_head + 1) % QUEUE_SIZE;
        ev[0] = e.type;
        ev[1] = e.a;
        ev[2] = e.b;
    } else if (paint_pending) {
        paint_pending = false;
        ev[0] = MIDP_EV_PAINT;
    } else if (serial_pending) {
        serial_pending = false;
        ev[0] = MIDP_EV_SERIAL;
    } else {
        got = false;
    }
    SDL_UnlockMutex(q_lock);
    if (got)
        return NATIVE_OK;
    t->state = TS_WAIT_EVENT;
    event_waiter = t;
    return NATIVE_RETRY;
}

static NativeResult Display_postRepaint0(VMThread *t, Value *args, Value *ret) {
    (void)t;
    (void)args;
    (void)ret;
    midp_post_event(MIDP_EV_PAINT, 0, 0);
    return NATIVE_OK;
}

static NativeResult Display_postSerial0(VMThread *t, Value *args, Value *ret) {
    (void)t;
    (void)args;
    (void)ret;
    midp_post_event(MIDP_EV_SERIAL, 0, 0);
    return NATIVE_OK;
}

// ---------------------------------------------------------------------------
// Lớp phủ video (gọi khi đang giữ fb_lock)

static bool any_overlay_visible(void) {
    for (int i = 0; i < MAX_OVERLAYS; i++) {
        if (overlays[i].used && overlays[i].visible && overlays[i].frame)
            return true;
    }
    return false;
}

static void draw_overlay(const Overlay *o) {
    int x0 = o->x < 0 ? 0 : o->x, y0 = o->y < 0 ? 0 : o->y;
    int x1 = o->x + o->w > fb_w ? fb_w : o->x + o->w, y1 = o->y + o->h > fb_h ? fb_h : o->y + o->h;
    if (x0 >= x1 || y0 >= y1 || o->fw <= 0 || o->fh <= 0)
        return;
    for (int y = y0; y < y1; y++) {
        const uint32_t *src = o->frame + (size_t)((y - o->y) * o->fh / o->h) * o->fw;
        uint32_t *dst = framebuffer + (size_t)y * fb_w;
        for (int x = x0; x < x1; x++)
            dst[x] = src[(x - o->x) * o->fw / o->w] | 0xff000000u;
    }
}

// Dựng lại framebuffer từ hình game + lớp phủ
static void compose_locked(void) {
    if (!framebuffer)
        return;
    bool visible = any_overlay_visible();
    if (visible && !fb_has_overlay) {
        // Framebuffer lúc này chỉ có hình game: giữ lại làm nền
        if (!fb_base)
            fb_base = malloc((size_t)fb_w * fb_h * 4);
        if (!fb_base)
            return;
        memcpy(fb_base, framebuffer, (size_t)fb_w * fb_h * 4);
    }
    if (fb_has_overlay || visible)
        memcpy(framebuffer, fb_base, (size_t)fb_w * fb_h * 4);
    fb_has_overlay = visible;
    if (visible) {
        for (int i = 0; i < MAX_OVERLAYS; i++) {
            if (overlays[i].used && overlays[i].visible && overlays[i].frame)
                draw_overlay(&overlays[i]);
        }
    }
    fb_dirty = true;
}

int midp_overlay_add(void) {
    int id = -1;
    SDL_LockMutex(fb_lock);
    for (int i = 0; i < MAX_OVERLAYS; i++) {
        if (!overlays[i].used) {
            memset(&overlays[i], 0, sizeof(Overlay));
            overlays[i].used = true;
            id = i;
            break;
        }
    }
    SDL_UnlockMutex(fb_lock);
    return id;
}

void midp_overlay_set(int id, bool visible, int x, int y, int w, int h) {
    if (id < 0 || id >= MAX_OVERLAYS)
        return;
    SDL_LockMutex(fb_lock);
    Overlay *o = &overlays[id];
    o->visible = visible && w > 0 && h > 0;
    o->x = x;
    o->y = y;
    o->w = w;
    o->h = h;
    compose_locked();
    SDL_UnlockMutex(fb_lock);
}

void midp_overlay_frame(int id, const uint32_t *frame, int fw, int fh) {
    if (id < 0 || id >= MAX_OVERLAYS)
        return;
    SDL_LockMutex(fb_lock);
    Overlay *o = &overlays[id];
    o->frame = frame;
    o->fw = fw;
    o->fh = fh;
    if (o->visible)
        compose_locked();
    SDL_UnlockMutex(fb_lock);
}

void midp_overlay_remove(int id) {
    if (id < 0 || id >= MAX_OVERLAYS)
        return;
    SDL_LockMutex(fb_lock);
    memset(&overlays[id], 0, sizeof(Overlay));
    compose_locked();
    SDL_UnlockMutex(fb_lock);
}

static NativeResult Display_flush0(VMThread *t, Value *args, Value *ret) {
    (void)ret;
    Object *arr = args[0].l;
    int w = args[1].i, h = args[2].i;
    if (!arr || w != fb_w || h != fb_h || ARRAY_LEN(arr) < w * h)
        return NATIVE_OK;
    SDL_LockMutex(fb_lock);
    if (fb_has_overlay) {
        // Hình game mới vào fb_base rồi vẽ lại lớp phủ lên trên
        memcpy(fb_base, ARRAY_DATA(arr, uint32_t), (size_t)w * h * 4);
        compose_locked();
    } else {
        memcpy(framebuffer, ARRAY_DATA(arr, uint32_t), (size_t)w * h * 4);
    }
    if (cfg.text_hires >= 2 && !fb_hi) {
        int k = cfg.text_hires;
        fb_hi = malloc((size_t)w * k * h * k * 4);
    }
    fb_hi_valid = fb_hi && !fb_has_overlay && midp_text_compose(arr, ARRAY_DATA(arr, uint32_t), w, h, fb_hi);
    fb_dirty = true;
    jlong now_ms = vm_time_ms();
    if (last_flush_ms && now_ms - last_flush_ms > flush_gap_max)
        flush_gap_max = (int)(now_ms - last_flush_ms);
    last_flush_ms = now_ms;
    SDL_UnlockMutex(fb_lock);

    // Giới hạn FPS: cho thread vừa vẽ ngủ tới mốc khung hình kế tiếp
    if (cfg.fps_limit > 0) {
        double period = 1000.0 / cfg.fps_limit;
        double now = (double)vm_time_ms();
        if (next_frame_ms > now && next_frame_ms - now <= period) {
            t->state = TS_SLEEPING;
            t->wake_time = (jlong)(next_frame_ms + 0.999);
            next_frame_ms += period;
        } else {
            next_frame_ms = now + period;
        }
    }
    return NATIVE_OK;
}

static NativeResult Display_vibrate0(VMThread *t, Value *args, Value *ret) {
    (void)t;
    (void)ret;
    if (cfg.vibrate)
        cfg.vibrate(args[0].i);
    return NATIVE_OK;
}

// ---------------------------------------------------------------------------
// MIDlet

static NativeResult MIDlet_getAppProperty0(VMThread *t, Value *args, Value *ret) {
    char key[256];
    jstring_to_cstr(args[0].l, key, sizeof(key));
    const char *v = cfg.app_property ? cfg.app_property(key) : NULL;
    ret->l = v ? jstring_new_utf8(t, v) : NULL;
    return NATIVE_OK;
}

static NativeResult MIDlet_platformRequest0(VMThread *t, Value *args, Value *ret) {
    (void)t;
    (void)ret;
    char *url = jstring_to_utf8(args[0].l);
    if (url && cfg.platform_request)
        cfg.platform_request(url);
    free(url);
    return NATIVE_OK;
}

static NativeResult MIDlet_notifyDestroyed0(VMThread *t, Value *args, Value *ret) {
    (void)t;
    (void)args;
    (void)ret;
    vm_log("MIDlet.notifyDestroyed()");
    exit_requested = true;
    return NATIVE_OK;
}

// ---------------------------------------------------------------------------
// Bàn phím ảo

static NativeResult Keyboard_show0(VMThread *t, Value *args, Value *ret) {
    char *title = jstring_to_utf8(args[0].l);
    char *text = jstring_to_utf8(args[1].l);
    char *r = cfg.keyboard ? cfg.keyboard(title, text, args[2].i, args[3].i) : NULL;
    free(title);
    free(text);
    ret->l = r ? jstring_new_utf8(t, r) : NULL;
    free(r);
    return NATIVE_OK;
}

// ---------------------------------------------------------------------------
// RecordStore: mỗi store 1 file <rms_dir>/<tên đã mã hoá>.rms

static void mkdirs(const char *path) {
    char buf[512];
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

static void rms_path(Object *name, char *out, size_t size) {
    char *u = jstring_to_utf8(name);
    size_t pos = (size_t)snprintf(out, size, "%s/", cfg.rms_dir);
    for (const unsigned char *p = (const unsigned char *)u; *p && pos + 4 < size; p++) {
        if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '_' ||
            *p == '-' || *p == '.')
            out[pos++] = (char)*p;
        else
            pos += (size_t)snprintf(out + pos, size - pos, "%%%02X", *p);
    }
    snprintf(out + pos, size - pos, ".rms");
    free(u);
}

static NativeResult RecordStore_load0(VMThread *t, Value *args, Value *ret) {
    char path[600];
    rms_path(args[0].l, path, sizeof(path));
    FILE *f = fopen(path, "rb");
    if (!f) {
        ret->l = NULL;
        return NATIVE_OK;
    }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    Object *arr = heap_new_prim_array(t, 'B', (jint)(n > 0 ? n : 0));
    if (arr && n > 0 && fread(ARRAY_DATA(arr, uint8_t), 1, (size_t)n, f) != (size_t)n)
        arr = NULL;
    fclose(f);
    ret->l = arr;
    return NATIVE_OK;
}

static NativeResult RecordStore_save0(VMThread *t, Value *args, Value *ret) {
    (void)t;
    char path[600], tmp[620];
    mkdirs(cfg.rms_dir);
    rms_path(args[0].l, path, sizeof(path));
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    Object *data = args[1].l;
    FILE *f = fopen(tmp, "wb");
    if (!f) {
        vm_log("Khong ghi duoc %s: %s", tmp, strerror(errno));
        ret->i = 0;
        return NATIVE_OK;
    }
    size_t n = (size_t)ARRAY_LEN(data);
    bool ok = fwrite(ARRAY_DATA(data, uint8_t), 1, n, f) == n;
    ok = fclose(f) == 0 && ok;
    // Ghi file tạm rồi đổi tên để không hỏng save khi bị tắt ngang
    if (ok) {
        remove(path);
        ok = rename(tmp, path) == 0;
    }
    ret->i = ok;
    return NATIVE_OK;
}

static NativeResult RecordStore_delete0(VMThread *t, Value *args, Value *ret) {
    (void)t;
    char path[600];
    rms_path(args[0].l, path, sizeof(path));
    ret->i = remove(path) == 0;
    return NATIVE_OK;
}

static int hexval(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return 0;
}

static NativeResult RecordStore_list0(VMThread *t, Value *args, Value *ret) {
    (void)args;
    char names[64][128];
    int count = 0;
    DIR *d = opendir(cfg.rms_dir);
    if (d) {
        struct dirent *e;
        while ((e = readdir(d)) && count < 64) {
            size_t len = strlen(e->d_name);
            if (len < 5 || strcmp(e->d_name + len - 4, ".rms") != 0)
                continue;
            size_t o = 0;
            for (size_t i = 0; i < len - 4 && o + 1 < sizeof(names[0]); i++) {
                if (e->d_name[i] == '%' && i + 2 < len - 4) {
                    names[count][o++] = (char)(hexval(e->d_name[i + 1]) * 16 + hexval(e->d_name[i + 2]));
                    i += 2;
                } else {
                    names[count][o++] = e->d_name[i];
                }
            }
            names[count][o] = '\0';
            count++;
        }
        closedir(d);
    }
    Class *sc = class_load(t, "java/lang/String");
    Class *ac = sc ? class_array_of(t, sc) : NULL;
    Object *arr = ac ? heap_alloc_array(t, ac, count) : NULL;
    if (!arr)
        return NATIVE_EXCEPTION;
    for (int i = 0; i < count; i++)
        ARRAY_DATA(arr, Object *)[i] = jstring_new_utf8(t, names[i]);
    ret->l = arr;
    return NATIVE_OK;
}

// ---------------------------------------------------------------------------

void midp_register_natives(void) {
    static bool done;
    if (done)
        return;
    done = true;

    const char *D = "javax/microedition/lcdui/Display";
    native_register(D, "screenWidth0", "()I", Display_screenWidth0);
    native_register(D, "screenHeight0", "()I", Display_screenHeight0);
    native_register(D, "waitEvent0", "([I)V", Display_waitEvent0);
    native_register(D, "postRepaint0", "()V", Display_postRepaint0);
    native_register(D, "postSerial0", "()V", Display_postSerial0);
    native_register(D, "flush0", "([III)V", Display_flush0);
    native_register(D, "vibrate0", "(I)V", Display_vibrate0);

    const char *M = "javax/microedition/midlet/MIDlet";
    native_register(M, "getAppProperty0", "(Ljava/lang/String;)Ljava/lang/String;", MIDlet_getAppProperty0);
    native_register(M, "notifyDestroyed0", "()V", MIDlet_notifyDestroyed0);
    native_register(M, "platformRequest0", "(Ljava/lang/String;)V", MIDlet_platformRequest0);

    native_register("j2menx/Keyboard", "show0", "(Ljava/lang/String;Ljava/lang/String;II)Ljava/lang/String;",
                    Keyboard_show0);

    const char *R = "javax/microedition/rms/RecordStore";
    native_register(R, "load0", "(Ljava/lang/String;)[B", RecordStore_load0);
    native_register(R, "save0", "(Ljava/lang/String;[B)Z", RecordStore_save0);
    native_register(R, "delete0", "(Ljava/lang/String;)Z", RecordStore_delete0);
    native_register(R, "list0", "()[Ljava/lang/String;", RecordStore_list0);

    midp_graphics_register();
    midp_net_register();
    midp_tls_register();
    midp_audio_register();
    midp_fileio_register();
    midp_m3g_register();
    midp_video_register();
}

bool midp_start(const MidpConfig *c, const char *midlet_class) {
    cfg = *c;
    midp_graphics_set_text_style(cfg.smooth_text, cfg.font_scale, cfg.text_hires, cfg.screen_w, cfg.screen_h);
    if (!q_lock) {
        q_lock = SDL_CreateMutex();
        fb_lock = SDL_CreateMutex();
    }
    q_head = q_tail = 0;
    paint_pending = serial_pending = false;
    event_waiter = NULL;
    exit_requested = false;
    next_frame_ms = 0;

    last_flush_ms = 0;
    flush_gap_max = 0;
    fb_w = cfg.screen_w;
    fb_h = cfg.screen_h;
    free(framebuffer);
    framebuffer = calloc((size_t)fb_w * fb_h, 4);
    free(fb_base);
    fb_base = NULL;
    fb_has_overlay = false;
    free(fb_hi);
    fb_hi = NULL;
    fb_hi_valid = false;
    if (!cfg.smooth_text)
        cfg.text_hires = 0;
    memset(overlays, 0, sizeof(overlays));
    fb_dirty = true;

    vm_set_property("microedition.platform", cfg.platform ? cfg.platform : "Nokia6300/07.21");
    char keys[96];
    snprintf(keys, sizeof(keys), "%d,%d,%d,%d,%d,%d,%d,%d", cfg.keycodes[0], cfg.keycodes[1], cfg.keycodes[2],
             cfg.keycodes[3], cfg.keycodes[4], cfg.keycodes[5], cfg.keycodes[6], cfg.keycodes[7]);
    vm_set_property("j2menx.keys", keys);
    vm_set_property("microedition.configuration", "CLDC-1.1");
    vm_set_property("microedition.profiles", "MIDP-2.0");
    vm_set_property("microedition.encoding", "UTF-8");
    bool en = cfg.lang && strcmp(cfg.lang, "en") == 0;
    vm_set_property("microedition.locale", en ? "en-US" : "vi-VN");
    vm_set_property("j2menx.lang", en ? "en" : "vi");
    vm_set_property("microedition.media.version", "1.1");
    vm_set_property("supports.mixing", "false");
    // JSR-75 FileConnection
    vm_set_property("microedition.io.file.FileConnection.version", "1.0");
    vm_set_property("fileconn.dir.memorycard", "file:///E:/");
    vm_set_property("fileconn.dir.photos", "file:///E:/Images/");
    vm_set_property("fileconn.dir.music", "file:///E:/Sounds/");
    vm_set_property("fileconn.dir.private", "file:///C:/private/");
    if (cfg.files_dir)
        vm_set_property("j2menx.files", cfg.files_dir);

    if (!vm_spawn_static("javax/microedition/lcdui/Display", "eventLoop", "()V", NULL, 0))
        return false;

    Value arg;
    arg.l = jstring_new_utf8(NULL, midlet_class);
    if (!vm_spawn_static("javax/microedition/midlet/Launcher", "start", "(Ljava/lang/String;)V", &arg, 1))
        return false;
    return true;
}

void midp_shutdown(void) {
    if (fb_lock)
        SDL_LockMutex(fb_lock);
    free(framebuffer);
    framebuffer = NULL;
    free(fb_base);
    fb_base = NULL;
    free(fb_hi);
    fb_hi = NULL;
    fb_hi_valid = false;
    fb_has_overlay = false;
    memset(overlays, 0, sizeof(overlays));
    fb_w = fb_h = 0;
    if (fb_lock)
        SDL_UnlockMutex(fb_lock);
    event_waiter = NULL;
    midp_graphics_shutdown();
    midp_tls_shutdown();
    midp_net_shutdown();
    midp_audio_shutdown();
    midp_m3g_shutdown();
    midp_video_shutdown();
}
