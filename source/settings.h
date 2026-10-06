// Cài đặt chung (<data_dir>/settings.ini) và cài đặt riêng từng game (<data_dir>/games/<tên>.ini)
#pragma once

#include <stdbool.h>

typedef struct {
    int fps_limit;          // 0 = không giới hạn
    int screen_w, screen_h; // kích thước màn hình mặc định cho game
    int lang;               // Lang (lang.h)
    int keymap;             // KeyMapId (keymap.h)
    int scale_mode;         // 0 sắc nét (sharp-bilinear), 1 điểm ảnh, 2 điểm ảnh số nguyên
    bool show_help;         // hiện bảng phím bên trái khi chơi
} Settings;

typedef struct {
    int fps_limit;          // -1 = theo cài đặt chung
    int screen_w, screen_h; // 0 = tự động (MANIFEST, rồi tới cài đặt chung)
    int keymap;             // -1 = theo cài đặt chung
} GameSettings;

typedef struct {
    int w, h;
} ScreenSize;

void settings_load(void);
bool settings_save(void);
Settings *settings(void);

void game_settings_load(const char *game, GameSettings *out);
bool game_settings_save(const char *game, const GameSettings *gs);

// Các lựa chọn
extern const int SETTINGS_FPS_CHOICES[];
extern const int SETTINGS_FPS_CHOICE_COUNT;
// Cỡ màn hình có sẵn, ghi theo hướng dọc (w <= h); hướng ngang = đảo w/h
extern const ScreenSize SETTINGS_SCREEN_CHOICES[];
extern const int SETTINGS_SCREEN_CHOICE_COUNT;

#define SCREEN_MIN 64
#define SCREEN_MAX 1280
bool settings_valid_screen(int w, int h);
