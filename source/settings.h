// Cài đặt của app, lưu ở <data_dir>/settings.ini
#pragma once

#include <stdbool.h>

typedef struct {
    int fps_limit;      // 0 = không giới hạn
} Settings;

void settings_load(void);
bool settings_save(void);
Settings *settings(void);

// Các mức giới hạn FPS cho phép chọn
extern const int SETTINGS_FPS_CHOICES[];
extern const int SETTINGS_FPS_CHOICE_COUNT;
