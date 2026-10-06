#include "settings_screen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gfx.h"
#include "input.h"
#include "platform.h"
#include "settings.h"

#define HEADER_H    80
#define FOOTER_H    72
#define LIST_X      40
#define LIST_TOP    110
#define ROW_H       60

#define COL_BG       RGB(0x24, 0x26, 0x2b)
#define COL_BAR      RGB(0x18, 0x19, 0x1d)
#define COL_ACCENT   RGB(0x00, 0xb4, 0xe6)
#define COL_TEXT     RGB(0xee, 0xee, 0xee)
#define COL_DIM      RGB(0x9a, 0x9f, 0xa8)
#define COL_ROW_SEL  RGB(0x33, 0x3a, 0x48)

typedef enum {
    ITEM_FPS,
    ITEM_SIZE,
    ITEM_ORIENT,
    ITEM_WIDTH,
    ITEM_HEIGHT,
} ItemId;

static int cursor;
static bool game_mode;
static bool custom;             // đang ở chế độ nhập kích thước tuỳ chỉnh
static char game_id[128];
static char game_title[128];
static GameSettings game;

// Con trỏ tới kích thước đang chỉnh (cài đặt chung hoặc của game)
static int *cur_w(void) { return game_mode ? &game.screen_w : &settings()->screen_w; }
static int *cur_h(void) { return game_mode ? &game.screen_h : &settings()->screen_h; }

static bool is_auto(void) {
    return game_mode && *cur_w() == 0;
}

// Vị trí trong danh sách cỡ có sẵn (bất kể hướng), -1 nếu không khớp
static int preset_index(int w, int h) {
    int a = w < h ? w : h, b = w < h ? h : w;
    for (int i = 0; i < SETTINGS_SCREEN_CHOICE_COUNT; i++) {
        if (SETTINGS_SCREEN_CHOICES[i].w == a && SETTINGS_SCREEN_CHOICES[i].h == b)
            return i;
    }
    return -1;
}

static void sync_custom(void) {
    custom = !is_auto() && preset_index(*cur_w(), *cur_h()) < 0;
}

void settings_screen_open(void) {
    cursor = 0;
    game_mode = false;
    sync_custom();
}

void settings_screen_open_game(const char *id, const char *title) {
    cursor = 0;
    game_mode = true;
    snprintf(game_id, sizeof(game_id), "%s", id);
    snprintf(game_title, sizeof(game_title), "%s", title);
    game_settings_load(game_id, &game);
    sync_custom();
}

// Danh sách mục đang hiện (phụ thuộc chế độ tự động / tuỳ chỉnh)
static int visible_items(ItemId *out) {
    int n = 0;
    out[n++] = ITEM_FPS;
    out[n++] = ITEM_SIZE;
    if (!is_auto()) {
        out[n++] = ITEM_ORIENT;
        if (custom) {
            out[n++] = ITEM_WIDTH;
            out[n++] = ITEM_HEIGHT;
        }
    }
    return n;
}

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

static void change_fps(int dir) {
    int base = game_mode ? 1 : 0;
    int n = SETTINGS_FPS_CHOICE_COUNT + base;
    int cur = fps_pos(game_mode ? game.fps_limit : settings()->fps_limit);
    int i = (cur + dir + n) % n;
    int v = (game_mode && i == 0) ? -1 : SETTINGS_FPS_CHOICES[i - base];
    if (game_mode)
        game.fps_limit = v;
    else
        settings()->fps_limit = v;
}

// Danh sách lựa chọn kích thước: [Tự động (chỉ game)] + các cỡ có sẵn + Tuỳ chỉnh
static void change_size(int dir) {
    int base = game_mode ? 1 : 0;
    int n = base + SETTINGS_SCREEN_CHOICE_COUNT + 1;
    int cur;
    if (is_auto())
        cur = 0;
    else if (custom)
        cur = n - 1;
    else
        cur = base + preset_index(*cur_w(), *cur_h());
    int i = (cur + dir + n) % n;

    bool landscape = *cur_w() > *cur_h();
    if (game_mode && i == 0) {
        *cur_w() = *cur_h() = 0;
        custom = false;
    } else if (i == n - 1) {
        // Tuỳ chỉnh: bắt đầu từ cỡ đang có
        if (is_auto()) {
            *cur_w() = settings()->screen_w;
            *cur_h() = settings()->screen_h;
        }
        custom = true;
    } else {
        ScreenSize s = SETTINGS_SCREEN_CHOICES[i - base];
        *cur_w() = landscape ? s.h : s.w;
        *cur_h() = landscape ? s.w : s.h;
        custom = false;
    }
}

