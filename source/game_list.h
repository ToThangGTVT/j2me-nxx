// Danh sách file .jar trong thư mục game
#pragma once

#include <stdbool.h>

typedef struct {
    char name[256];
    char path[512];
    long size;          // byte, -1 nếu là item demo
} GameEntry;

typedef struct {
    GameEntry *items;
    int count;
    bool demo;          // true nếu không có .jar nào, đang dùng list demo
} GameList;

void game_list_scan(GameList *list, const char *dir);
void game_list_free(GameList *list);
