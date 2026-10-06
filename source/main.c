// J2ME-NX - J2ME emulator cho Nintendo Switch
//
// Màn hình chọn game (.jar) -> chạy MIDlet trên máy ảo Java tự viết (source/vm, source/midp).
// Chạy được cả trên Switch (devkitPro) và desktop (để test nhanh).
// Desktop: có thể truyền đường dẫn .jar làm tham số để chạy thẳng game.

#include <stdio.h>
#include <SDL.h>

#include "emu.h"
#include "game_list.h"
#include "gfx.h"
#include "input.h"
#include "menu.h"
#include "platform.h"
#include "settings.h"
#include "settings_screen.h"

static void launch(Menu *menu, const char *path) {
    char err[256] = "";
    if (!emu_start(path, err, sizeof(err)))
        snprintf(menu->status, sizeof(menu->status), "Loi: %s", err);
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

    if (!gfx_init("J2ME-NX")) {
        ret = 1;
        goto out;
    }
    input_init();
    settings_load();
    game_list_scan(&list, games_dir);
    bool in_settings = false;
#ifndef __SWITCH__
    // Desktop: J2ME_NX_SCREEN=settings mở thẳng màn hình cài đặt (để test giao diện)
    const char *start_screen = SDL_getenv("J2ME_NX_SCREEN");
    if (start_screen && SDL_strcmp(start_screen, "settings") == 0) {
        settings_screen_open();
        in_settings = true;
    }
#endif
    if (argc > 1)
        launch(&menu, argv[1]);

    bool running = true;
    while (running) {
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
                snprintf(menu.status, sizeof(menu.status), "%s", msg[0] ? msg : "Da thoat game");
                emu_stop();
#ifndef __SWITCH__
                // Kịch bản test (J2ME_NX_QUIT): thoát app luôn khi game kết thúc
                if (SDL_getenv("J2ME_NX_QUIT"))
                    running = false;
#endif
            } else {
                emu_draw();
                gfx_present();
                continue;
            }
        }

        if (in_settings) {
            in_settings = settings_screen_update();
            if (in_settings) {
                settings_screen_draw();
                gfx_present();
                continue;
            }
            snprintf(menu.status, sizeof(menu.status), "Da luu cai dat");
        }

        switch (menu_update(&menu, &list)) {
        case MENU_QUIT:
            running = false;
            break;
        case MENU_RESCAN:
            game_list_scan(&list, games_dir);
            menu.cursor = menu.scroll = 0;
            snprintf(menu.status, sizeof(menu.status), "Da quet lai: %d file", list.demo ? 0 : list.count);
            break;
        case MENU_SETTINGS:
            settings_screen_open();
            in_settings = true;
            break;
        case MENU_GAME_OPTIONS:
            if (list.demo) {
                snprintf(menu.status, sizeof(menu.status), "Day la list demo");
            } else {
                char id[256];
                game_list_id(&list.items[menu.cursor], id, sizeof(id));
                settings_screen_open_game(id, list.items[menu.cursor].name);
                in_settings = true;
            }
            break;
        case MENU_LAUNCH:
            if (list.demo)
                snprintf(menu.status, sizeof(menu.status), "Day la list demo: chep file .jar vao %s", games_dir);
            else
                launch(&menu, list.items[menu.cursor].path);
            break;
        case MENU_NONE:
            break;
        }

        menu_draw(&menu, &list, games_dir);
        gfx_present();
    }

    emu_stop();
    game_list_free(&list);
    input_exit();
out:
    gfx_exit();
    SDL_Quit();
    platform_exit();
    return ret;
}
