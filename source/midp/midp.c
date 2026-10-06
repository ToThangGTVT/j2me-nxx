// Native cho Display (sự kiện / màn hình), MIDlet, RecordStore, bàn phím ảo
#include "midp.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

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

static uint32_t *framebuffer;
static int fb_w, fb_h;
static bool fb_dirty;
static double next_frame_ms;        // mốc được phép đẩy khung hình tiếp theo (giới hạn FPS)
static bool exit_requested;

// ---------------------------------------------------------------------------
// Hàng đợi sự kiện

static void wake_waiter(void) {
    if (event_waiter && event_waiter->state == TS_WAIT_EVENT)
        event_waiter->state = TS_RUNNABLE;
}

void midp_post_event(int type, int a, int b) {
    if (type == MIDP_EV_PAINT) {
        paint_pending = true;
    } else if (type == MIDP_EV_SERIAL) {
        serial_pending = true;
    } else {
        int next = (q_tail + 1) % QUEUE_SIZE;
        if (next == q_head)
            return;     // đầy: bỏ sự kiện
        queue[q_tail] = (Event){ type, a, b };
        q_tail = next;
    }
    wake_waiter();
}

void midp_post_key(int code, bool pressed) {
    midp_post_event(pressed ? MIDP_EV_KEY_PRESSED : MIDP_EV_KEY_RELEASED, code, 0);
}

const uint32_t *midp_framebuffer(int *w, int *h, bool *dirty) {
    *w = fb_w;
    *h = fb_h;
    if (dirty) {
        *dirty = fb_dirty;
        fb_dirty = false;
    }
    return framebuffer;
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
    if (q_head != q_tail) {
        Event e = queue[q_head];
        q_head = (q_head + 1) % QUEUE_SIZE;
        ev[0] = e.type;
        ev[1] = e.a;
        ev[2] = e.b;
        return NATIVE_OK;
    }
    if (paint_pending) {
        paint_pending = false;
        ev[0] = MIDP_EV_PAINT;
        return NATIVE_OK;
    }
    if (serial_pending) {
        serial_pending = false;
        ev[0] = MIDP_EV_SERIAL;
        return NATIVE_OK;
    }
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

static NativeResult Display_flush0(VMThread *t, Value *args, Value *ret) {
    (void)ret;
    Object *arr = args[0].l;
    int w = args[1].i, h = args[2].i;
    if (!arr || w != fb_w || h != fb_h || ARRAY_LEN(arr) < w * h)
        return NATIVE_OK;
    memcpy(framebuffer, ARRAY_DATA(arr, uint32_t), (size_t)w * h * 4);
    fb_dirty = true;

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
}

bool midp_start(const MidpConfig *c, const char *midlet_class) {
    cfg = *c;
    q_head = q_tail = 0;
    paint_pending = serial_pending = false;
    event_waiter = NULL;
    exit_requested = false;
    next_frame_ms = 0;

    fb_w = cfg.screen_w;
    fb_h = cfg.screen_h;
    free(framebuffer);
    framebuffer = calloc((size_t)fb_w * fb_h, 4);
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
    free(framebuffer);
    framebuffer = NULL;
    fb_w = fb_h = 0;
    event_waiter = NULL;
    midp_graphics_shutdown();
    midp_tls_shutdown();
    midp_net_shutdown();
    midp_audio_shutdown();
    midp_m3g_shutdown();
}
