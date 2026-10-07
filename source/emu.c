#include "emu.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "gfx.h"
#include "input.h"
#include "keymap.h"
#include "lang.h"
#include "manifest.h"
#include "midp/midp.h"
#include "platform.h"
#include "settings.h"
#include "video_screen.h"
#include "vm/vm.h"
#include "vm/zip.h"

// Thư viện CLDC/MIDP đã biên dịch, nhúng lúc build (classlib_data.c)
extern const unsigned char classlib_jar[];
extern const size_t classlib_jar_size;

// Mỗi lượt VM chạy tối đa bấy nhiêu ms rồi mới kiểm tra cờ dừng / ngủ
#define VM_SLICE_MS      8
#define REPEAT_DELAY_MS  400
#define REPEAT_RATE_MS   100
#define EXIT_CONFIRM_MS  2000

static bool running;
static ZipFile *syslib, *game;
static Manifest manifest;
static char game_name[128];
static char rms_dir[512];
static char files_dir[512];
static char exit_msg[256];
static FILE *log_file;

static SDL_Texture *screen_tex;
// Sharp-bilinear: phóng nguyên lần (giữ điểm ảnh) vào texture trung gian rồi thu mịn xuống màn hình
static SDL_Texture *sharp_tex;
static bool sharp_dirty;
// Đo hiệu năng. Cài đặt -> Hiện FPS: hiện ở góc màn hình và ghi log ra <data>/log.txt
// (desktop: J2ME_NX_PROF=1 ghi ra stderr)
static double stat_vm_max, stat_vm_sum;
static Uint64 stat_last_frame;

// ---------------------------------------------------------------------------
// Luồng VM: chạy code Java song song với luồng chính (nhận phím, vẽ, chờ vsync).
// Luồng chính không chờ VM: phím đi qua hàng đợi, màn hình qua framebuffer (khoá riêng
// trong midp.c). vm_lock chỉ dùng khi dừng game và mở bàn phím ảo: luồng chính tăng
// main_waiting, VM dừng sau lượt thread hiện tại và nhường.
static SDL_mutex *vm_lock;
static SDL_cond *vm_wake;          // báo luồng VM: có sự kiện mới / phải dừng
static SDL_cond *kb_cond;          // bàn phím ảo: yêu cầu / đã xong
static PlatformThread *vm_thread;
static volatile int main_waiting;
static volatile bool vm_stop_req;
static volatile bool vm_dead;
// Thống kê do luồng VM tính mỗi giây
static volatile int pub_cpu;
static volatile double pub_vm_max;

static struct {
    bool pending, done;
    const char *title, *text;
    int max_len, type;
    char *result;
} kb;

static void lock_vm(void) {
    if (!vm_thread) {
        return;
    }
    __atomic_add_fetch(&main_waiting, 1, __ATOMIC_SEQ_CST);
    SDL_LockMutex(vm_lock);
    __atomic_sub_fetch(&main_waiting, 1, __ATOMIC_SEQ_CST);
}

static void unlock_vm(void) {
    if (!vm_thread)
        return;
    SDL_CondSignal(vm_wake);
    SDL_UnlockMutex(vm_lock);
}

static int vm_thread_main(void *arg) {
    (void)arg;
    SDL_LockMutex(vm_lock);
    jlong stat_start = vm_time_ms();
    while (!vm_stop_req) {
        midp_poll_events();
        midp_audio_poll();
        Uint64 t0 = SDL_GetPerformanceCounter();
        VMRunResult r = vm_run_slice(VM_SLICE_MS);
        double ms = (SDL_GetPerformanceCounter() - t0) * 1000.0 / SDL_GetPerformanceFrequency();
        stat_vm_sum += ms;
        if (ms > stat_vm_max)
            stat_vm_max = ms;
        if (ms > 30)
            vm_prof_log("vm slice %.1f ms", ms);
        if (r == VM_RUN_DEAD) {
            vm_dead = true;
            break;
        }
        jlong now = vm_time_ms();
        if (now - stat_start >= 1000) {
            pub_cpu = (int)(stat_vm_sum * 100.0 / (double)(now - stat_start));
            pub_vm_max = stat_vm_max;
            stat_vm_sum = 0;
            stat_vm_max = 0;
            stat_start = now;
        }
        if (r == VM_RUN_IDLE) {
            // Ngủ tới khi thread Java thức dậy; phím mới thì luồng chính báo vm_wake
            // (có thể lỡ tín hiệu nên chờ tối đa 4ms)
            jlong next = vm_next_wakeup();
            jlong wait = next < 0 ? 4 : next - now;
            if (wait > 4)
                wait = 4;
            if (wait > 0 && !main_waiting)
                SDL_CondWaitTimeout(vm_wake, vm_lock, (Uint32)wait);
        }
        // Nhường khoá cho luồng chính (CondWait nhả khoá, unlock_vm báo lại)
        while (main_waiting && !vm_stop_req)
            SDL_CondWaitTimeout(vm_wake, vm_lock, 1);
    }
    SDL_UnlockMutex(vm_lock);
    return 0;
}

