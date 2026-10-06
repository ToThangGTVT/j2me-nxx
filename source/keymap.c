#include "keymap.h"

static const KeyMap maps[KEYMAP_COUNT] = {
    [KEYMAP_NOKIA]        = { "Nokia", "Nokia6300/07.21", -1, -2, -3, -4, -5, -6, -7, -8 },
    [KEYMAP_SONYERICSSON] = { "Sony Ericsson", "SonyEricssonK800i/R1KG001", -1, -2, -3, -4, -5, -6, -7, -8 },
    [KEYMAP_SAMSUNG]      = { "Samsung", "SAMSUNG-SGH-E250", -1, -2, -3, -4, -5, -6, -7, -8 },
    [KEYMAP_MOTOROLA]     = { "Motorola", "MOT-RAZRV3", -1, -6, -2, -5, -20, -21, -22, -8 },
    [KEYMAP_SIEMENS]      = { "Siemens", "SIE-S65", -59, -60, -61, -62, -26, -1, -4, -12 },
    [KEYMAP_LG]           = { "LG", "LG-KG800", -1, -2, -3, -4, -5, -202, -203, -204 },
    [KEYMAP_MOTOROLA_OLD] = { "Motorola (cu)", "MOT-T720", 1, 6, 2, 5, 20, 21, 22, 23 },
};

const KeyMap *keymap_get(int id) {
    if (id < 0 || id >= KEYMAP_COUNT)
        id = KEYMAP_NOKIA;
    return &maps[id];
}

int keymap_translate(const KeyMap *km, int code) {
    switch (code) {
    case -1: return km->up;
    case -2: return km->down;
    case -3: return km->left;
    case -4: return km->right;
    case -5: return km->fire;
    case -6: return km->soft_left;
    case -7: return km->soft_right;
    case -8: return km->clear;
    default: return code;
    }
}
