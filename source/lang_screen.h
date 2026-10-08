// Bảng chọn ngôn ngữ khi mở app lần đầu (chưa có settings.ini)
#pragma once

#include <stdbool.h>
#include <SDL.h>

void lang_screen_open(void);
// Chạm / bấm chuột vào nút ngôn ngữ
void lang_screen_handle_event(const SDL_Event *e);
// Trả về false khi đã chọn xong (đã lưu cài đặt)
bool lang_screen_update(void);
void lang_screen_draw(void);
