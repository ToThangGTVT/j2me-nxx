// J2ME-NXX - J2ME emulator cho Nintendo Switch
//
// Màn hình chọn game (.jar) -> chạy MIDlet trên máy ảo Java tự viết (source/vm, source/midp).
// Chạy được cả trên Switch (devkitPro) và desktop (để test nhanh).
// Desktop: có thể truyền đường dẫn .jar làm tham số để chạy thẳng game.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <SDL.h>

#include "emu.h"
#include "game_list.h"
#include "gfx.h"
#include "input.h"
#include "lang.h"
#include "menu.h"
#include "platform.h"
#include "settings.h"
#include "settings_screen.h"
#include "video_screen.h"

// Test desktop: J2ME_NX_APPSHOT=<file.bmp> chụp màn hình app sau 1.5 giây (J2ME_NX_APPSHOT_MS để đổi); trả về true khi đã chụp
static bool debug_appshot(void) {
#ifndef __SWITCH__
    const char *appshot = SDL_getenv("J2ME_NX_APPSHOT");
    const char *at = SDL_getenv("J2ME_NX_APPSHOT_MS");
    if (appshot && SDL_GetTicks() > (Uint32)(at ? atoi(at) : 1500)) {
        SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormat(0, SCREEN_W, SCREEN_H, 32, SDL_PIXELFORMAT_ARGB8888);
        SDL_Rect vp = { 0, 0, SCREEN_W, SCREEN_H };
        SDL_RenderReadPixels(gfx_renderer(), &vp, SDL_PIXELFORMAT_ARGB8888, surf->pixels, surf->pitch);
        SDL_SaveBMP(surf, appshot);
        SDL_FreeSurface(surf);
        return true;
    }
#endif
    return false;
}

// Test desktop: J2ME_NX_PRESS="1000:Return,1300:Down" bấm phím bàn phím (tên theo SDL) theo mốc ms
static void debug_press(void) {
#ifndef __SWITCH__
    static Uint32 last;
    const char *spec = SDL_getenv("J2ME_NX_PRESS");
    Uint32 now = SDL_GetTicks();
    for (const char *p = spec; p && *p;) {
        unsigned at;
        char name[32];
        int n = 0;
        if (sscanf(p, "%u:%31[^,]%n", &at, name, &n) != 2)
            break;
        // Nhấn tại mốc at, nhả sau 80ms (để vòng lặp kịp thấy phím được giữ)
        for (int phase = 0; phase < 2; phase++) {
            Uint32 t = at + (phase ? 80 : 0);
            if (t > last && t <= now) {
                SDL_Event e = { .type = phase ? SDL_KEYUP : SDL_KEYDOWN };
                e.key.keysym.sym = SDL_GetKeyFromName(name);
                SDL_PushEvent(&e);
            }
        }
        p += n;
        if (*p == ',')
            p++;
    }
    last = now;
#endif
}

static void launch(Menu *menu, const char *path, const char *id, int midlet) {
    char err[256] = "";
    if (!emu_start(path, id, midlet, err, sizeof(err)))
        snprintf(menu->status, sizeof(menu->status), tr(S_ERROR_FMT), err);
    else
        menu->status[0] = '\0';
}

int main(int argc, char *argv[]) {
    if (!platform_init())
        return 1;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) < 0) {
        printf("SDL_Init: %s\n", SDL_GetError());
        platform_exit();
        return 1;
    }

    int ret = 0;
    GameList list = { 0 };
    Menu menu = { 0 };
    const char *games_dir = platform_games_dir();

    if (!gfx_init("J2ME-NXX")) {
        ret = 1;
        goto out;
    }
    input_init();
    settings_load();
    game_list_scan(&list, games_dir);
    bool in_settings = false;
    bool in_video = false;
