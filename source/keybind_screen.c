#include "keybind_screen.h"

#include <stdio.h>
#include <string.h>

#include "gfx.h"
#include "input.h"
#include "keybind.h"
#include "lang.h"
#include "midp/midp.h"

#define HEADER_H    80
#define FOOTER_H    72
#define LIST_X      40
#define LIST_TOP    110
#define LIST_W      620
#define ROW_H       52
#define LIST_ROWS   ((SCREEN_H - FOOTER_H - LIST_TOP - 16) / ROW_H)
#define SCROLLBAR_W 6
#define DRAG_SLOP   12      // kéo quá chừng này px thì là cuộn, không phải chạm chọn

// Bàn phím điện thoại bên phải
#define PAD_X       (LIST_X + LIST_W + 60)
#define PAD_W       (SCREEN_W - LIST_X - PAD_X)
#define PAD_TOP     (LIST_TOP + 40)
#define PAD_COLS    3
#define PAD_GAP     8
#define PAD_KEY_W   ((PAD_W - (PAD_COLS - 1) * PAD_GAP) / PAD_COLS)
#define PAD_KEY_H   48

#define COL_BG       RGB(0x24, 0x26, 0x2b)
#define COL_BAR      RGB(0x18, 0x19, 0x1d)
#define COL_ACCENT   RGB(0x00, 0xb4, 0xe6)
#define COL_TEXT     RGB(0xee, 0xee, 0xee)
#define COL_DIM      RGB(0x9a, 0x9f, 0xa8)
#define COL_ROW_SEL  RGB(0x33, 0x3a, 0x48)
#define COL_TRACK    RGB(0x34, 0x37, 0x3e)
#define COL_KEY      RGB(0x30, 0x34, 0x3c)

// Ô "Mặc định" trên bàn phím (không phải mã phím)
#define PAD_DEFAULT  BIND_INHERIT

// Bố cục bàn phím điện thoại: 3 cột, hàng cuối là ô Mặc định rộng hết hàng
static const int pad[][PAD_COLS] = {
    { MIDP_KEY_SOFT_LEFT, MIDP_KEY_UP, MIDP_KEY_SOFT_RIGHT },
    { MIDP_KEY_LEFT, MIDP_KEY_FIRE, MIDP_KEY_RIGHT },
    { MIDP_KEY_CLEAR, MIDP_KEY_DOWN, BIND_NONE },
    { '1', '2', '3' },
    { '4', '5', '6' },
    { '7', '8', '9' },
    { MIDP_KEY_STAR, '0', MIDP_KEY_POUND },
};
#define PAD_ROWS ((int)(sizeof(pad) / sizeof(pad[0])))

static int *binds;
static const int *global;       // NULL: đang chỉnh cài đặt chung
static char subtitle[128];
static int cursor;
static int scroll;
static int followed = -1;       // vị trí con trỏ lần cuối đã cuộn theo

// Cảm ứng / chuột
static bool touching;
static bool dragging;
static int touch_y0;
static int scroll0;
static int tap_x = -1, tap_y = -1;     // chạm xong (chưa xử lý)

void keybind_screen_open(int *b, const int *g, const char *sub) {
    binds = b;
    global = g;
    snprintf(subtitle, sizeof(subtitle), "%s", sub);
    cursor = scroll = 0;
    followed = -1;
    touching = dragging = false;
    tap_x = tap_y = -1;
}

// Phím thật sự được gửi khi bấm nút b
static int effective(int b) {
    return binds[b] == BIND_INHERIT && global ? global[b] : binds[b];
}

static int max_scroll(void) {
    return BIND_COUNT > LIST_ROWS ? BIND_COUNT - LIST_ROWS : 0;
}

static void clamp_scroll(void) {
    if (scroll > max_scroll())
        scroll = max_scroll();
    if (scroll < 0)
        scroll = 0;
}

static void touch(int type, int x, int y) {
    if (type == SDL_FINGERDOWN) {
        touching = true;
        dragging = false;
        touch_y0 = y;
        scroll0 = scroll;
    } else if (type == SDL_FINGERMOTION) {
        if (!touching)
            return;
        // Kéo trong danh sách thì cuộn
        if (!dragging && x < LIST_X + LIST_W && (y - touch_y0 > DRAG_SLOP || touch_y0 - y > DRAG_SLOP))
            dragging = true;
        if (dragging) {
            scroll = scroll0 - (y - touch_y0) / ROW_H;
            clamp_scroll();
        }
    } else if (type == SDL_FINGERUP) {
        if (touching && !dragging) {
            tap_x = x;
            tap_y = y;
        }
        touching = dragging = false;
    }
}

