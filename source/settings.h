// Cài đặt chung (<data_dir>/settings.ini) và cài đặt riêng từng game (<data_dir>/games/<tên>.ini)
#pragma once

#include <stdbool.h>

typedef struct {
    int fps_limit;          // 0 = không giới hạn
    int screen_w, screen_h; // kích thước màn hình mặc định cho game
} Settings;

typedef struct {
    int fps_limit;          // -1 = theo cài đặt chung
    int screen_w, screen_h; // 0 = tự động (MANIFEST, rồi tới cài đặt chung)
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
extern const ScreenSize SETTINGS_SCREEN_CHOICES[];
extern const int SETTINGS_SCREEN_CHOICE_COUNT;