#ifndef __SWITCH__
    // Desktop: J2ME_NX_SCREEN=settings mở thẳng màn hình cài đặt (để test giao diện)
    const char *start_screen = SDL_getenv("J2ME_NX_SCREEN");
    if (start_screen && SDL_strcmp(start_screen, "settings") == 0) {
        settings_screen_open();
        in_settings = true;
    }
#endif
    if (argc > 1) {
        // Desktop: tham số là .jar thì chạy game, file khác thì mở bằng trình xem video
        const char *dot = strrchr(argv[1], '.');
        if (dot && strcasecmp(dot, ".jar") != 0) {
            char err[160];
            in_video = video_screen_open(argv[1], argv[1], err, sizeof(err));
            if (!in_video)
                snprintf(menu.status, sizeof(menu.status), "%s", err);
        } else {
            launch(&menu, argv[1], NULL, argc > 2 ? atoi(argv[2]) : 1);
        }
    }

    bool running = true;
    while (running) {
        debug_press();
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT)
                running = false;
            input_handle_event(&e);
            if (emu_running())
                emu_handle_event(&e);
        }
        input_update();

        if (emu_running()) {
            if (!emu_update()) {
                const char *msg = emu_exit_message();
                snprintf(menu.status, sizeof(menu.status), "%s", msg[0] ? msg : tr(S_GAME_EXITED));
                emu_stop();
#ifndef __SWITCH__
                // Kịch bản test (J2ME_NX_QUIT): thoát app luôn khi game kết thúc
                if (SDL_getenv("J2ME_NX_QUIT"))
                    running = false;
#endif
            } else {
                emu_draw();
                if (debug_appshot())
                    running = false;
                gfx_present();
                continue;
            }
        }

        if (in_video) {
            in_video = video_screen_update();
            if (in_video) {
                video_screen_draw();
                if (debug_appshot())
                    running = false;
                gfx_present();
                continue;
            }
            video_screen_close();
        }

        if (in_settings) {
            in_settings = settings_screen_update();
            if (in_settings) {
                settings_screen_draw();
                if (debug_appshot())
                    running = false;
                gfx_present();
                continue;
            }
            snprintf(menu.status, sizeof(menu.status), "%s", tr(S_SETTINGS_SAVED));
        }

        switch (menu_update(&menu, &list)) {
        case MENU_QUIT:
            running = false;
            break;
        case MENU_RESCAN:
            menu_free_textures(&list);
            game_list_scan(&list, games_dir);
            menu.cursor = menu.scroll = 0;
            snprintf(menu.status, sizeof(menu.status), tr(S_RESCANNED), list.count);
            break;
        case MENU_SETTINGS:
            settings_screen_open();
            in_settings = true;
            break;
        case MENU_GAME_OPTIONS:
            if (list.items[menu.cursor].video) {
                snprintf(menu.status, sizeof(menu.status), "%s", tr(S_VIDEO_NO_OPTIONS));
            } else {
                char id[256];
                game_list_id(&list.items[menu.cursor], id, sizeof(id));
                game_list_load_info(&list.items[menu.cursor]);
                settings_screen_open_game(id, list.items[menu.cursor].title);
                in_settings = true;
            }
            break;
        case MENU_LAUNCH:
            if (list.items[menu.cursor].video) {
                char err[160];
                in_video = video_screen_open(list.items[menu.cursor].path, list.items[menu.cursor].title, err,
                                             sizeof(err));
                if (!in_video)
                    snprintf(menu.status, sizeof(menu.status), "%s", err);
            } else {
                char id[256];
                game_list_id(&list.items[menu.cursor], id, sizeof(id));
                launch(&menu, list.items[menu.cursor].path, id, menu.midlet);
            }
            break;
        case MENU_NONE:
            break;
        }

        menu_draw(&menu, &list, games_dir);
        if (debug_appshot())
            running = false;
        gfx_present();
    }

    video_screen_close();
    emu_stop();
    menu_free_textures(&list);
    game_list_free(&list);
    input_exit();
out:
    gfx_exit();
    SDL_Quit();
    platform_exit();
    return ret;
}
