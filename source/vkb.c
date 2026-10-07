#include "vkb.h"

#include <stdio.h>
#include <string.h>

#include "gfx.h"
#include "midp/midp.h"

#define REPEAT_DELAY_MS 400
#define REPEAT_RATE_MS  80
#define CAPS_TAP_MS     400     // bấm Shift 2 lần trong khoảng này: khoá chữ hoa
#define DRAG_SLOP       12      // di quá bấy nhiêu px thì là kéo, không phải chạm

#define BUBBLE_R    32
#define HALF        42          // 1/2 phím chuẩn (mỗi hàng 20 nửa phím)
#define KEY_H       54
#define GAP         6
#define PAD         8
#define HEADER_H    40
#define ROWS        5
#define PANEL_W     (20 * HALF + 2 * PAD)
#define PANEL_H     (HEADER_H + ROWS * KEY_H + PAD)
#define CLOSE_W     56

#define COL_PANEL   ((SDL_Color){ 0x18, 0x19, 0x1d, 235 })
#define COL_HEADER  ((SDL_Color){ 0x24, 0x26, 0x2b, 245 })
#define COL_KEY     RGB(0x3a, 0x3e, 0x47)
#define COL_SPECIAL RGB(0x2a, 0x2d, 0x33)
#define COL_DOWN    RGB(0x00, 0xb4, 0xe6)
#define COL_ACCENT  RGB(0x00, 0xb4, 0xe6)
#define COL_TEXT    RGB(0xee, 0xee, 0xee)
#define COL_DIM     RGB(0x9a, 0x9f, 0xa8)

enum {
    K_CHAR,
    K_SHIFT,
    K_DEL,
    K_ENTER,
};

typedef struct {
    const char *label, *shift_label;    // shift_label NULL: chữ cái (hoa / thường)
    int code, shift_code;
    int halves;                         // độ rộng theo nửa phím
    int kind;
} Key;

#define CH(c) { #c, NULL, (#c)[0], (#c)[0] - 32, 2, K_CHAR }
#define SYM(c, s) { c, s, (c)[0], (s)[0], 2, K_CHAR }

static const Key row0[] = { SYM("1", "!"), SYM("2", "@"), SYM("3", "#"), SYM("4", "$"), SYM("5", "%"),
                            SYM("6", "^"), SYM("7", "&"), SYM("8", "*"), SYM("9", "("), SYM("0", ")") };
static const Key row1[] = { CH(q), CH(w), CH(e), CH(r), CH(t), CH(y), CH(u), CH(i), CH(o), CH(p) };
static const Key row2[] = { CH(a), CH(s), CH(d), CH(f), CH(g), CH(h), CH(j), CH(k), CH(l) };
static const Key row3[] = {
    { "Shift", "Shift", 0, 0, 3, K_SHIFT },
    CH(z), CH(x), CH(c), CH(v), CH(b), CH(n), CH(m),
    { "Del", "Del", MIDP_KEY_CLEAR, MIDP_KEY_CLEAR, 3, K_DEL },
};
static const Key row4[] = {
    SYM("*", "+"), SYM("#", "="), SYM(",", "?"),
    { "", "", ' ', ' ', 8, K_CHAR },
    SYM(".", "-"),
    { "Enter", "Enter", MIDP_KEY_FIRE, MIDP_KEY_FIRE, 4, K_ENTER },
};

static const struct {
    const Key *keys;
    int count;
    int indent;     // lùi vào (nửa phím)
} rows[ROWS] = {
    { row0, 10, 0 },
    { row1, 10, 0 },
    { row2, 9, 1 },
    { row3, 9, 0 },
    { row4, 6, 0 },
};

typedef enum {
    T_NONE,
    T_BUBBLE,       // đang chạm bong bóng (chạm = mở, kéo = di chuyển)
    T_HEADER,       // kéo bàn phím bằng thanh tiêu đề
    T_CLOSE,
    T_KEY,
    T_PANEL,        // chạm vào khoảng trống trong bàn phím
} TouchKind;

#define MAX_TOUCH 5

typedef struct {
    bool active;
    SDL_FingerID id;
    TouchKind kind;
    int row, col;               // phím đang giữ
    int code;                   // mã đã gửi khi bấm (để nhả đúng mã dù Shift đổi)
    int sx, sy, ox, oy;         // điểm chạm đầu, vị trí ban đầu của bong bóng / bàn phím
    bool moved;
    Uint32 repeat_at;
} Touch;

static bool enabled, open;
static VkbSend send_fn;
static int bubble_x = SCREEN_W - 60, bubble_y = SCREEN_H - 140;     // tâm bong bóng (giữ qua các game)
static int panel_x = (SCREEN_W - PANEL_W) / 2, panel_y = SCREEN_H - PANEL_H - 8;
static bool shift, caps;
static Uint32 shift_tap_at;
static Touch touches[MAX_TOUCH];

static int clampi(int v, int lo, int hi) {
    return v < lo ? lo : v > hi ? hi : v;
}