// Bàn phím ảo phải mở trên luồng chính: luồng VM gửi yêu cầu rồi chờ (CondWait nhả vm_lock)
static char *vm_keyboard(const char *title, const char *text, int max_len, int type) {
    if (!vm_thread)
        return platform_keyboard(title, text, max_len, type);
    kb.title = title;
    kb.text = text;
    kb.max_len = max_len;
    kb.type = type;
    kb.result = NULL;
    kb.done = false;
    kb.pending = true;
    while (!kb.done && !vm_stop_req)
        SDL_CondWait(kb_cond, vm_lock);
    kb.pending = false;
    return kb.result;
}

// Luồng chính, đang giữ vm_lock
static void serve_keyboard(void) {
    if (!kb.pending || kb.done)
        return;
    kb.result = platform_keyboard(kb.title, kb.text, kb.max_len, kb.type);
    kb.done = true;
    SDL_CondSignal(kb_cond);
}

static void vm_thread_stop(void) {
    if (!vm_thread)
        return;
    lock_vm();
    vm_stop_req = true;
    SDL_CondSignal(kb_cond);
    unlock_vm();
    platform_thread_join(vm_thread);
    vm_thread = NULL;
    vm_set_preempt_flag(NULL);
}

// ---------------------------------------------------------------------------
// MIDlet.platformRequest: link video -> trình xem video đè lên game, trang web -> trình duyệt.
// Luồng VM chỉ ghi yêu cầu; luồng chính mở link (luồng mạng mở ở luồng phụ để không đứng hình).

typedef enum {
    LINK_NONE,
    LINK_OPENING,           // đang thử mở bằng FFmpeg
    LINK_VIDEO,             // trình xem video đang hiện
} LinkState;

static SDL_mutex *req_lock;
static char *req_url;                   // yêu cầu chờ xử lý (malloc)
static LinkState link_state;
static SDL_Thread *link_thread;
static volatile int link_abort, link_done;
static VideoDec *link_dec;
static char link_url[2048];
static char toast[256];
static Uint32 toast_until;

static void show_toast(const char *msg, Uint32 ms) {
    snprintf(toast, sizeof(toast), "%s", msg);
    toast_until = SDL_GetTicks() + ms;
}

static void host_platform_request(const char *url) {
    SDL_LockMutex(req_lock);
    free(req_url);
    req_url = url[0] ? strdup(url) : NULL;
    SDL_UnlockMutex(req_lock);
}

static int link_open_thread(void *arg) {
    (void)arg;
    VDecOptions opt;
    video_screen_options(&opt);
    opt.abort = &link_abort;
    link_dec = vdec_open_url(link_url, &opt);
    __atomic_store_n(&link_done, 1, __ATOMIC_SEQ_CST);
    return 0;
}

// Tên hiển thị: phần cuối đường dẫn, bỏ query
static void link_title(char *out, size_t size) {
    const char *s = link_url;
    const char *p = strstr(s, "://");
    if (p)
        s = p + 3;
    char buf[256];
    snprintf(buf, sizeof(buf), "%.255s", s);
    char *q = strchr(buf, '?');
    if (q)
        *q = '\0';
    char *slash = strrchr(buf, '/');
    snprintf(out, size, "%s", slash && slash[1] ? slash + 1 : buf);
}

static void link_stop(void);
static void release_all_keys(void);

static void start_video(VideoDec *d) {
    // Tiếng của game tạm đóng: Switch không mở được 2 thiết bị âm thanh cùng lúc
    lock_vm();
    midp_audio_suspend(true);
    unlock_vm();
    char title[256], err[160];
    link_title(title, sizeof(title));
    if (video_screen_open_dec(d, title, err, sizeof(err))) {
        link_state = LINK_VIDEO;
        return;
    }
    lock_vm();
    midp_audio_suspend(false);
    unlock_vm();
    show_toast(err, 3000);
    link_state = LINK_NONE;
}

static void open_browser(void) {
    OpenUrlResult r = platform_open_url(link_url);
    if (r == OPEN_URL_NEED_APP)
        show_toast(tr(S_BROWSER_NEEDS_APP), 6000);
    else if (r == OPEN_URL_FAILED)
        show_toast(tr(S_LINK_FAILED), 3000);
}

