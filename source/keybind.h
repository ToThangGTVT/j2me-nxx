// Ánh xạ nút Switch -> phím của điện thoại ảo (mã logic kiểu Nokia, xem keymap.h)
#pragma once

#include <stdbool.h>

typedef enum {
    BIND_A,
    BIND_B,
    BIND_X,
    BIND_Y,
    BIND_L,
    BIND_R,
    BIND_ZL,
    BIND_ZR,
    BIND_PLUS,
    BIND_LSTICK,            // bấm stick trái
    BIND_RSTICK,            // bấm stick phải
    BIND_UP,                // D-pad và stick trái
    BIND_DOWN,
    BIND_LEFT,
    BIND_RIGHT,
    BIND_RS_UP,             // stick phải
    BIND_RS_DOWN,
    BIND_RS_LEFT,
    BIND_RS_RIGHT,
    BIND_COUNT,
} BindButton;

#define BIND_NONE       0       // nút không bấm phím nào
#define BIND_INHERIT    1000    // tuỳ chọn riêng của ứng dụng: theo cài đặt chung

int keybind_default(BindButton b);
// Đặt cả bảng về mặc định
void keybind_reset(int *binds);
bool keybind_is_default(const int *binds);
// Số nút khác mặc định (cài đặt chung) / không theo cài đặt chung (tuỳ chọn riêng)
int keybind_changed(const int *binds, bool per_app);

// Tên trong file ini ("a", "zl", "rs_up"...); keybind_from_id trả -1 nếu không có
const char *keybind_id(BindButton b);
int keybind_from_id(const char *id);
// Tên hiển thị của nút (icon Switch hoặc chữ)
const char *keybind_button_name(BindButton b);

// Nút joystick của SDL2 bản Switch -> BindButton, -1 nếu không ánh xạ được (vd nút -)
int keybind_from_joy(int button);

// Các phím điện thoại chọn được, theo thứ tự khi bấm trái / phải
int keybind_key_count(void);
int keybind_key_at(int i);
int keybind_key_index(int code);    // -1 nếu không có trong danh sách
bool keybind_valid(int code);
const char *keybind_key_name(int code);
