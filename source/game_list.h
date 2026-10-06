// Danh sách game .jar trong thư mục games (quét cả thư mục con)
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    char name[256];         // đường dẫn tương đối trong thư mục games, vd "RPG/abc.jar"
    char path[512];         // đường dẫn đầy đủ
    long size;              // byte, -1 nếu là item demo

    // Đọc lười từ MANIFEST.MF / JAD (game_list_load_info)
    bool info_loaded;
    bool valid;             // có MIDlet-1, chạy được
    char title[128];        // MIDlet-Name, hoặc tên file
    char vendor[96];
    char version[32];
    uint32_t *icon;         // ARGB, NULL nếu không có
    int icon_w, icon_h;
    void *icon_tex;         // texture SDL do menu tạo (menu_free_textures giải phóng)
} GameEntry;

typedef struct {
    GameEntry *items;
    int count;
    bool demo;              // true nếu không có .jar nào, đang dùng list demo
} GameList;

void game_list_scan(GameList *list, const char *dir);
void game_list_load_info(GameEntry *g);
// Tên file không có đuôi .jar (và thư mục con): dùng làm khoá cho save RMS / tuỳ chọn riêng
void game_list_id(const GameEntry *g, char *out, size_t size);
void game_list_free(GameList *list);