static void swap_orientation(void) {
    int w = *cur_w();
    *cur_w() = *cur_h();
    *cur_h() = w;
}

static int clamp_size(int v) {
    return v < SCREEN_MIN ? SCREEN_MIN : v > SCREEN_MAX ? SCREEN_MAX : v;
}

static void edit_number(int *value, const char *title) {
    char text[16];
    snprintf(text, sizeof(text), "%d", *value);
    char *r = platform_keyboard(title, text, 4, 2);     // 2 = TextField.NUMERIC
    if (r) {
        int v = atoi(r);
        if (v > 0)
            *value = clamp_size(v);
        free(r);
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

    ItemId items[8];
    int n = visible_items(items);
    if (cursor >= n)
        cursor = n - 1;
    if (input_pressed(BTN_DOWN))
        cursor = (cursor + 1) % n;
    if (input_pressed(BTN_UP))
        cursor = (cursor + n - 1) % n;

    int dir = input_pressed(BTN_RIGHT) ? 1 : input_pressed(BTN_LEFT) ? -1 : 0;
    int big = input_pressed(BTN_R) ? 10 : input_pressed(BTN_L) ? -10 : 0;
    bool a = input_pressed(BTN_A);

    switch (items[cursor]) {
    case ITEM_FPS:
        if (dir || a)
            change_fps(dir ? dir : 1);
        break;
    case ITEM_SIZE:
        if (dir || a)
            change_size(dir ? dir : 1);
        break;
    case ITEM_ORIENT:
        if (dir || a)
            swap_orientation();
        break;
    case ITEM_WIDTH:
    case ITEM_HEIGHT: {
        int *v = items[cursor] == ITEM_WIDTH ? cur_w() : cur_h();
        if (dir || big)
            *v = clamp_size(*v + dir + big);
        if (a)
            edit_number(v, items[cursor] == ITEM_WIDTH ? "Chieu rong man hinh" : "Chieu cao man hinh");
        break;
    }
    }
    return true;
}

static void fps_text(int fps, char *out, size_t size) {
    if (fps > 0)
        snprintf(out, size, "%d FPS", fps);
    else
        snprintf(out, size, "Khong gioi han");
}

static void item_text(ItemId item, const char **label, const char **hint, char *value, size_t size) {
    Settings *s = settings();
    char tmp[48];
    int w = *cur_w(), h = *cur_h();
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
    case ITEM_SIZE:
        *label = game_mode ? "Kich thuoc man hinh" : "Kich thuoc man hinh mac dinh";
        *hint = game_mode ? "Tu dong: lay tu MANIFEST cua game, neu khong co thi dung kich thuoc mac dinh. "
                            "Chon 'Tuy chinh' de nhap kich thuoc bat ky."
                          : "Dung cho game khong khai bao kich thuoc. Pho bien nhat la 240x320. "
                            "Chon 'Tuy chinh' de nhap kich thuoc bat ky.";
        if (is_auto())
            snprintf(value, size, "Tu dong");
        else if (custom)
            snprintf(value, size, "Tuy chinh");
        else {
            int i = preset_index(w, h);
            snprintf(value, size, "%d x %d", SETTINGS_SCREEN_CHOICES[i].w, SETTINGS_SCREEN_CHOICES[i].h);
        }
        break;
    case ITEM_ORIENT:
        *label = "Huong man hinh";
        *hint = "Doc: cao hon rong (dien thoai thuong). Ngang: rong hon cao (vd 320x240, 640x360).";
        snprintf(value, size, "%s  (%d x %d)", w == h ? "Vuong" : w < h ? "Doc" : "Ngang", w, h);
        break;
    case ITEM_WIDTH:
        *label = "Chieu rong";
        *hint = "Trai/Phai: +-1, L/R: +-10, A: nhap so. Gioi han 64 - 1280.";
        snprintf(value, size, "%d px", w);
        break;
    case ITEM_HEIGHT:
        *label = "Chieu cao";
        *hint = "Trai/Phai: +-1, L/R: +-10, A: nhap so. Gioi han 64 - 1280.";
        snprintf(value, size, "%d px", h);
        break;
    }
}

