#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>

#ifdef __SWITCH__
#include <switch.h>

bool platform_init(void) {
    // Mạng cho game online (socket:// và http://); lỗi thì game chỉ không kết nối được
    socketInitializeDefault();
    Result rc = plInitialize(PlServiceType_User);
    if (R_FAILED(rc)) {
        printf("plInitialize failed: 0x%x\n", rc);
        return false;
    }
    return true;
}

void platform_exit(void) {
    plExit();
    socketExit();
}

const char *platform_games_dir(void) {
    return "sdmc:/switch/j2me-nx/games";
}

const char *platform_data_dir(void) {
    return "sdmc:/switch/j2me-nx";
}

char *platform_keyboard(const char *title, const char *text, int max_len, int type) {
    SwkbdConfig kbd;
    if (R_FAILED(swkbdCreate(&kbd, 0)))
        return NULL;
    swkbdConfigMakePresetDefault(&kbd);
    swkbdConfigSetHeaderText(&kbd, title);
    swkbdConfigSetInitialText(&kbd, text);
    if (max_len > 0)
        swkbdConfigSetStringLenMax(&kbd, max_len);
    // TextField.NUMERIC = 2, PHONENUMBER = 3, DECIMAL = 5
    if (type == 2 || type == 3 || type == 5)
        swkbdConfigSetType(&kbd, SwkbdType_NumPad);
    char out[1024] = "";
    Result rc = swkbdShow(&kbd, out, sizeof(out));
    swkbdClose(&kbd);
    return R_SUCCEEDED(rc) ? strdup(out) : NULL;
}

static TTF_Font *open_shared_font(int ptsize) {
    PlFontData font;
    if (R_FAILED(plGetSharedFontByType(&font, PlSharedFontType_Standard)))
        return NULL;
    // Bộ nhớ shared font do pl service giữ, không cần free
    SDL_RWops *rw = SDL_RWFromConstMem(font.address, font.size);
    return TTF_OpenFontRW(rw, 1, ptsize);
}

#else // desktop

bool platform_init(void) {
    return true;
}

void platform_exit(void) {
}

const char *platform_games_dir(void) {
    const char *dir = SDL_getenv("J2ME_NX_GAMES");
    return dir ? dir : "games";
}

const char *platform_data_dir(void) {
    const char *dir = SDL_getenv("J2ME_NX_DATA");
    return dir ? dir : "data";
}

char *platform_keyboard(const char *title, const char *text, int max_len, int type) {
    // Desktop: chưa có hộp nhập liệu, coi như huỷ
    (void)title;
    (void)text;
    (void)max_len;
    (void)type;
    return NULL;
}

static TTF_Font *open_system_font(int ptsize) {
    static const char *candidates[] = {
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/System/Library/Fonts/Helvetica.ttc",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "C:/Windows/Fonts/arial.ttf",
    };
    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
        TTF_Font *f = TTF_OpenFont(candidates[i], ptsize);
        if (f)
            return f;
    }
    return NULL;
}

#endif

// Font nhúng (Google Sans, có đủ chữ tiếng Việt); lỗi thì dùng font hệ thống
extern const unsigned char ui_font_ttf[];
extern const size_t ui_font_ttf_size;

TTF_Font *platform_open_font(int ptsize) {
    SDL_RWops *rw = SDL_RWFromConstMem(ui_font_ttf, (int)ui_font_ttf_size);
    TTF_Font *f = rw ? TTF_OpenFontRW(rw, 1, ptsize) : NULL;
    if (f)
        return f;
#ifdef __SWITCH__
    return open_shared_font(ptsize);
#else
    return open_system_font(ptsize);
#endif
}