// Luồng chính, mỗi frame
static void link_update(void) {
    if (link_state == LINK_NONE) {
        SDL_LockMutex(req_lock);
        char *u = req_url;
        req_url = NULL;
        SDL_UnlockMutex(req_lock);
        if (!u)
            return;
        snprintf(link_url, sizeof(link_url), "%s", u);
        free(u);
        release_all_keys();
        if (!strstr(link_url, "://")) {
            // File thật trong sandbox (file:/// đã đổi đường dẫn ở phía Java)
            VDecOptions opt;
            video_screen_options(&opt);
            VideoDec *d = vdec_open_file(link_url, &opt);
            if (d)
                start_video(d);
            else
                show_toast(tr(S_LINK_FAILED), 3000);
            return;
        }
        link_abort = link_done = 0;
        link_dec = NULL;
        link_thread = SDL_CreateThread(link_open_thread, "link", NULL);
        if (!link_thread) {
            open_browser();
            return;
        }
        link_state = LINK_OPENING;
        return;
    }
    if (link_state == LINK_OPENING) {
        if (input_pressed(BTN_B))
            link_abort = 1;
        if (!__atomic_load_n(&link_done, __ATOMIC_SEQ_CST))
            return;
        SDL_WaitThread(link_thread, NULL);
        link_thread = NULL;
        link_state = LINK_NONE;
        VideoDec *d = link_dec;
        link_dec = NULL;
        if (d)
            start_video(d);
        else if (!link_abort)
            open_browser();     // không phải luồng video: coi là trang web
        return;
    }
    if (link_state == LINK_VIDEO && !video_screen_update()) {
        video_screen_close();
        lock_vm();
        midp_audio_suspend(false);
        unlock_vm();
        link_state = LINK_NONE;
    }
}

// Dừng hẳn (thoát game)
static void link_stop(void) {
    if (link_thread) {
        link_abort = 1;
        SDL_WaitThread(link_thread, NULL);
        link_thread = NULL;
    }
    vdec_close(link_dec);
    link_dec = NULL;
    if (link_state == LINK_VIDEO)
        video_screen_close();
    link_state = LINK_NONE;
    if (req_lock) {
        SDL_LockMutex(req_lock);
        free(req_url);
        req_url = NULL;
        SDL_UnlockMutex(req_lock);
    }
    toast_until = 0;
}

// Gọi sau khi đã mở log_file
static void prof_start(void) {
    FILE *f = SDL_getenv("J2ME_NX_PROF") ? stderr : settings()->show_fps ? log_file : NULL;
    vm_prof_open(f);
}

static void prof_stop(void) {
    vm_prof_flush();
    vm_prof_open(NULL);
}
static int scr_w, scr_h;
static int fps_limit;
static const KeyMap *keymap;
static SDL_Rect dst;

// Phím J2ME đang giữ (đếm số nguồn: tay cầm + bàn phím)
#define KEY_INDEX(code) ((code) + 16)
#define KEY_SLOTS 80
static int key_held[KEY_SLOTS];
static Uint32 key_repeat_at[KEY_SLOTS];
static Uint32 exit_confirm_until;
static bool exit_now;
static bool pointer_down;

// ---------------------------------------------------------------------------
// Host callback cho VM / MIDP

static uint8_t *host_read_file(const char *name, size_t *size, bool *from_game) {
    uint8_t *d = zip_read(syslib, name, size);
    if (d) {
        *from_game = false;
        return d;
    }
    d = zip_read(game, name, size);
    *from_game = d != NULL;
    return d;
}

static uint8_t *host_read_resource(const char *name, size_t *size) {
    return zip_read(game, name, size);
}

static void host_log(const char *msg) {
    if (log_file) {
        fprintf(log_file, "%s\n", msg);
        fflush(log_file);
    }
}

static void host_exit(int status) {
    (void)status;
    exit_now = true;
}

static const char *host_app_property(const char *key) {
    return manifest_get(&manifest, key);
}

// ---------------------------------------------------------------------------

static void base_name(const char *path, char *out, size_t size) {
    const char *s = strrchr(path, '/');
    s = s ? s + 1 : path;
    snprintf(out, size, "%s", s);
    char *dot = strrchr(out, '.');
    if (dot && strcasecmp(dot, ".jar") == 0)
        *dot = '\0';
}

// Ưu tiên: tuỳ chọn riêng của game > khai báo trong MANIFEST/JAD > cài đặt chung
static void parse_screen_size(const GameSettings *gs) {
    if (gs->screen_w) {
        scr_w = gs->screen_w;
        scr_h = gs->screen_h;
        return;
    }
    scr_w = settings()->screen_w;
    scr_h = settings()->screen_h;
    const char *v = manifest_get(&manifest, "Nokia-MIDlet-Original-Display-Size");
    if (!v)
        v = manifest_get(&manifest, "J2ME-NX-Screen-Size");
    int w, h;
    if (v && sscanf(v, "%d%*[ ,xX]%d", &w, &h) == 2 && settings_valid_screen(w, h)) {
        scr_w = w;
        scr_h = h;
    }
}

static void compute_dst(void) {
    float s = (float)SCREEN_W / scr_w;
    if ((float)SCREEN_H / scr_h < s)
        s = (float)SCREEN_H / scr_h;
    // Phóng số nguyên: điểm ảnh đều nhau (nhỏ hơn màn hình thì giữ 1x... trừ khi không vừa)
    if (settings()->scale_mode == 2 && s >= 1.0f)
        s = (float)(int)s;
    dst.w = (int)(scr_w * s);
    dst.h = (int)(scr_h * s);
    dst.x = (SCREEN_W - dst.w) / 2;
    dst.y = (SCREEN_H - dst.h) / 2;
}

