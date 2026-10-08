// Phím ảo trên màn hình cảm ứng khi chạy ứng dụng: cần điều khiển (đi theo ánh xạ của stick trái)
// và các phím của điện thoại. Bố cục do người dùng chỉnh (vpad_screen.c), lưu trong cài đặt chung.
#pragma once

#include <stdbool.h>
#include <SDL.h>

#include "vkb.h"

typedef enum {
    VP_STICK,
    VP_SOFT_LEFT,
    VP_SOFT_RIGHT,
    VP_FIRE,
    VP_CLEAR,
    VP_1,
    VP_2,
    VP_3,
    VP_4,
    VP_5,
    VP_6,
    VP_7,
    VP_8,
    VP_9,
    VP_STAR,
    VP_0,
    VP_POUND,
    VP_COUNT,
} VpadItem;

// Kiểu bàn phím (bộ phím và bố cục mặc định). Mới có Nokia
typedef enum {
    VPAD_STYLE_NOKIA,
    VPAD_STYLE_COUNT,
} VpadStyle;

typedef struct {
    int x, y, w, h;         // toạ độ màn hình 1280x720; cần điều khiển là hình tròn nội tiếp (w = h)
    int r;                  // bo góc
    bool hidden;
} VpadKey;

typedef struct {
    int style;              // VpadStyle
    int opacity;            // độ rõ, %
    bool snap;              // kéo phím tới gần phím khác / mép màn hình thì hít vào
    VpadKey keys[VP_COUNT];
} VpadLayout;

#define VPAD_OPACITY_MIN 20
#define VPAD_OPACITY_MAX 100
#define VPAD_SIZE_MIN    40

void vpad_layout_default(VpadLayout *l);
// Phím i khác bố cục mặc định
bool vpad_key_changed(const VpadLayout *l, VpadItem i);
bool vpad_layout_changed(const VpadLayout *l);
// Phím đọc từ file nằm gọn trong màn hình, đủ lớn
bool vpad_key_valid(const VpadKey *k);
const char *vpad_style_name(int style);

// Tên trong file ini ("stick", "soft_left", "5"...); vpad_item_from_id trả -1 nếu không có
const char *vpad_item_id(VpadItem i);
int vpad_item_from_id(const char *id);
// Tên hiển thị
void vpad_item_name(VpadItem i, char *out, size_t size);
// Mã phím J2ME (kiểu Nokia) của phím, 0 với cần điều khiển
int vpad_item_code(VpadItem i);

// Vẽ 1 phím; alpha nhân thêm với độ rõ của bố cục. Cần điều khiển: (kx, ky) là độ lệch của núm
void vpad_draw_item(const VpadLayout *l, VpadItem i, bool down, int kx, int ky, Uint8 alpha);

// Khi chạy ứng dụng. key(code, down): bấm / nhả phím J2ME; dirs: phím của 4 hướng lên, xuống, trái, phải
typedef void (*VpadKeyFn)(int code, bool down);
void vpad_start(bool enabled, const VpadLayout *l, const int dirs[4], VpadKeyFn key);
bool vpad_enabled(void);
// Chạm / chuột (toạ độ 1280x720). true = phím ảo đã nhận, không chuyển cho ứng dụng
bool vpad_pointer(SDL_FingerID id, VkbPointer type, int x, int y);
// Nhả mọi phím đang giữ
void vpad_release_all(void);
void vpad_draw(void);
