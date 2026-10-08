// Bàn phím ảo QWERTY trong game: bong bóng nổi (kéo được), chạm để mở bàn phím nổi; nút × thu về bong bóng
#pragma once

#include <stdbool.h>
#include <SDL.h>

// Gửi phím cho game: type là MIDP_EV_KEY_PRESSED / RELEASED / REPEATED, code là mã phím J2ME (chưa qua keymap)
typedef void (*VkbSend)(int type, int code);

typedef enum {
    VKB_DOWN,
    VKB_MOVE,
    VKB_UP,
} VkbPointer;

// Gọi khi bắt đầu game; enabled = false thì không có bong bóng
void vkb_start(bool enabled, VkbSend send);
// Chạm / chuột (toạ độ màn hình 1280x720). id: finger id, chuột = -1. true = bàn phím đã nhận, không chuyển cho game
bool vkb_pointer(SDL_FingerID id, VkbPointer type, int x, int y);
// Gọi mỗi frame: lặp phím khi giữ
void vkb_update(void);
void vkb_draw(void);
// Nhả mọi phím đang giữ
void vkb_release_all(void);