static void key_rect(int r, int c, SDL_Rect *out) {
    int x = panel_x + PAD + rows[r].indent * HALF;
    for (int i = 0; i < c; i++)
        x += rows[r].keys[i].halves * HALF;
    out->x = x + GAP / 2;
    out->y = panel_y + HEADER_H + r * KEY_H + GAP / 2;
    out->w = rows[r].keys[c].halves * HALF - GAP;
    out->h = KEY_H - GAP;
}

static bool in_rect(const SDL_Rect *r, int x, int y) {
    return x >= r->x && y >= r->y && x < r->x + r->w && y < r->y + r->h;
}

static bool key_at(int x, int y, int *row, int *col) {
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < rows[r].count; c++) {
            SDL_Rect k;
            key_rect(r, c, &k);
            // Tính cả khe giữa các phím để chạm không bị lọt
            k.x -= GAP / 2;
            k.y -= GAP / 2;
            k.w += GAP;
            k.h += GAP;
            if (in_rect(&k, x, y)) {
                *row = r;
                *col = c;
                return true;
            }
        }
    }
    return false;
}

static void release_touch(Touch *t) {
    if (t->kind == T_KEY && t->code && send_fn)
        send_fn(MIDP_EV_KEY_RELEASED, t->code);
    t->active = false;
}

void vkb_release_all(void) {
    for (int i = 0; i < MAX_TOUCH; i++) {
        if (touches[i].active)
            release_touch(&touches[i]);
    }
}

void vkb_start(bool on, VkbSend send) {
    vkb_release_all();
    memset(touches, 0, sizeof(touches));
    enabled = on;
    send_fn = send;
    open = false;
    shift = caps = false;
}

static void press_key(Touch *t) {
    const Key *k = &rows[t->row].keys[t->col];
    t->code = 0;
    if (k->kind == K_SHIFT) {
        Uint32 now = SDL_GetTicks();
        if (caps) {
            caps = shift = false;
        } else if (shift && now - shift_tap_at < CAPS_TAP_MS) {
            caps = true;
        } else {
            shift = !shift;
        }
        shift_tap_at = now;
        return;
    }
    t->code = (shift || caps) ? k->shift_code : k->code;
    t->repeat_at = SDL_GetTicks() + REPEAT_DELAY_MS;
    if (send_fn)
        send_fn(MIDP_EV_KEY_PRESSED, t->code);
    if (k->kind == K_CHAR && shift && !caps)
        shift = false;      // Shift 1 lần chỉ cho 1 ký tự
}

static Touch *find_touch(SDL_FingerID id) {
    for (int i = 0; i < MAX_TOUCH; i++) {
        if (touches[i].active && touches[i].id == id)
            return &touches[i];
    }
    return NULL;
}

static bool in_bubble(int x, int y) {
    int dx = x - bubble_x, dy = y - bubble_y;
    int r = BUBBLE_R + 8;   // vùng chạm rộng hơn hình một chút
    return dx * dx + dy * dy <= r * r;
}

static bool in_panel(int x, int y) {
    return x >= panel_x && y >= panel_y && x < panel_x + PANEL_W && y < panel_y + PANEL_H;
}

bool vkb_pointer(SDL_FingerID id, VkbPointer type, int x, int y) {
    if (!enabled)
        return false;
    Touch *t = find_touch(id);
    if (type == VKB_DOWN) {
        if (t)
            release_touch(t);   // mất sự kiện nhả: coi như chạm mới
        TouchKind kind = T_NONE;
        int row = 0, col = 0;
        SDL_Rect close = { panel_x + PANEL_W - CLOSE_W, panel_y, CLOSE_W, HEADER_H };
        if (open && in_panel(x, y)) {
            if (in_rect(&close, x, y))
                kind = T_CLOSE;
            else if (y < panel_y + HEADER_H)
                kind = T_HEADER;
            else if (key_at(x, y, &row, &col))
                kind = T_KEY;
            else
                kind = T_PANEL;
        } else if (!open && in_bubble(x, y)) {
            kind = T_BUBBLE;
        } else {
            return false;
        }
        t = NULL;
        for (int i = 0; i < MAX_TOUCH && !t; i++) {
            if (!touches[i].active)
                t = &touches[i];
        }
        if (!t)
            return true;
        memset(t, 0, sizeof(*t));
        t->active = true;
        t->id = id;
        t->kind = kind;
        t->row = row;
        t->col = col;
        t->sx = x;
        t->sy = y;
        t->ox = kind == T_BUBBLE ? bubble_x : panel_x;
        t->oy = kind == T_BUBBLE ? bubble_y : panel_y;
        if (kind == T_KEY)
            press_key(t);
        return true;
    }
    if (!t)
        return false;

    int dx = x - t->sx, dy = y - t->sy;
    if (dx * dx + dy * dy > DRAG_SLOP * DRAG_SLOP)
        t->moved = true;
    if (type == VKB_MOVE) {
        if (t->kind == T_BUBBLE && t->moved) {
            bubble_x = clampi(t->ox + dx, BUBBLE_R, SCREEN_W - BUBBLE_R);
            bubble_y = clampi(t->oy + dy, BUBBLE_R, SCREEN_H - BUBBLE_R);
        } else if (t->kind == T_HEADER) {
            panel_x = clampi(t->ox + dx, 0, SCREEN_W - PANEL_W);
            panel_y = clampi(t->oy + dy, 0, SCREEN_H - PANEL_H);
        }
        return true;
    }

    // VKB_UP
    if (t->kind == T_BUBBLE && !t->moved) {
        open = true;
    } else if (t->kind == T_CLOSE) {
        SDL_Rect close = { panel_x + PANEL_W - CLOSE_W, panel_y, CLOSE_W, HEADER_H };
        if (in_rect(&close, x, y)) {
            release_touch(t);
            vkb_release_all();
            open = false;
            return true;
        }
    }
    release_touch(t);
    return true;
}