// Khung xem trước tỉ lệ màn hình
static void draw_preview(int x, int y, int box_w, int box_h) {
    int w = is_auto() ? settings()->screen_w : *cur_w();
    int h = is_auto() ? settings()->screen_h : *cur_h();
    if (w <= 0 || h <= 0)
        return;
    float s = (float)box_w / w;
    if ((float)box_h / h < s)
        s = (float)box_h / h;
    int pw = (int)(w * s), ph = (int)(h * s);
    int px = x + (box_w - pw) / 2, py = y + (box_h - ph) / 2;
    gfx_fill_rect(px - 3, py - 3, pw + 6, ph + 6, COL_ACCENT);
    gfx_fill_rect(px, py, pw, ph, RGB(0x10, 0x11, 0x14));
    char label[32];
    snprintf(label, sizeof(label), "%dx%d", w, h);
    gfx_text(FONT_SMALL, px + pw / 2, py + (ph - gfx_font_height(FONT_SMALL)) / 2, pw, ALIGN_CENTER, COL_DIM, label);
}

void settings_screen_draw(void) {
    gfx_clear(COL_BG);

    gfx_fill_rect(0, 0, SCREEN_W, HEADER_H, COL_BAR);
    gfx_fill_rect(0, HEADER_H, SCREEN_W, 2, COL_ACCENT);
    int ty = (HEADER_H - gfx_font_height(FONT_LARGE)) / 2;
    int tw = gfx_text(FONT_LARGE, LIST_X, ty, 0, ALIGN_LEFT, COL_TEXT, game_mode ? "Tuy chon game" : "Cai dat");
    if (game_mode)
        gfx_text(FONT_NORMAL, LIST_X + tw + 24, ty + gfx_font_height(FONT_LARGE) - gfx_font_height(FONT_NORMAL) - 4,
                 SCREEN_W - LIST_X * 2 - tw - 24, ALIGN_LEFT, COL_DIM, game_title);

    // Cột trái: các mục; cột phải: xem trước tỉ lệ màn hình
    int list_w = SCREEN_W - 2 * LIST_X - 300;
    ItemId items[8];
    int n = visible_items(items);
    if (cursor >= n)
        cursor = n - 1;
    int font_y = (ROW_H - gfx_font_height(FONT_NORMAL)) / 2;
    const char *sel_hint = "";
    for (int i = 0; i < n; i++) {
        int y = LIST_TOP + i * ROW_H;
        bool sel = i == cursor;
        const char *label, *hint;
        char value[96];
        item_text(items[i], &label, &hint, value, sizeof(value));
        if (sel) {
            gfx_fill_rect(LIST_X, y, list_w, ROW_H, COL_ROW_SEL);
            gfx_fill_rect(LIST_X, y, 6, ROW_H, COL_ACCENT);
            sel_hint = hint;
        }
        gfx_text(FONT_NORMAL, LIST_X + 28, y + font_y, 0, ALIGN_LEFT, COL_TEXT, label);
        char shown[128];
        snprintf(shown, sizeof(shown), sel ? "<  %s  >" : "%s", value);
        gfx_text(FONT_NORMAL, LIST_X + list_w - 24, y + font_y, 0, ALIGN_RIGHT, sel ? COL_ACCENT : COL_DIM, shown);
    }
    gfx_text_wrapped(FONT_SMALL, LIST_X + 28, LIST_TOP + n * ROW_H + 20, list_w - 56, COL_DIM, sel_hint);

    draw_preview(SCREEN_W - LIST_X - 260, LIST_TOP, 260, 400);

    int y0 = SCREEN_H - FOOTER_H;
    gfx_fill_rect(0, y0, SCREEN_W, FOOTER_H, COL_BAR);
    gfx_text(FONT_NORMAL, SCREEN_W - LIST_X, y0 + (FOOTER_H - gfx_font_height(FONT_NORMAL)) / 2, 0, ALIGN_RIGHT,
             COL_TEXT, "(<>) Doi gia tri   (A) Chon / nhap so   (B) Luu va quay lai");
}
