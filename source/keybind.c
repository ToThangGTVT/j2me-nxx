#include "keybind.h"

#include <string.h>

#include "gfx.h"
#include "lang.h"
#include "midp/midp.h"

static const struct {
    const char *id;
    int def;
} buttons[BIND_COUNT] = {
    [BIND_A]        = { "a", MIDP_KEY_FIRE },
    [BIND_B]        = { "b", MIDP_KEY_SOFT_RIGHT },
    [BIND_X]        = { "x", MIDP_KEY_POUND },
    [BIND_Y]        = { "y", MIDP_KEY_STAR },
    [BIND_L]        = { "l", MIDP_KEY_SOFT_LEFT },
    [BIND_R]        = { "r", MIDP_KEY_SOFT_RIGHT },
    [BIND_ZL]       = { "zl", '1' },
    [BIND_ZR]       = { "zr", '3' },
    [BIND_PLUS]     = { "plus", MIDP_KEY_SOFT_LEFT },
    [BIND_LSTICK]   = { "ls", '5' },
    [BIND_RSTICK]   = { "rs", '0' },
    [BIND_UP]       = { "up", MIDP_KEY_UP },
    [BIND_DOWN]     = { "down", MIDP_KEY_DOWN },
    [BIND_LEFT]     = { "left", MIDP_KEY_LEFT },
    [BIND_RIGHT]    = { "right", MIDP_KEY_RIGHT },
    [BIND_RS_UP]    = { "rs_up", '2' },
    [BIND_RS_DOWN]  = { "rs_down", '8' },
    [BIND_RS_LEFT]  = { "rs_left", '4' },
    [BIND_RS_RIGHT] = { "rs_right", '6' },
};

static const int keys[] = {
    BIND_NONE,
    MIDP_KEY_UP, MIDP_KEY_DOWN, MIDP_KEY_LEFT, MIDP_KEY_RIGHT, MIDP_KEY_FIRE,
    MIDP_KEY_SOFT_LEFT, MIDP_KEY_SOFT_RIGHT, MIDP_KEY_CLEAR,
    '1', '2', '3', '4', '5', '6', '7', '8', '9', MIDP_KEY_STAR, '0', MIDP_KEY_POUND,
};
#define KEY_COUNT ((int)(sizeof(keys) / sizeof(keys[0])))

int keybind_default(BindButton b) {
    return b >= 0 && b < BIND_COUNT ? buttons[b].def : BIND_NONE;
}

void keybind_reset(int *binds) {
    for (int i = 0; i < BIND_COUNT; i++)
        binds[i] = buttons[i].def;
}

bool keybind_is_default(const int *binds) {
    return keybind_changed(binds, false) == 0;
}

int keybind_changed(const int *binds, bool per_app) {
    int n = 0;
    for (int i = 0; i < BIND_COUNT; i++) {
        if (per_app ? binds[i] != BIND_INHERIT : binds[i] != buttons[i].def)
            n++;
    }
    return n;
}

const char *keybind_id(BindButton b) {
    return b >= 0 && b < BIND_COUNT ? buttons[b].id : "";
}

int keybind_from_id(const char *id) {
    for (int i = 0; i < BIND_COUNT; i++) {
        if (strcmp(buttons[i].id, id) == 0)
            return i;
    }
    return -1;
}

const char *keybind_button_name(BindButton b) {
    switch (b) {
    case BIND_A:        return ICON_A;
    case BIND_B:        return ICON_B;
    case BIND_X:        return ICON_X;
    case BIND_Y:        return ICON_Y;
    case BIND_L:        return ICON_L;
    case BIND_R:        return ICON_R;
    case BIND_ZL:       return ICON_ZL;
    case BIND_ZR:       return ICON_ZR;
    case BIND_PLUS:     return ICON_PLUS;
    case BIND_LSTICK:   return tr(S_BTN_LSTICK);
    case BIND_RSTICK:   return tr(S_BTN_RSTICK);
    case BIND_UP:       return ICON_UP;
    case BIND_DOWN:     return ICON_DOWN;
    case BIND_LEFT:     return ICON_LEFT;
    case BIND_RIGHT:    return ICON_RIGHT;
    case BIND_RS_UP:    return tr(S_BTN_RS_UP);
    case BIND_RS_DOWN:  return tr(S_BTN_RS_DOWN);
    case BIND_RS_LEFT:  return tr(S_BTN_RS_LEFT);
    case BIND_RS_RIGHT: return tr(S_BTN_RS_RIGHT);
    default:            return "?";
    }
}

int keybind_from_joy(int button) {
    switch (button) {
    case 0:  return BIND_A;
    case 1:  return BIND_B;
    case 2:  return BIND_X;
    case 3:  return BIND_Y;
    case 4:  return BIND_LSTICK;
    case 5:  return BIND_RSTICK;
    case 6:  return BIND_L;
    case 7:  return BIND_R;
    case 8:  return BIND_ZL;
    case 9:  return BIND_ZR;
    case 10: return BIND_PLUS;
    case 12: case 16: return BIND_LEFT;     // D-pad / stick trái
    case 13: case 17: return BIND_UP;
    case 14: case 18: return BIND_RIGHT;
    case 15: case 19: return BIND_DOWN;
    case 20: return BIND_RS_LEFT;
    case 21: return BIND_RS_UP;
    case 22: return BIND_RS_RIGHT;
    case 23: return BIND_RS_DOWN;
    default: return -1;
    }
}

int keybind_key_count(void) {
    return KEY_COUNT;
}

int keybind_key_at(int i) {
    return i >= 0 && i < KEY_COUNT ? keys[i] : BIND_NONE;
}

int keybind_key_index(int code) {
    for (int i = 0; i < KEY_COUNT; i++) {
        if (keys[i] == code)
            return i;
    }
    return -1;
}

bool keybind_valid(int code) {
    return keybind_key_index(code) >= 0;
}

const char *keybind_key_name(int code) {
    static const char *const digits[] = { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9" };
    switch (code) {
    case BIND_NONE:             return tr(S_KEY_NONE);
    case MIDP_KEY_UP:           return tr(S_KEY_UP);
    case MIDP_KEY_DOWN:         return tr(S_KEY_DOWN);
    case MIDP_KEY_LEFT:         return tr(S_KEY_LEFT);
    case MIDP_KEY_RIGHT:        return tr(S_KEY_RIGHT);
    case MIDP_KEY_FIRE:         return "Fire";
    case MIDP_KEY_SOFT_LEFT:    return tr(S_HELP_SOFT_LEFT);
    case MIDP_KEY_SOFT_RIGHT:   return tr(S_HELP_SOFT_RIGHT);
    case MIDP_KEY_CLEAR:        return tr(S_KEY_CLEAR);
    case MIDP_KEY_STAR:         return "*";
    case MIDP_KEY_POUND:        return "#";
    default:
        return code >= '0' && code <= '9' ? digits[code - '0'] : "?";
    }
}
