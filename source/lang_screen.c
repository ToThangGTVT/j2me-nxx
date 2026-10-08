#include "lang_screen.h"

#include "gfx.h"
#include "input.h"
#include "lang.h"
#include "settings.h"

#define PANEL_W     760
#define PANEL_H     440
#define BTN_W       300
#define BTN_H       120
#define BTN_GAP     40

#define COL_BG       RGB(0x24, 0x26, 0x2b)
#define COL_BAR      RGB(0x18, 0x19, 0x1d)
#define COL_ACCENT   RGB(0x00, 0xb4, 0xe6)
#define COL_TEXT     RGB(0xee, 0xee, 0xee)
#define COL_DIM      RGB(0x9a, 0x9f, 0xa8)
#define COL_ROW_SEL  RGB(0x33, 0x3a, 0x48)

// Thứ tự nút từ trái sang phải
static const Lang choices[] = { LANG_VI, LANG_EN };
#define CHOICE_COUNT ((int)(sizeof(choices) / sizeof(choices[0])))

static int cursor;
static int tapped;              // nút vừa được chạm, -1 nếu chưa

static int panel_x(void) { return (SCREEN_W - PANEL_W) / 2; }
static int panel_y(void) { return (SCREEN_H - PANEL_H) / 2; }
static int btn_y(void) { return panel_y() + 220; }

static int btn_x(int i) {
    int total = CHOICE_COUNT * BTN_W + (CHOICE_COUNT - 1) * BTN_GAP;
    return (SCREEN_W - total) / 2 + i * (BTN_W + BTN_GAP);
}

static int btn_at(int x, int y) {
    for (int i = 0; i < CHOICE_COUNT; i++) {
        if (x >= btn_x(i) && x < btn_x(i) + BTN_W && y >= btn_y() && y < btn_y() + BTN_H)
            return i;
    }
    return -1;
}

void lang_screen_open(void) {
    cursor = 0;
    tapped = -1;
}

void lang_screen_handle_event(const SDL_Event *e) {
    int i = -1;
    if (e->type == SDL_FINGERUP)
        i = btn_at((int)(e->tfinger.x * SCREEN_W), (int)(e->tfinger.y * SCREEN_H));
    else if (e->type == SDL_MOUSEBUTTONUP && e->button.which != SDL_TOUCH_MOUSEID &&
             e->button.button == SDL_BUTTON_LEFT)
        i = btn_at(e->button.x, e->button.y);
    if (i >= 0)
        tapped = i;
}

bool lang_screen_update(void) {
    if (input_pressed(BTN_LEFT) || input_pressed(BTN_UP))
        cursor = (cursor + CHOICE_COUNT - 1) % CHOICE_COUNT;
    if (input_pressed(BTN_RIGHT) || input_pressed(BTN_DOWN))
        cursor = (cursor + 1) % CHOICE_COUNT;
    int pick = tapped >= 0 ? tapped : input_pressed(BTN_A) ? cursor : -1;
    tapped = -1;
    if (pick < 0)
        return true;
    settings()->lang = choices[pick];
    lang_set(choices[pick]);
    settings_save();
    return false;
}

// Chữ trên bảng luôn là tiếng Anh: người dùng chưa chọn ngôn ngữ
void lang_screen_draw(void) {
    gfx_clear(COL_BG);

    int x = panel_x(), y = panel_y();
    gfx_fill_rect(x, y, PANEL_W, PANEL_H, COL_BAR);
    gfx_fill_rect(x, y, PANEL_W, 4, COL_ACCENT);

    int cx = SCREEN_W / 2;
    int ty = y + 48;
    gfx_text(FONT_LARGE, cx, ty, PANEL_W - 64, ALIGN_CENTER, COL_TEXT, "Welcome to J2ME-NXX");
    ty += gfx_font_height(FONT_LARGE) + 20;
    gfx_text(FONT_NORMAL, cx, ty, PANEL_W - 64, ALIGN_CENTER, COL_TEXT, "Choose your language");
    ty += gfx_font_height(FONT_NORMAL) + 10;
    gfx_text(FONT_SMALL, cx, ty, PANEL_W - 64, ALIGN_CENTER, COL_DIM, "You can change it later in Settings.");

    for (int i = 0; i < CHOICE_COUNT; i++) {
        bool sel = i == cursor;
        int bx = btn_x(i), by = btn_y();
        if (sel)
            gfx_fill_rect(bx - 4, by - 4, BTN_W + 8, BTN_H + 8, COL_ACCENT);
        gfx_fill_rect(bx, by, BTN_W, BTN_H, sel ? COL_ROW_SEL : COL_BG);
        gfx_text(FONT_LARGE, bx + BTN_W / 2, by + (BTN_H - gfx_font_height(FONT_LARGE)) / 2, BTN_W - 24,
                 ALIGN_CENTER, sel ? COL_TEXT : COL_DIM, lang_name(choices[i]));
    }

    gfx_text(FONT_SMALL, cx, y + PANEL_H - 24 - gfx_font_height(FONT_SMALL), PANEL_W - 64, ALIGN_CENTER, COL_DIM,
             ICON_LEFT ICON_RIGHT " Select   " ICON_A " Confirm");
}