void keybind_screen_handle_event(const SDL_Event *e) {
    switch (e->type) {
    case SDL_FINGERDOWN:
    case SDL_FINGERUP:
    case SDL_FINGERMOTION:
        touch(e->type, (int)(e->tfinger.x * SCREEN_W), (int)(e->tfinger.y * SCREEN_H));
        break;
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
        if (e->button.which != SDL_TOUCH_MOUSEID && e->button.button == SDL_BUTTON_LEFT)
            touch(e->type == SDL_MOUSEBUTTONDOWN ? SDL_FINGERDOWN : SDL_FINGERUP, e->button.x, e->button.y);
        break;
    case SDL_MOUSEMOTION:
        if (e->motion.which != SDL_TOUCH_MOUSEID && (e->motion.state & SDL_BUTTON_LMASK))
            touch(SDL_FINGERMOTION, e->motion.x, e->motion.y);
        break;
    default:
        break;
    }
}

static void pad_rect(int row, int col, SDL_Rect *r) {
    r->x = PAD_X + col * (PAD_KEY_W + PAD_GAP);
    r->y = PAD_TOP + row * (PAD_KEY_H + PAD_GAP);
    r->w = PAD_KEY_W;
    r->h = PAD_KEY_H;
}

static void default_rect(SDL_Rect *r) {
    r->x = PAD_X;
    r->y = PAD_TOP + PAD_ROWS * (PAD_KEY_H + PAD_GAP) + 8;
    r->w = PAD_W;
    r->h = PAD_KEY_H;
}

static bool in_rect(const SDL_Rect *r, int x, int y) {
    return x >= r->x && y >= r->y && x < r->x + r->w && y < r->y + r->h;
}

// Ô bàn phím ở (x, y): mã phím, PAD_DEFAULT, hoặc -1000 nếu không trúng ô nào
static int pad_at(int x, int y) {
    SDL_Rect r;
    for (int row = 0; row < PAD_ROWS; row++) {
        for (int col = 0; col < PAD_COLS; col++) {
            pad_rect(row, col, &r);
            if (in_rect(&r, x, y))
                return pad[row][col];
        }
    }
    default_rect(&r);
    return in_rect(&r, x, y) ? PAD_DEFAULT : -1000;
}

static void set_default(int b) {
    binds[b] = global ? BIND_INHERIT : keybind_default(b);
}

// Trái / phải: đổi qua các phím; tuỳ chọn riêng có thêm "Mặc định" ở đầu
static void cycle(int b, int dir) {
    int base = global ? 1 : 0;
    int n = keybind_key_count() + base;
    int cur = binds[b] == BIND_INHERIT ? 0 : keybind_key_index(binds[b]) + base;
    if (cur < base)
        cur = base;
    int i = ((cur + dir) % n + n) % n;
    binds[b] = global && i == 0 ? BIND_INHERIT : keybind_key_at(i - base);
}

static void handle_tap(int x, int y) {
    if (x >= LIST_X && x < LIST_X + LIST_W && y >= LIST_TOP && y < LIST_TOP + LIST_ROWS * ROW_H) {
        int i = scroll + (y - LIST_TOP) / ROW_H;
        if (i < BIND_COUNT)
            cursor = i;
        return;
    }
    int key = pad_at(x, y);
    if (key == PAD_DEFAULT)
        set_default(cursor);
    else if (key != -1000)
        binds[cursor] = key;
}

bool keybind_screen_update(void) {
    if (input_pressed(BTN_B) || input_pressed(BTN_PLUS) || input_pressed(BTN_MINUS))
        return false;

    if (tap_x >= 0) {
        handle_tap(tap_x, tap_y);
        tap_x = tap_y = -1;
    }

    if (input_pressed(BTN_DOWN))
        cursor = (cursor + 1) % BIND_COUNT;
    if (input_pressed(BTN_UP))
        cursor = (cursor + BIND_COUNT - 1) % BIND_COUNT;
    if (input_pressed(BTN_RIGHT) || input_pressed(BTN_A))
        cycle(cursor, 1);
    if (input_pressed(BTN_LEFT))
        cycle(cursor, -1);
    if (input_pressed(BTN_Y))
        set_default(cursor);
    if (input_pressed(BTN_X)) {
        for (int b = 0; b < BIND_COUNT; b++)
            set_default(b);
    }

    // Con trỏ đổi thì cuộn theo; kéo bằng tay thì để yên dù con trỏ ra ngoài màn hình
    if (!dragging && cursor != followed) {
        if (cursor < scroll)
            scroll = cursor;
        else if (cursor >= scroll + LIST_ROWS)
            scroll = cursor - LIST_ROWS + 1;
        followed = cursor;
    }
    clamp_scroll();
    return true;
}

static void value_text(int b, char *out, size_t size) {
    if (binds[b] == BIND_INHERIT && global)
        snprintf(out, size, tr(S_DEFAULT_FMT), keybind_key_name(global[b]));
    else
        snprintf(out, size, "%s", keybind_key_name(binds[b]));
}