void vkb_update(void) {
    if (!enabled || !open)
        return;
    Uint32 now = SDL_GetTicks();
    for (int i = 0; i < MAX_TOUCH; i++) {
        Touch *t = &touches[i];
        if (t->active && t->kind == T_KEY && t->code && SDL_TICKS_PASSED(now, t->repeat_at)) {
            if (send_fn)
                send_fn(MIDP_EV_KEY_REPEATED, t->code);
            t->repeat_at = now + REPEAT_RATE_MS;
        }
    }
}

static bool key_held(int r, int c) {
    for (int i = 0; i < MAX_TOUCH; i++) {
        if (touches[i].active && touches[i].kind == T_KEY && touches[i].row == r && touches[i].col == c)
            return true;
    }
    return false;
}

// Hình tròn đặc vẽ bằng các dòng ngang
static void fill_circle(int cx, int cy, int r, SDL_Color c) {
    for (int dy = -r; dy <= r; dy++) {
        int dx = 0;
        while ((dx + 1) * (dx + 1) + dy * dy <= r * r)
            dx++;
        gfx_fill_rect(cx - dx, cy + dy, dx * 2 + 1, 1, c);
    }
}

static void draw_bubble(void) {
    fill_circle(bubble_x, bubble_y, BUBBLE_R, (SDL_Color){ 0x00, 0xb4, 0xe6, 200 });
    fill_circle(bubble_x, bubble_y, BUBBLE_R - 3, (SDL_Color){ 0x18, 0x19, 0x1d, 220 });
    // Biểu tượng bàn phím: 2 hàng phím nhỏ + phím cách
    int w = 7, h = 6, gap = 3;
    int x0 = bubble_x - (4 * w + 3 * gap) / 2, y0 = bubble_y - 10;
    for (int r = 0; r < 2; r++) {
        for (int c = 0; c < 4; c++)
            gfx_fill_rect(x0 + c * (w + gap), y0 + r * (h + gap), w, h, COL_TEXT);
    }
    gfx_fill_rect(x0 + w + gap, y0 + 2 * (h + gap), 2 * w + gap, h, COL_TEXT);
}

void vkb_draw(void) {
    if (!enabled)
        return;
    if (!open) {
        draw_bubble();
        return;
    }
    gfx_fill_rect(panel_x, panel_y, PANEL_W, PANEL_H, COL_PANEL);
    gfx_fill_rect(panel_x, panel_y, PANEL_W, HEADER_H, COL_HEADER);
    // Tay cầm để kéo
    for (int i = 0; i < 3; i++)
        gfx_fill_rect(panel_x + PANEL_W / 2 - 24, panel_y + 12 + i * 7, 48, 3, COL_DIM);
    if (caps || shift)
        gfx_text(FONT_SMALL, panel_x + PAD + 6, panel_y + (HEADER_H - gfx_font_height(FONT_SMALL)) / 2, 0, ALIGN_LEFT,
                 COL_ACCENT, caps ? "CAPS" : "Shift");
    SDL_Rect close = { panel_x + PANEL_W - CLOSE_W, panel_y, CLOSE_W, HEADER_H };
    gfx_text(FONT_NORMAL, close.x + close.w / 2, close.y + (HEADER_H - gfx_font_height(FONT_NORMAL)) / 2, 0,
             ALIGN_CENTER, COL_TEXT, "×");

    bool upper = shift || caps;
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < rows[r].count; c++) {
            const Key *k = &rows[r].keys[c];
            SDL_Rect kr;
            key_rect(r, c, &kr);
            bool down = key_held(r, c) || (k->kind == K_SHIFT && upper);
            gfx_fill_rect(kr.x, kr.y, kr.w, kr.h, down ? COL_DOWN : k->kind == K_CHAR ? COL_KEY : COL_SPECIAL);
            char label[8];
            if (k->shift_label)
                snprintf(label, sizeof(label), "%s", upper ? k->shift_label : k->label);
            else
                snprintf(label, sizeof(label), "%c", upper ? k->label[0] - 32 : k->label[0]);
            FontId f = k->kind == K_CHAR ? FONT_NORMAL : FONT_SMALL;
            gfx_text(f, kr.x + kr.w / 2, kr.y + (kr.h - gfx_font_height(f)) / 2, kr.w - 4, ALIGN_CENTER, COL_TEXT,
                     label);
        }
    }
}
