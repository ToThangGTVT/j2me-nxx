// Màn hình cài đặt: cài đặt chung, hoặc tuỳ chọn riêng của 1 game
#pragma once

#include <stdbool.h>

void settings_screen_open(void);
// game: tên file JAR không có đuôi; title: tên hiển thị
void settings_screen_open_game(const char *game, const char *title);
// Trả về false khi người dùng đóng màn hình (đã lưu cài đặt)
bool settings_screen_update(void);
void settings_screen_draw(void);
