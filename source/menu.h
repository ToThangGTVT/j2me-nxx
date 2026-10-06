// Màn hình chọn game
#pragma once

#include "game_list.h"

typedef enum {
    MENU_NONE,
    MENU_LAUNCH,    // chọn game: menu.cursor là index trong list
    MENU_RESCAN,
    MENU_SETTINGS,
    MENU_GAME_OPTIONS,  // tuỳ chọn riêng của game menu.cursor
    MENU_QUIT,
} MenuAction;

typedef struct {
    int cursor;
    int scroll;
    char status[160];
} Menu;

MenuAction menu_update(Menu *m, const GameList *list);
void menu_draw(const Menu *m, const GameList *list, const char *games_dir);
