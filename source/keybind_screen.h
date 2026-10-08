// Màn hình ánh xạ phím: chọn phím điện thoại cho từng nút Switch, bằng tay cầm hoặc cảm ứng
#pragma once

#include <stdbool.h>
#include <SDL.h>

// binds: bảng đang chỉnh (BIND_COUNT phần tử), sửa trực tiếp.
// global: bảng của cài đặt chung khi đang chỉnh tuỳ chọn riêng của ứng dụng (binds có thể là BIND_INHERIT),
// NULL khi đang chỉnh cài đặt chung.
void keybind_screen_open(int *binds, const int *global, const char *subtitle);
void keybind_screen_handle_event(const SDL_Event *e);
// Trả về false khi người dùng quay lại
bool keybind_screen_update(void);
void keybind_screen_draw(void);
