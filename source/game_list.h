// Danh sách game .jar (và file video) trong thư mục games (quét cả thư mục con)
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    char name[256];         // đường dẫn tương đối trong thư mục games, vd "RPG/abc.jar"
    char path[512];         // đường dẫn đầy đủ
    long size;              // byte
    bool video;             // file video (.3gp, .mp4...): mở bằng trình xem video

    // Đọc lười từ MANIFEST.MF / JAD (game_list_load_info)
    bool info_loaded;
    bool valid;             // có MIDlet-1, chạy được
    char title[128];        // MIDlet-Name, hoặc tên file
    char vendor[96];
    char version[32];
    int midlet_count;       // số MIDlet trong suite (MIDlet-1..N)
    char midlets[8][64];    // tên hiển thị của từng MIDlet
    uint32_t *icon;         // ARGB, NULL nếu không có
    int icon_w, icon_h;
    int icon_img;           // ảnh NanoVG do giao diện tạo (0 = chưa có)
} GameEntry;

typedef struct {
    GameEntry *items;
    int count;
} GameList;

void game_list_scan(GameList *list, const char *dir);
void game_list_load_info(GameEntry *g);
// Tên file không có đuôi .jar (và thư mục con): dùng làm khoá cho save RMS / tuỳ chọn riêng
void game_list_id(const GameEntry *g, char *out, size_t size);
void game_list_free(GameList *list);

// File .jad cùng tên cạnh file .jar (đuôi .jad hoặc .JAD); false nếu không có
bool game_list_find_jad(const GameEntry *g, char *out, size_t size);
// Xoá file game (kèm .jad cùng tên) hoặc file video. Không đụng tới save RMS / tuỳ chọn riêng
bool game_list_delete(const GameEntry *g);

// Đuôi file video mở được bằng trình xem video (".3gp", ".mp4"...)
extern const char *const game_list_video_exts[];
extern const int game_list_video_ext_count;
bool game_list_is_video(const char *name);
