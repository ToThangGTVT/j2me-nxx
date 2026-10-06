// Màn hình cài đặt (mở từ danh sách game)
#pragma once

#include <stdbool.h>

void settings_screen_open(void);
// Trả về false khi người dùng đóng màn hình (đã lưu cài đặt)
bool settings_screen_update(void);
void settings_screen_draw(void);
