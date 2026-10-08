// Gom Joy-Con / Pro Controller (Switch) và bàn phím (desktop) về một bộ nút chung
#pragma once

#include <stdbool.h>
#include <SDL.h>

typedef enum {
    BTN_UP,
    BTN_DOWN,
    BTN_LEFT,
    BTN_RIGHT,
    BTN_A,
    BTN_B,
    BTN_X,
    BTN_Y,
    BTN_L,
    BTN_R,
    BTN_PLUS,
    BTN_MINUS,
    BTN_COUNT
} Button;

void input_init(void);
void input_exit(void);

// Gọi cho mỗi SDL_Event trong vòng poll
void input_handle_event(const SDL_Event *e);

// Gọi 1 lần mỗi frame, sau khi poll xong
void input_update(void);

// Vừa bấm frame này (nút hướng có auto-repeat khi giữ)
bool input_pressed(Button b);
bool input_held(Button b);
// Còn nút nào đang giữ (để chờ nhả hết rồi mới trả phím cho giao diện)
bool input_any_held(void);