static void draw_list(void) {
    int row_w = BIND_COUNT > LIST_ROWS ? LIST_W - SCROLLBAR_W - 12 : LIST_W;
    int font_y = (ROW_H - gfx_font_height(FONT_NORMAL)) / 2;
    for (int i = scroll; i < scroll + LIST_ROWS && i < BIND_COUNT; i++) {
        int y = LIST_TOP + (i - scroll) * ROW_H;
        bool sel = i == cursor;
        if (sel) {
            gfx_fill_rect(LIST_X, y, row_w, ROW_H, COL_ROW_SEL);
            gfx_fill_rect(LIST_X, y, 6, ROW_H, COL_ACCENT);
        }
        gfx_text(FONT_NORMAL, LIST_X + 28, y + font_y, row_w / 2, ALIGN_LEFT, COL_TEXT, keybind_button_name(i));
        char value[96], shown[128];
        value_text(i, value, sizeof(value));
        snprintf(shown, sizeof(shown), sel ? "<  %s  >" : "%s", value);
        bool inherit = global && binds[i] == BIND_INHERIT;
        gfx_text(FONT_NORMAL, LIST_X + row_w - 24, y + font_y, 0, ALIGN_RIGHT,
                 sel ? COL_ACCENT : inherit ? COL_DIM : COL_TEXT, shown);
    }
    if (BIND_COUNT > LIST_ROWS) {
        int track_x = LIST_X + LIST_W - SCROLLBAR_W, track_h = LIST_ROWS * ROW_H;
        int thumb_h = track_h * LIST_ROWS / BIND_COUNT;
        int thumb_y = LIST_TOP + (track_h - thumb_h) * scroll / max_scroll();
        gfx_fill_rect(track_x, LIST_TOP, SCROLLBAR_W, track_h, COL_TRACK);
        gfx_fill_rect(track_x, thumb_y, SCROLLBAR_W, thumb_h, COL_ACCENT);
    }
}

static void draw_key(const SDL_Rect *r, const char *label, bool on, bool dim_on) {
    if (on)
        gfx_fill_rect(r->x - 3, r->y - 3, r->w + 6, r->h + 6, COL_ACCENT);
    else if (dim_on)
        gfx_fill_rect(r->x - 2, r->y - 2, r->w + 4, r->h + 4, COL_DIM);
    gfx_fill_rect(r->x, r->y, r->w, r->h, on ? COL_ROW_SEL : COL_KEY);
    FontId font = gfx_text_width(FONT_NORMAL, label) <= r->w - 16 ? FONT_NORMAL : FONT_SMALL;
    gfx_text(font, r->x + r->w / 2, r->y + (r->h - gfx_font_height(font)) / 2, r->w - 8, ALIGN_CENTER,
             on ? COL_TEXT : COL_DIM, label);
}

static void draw_pad(void) {
    gfx_text(FONT_NORMAL, PAD_X, LIST_TOP, PAD_W, ALIGN_LEFT, COL_DIM, tr(S_KEYBIND_PHONE));
    bool inherit = global && binds[cursor] == BIND_INHERIT;
    int key = effective(cursor);
    SDL_Rect r;
    for (int row = 0; row < PAD_ROWS; row++) {
        for (int col = 0; col < PAD_COLS; col++) {
            pad_rect(row, col, &r);
            int k = pad[row][col];
            // Đang theo cài đặt chung: viền xám ở phím của cài đặt chung
            draw_key(&r, keybind_key_name(k), !inherit && k == key, inherit && k == key);
        }
    }
    default_rect(&r);
    bool is_default = inherit || (!global && binds[cursor] == keybind_default(cursor));
    draw_key(&r, tr(S_KEYBIND_DEFAULT), is_default, false);
}

void keybind_screen_draw(void) {
    gfx_clear(COL_BG);

    gfx_fill_rect(0, 0, SCREEN_W, HEADER_H, COL_BAR);
    gfx_fill_rect(0, HEADER_H, SCREEN_W, 2, COL_ACCENT);
    int ty = (HEADER_H - gfx_font_height(FONT_LARGE)) / 2;
    int tw = gfx_text(FONT_LARGE, LIST_X, ty, 0, ALIGN_LEFT, COL_TEXT, tr(S_KEYBIND));
    gfx_text(FONT_NORMAL, LIST_X + tw + 24, ty + gfx_font_height(FONT_LARGE) - gfx_font_height(FONT_NORMAL) - 4,
             SCREEN_W - LIST_X * 2 - tw - 24, ALIGN_LEFT, COL_DIM, subtitle);

    draw_list();
    draw_pad();

    int y0 = SCREEN_H - FOOTER_H;
    gfx_fill_rect(0, y0, SCREEN_W, FOOTER_H, COL_BAR);
    gfx_text(FONT_NORMAL, SCREEN_W - LIST_X, y0 + (FOOTER_H - gfx_font_height(FONT_NORMAL)) / 2, 0, ALIGN_RIGHT,
             COL_TEXT, tr(S_KEYBIND_HINTS));
}