bool emu_start(const char *jar_path, const char *game_id, int midlet, char *err, size_t err_size) {
    emu_stop();
    exit_msg[0] = '\0';
    exit_now = false;
    exit_confirm_until = 0;
    memset(key_held, 0, sizeof(key_held));
    pointer_down = false;

    syslib = zip_open_mem(classlib_jar, classlib_jar_size, false);
    game = zip_open_file(jar_path);
    if (!syslib || !game) {
        snprintf(err, err_size, "%s", tr(!syslib ? S_ERR_SYSLIB : S_ERR_OPEN_JAR));
        emu_stop();
        return false;
    }

    // Thuộc tính: JAD (nếu có, cạnh file JAR) ưu tiên hơn MANIFEST.MF
    manifest_load(&manifest, jar_path, game);

    char cls[256];
    if (midlet < 1)
        midlet = 1;
    if (!manifest_midlet_entry(&manifest, midlet, 2, cls, sizeof(cls))) {
        snprintf(err, err_size, "%s", tr(S_ERR_NO_MIDLET));
        emu_stop();
        return false;
    }

    base_name(jar_path, game_name, sizeof(game_name));
    const char *name = manifest_get(&manifest, "MIDlet-Name");
    if (name)
        snprintf(game_name, sizeof(game_name), "%s", name);

    char base[256];
    if (game_id)
        snprintf(base, sizeof(base), "%s", game_id);
    else
        base_name(jar_path, base, sizeof(base));
    snprintf(rms_dir, sizeof(rms_dir), "%s/rms/%s", platform_data_dir(), base);
    snprintf(files_dir, sizeof(files_dir), "%s/files/%s", platform_data_dir(), base);

    mkdir(platform_data_dir(), 0777);
    char log_path[600];
    snprintf(log_path, sizeof(log_path), "%s/log.txt", platform_data_dir());
    log_file = fopen(log_path, "w");
    if (log_file)
        fprintf(log_file, "J2ME-NXX v" APP_VERSION_STR " - %s (%s)\n", jar_path, cls);
    prof_start();

    GameSettings gs;
    game_settings_load(base, &gs);
    fps_limit = gs.fps_limit >= 0 ? gs.fps_limit : settings()->fps_limit;
    int km = gs.keymap >= 0 ? gs.keymap : settings()->keymap;
    // Game Motorola MIDP 1.0 (có thuộc tính Mot-*) dùng mã phím dương của máy Motorola cũ
    const char *profile = manifest_get(&manifest, "MicroEdition-Profile");
    if (gs.keymap < 0 && km == KEYMAP_NOKIA && profile && strstr(profile, "MIDP-1") &&
        (manifest_get(&manifest, "Mot-Program-Space-Requirement") || manifest_get(&manifest, "Mot-Data-Space-Requirement")))
        km = KEYMAP_MOTOROLA_OLD;
    keymap = keymap_get(km);
    parse_screen_size(&gs);
    compute_dst();

    VMHost host = {
        .read_file = host_read_file,
        .read_resource = host_read_resource,
        .log = host_log,
        .exit_request = host_exit,
    };
    midp_register_natives();
    if (!vm_init(&host)) {
        snprintf(err, err_size, tr(S_ERR_VM), vm_last_error());
        emu_stop();
        return false;
    }

    MidpConfig mc = {
        .screen_w = scr_w,
        .screen_h = scr_h,
        .rms_dir = rms_dir,
        .files_dir = files_dir,
        .app_property = host_app_property,
        .keyboard = vm_keyboard,
        .platform_request = host_platform_request,
        .vibrate = NULL,
        .fps_limit = fps_limit,
        .smooth_text = gs.smooth_text >= 0 ? gs.smooth_text != 0 : settings()->smooth_text,
        .font_scale = gs.font_scale > 0 ? gs.font_scale : settings()->font_scale,
        .lang = lang_code(lang_get()),
        .platform = keymap->platform,
        .keycodes = { keymap->up, keymap->down, keymap->left, keymap->right, keymap->fire, keymap->soft_left,
                      keymap->soft_right, keymap->clear },
    };
    if (!midp_start(&mc, cls)) {
        snprintf(err, err_size, tr(S_ERR_MIDLET), vm_last_error());
        emu_stop();
        return false;
    }

    screen_tex = SDL_CreateTexture(gfx_renderer(), SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, scr_w, scr_h);
    SDL_SetTextureScaleMode(screen_tex, SDL_ScaleModeNearest);
    // Hệ số nguyên nhỏ nhất >= tỉ lệ phóng; tỉ lệ đã là số nguyên thì không cần texture trung gian
    int k = (dst.h + scr_h - 1) / scr_h;
    if (settings()->scale_mode == 0 && dst.h % scr_h != 0 && k >= 2) {
        if (k > 4)
            k = 4;
        sharp_tex = SDL_CreateTexture(gfx_renderer(), SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, scr_w * k,
                                      scr_h * k);
        if (sharp_tex)
            SDL_SetTextureScaleMode(sharp_tex, SDL_ScaleModeLinear);
        sharp_dirty = true;
    }
    stat_vm_max = 0;
    stat_vm_sum = 0;
    stat_last_frame = 0;
    vm_prof_log("screen %dx%d  scale %d  fps_limit %d", scr_w, scr_h, settings()->scale_mode, fps_limit);

    if (!vm_lock) {
        vm_lock = SDL_CreateMutex();
        vm_wake = SDL_CreateCond();
        req_lock = SDL_CreateMutex();
        kb_cond = SDL_CreateCond();
    }
    vm_stop_req = false;
    vm_dead = false;
    main_waiting = 0;
    memset(&kb, 0, sizeof(kb));
    vm_set_preempt_flag(&main_waiting);
    vm_thread = platform_thread_start(vm_thread_main, NULL);
    if (!vm_thread) {
        vm_set_preempt_flag(NULL);
        snprintf(err, err_size, tr(S_ERR_VM), "thread");
        emu_stop();
        return false;
    }
    running = true;
    return true;
}

