// Màn hình chọn game
#pragma once

#include "game_list.h"

typedef enum {
    MENU_NONE,
    MENU_LAUNCH,    // chọn game: menu.cursor là index trong list
    MENU_RESCAN,
    MENU_SETTINGS,
    MENU_GAME_OPTIONS,  // tuỳ chọn riêng của game menu.cursor
    MENU_UPDATE,        // mở màn hình cập nhật (có bản mới)
    MENU_UPLOAD,        // gửi game từ điện thoại (mã QR)
    MENU_QUIT,
} MenuAction;

typedef struct {
    int cursor;
    int scroll;
    char status[160];
    bool picking;       // đang hiện hộp chọn MIDlet
    int pick;           // MIDlet đang chọn (0-based)
    int midlet;         // MIDlet sẽ chạy khi MENU_LAUNCH (từ 1)
} Menu;

MenuAction menu_update(Menu *m, GameList *list);
// Vẽ danh sách; đọc dần thông tin (MANIFEST, icon) của các game đang hiện
void menu_draw(const Menu *m, GameList *list, const char *games_dir);
// Giải phóng texture icon trước khi quét lại / thoát
void menu_free_textures(GameList *list);
