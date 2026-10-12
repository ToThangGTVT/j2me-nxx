// Cảm ứng trên Switch: SDL2 bản Switch báo chạm sai (chuyển động mang số thứ tự trong mảng thay vì mã ngón,
// SDL gặp mã lạ thì tự sinh FINGERDOWN không bao giờ có FINGERUP -> phím ảo kẹt; chạm mới chỉ báo khi số ngón
// tăng). Ở đây đọc thẳng màn cảm ứng, so với lần đọc trước theo mã ngón rồi tự sinh sự kiện SDL_FINGER*.
#pragma once

#include <stdbool.h>
#include <SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TOUCH_MAX 16

typedef struct {
    SDL_FingerID id;
    float x, y;             // 0..1 như tfinger.x / tfinger.y
} TouchPoint;

typedef struct {
    int count;
    TouchPoint p[TOUCH_MAX];
} TouchFrame;

typedef void (*TouchSendFn)(SDL_Event *e);

// Sự kiện từ old sang cur: nhả trước, rồi di chuyển, rồi chạm mới
void touch_diff(const TouchFrame *old, const TouchFrame *cur, TouchSendFn send);

#ifdef __SWITCH__
// Đọc màn cảm ứng, gửi sự kiện thay đổi so với lần trước
void touch_poll(TouchSendFn send);
#endif

#ifdef __cplusplus
}
#endif