void emu_stop(void) {
    vm_thread_stop();
    link_stop();
    if (running || syslib || game) {
        vm_shutdown();
        midp_shutdown();
    }
    running = false;
    prof_stop();
    if (screen_tex)
        SDL_DestroyTexture(screen_tex);
    screen_tex = NULL;
    if (sharp_tex)
        SDL_DestroyTexture(sharp_tex);
    sharp_tex = NULL;
    zip_close(syslib);
    zip_close(game);
    syslib = game = NULL;
    manifest_free(&manifest);
    if (log_file)
        fclose(log_file);
    log_file = NULL;
}

bool emu_running(void) {
    return running;
}

const char *emu_exit_message(void) {
    return exit_msg;
}

// ---------------------------------------------------------------------------
// Phím

static void key_change(int code, bool down) {
    if (!code)
        return;
    int i = KEY_INDEX(code);
    if (i < 0 || i >= KEY_SLOTS)
        return;
    if (down) {
        if (key_held[i]++ == 0) {
            midp_post_key(keymap_translate(keymap, code), true);
            key_repeat_at[i] = SDL_GetTicks() + REPEAT_DELAY_MS;
        }
    } else if (key_held[i] > 0 && --key_held[i] == 0) {
        midp_post_key(keymap_translate(keymap, code), false);
    }
}

// Nhả mọi phím đang giữ (trước khi trình xem video lấy hết phím)
static void release_all_keys(void) {
    for (int i = 0; i < KEY_SLOTS; i++) {
        if (key_held[i]) {
            midp_post_key(keymap_translate(keymap, i - 16), false);
            key_held[i] = 0;
        }
    }
}

static void update_repeat(void) {
    Uint32 now = SDL_GetTicks();
    for (int i = 0; i < KEY_SLOTS; i++) {
        if (key_held[i] && SDL_TICKS_PASSED(now, key_repeat_at[i])) {
            midp_post_event_async(MIDP_EV_KEY_REPEATED, keymap_translate(keymap, i - 16), 0);
            key_repeat_at[i] = now + REPEAT_RATE_MS;
        }
    }
}

// Nút joystick của SDL2 bản Switch -> phím J2ME
static int joy_to_key(int button) {
    switch (button) {
    case 0:  return MIDP_KEY_FIRE;          // A
    case 1:  return MIDP_KEY_SOFT_RIGHT;    // B
    case 2:  return MIDP_KEY_POUND;         // X
    case 3:  return MIDP_KEY_STAR;          // Y
    case 4:  return '5';                    // bấm stick trái
    case 5:  return '0';                    // bấm stick phải
    case 6:  return MIDP_KEY_SOFT_LEFT;     // L
    case 7:  return MIDP_KEY_SOFT_RIGHT;    // R
    case 8:  return '1';                    // ZL
    case 9:  return '3';                    // ZR
    case 10: return MIDP_KEY_SOFT_LEFT;     // +
    case 12: case 16: return MIDP_KEY_LEFT;
    case 13: case 17: return MIDP_KEY_UP;
    case 14: case 18: return MIDP_KEY_RIGHT;
    case 15: case 19: return MIDP_KEY_DOWN;
    case 20: return '4';                    // stick phải
    case 21: return '2';
    case 22: return '6';
    case 23: return '8';
    default: return 0;
    }
}

static int keyboard_to_key(SDL_Keycode k) {
    switch (k) {
    case SDLK_UP:       return MIDP_KEY_UP;
    case SDLK_DOWN:     return MIDP_KEY_DOWN;
    case SDLK_LEFT:     return MIDP_KEY_LEFT;
    case SDLK_RIGHT:    return MIDP_KEY_RIGHT;
    case SDLK_RETURN:
    case SDLK_SPACE:    return MIDP_KEY_FIRE;
    case SDLK_F1:
    case SDLK_q:        return MIDP_KEY_SOFT_LEFT;
    case SDLK_F2:
    case SDLK_w:
    case SDLK_BACKSPACE: return MIDP_KEY_SOFT_RIGHT;
    case SDLK_KP_MULTIPLY:
    case SDLK_a:        return MIDP_KEY_STAR;
    case SDLK_KP_DIVIDE:
    case SDLK_s:        return MIDP_KEY_POUND;
    case SDLK_KP_0:     return '0';
    case SDLK_KP_1:     return '1';
    case SDLK_KP_2:     return '2';
    case SDLK_KP_3:     return '3';
    case SDLK_KP_4:     return '4';
    case SDLK_KP_5:     return '5';
    case SDLK_KP_6:     return '6';
    case SDLK_KP_7:     return '7';
    case SDLK_KP_8:     return '8';
    case SDLK_KP_9:     return '9';
    default:
        if (k >= SDLK_0 && k <= SDLK_9)
            return '0' + (k - SDLK_0);
        return 0;
    }
}

