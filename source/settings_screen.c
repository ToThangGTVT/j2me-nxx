#include "settings_screen.h"

#include <stdio.h>
#include <string.h>

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
    ITEM_SCREEN,
    ITEM_COUNT,
};

static int cursor;
static bool game_mode;
static char game_id[128];
static char game_title[128];
static GameSettings game;

void settings_screen_open(void) {
    cursor = 0;
    game_mode = false;
}

void settings_screen_open_game(const char *id, const char *title) {
    cursor = 0;
    game_mode = true;
    snprintf(game_id, sizeof(game_id), "%s", id);
    snprintf(game_title, sizeof(game_title), "%s", title);
    game_settings_load(game_id, &game);
}

// Chỉ số trong danh sách lựa chọn; ở chế độ game có thêm lựa chọn "mặc định" ở vị trí 0
static int fps_pos(int fps) {
    int base = game_mode ? 1 : 0;
    if (game_mode && fps < 0)
        return 0;
    for (int i = 0; i < SETTINGS_FPS_CHOICE_COUNT; i++) {
        if (SETTINGS_FPS_CHOICES[i] == fps)
            return i + base;
    }
    return base;
}

static int screen_pos(int w, int h) {
    int base = game_mode ? 1 : 0;
    if (game_mode && w == 0)
        return 0;
    for (int i = 0; i < SETTINGS_SCREEN_CHOICE_COUNT; i++) {
        if (SETTINGS_SCREEN_CHOICES[i].w == w && SETTINGS_SCREEN_CHOICES[i].h == h)
            return i + base;
    }
    return base;
}

static void change(int item, int dir) {
    int base = game_mode ? 1 : 0;
    if (item == ITEM_FPS) {
        int n = SETTINGS_FPS_CHOICE_COUNT + base;
        int cur = fps_pos(game_mode ? game.fps_limit : settings()->fps_limit);
        int i = (cur + dir + n) % n;
        int v = (game_mode && i == 0) ? -1 : SETTINGS_FPS_CHOICES[i - base];
        if (game_mode)
            game.fps_limit = v;
        else
            settings()->fps_limit = v;
    } else if (item == ITEM_SCREEN) {
        int n = SETTINGS_SCREEN_CHOICE_COUNT + base;
        int cur = game_mode ? screen_pos(game.screen_w, game.screen_h)
                            : screen_pos(settings()->screen_w, settings()->screen_h);
        int i = (cur + dir + n) % n;
        ScreenSize s = (game_mode && i == 0) ? (ScreenSize){ 0, 0 } : SETTINGS_SCREEN_CHOICES[i - base];
        if (game_mode) {
            game.screen_w = s.w;
            game.screen_h = s.h;
        } else {
            settings()->screen_w = s.w;
            settings()->screen_h = s.h;
        }
    }
}

bool settings_screen_update(void) {
    if (input_pressed(BTN_B) || input_pressed(BTN_X) || input_pressed(BTN_PLUS) || input_pressed(BTN_MINUS)) {
        if (game_mode)
            game_settings_save(game_id, &game);
        else
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

static void fps_text(int fps, char *out, size_t size) {
    if (fps > 0)
        snprintf(out, size, "%d FPS", fps);
    else
        snprintf(out, size, "Khong gioi han");
}

static void item_text(int item, const char **label, const char **hint, char *value, size_t size) {
    Settings *s = settings();
    char tmp[48];
    switch (item) {
    case ITEM_FPS:
        *label = "Gioi han FPS";
        *hint = "So khung hinh toi da moi giay cua game. Giup game chay dung toc do va do ton pin.";
        if (game_mode && game.fps_limit < 0) {
            fps_text(s->fps_limit, tmp, sizeof(tmp));
            snprintf(value, size, "Mac dinh (%s)", tmp);
        } else {
            fps_text(game_mode ? game.fps_limit : s->fps_limit, value, size);
        }
        break;
    case ITEM_SCREEN:
        *label = game_mode ? "Kich thuoc man hinh" : "Kich thuoc man hinh mac dinh";
        *hint = game_mode ? "Tu dong: lay tu MANIFEST cua game, neu khong co thi dung kich thuoc mac dinh."
                          : "Dung cho game khong khai bao kich thuoc. Game dien thoai pho bien nhat la 240x320.";
        if (game_mode && game.screen_w == 0)
            snprintf(value, size, "Tu dong");
        else if (game_mode)
            snprintf(value, size, "%dx%d", game.screen_w, game.screen_h);
        else
            snprintf(value, size, "%dx%d", s->screen_w, s->screen_h);
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
    int ty = (HEADER_H - gfx_font_height(FONT_LARGE)) / 2;
    int w = gfx_text(FONT_LARGE, LIST_X, ty, 0, ALIGN_LEFT, COL_TEXT, game_mode ? "Tuy chon game" : "Cai dat");
    if (game_mode)
        gfx_text(FONT_NORMAL, LIST_X + w + 24, ty + gfx_font_height(FONT_LARGE) - gfx_font_height(FONT_NORMAL) - 4,
                 SCREEN_W - LIST_X * 2 - w - 24, ALIGN_LEFT, COL_DIM, game_title);

    int font_y = (ROW_H - gfx_font_height(FONT_NORMAL)) / 2;
    for (int i = 0; i < ITEM_COUNT; i++) {
        int y = LIST_TOP + i * ROW_H;
        bool sel = i == cursor;
        const char *label, *hint;
        char value[96];
        item_text(i, &label, &hint, value, sizeof(value));

        if (sel) {
            gfx_fill_rect(LIST_X, y, SCREEN_W - 2 * LIST_X, ROW_H, COL_ROW_SEL);
            gfx_fill_rect(LIST_X, y, 6, ROW_H, COL_ACCENT);
        }
        gfx_text(FONT_NORMAL, LIST_X + 28, y + font_y, 0, ALIGN_LEFT, COL_TEXT, label);

        char shown[128];
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
