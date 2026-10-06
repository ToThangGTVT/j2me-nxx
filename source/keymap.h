// Mã phím J2ME theo hãng điện thoại. Trong app luôn dùng mã Nokia làm mã logic,
// đổi sang mã của hãng ngay trước khi gửi cho game.
#pragma once

typedef enum {
    KEYMAP_NOKIA,
    KEYMAP_SONYERICSSON,
    KEYMAP_SAMSUNG,
    KEYMAP_MOTOROLA,
    KEYMAP_SIEMENS,
    KEYMAP_LG,
    KEYMAP_MOTOROLA_OLD,    // T720 / V300 (MIDP 1.0): mã phím dương
    KEYMAP_COUNT,
} KeyMapId;

typedef struct {
    const char *name;
    const char *platform;   // microedition.platform
    int up, down, left, right, fire, soft_left, soft_right, clear;
} KeyMap;

const KeyMap *keymap_get(int id);
// Đổi mã logic (kiểu Nokia) sang mã của hãng; phím số / * / # giữ nguyên
int keymap_translate(const KeyMap *km, int nokia_code);