static void request_exit(void) {
    Uint32 now = SDL_GetTicks();
    if (exit_confirm_until && !SDL_TICKS_PASSED(now, exit_confirm_until))
        exit_now = true;
    else
        exit_confirm_until = now + EXIT_CONFIRM_MS;
}

static bool to_screen(int lx, int ly, int *sx, int *sy) {
    *sx = (lx - dst.x) * scr_w / dst.w;
    *sy = (ly - dst.y) * scr_h / dst.h;
    return lx >= dst.x && ly >= dst.y && lx < dst.x + dst.w && ly < dst.y + dst.h;
}

static void pointer(int type, int lx, int ly) {
    int x, y;
    bool inside = to_screen(lx, ly, &x, &y);
    if (type == MIDP_EV_POINTER_PRESSED) {
        if (!inside)
            return;
        pointer_down = true;
    } else if (!pointer_down) {
        return;
    }
    if (!inside) {
        x = x < 0 ? 0 : x >= scr_w ? scr_w - 1 : x;
        y = y < 0 ? 0 : y >= scr_h ? scr_h - 1 : y;
    }
    if (type == MIDP_EV_POINTER_RELEASED)
        pointer_down = false;
    midp_post_event_async(type, x, y);
}

static void handle_event(const SDL_Event *e);

void emu_handle_event(const SDL_Event *e) {
    if (!running || link_state != LINK_NONE)
        return;     // trình xem video / đang mở link: phím không vào game
    handle_event(e);
    if (vm_wake)
        SDL_CondSignal(vm_wake);
}

static void handle_event(const SDL_Event *e) {
    switch (e->type) {
    case SDL_JOYBUTTONDOWN:
    case SDL_JOYBUTTONUP:
        if (e->jbutton.button == 11) {          // nút -: thoát game
            if (e->type == SDL_JOYBUTTONDOWN)
                request_exit();
            break;
        }
        key_change(joy_to_key(e->jbutton.button), e->type == SDL_JOYBUTTONDOWN);
        break;
    case SDL_KEYDOWN:
    case SDL_KEYUP:
        if (e->key.repeat)
            break;
        if (e->key.keysym.sym == SDLK_ESCAPE) {
            if (e->type == SDL_KEYDOWN)
                request_exit();
            break;
        }
        key_change(keyboard_to_key(e->key.keysym.sym), e->type == SDL_KEYDOWN);
        break;
    case SDL_FINGERDOWN:
    case SDL_FINGERUP:
    case SDL_FINGERMOTION: {
        int type = e->type == SDL_FINGERDOWN ? MIDP_EV_POINTER_PRESSED
                 : e->type == SDL_FINGERUP ? MIDP_EV_POINTER_RELEASED : MIDP_EV_POINTER_DRAGGED;
        pointer(type, (int)(e->tfinger.x * SCREEN_W), (int)(e->tfinger.y * SCREEN_H));
        break;
    }
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
        if (e->button.which == SDL_TOUCH_MOUSEID || e->button.button != SDL_BUTTON_LEFT)
            break;
        pointer(e->type == SDL_MOUSEBUTTONDOWN ? MIDP_EV_POINTER_PRESSED : MIDP_EV_POINTER_RELEASED,
                e->button.x, e->button.y);
        break;
    case SDL_MOUSEMOTION:
        if (e->motion.which != SDL_TOUCH_MOUSEID && (e->motion.state & SDL_BUTTON_LMASK))
            pointer(MIDP_EV_POINTER_DRAGGED, e->motion.x, e->motion.y);
        break;
    default:
        break;
    }
}

#ifndef __SWITCH__
// Kịch bản test tự động (chỉ bản desktop), theo mốc ms tính từ lúc game chạy:
//   J2ME_NX_KEYS="1500:-6,2000:-2"        bấm + nhả phím J2ME
//   J2ME_NX_SHOTS="1000:/tmp/a.bmp,..."   chụp màn hình J2ME ra BMP
//   J2ME_NX_QUIT=5000                     thoát app
static Uint32 script_start;

