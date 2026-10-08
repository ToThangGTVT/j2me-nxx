// Màn hình chỉnh bố cục phím ảo: kéo phím để di chuyển, kéo góc để đổi kích thước, bo góc, ẩn / hiện,
// độ rõ, hít vào nhau. Dùng cảm ứng là chính, tay cầm chỉnh được vị trí và kích thước
#pragma once

#include <stdbool.h>
#include <SDL.h>

#include "vpad.h"

// layout: bố cục đang chỉnh, sửa trực tiếp
void vpad_screen_open(VpadLayout *layout);
void vpad_screen_handle_event(const SDL_Event *e);
// Trả về false khi người dùng quay lại
bool vpad_screen_update(void);
void vpad_screen_draw(void);
