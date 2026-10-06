// Danh sách file .jar trong thư mục game
#pragma once

#include <stdbool.h>
#include <stddef.h>

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
// Tên file không có đuôi .jar: dùng làm khoá cho save RMS / tuỳ chọn riêng
void game_list_id(const GameEntry *g, char *out, size_t size);
void game_list_free(GameList *list);