static void script_step(void) {
    if (!script_start)
        script_start = SDL_GetTicks();
    Uint32 t = SDL_GetTicks() - script_start;
    static Uint32 last;
    const char *keys = SDL_getenv("J2ME_NX_KEYS");
    const char *shots = SDL_getenv("J2ME_NX_SHOTS");
    const char *quit = SDL_getenv("J2ME_NX_QUIT");
    for (const char *p = keys; p && *p;) {
        unsigned at;
        int code, n = 0;
        if (sscanf(p, "%u:%d%n", &at, &code, &n) != 2)
            break;
        // Giữ phím 120ms như bấm tay (nhiều game đọc trạng thái phím trong vòng lặp vẽ)
        if (at > last && at <= t)
            midp_post_key(keymap_translate(keymap, code), true);
        if (at + 120 > last && at + 120 <= t)
            midp_post_key(keymap_translate(keymap, code), false);
        p += n;
        if (*p == ',')
            p++;
    }
    for (const char *p = shots; p && *p;) {
        unsigned at;
        char path[256];
        int n = 0;
        if (sscanf(p, "%u:%255[^,]%n", &at, path, &n) != 2)
            break;
        if (at > last && at <= t) {
            int w, h;
            const uint32_t *fb = midp_framebuffer_lock(&w, &h, NULL);
            SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormatFrom((void *)fb, w, h, 32, w * 4,
                                                                   SDL_PIXELFORMAT_ARGB8888);
            if (surf) {
                SDL_SaveBMP(surf, path);
                SDL_FreeSurface(surf);
            }
            midp_framebuffer_unlock();
        }
        p += n;
        if (*p == ',')
            p++;
    }
    if (quit && t >= (Uint32)atoi(quit)) {
        SDL_Event e = { .type = SDL_QUIT };
        SDL_PushEvent(&e);
    }
    last = t;
}
#endif

bool emu_update(void) {
    if (!running)
        return false;
#ifndef __SWITCH__
    script_step();
#endif
    if (kb.pending && !kb.done) {
        lock_vm();
        serve_keyboard();
        unlock_vm();
    }
    link_update();
    if (link_state == LINK_NONE)
        update_repeat();
    bool dead = vm_dead, quit = exit_now || midp_exit_requested();
    if (exit_now) {
        exit_msg[0] = '\0';
        return false;
    }
    if (dead) {
        snprintf(exit_msg, sizeof(exit_msg), "%s", tr(S_GAME_ENDED));
        return false;
    }
    return !quit;
}

// ---------------------------------------------------------------------------
// Vẽ

#define COL_BG    RGB(0x10, 0x11, 0x14)
#define COL_TEXT  RGB(0xee, 0xee, 0xee)
#define COL_DIM   RGB(0x8a, 0x8f, 0x98)
#define COL_WARN  RGB(0xff, 0xc1, 0x4d)

static void draw_help(void) {
    const char *lines[][2] = {
        { "D-pad / L-stick", tr(S_HELP_DPAD) },
        { "A", "Fire (5)" },
        { "B / R", tr(S_HELP_SOFT_RIGHT) },
        { "L / +", tr(S_HELP_SOFT_LEFT) },
        { "Y / X", "* / #" },
        { "ZL / ZR", "1 / 3" },
        { "R-stick", "2 4 6 8" },
        { tr(S_HELP_STICK_CLICK), "5 / 0" },
        { "-", tr(S_HELP_EXIT) },
    };
    int panel_w = dst.x;
    if (panel_w < 200)
        return;
    int x = 32, y = 40;
    gfx_text(FONT_LARGE, x, y, panel_w - 48, ALIGN_LEFT, COL_TEXT, game_name);
    y += gfx_font_height(FONT_LARGE) + 4;
    char info[96];
    if (fps_limit > 0)
        snprintf(info, sizeof(info), tr(S_SCREEN_INFO_FPS), scr_w, scr_h, fps_limit);
    else
        snprintf(info, sizeof(info), "%dx%d", scr_w, scr_h);
    gfx_text(FONT_SMALL, x, y, 0, ALIGN_LEFT, COL_DIM, info);
    y += 48;
    for (size_t i = 0; i < sizeof(lines) / sizeof(lines[0]); i++) {
        gfx_text(FONT_SMALL, x, y, 150, ALIGN_LEFT, COL_TEXT, lines[i][0]);
        gfx_text(FONT_SMALL, x + 160, y, panel_w - x - 170, ALIGN_LEFT, COL_DIM, lines[i][1]);
        y += gfx_font_height(FONT_SMALL) + 8;
    }
}

