#include "settings_screen.h"

#include <stdio.h>

#include "gfx.h"
#include "input.h"
#include "settings.h"

#define HEADER_H    80
#define FOOTER_H    72
#define LIST_X      40
#define LIST_TOP    120
#define ROW_H       64

#define COL_BG       RGB(0x24, 0x26, 0x2b)
#define COL_BAR      RGB(0x18, 0x19, 0x1d)
#define COL_ACCENT   RGB(0x00, 0xb4, 0xe6)
#define COL_TEXT     RGB(0xee, 0xee, 0xee)
#define COL_DIM      RGB(0x9a, 0x9f, 0xa8)
#define COL_ROW_SEL  RGB(0x33, 0x3a, 0x48)

enum {
    ITEM_FPS,
    ITEM_COUNT,
};

static int cursor;

void settings_screen_open(void) {
    cursor = 0;
}

static int fps_index(int fps) {
    for (int i = 0; i < SETTINGS_FPS_CHOICE_COUNT; i++) {
        if (SETTINGS_FPS_CHOICES[i] == fps)
            return i;
    }
    return 0;
}

static void change(int item, int dir) {
    Settings *s = settings();
    if (item == ITEM_FPS) {
        int n = SETTINGS_FPS_CHOICE_COUNT;
        int i = (fps_index(s->fps_limit) + dir + n) % n;
        s->fps_limit = SETTINGS_FPS_CHOICES[i];
    }
}

bool settings_screen_update(void) {
    if (input_pressed(BTN_B) || input_pressed(BTN_X) || input_pressed(BTN_PLUS)) {
        settings_save();
        return false;
    }
    if (input_pressed(BTN_DOWN))
        cursor = (cursor + 1) % ITEM_COUNT;
    if (input_pressed(BTN_UP))
        cursor = (cursor + ITEM_COUNT - 1) % ITEM_COUNT;
    if (input_pressed(BTN_RIGHT) || input_pressed(BTN_A))
        change(cursor, 1);
    if (input_pressed(BTN_LEFT))
        change(cursor, -1);
    return true;
}

static void item_text(int item, const char **label, const char **hint, char *value, size_t size) {
    Settings *s = settings();
    switch (item) {
    case ITEM_FPS:
        *label = "Gioi han FPS";
        *hint = "So khung hinh toi da moi giay cua game. Giup game chay dung toc do va do ton pin.";
        if (s->fps_limit > 0)
            snprintf(value, size, "%d FPS", s->fps_limit);
        else
            snprintf(value, size, "Khong gioi han");
        break;
    default:
        *label = *hint = "";
        value[0] = '\0';
        break;
    }
}

void settings_screen_draw(void) {
    gfx_clear(COL_BG);

    gfx_fill_rect(0, 0, SCREEN_W, HEADER_H, COL_BAR);
    gfx_fill_rect(0, HEADER_H, SCREEN_W, 2, COL_ACCENT);
    gfx_text(FONT_LARGE, LIST_X, (HEADER_H - gfx_font_height(FONT_LARGE)) / 2, 0, ALIGN_LEFT, COL_TEXT, "Cai dat");

    int font_y = (ROW_H - gfx_font_height(FONT_NORMAL)) / 2;
    for (int i = 0; i < ITEM_COUNT; i++) {
        int y = LIST_TOP + i * ROW_H;
        bool sel = i == cursor;
        const char *label, *hint;
        char value[64];
        item_text(i, &label, &hint, value, sizeof(value));

        if (sel) {
            gfx_fill_rect(LIST_X, y, SCREEN_W - 2 * LIST_X, ROW_H, COL_ROW_SEL);
            gfx_fill_rect(LIST_X, y, 6, ROW_H, COL_ACCENT);
        }
        gfx_text(FONT_NORMAL, LIST_X + 28, y + font_y, 0, ALIGN_LEFT, COL_TEXT, label);

        char shown[96];
        snprintf(shown, sizeof(shown), sel ? "<  %s  >" : "%s", value);
        gfx_text(FONT_NORMAL, SCREEN_W - LIST_X - 28, y + font_y, 0, ALIGN_RIGHT, sel ? COL_ACCENT : COL_DIM, shown);

        if (sel)
            gfx_text(FONT_SMALL, LIST_X + 28, LIST_TOP + ITEM_COUNT * ROW_H + 24, SCREEN_W - 2 * LIST_X - 56,
                     ALIGN_LEFT, COL_DIM, hint);
    }

    int y0 = SCREEN_H - FOOTER_H;
    gfx_fill_rect(0, y0, SCREEN_W, FOOTER_H, COL_BAR);
    gfx_text(FONT_NORMAL, SCREEN_W - LIST_X, y0 + (FOOTER_H - gfx_font_height(FONT_NORMAL)) / 2, 0, ALIGN_RIGHT,
             COL_TEXT, "(<>) Doi gia tri     (B) Luu va quay lai");
}
