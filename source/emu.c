#include "emu.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "gfx.h"
#include "keymap.h"
#include "lang.h"
#include "manifest.h"
#include "midp/midp.h"
#include "platform.h"
#include "settings.h"
#include "vm/vm.h"
#include "vm/zip.h"

// Thư viện CLDC/MIDP đã biên dịch, nhúng lúc build (classlib_data.c)
extern const unsigned char classlib_jar[];
extern const size_t classlib_jar_size;

#define VM_BUDGET_MS     12
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
        fprintf(log_file, "J2ME-NX v" APP_VERSION_STR " - %s (%s)\n", jar_path, cls);

    GameSettings gs;
    game_settings_load(base, &gs);
    fps_limit = gs.fps_limit >= 0 ? gs.fps_limit : settings()->fps_limit;
    keymap = keymap_get(gs.keymap >= 0 ? gs.keymap : settings()->keymap);
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
        .keyboard = platform_keyboard,
        .vibrate = NULL,
        .fps_limit = fps_limit,
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
    SDL_SetTextureScaleMode(screen_tex, settings()->scale_mode == 0 ? SDL_ScaleModeLinear : SDL_ScaleModeNearest);
    running = true;
    return true;
}

void emu_stop(void) {
    if (running || syslib || game) {
        vm_shutdown();
        midp_shutdown();
    }
    running = false;
    if (screen_tex)
        SDL_DestroyTexture(screen_tex);
    screen_tex = NULL;
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

static void update_repeat(void) {
    Uint32 now = SDL_GetTicks();
    for (int i = 0; i < KEY_SLOTS; i++) {
        if (key_held[i] && SDL_TICKS_PASSED(now, key_repeat_at[i])) {
            midp_post_event(MIDP_EV_KEY_REPEATED, keymap_translate(keymap, i - 16), 0);
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
    midp_post_event(type, x, y);
}

void emu_handle_event(const SDL_Event *e) {
    if (!running)
        return;
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
        if (at > last && at <= t) {
            midp_post_key(keymap_translate(keymap, code), true);
            midp_post_key(keymap_translate(keymap, code), false);
        }
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
            const uint32_t *fb = midp_framebuffer(&w, &h, NULL);
            SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormatFrom((void *)fb, w, h, 32, w * 4,
                                                                   SDL_PIXELFORMAT_ARGB8888);
            if (surf) {
                SDL_SaveBMP(surf, path);
                SDL_FreeSurface(surf);
            }
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
    if (exit_now) {
        exit_msg[0] = '\0';
        return false;
    }
    update_repeat();
    midp_audio_poll();
    if (!vm_run(VM_BUDGET_MS)) {
        snprintf(exit_msg, sizeof(exit_msg), "%s", tr(S_GAME_ENDED));
        return false;
    }
    if (midp_exit_requested() || exit_now)
        return false;
    return true;
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

void emu_draw(void) {
    gfx_clear(COL_BG);
    if (!running)
        return;
    int w, h;
    bool dirty;
    const uint32_t *fb = midp_framebuffer(&w, &h, &dirty);
    if (fb && dirty)
        SDL_UpdateTexture(screen_tex, NULL, fb, w * 4);
    SDL_RenderCopy(gfx_renderer(), screen_tex, NULL, &dst);
    if (settings()->show_help)
        draw_help();

    if (exit_confirm_until && !SDL_TICKS_PASSED(SDL_GetTicks(), exit_confirm_until)) {
        const char *msg = tr(S_EXIT_CONFIRM);
        int tw = gfx_text_width(FONT_NORMAL, msg) + 80, th = 56;
        gfx_fill_rect((SCREEN_W - tw) / 2, SCREEN_H - th - 24, tw, th, RGB(0x30, 0x30, 0x30));
        gfx_text(FONT_NORMAL, SCREEN_W / 2, SCREEN_H - th - 24 + (th - gfx_font_height(FONT_NORMAL)) / 2, 0,
                 ALIGN_CENTER, COL_WARN, msg);
    }
}