// Thống kê: số khung hình game vẽ ra trong 1 giây, % thời gian luồng VM chạy code Java
static void draw_stats(bool dirty) {
    static Uint32 window_start;
    static int frames, shown_fps, shown_busy;
    static double shown_vm, frame_max;
    static int slow_frames, host_frames;

    if (dirty)
        frames++;
    // Khoảng cách giữa 2 lần vẽ của app (gồm cả VM, vẽ, chờ vsync): lớn = khựng
    Uint64 pc = SDL_GetPerformanceCounter();
    if (stat_last_frame) {
        double gap = (pc - stat_last_frame) * 1000.0 / SDL_GetPerformanceFrequency();
        if (gap > frame_max)
            frame_max = gap;
        if (gap > 20)
            slow_frames++;
        host_frames++;
        if (gap > 50)
            vm_prof_log("frame gap %.1f ms", gap);
    }
    stat_last_frame = pc;
    Uint32 now = SDL_GetTicks();
    if (!window_start)
        window_start = now;
    Uint32 elapsed = now - window_start;
    if (elapsed >= 1000) {
        shown_fps = (int)(frames * 1000u / elapsed);
        shown_vm = pub_vm_max;
        shown_busy = pub_cpu;
        size_t ram_used, ram_total;
        platform_mem_usage(&ram_used, &ram_total);
        vm_prof_log("fps %d  cpu %d%%  vm max %.1f ms  game gap max %d ms  frame max %.1f ms  slow %d/%d  heap %zuK  "
                    "ram %zu/%zuM",
                    shown_fps, shown_busy, shown_vm, midp_take_frame_gap_max(), frame_max, slow_frames, host_frames,
                    heap_used() / 1024, ram_used >> 20, ram_total >> 20);
        vm_prof_flush();
        frame_max = 0;
        slow_frames = 0;
        host_frames = 0;
        frames = 0;
        window_start = now;
    }
    if (!settings()->show_fps)
        return;
    // Java: heap của VM; RAM: bộ nhớ cả app đang dùng / tối đa được cấp (Album ít hơn nhiều so với full RAM)
    size_t ram_used, ram_total;
    platform_mem_usage(&ram_used, &ram_total);
    char ram[48];
    if (ram_total)
        snprintf(ram, sizeof(ram), "%zu/%zuM", ram_used >> 20, ram_total >> 20);
    else
        snprintf(ram, sizeof(ram), "%zuM", ram_used >> 20);
    char buf[128];
    snprintf(buf, sizeof(buf), "FPS %d   CPU %d%%   Java %zuK   RAM %s", shown_fps, shown_busy, heap_used() / 1024, ram);
    int tw = gfx_text_width(FONT_SMALL, buf) + 16, th = gfx_font_height(FONT_SMALL) + 8;
    gfx_fill_rect(8, 8, tw, th, RGB(0, 0, 0));
    gfx_text(FONT_SMALL, 16, 12, 0, ALIGN_LEFT, shown_fps < 20 ? COL_WARN : COL_TEXT, buf);
}

static void draw_game(void);

void emu_draw(void) {
    if (link_state == LINK_VIDEO) {
        video_screen_draw();
        return;
    }
    gfx_clear(COL_BG);
    if (!running)
        return;
    draw_game();
}

static void draw_game(void) {
    int w, h;
    bool dirty;
    Uint64 tl0 = SDL_GetPerformanceCounter();
    const uint32_t *fb = midp_framebuffer_lock(&w, &h, &dirty);
    Uint64 tl1 = SDL_GetPerformanceCounter();
    if (fb && dirty)
        SDL_UpdateTexture(screen_tex, NULL, fb, w * 4);
    midp_framebuffer_unlock();
    Uint64 tl2 = SDL_GetPerformanceCounter();
    double f = 1000.0 / SDL_GetPerformanceFrequency();
    if ((tl2 - tl0) * f > 5)
        vm_prof_log("fb chờ khoá %.1f ms, upload %.1f ms", (tl1 - tl0) * f, (tl2 - tl1) * f);
    if (sharp_tex) {
        SDL_Renderer *r = gfx_renderer();
        if (dirty || sharp_dirty) {
            SDL_SetRenderTarget(r, sharp_tex);
            SDL_RenderCopy(r, screen_tex, NULL, NULL);
            SDL_SetRenderTarget(r, NULL);
            sharp_dirty = false;
        }
        SDL_RenderCopy(r, sharp_tex, NULL, &dst);
    } else {
        SDL_RenderCopy(gfx_renderer(), screen_tex, NULL, &dst);
    }
    if (settings()->show_help)
        draw_help();
    draw_stats(fb && dirty);

    const char *note = link_state == LINK_OPENING ? tr(S_LINK_OPENING)
                     : toast_until && !SDL_TICKS_PASSED(SDL_GetTicks(), toast_until) ? toast : NULL;
    if (note) {
        int tw = gfx_text_width(FONT_NORMAL, note) + 80, th = 56;
        if (tw > SCREEN_W - 40)
            tw = SCREEN_W - 40;
        gfx_fill_rect((SCREEN_W - tw) / 2, 24, tw, th, RGB(0x30, 0x30, 0x30));
        gfx_text(FONT_NORMAL, SCREEN_W / 2, 24 + (th - gfx_font_height(FONT_NORMAL)) / 2, tw - 40, ALIGN_CENTER,
                 COL_TEXT, note);
    }

    if (exit_confirm_until && !SDL_TICKS_PASSED(SDL_GetTicks(), exit_confirm_until)) {
        const char *msg = tr(S_EXIT_CONFIRM);
        int tw = gfx_text_width(FONT_NORMAL, msg) + 80, th = 56;
        gfx_fill_rect((SCREEN_W - tw) / 2, SCREEN_H - th - 24, tw, th, RGB(0x30, 0x30, 0x30));
        gfx_text(FONT_NORMAL, SCREEN_W / 2, SCREEN_H - th - 24 + (th - gfx_font_height(FONT_NORMAL)) / 2, 0,
                 ALIGN_CENTER, COL_WARN, msg);
    }
}
