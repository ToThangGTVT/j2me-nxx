#include "vpad_screen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gfx.h"
#include "input.h"
#include "lang.h"

#define BAR_H           76
#define BTN_H           56
#define SNAP_DIST       14      // cách cạnh phím khác / mép màn hình bấy nhiêu px thì hít vào
#define HANDLE_R        36      // vùng chạm của nút đổi kích thước ở góc dưới phải
#define DRAG_SLOP       8
#define MOVE_STEP       4
#define SIZE_STEP       8
#define RADIUS_STEP     4
#define OPACITY_STEP    10
#define RESET_CONFIRM_MS 2500

#define COL_BG       RGB(0x10, 0x11, 0x14)
#define COL_BAR      ((SDL_Color){ 0x18, 0x19, 0x1d, 240 })
#define COL_BTN      RGB(0x33, 0x3a, 0x48)
#define COL_ACCENT   RGB(0x00, 0xb4, 0xe6)
#define COL_TEXT     RGB(0xee, 0xee, 0xee)
#define COL_DIM      RGB(0x9a, 0x9f, 0xa8)
#define COL_OFF      RGB(0x5a, 0x5e, 0x66)

typedef enum {
    TB_HIDE,
    TB_RADIUS_DOWN,
    TB_RADIUS,
    TB_RADIUS_UP,
    TB_OPACITY_DOWN,
    TB_OPACITY,
    TB_OPACITY_UP,
    TB_SNAP,
    TB_RESET,
    TB_DONE,
    TB_COUNT,
} Tool;

// Vị trí ngang, độ rộng các nút trên thanh công cụ (bên trái là tên phím đang chọn)
#define NAME_X 20
#define NAME_W 200
static const struct {
    int x, w;
} tools[TB_COUNT] = {
    [TB_HIDE]         = { 230, 110 },
    [TB_RADIUS_DOWN]  = { 352, 52 },
    [TB_RADIUS]       = { 404, 140 },
    [TB_RADIUS_UP]    = { 544, 52 },
    [TB_OPACITY_DOWN] = { 612, 52 },
    [TB_OPACITY]      = { 664, 140 },
    [TB_OPACITY_UP]   = { 804, 52 },
    [TB_SNAP]         = { 872, 150 },
    [TB_RESET]        = { 1034, 120 },
    [TB_DONE]         = { 1166, 100 },
};

typedef enum {
    DRAG_NONE,
    DRAG_PENDING,       // đã chạm phím, chưa kéo quá DRAG_SLOP
    DRAG_MOVE,
    DRAG_RESIZE,
} Drag;

static VpadLayout *lay;
static int sel;                 // phím đang chọn, -1 = chưa chọn
static bool done;
static Uint32 reset_until;      // đã chạm "Mặc định" 1 lần: chạm lần nữa trước mốc này thì đặt lại

static Drag drag;
static SDL_FingerID drag_id;
static int drag_x0, drag_y0;
static VpadKey drag_from;

static int clampi(int v, int lo, int hi) {
    return v < lo ? lo : v > hi ? hi : v;
}

static bool in_rect(const SDL_Rect *r, int x, int y) {
    return x >= r->x && y >= r->y && x < r->x + r->w && y < r->y + r->h;
}

void vpad_screen_open(VpadLayout *l) {
    lay = l;
    sel = -1;
    done = false;
    reset_until = 0;
    drag = DRAG_NONE;
}

// ---------------------------------------------------------------------------
// Thanh công cụ ở mép trên hoặc dưới: không che phím đang chọn, còn lại chọn mép che ít phím hơn

// Diện tích phím (hoặc chỉ phím only nếu only >= 0) bị dải [y0, y1) che
static int overlap(int y0, int y1, int only) {
    int sum = 0;
    for (int i = 0; i < VP_COUNT; i++) {
        const VpadKey *k = &lay->keys[i];
        int top = k->y > y0 ? k->y : y0;
        int bottom = k->y + k->h < y1 ? k->y + k->h : y1;
        if ((only < 0 ? !k->hidden : i == only) && bottom > top)
            sum += (bottom - top) * k->w;
    }
    return sum;
}

static void bar_rect(SDL_Rect *r) {
    int top = overlap(0, BAR_H, -1), bottom = overlap(SCREEN_H - BAR_H, SCREEN_H, -1);
    bool at_bottom = bottom < top;
    if (sel >= 0) {
        int sel_top = overlap(0, BAR_H, sel), sel_bottom = overlap(SCREEN_H - BAR_H, SCREEN_H, sel);
        if (sel_top != sel_bottom)
            at_bottom = sel_bottom < sel_top;
    }
    r->x = 0;
    r->y = at_bottom ? SCREEN_H - BAR_H : 0;
    r->w = SCREEN_W;
    r->h = BAR_H;
}

static void tool_rect(Tool t, SDL_Rect *r) {
    SDL_Rect bar;
    bar_rect(&bar);
    r->x = tools[t].x;
    r->y = bar.y + (BAR_H - BTN_H) / 2;
    r->w = tools[t].w;
    r->h = BTN_H;
}

static bool can_round(void) {
    return sel > VP_STICK;
}

static void change_radius(int dir) {
    if (!can_round())
        return;
    VpadKey *k = &lay->keys[sel];
    int max = (k->w < k->h ? k->w : k->h) / 2;
    k->r = clampi((k->r > max ? max : k->r) + dir * RADIUS_STEP, 0, max);
}

static void reset_all(void) {
    Uint32 now = SDL_GetTicks();
    if (reset_until && !SDL_TICKS_PASSED(now, reset_until)) {
        vpad_layout_default(lay);
        sel = -1;
        reset_until = 0;
    } else {
        reset_until = now + RESET_CONFIRM_MS;
    }
}

static void tool_tap(int x, int y) {
    for (int t = 0; t < TB_COUNT; t++) {
        SDL_Rect r;
        tool_rect(t, &r);
        if (!in_rect(&r, x, y))
            continue;
        switch (t) {
        case TB_HIDE:
            if (sel >= 0)
                lay->keys[sel].hidden = !lay->keys[sel].hidden;
            break;
        case TB_RADIUS_DOWN:
            change_radius(-1);
            break;
        case TB_RADIUS_UP:
            change_radius(1);
            break;
        case TB_OPACITY_DOWN:
        case TB_OPACITY_UP:
            lay->opacity = clampi(lay->opacity + (t == TB_OPACITY_UP ? OPACITY_STEP : -OPACITY_STEP),
                                  VPAD_OPACITY_MIN, VPAD_OPACITY_MAX);
            break;
        case TB_SNAP:
            lay->snap = !lay->snap;
            break;
        case TB_RESET:
            reset_all();
            break;
        case TB_DONE:
            done = true;
            break;
        default:
            break;
        }
        return;
    }
}

// ---------------------------------------------------------------------------
// Hít vào cạnh phím khác / mép màn hình

static void consider(int d, int *best, int *adj) {
    if (abs(d) < *best) {
        *best = abs(d);
        *adj = d;
    }
}

// Phím khác o nằm sát phím [a0, a1) theo trục kia (chồng lên hoặc cách dưới SNAP_DIST)
static bool near_span(int a0, int a1, int b0, int b1) {
    return a0 < b1 + SNAP_DIST && a1 > b0 - SNAP_DIST;
}

// Kéo phím: cạnh trái / phải hít vào cạnh phím bên cạnh hoặc thẳng hàng với phím trên / dưới (tương tự trục dọc)
static void snap_move(int self, int *x, int *y) {
    const VpadKey *k = &lay->keys[self];
    int bx = SNAP_DIST + 1, ax = 0, by = SNAP_DIST + 1, ay = 0;
    consider(-*x, &bx, &ax);
    consider(SCREEN_W - (*x + k->w), &bx, &ax);
    consider(-*y, &by, &ay);
    consider(SCREEN_H - (*y + k->h), &by, &ay);
    for (int j = 0; j < VP_COUNT; j++) {
        const VpadKey *o = &lay->keys[j];
        if (j == self || o->hidden)
            continue;
        if (near_span(*y, *y + k->h, o->y, o->y + o->h)) {
            consider(o->x + o->w - *x, &bx, &ax);
            consider(o->x - (*x + k->w), &bx, &ax);
            consider(o->x - *x, &bx, &ax);
            consider(o->x + o->w - (*x + k->w), &bx, &ax);
        }
        if (near_span(*x, *x + k->w, o->x, o->x + o->w)) {
            consider(o->y + o->h - *y, &by, &ay);
            consider(o->y - (*y + k->h), &by, &ay);
            consider(o->y - *y, &by, &ay);
            consider(o->y + o->h - (*y + k->h), &by, &ay);
        }
    }
    if (bx <= SNAP_DIST)
        *x += ax;
    if (by <= SNAP_DIST)
        *y += ay;
}

// Đổi kích thước: cạnh phải / dưới hít vào cạnh phím khác hoặc mép màn hình
static void snap_size(int self, int *w, int *h) {
    const VpadKey *k = &lay->keys[self];
    int right = k->x + *w, bottom = k->y + *h;
    int bx = SNAP_DIST + 1, ax = 0, by = SNAP_DIST + 1, ay = 0;
    consider(SCREEN_W - right, &bx, &ax);
    consider(SCREEN_H - bottom, &by, &ay);
    for (int j = 0; j < VP_COUNT; j++) {
        const VpadKey *o = &lay->keys[j];
        if (j == self || o->hidden)
            continue;
        if (near_span(k->y, bottom, o->y, o->y + o->h)) {
            consider(o->x - right, &bx, &ax);
            consider(o->x + o->w - right, &bx, &ax);
        }
        if (near_span(k->x, right, o->x, o->x + o->w)) {
            consider(o->y - bottom, &by, &ay);
            consider(o->y + o->h - bottom, &by, &ay);
        }
    }
    if (bx <= SNAP_DIST)
        *w += ax;
    if (by <= SNAP_DIST)
        *h += ay;
}

static void move_to(int x, int y, bool snap) {
    VpadKey *k = &lay->keys[sel];
    x = clampi(x, 0, SCREEN_W - k->w);
    y = clampi(y, 0, SCREEN_H - k->h);
    if (snap && lay->snap) {
        snap_move(sel, &x, &y);
        x = clampi(x, 0, SCREEN_W - k->w);
        y = clampi(y, 0, SCREEN_H - k->h);
    }
    k->x = x;
    k->y = y;
}

static void resize_to(int w, int h, bool snap) {
    VpadKey *k = &lay->keys[sel];
    if (sel == VP_STICK)
        w = h = (w + h) / 2;
    w = clampi(w, VPAD_SIZE_MIN, SCREEN_W - k->x);
    h = clampi(h, VPAD_SIZE_MIN, SCREEN_H - k->y);
    if (snap && lay->snap) {
        snap_size(sel, &w, &h);
        w = clampi(w, VPAD_SIZE_MIN, SCREEN_W - k->x);
        h = clampi(h, VPAD_SIZE_MIN, SCREEN_H - k->y);
    }
    // Cần điều khiển luôn tròn
    if (sel == VP_STICK) {
        int s = w < h ? w : h;
        w = h = s;
    }
    k->w = w;
    k->h = h;
    int max = (w < h ? w : h) / 2;
    if (k->r > max)
        k->r = max;
}

// ---------------------------------------------------------------------------
// Cảm ứng / chuột

static int item_at(int x, int y) {
    // Phím ẩn vẫn chọn được (để hiện lại); phím vẽ sau nằm trên
    for (int i = VP_COUNT - 1; i >= 0; i--) {
        const VpadKey *k = &lay->keys[i];
        if (i == VP_STICK) {
            int r = k->w / 2, dx = x - (k->x + r), dy = y - (k->y + r);
            if (dx * dx + dy * dy <= r * r)
                return i;
        } else if (x >= k->x && y >= k->y && x < k->x + k->w && y < k->y + k->h) {
            return i;
        }
    }
    return -1;
}

static bool on_handle(int x, int y) {
    if (sel < 0)
        return false;
    const VpadKey *k = &lay->keys[sel];
    int dx = x - (k->x + k->w), dy = y - (k->y + k->h);
    return dx * dx + dy * dy <= HANDLE_R * HANDLE_R;
}

static void pointer(SDL_FingerID id, int type, int x, int y) {
    if (type == SDL_FINGERDOWN) {
        if (drag != DRAG_NONE)
            return;     // chỉ 1 ngón kéo
        SDL_Rect bar;
        bar_rect(&bar);
        if (in_rect(&bar, x, y)) {
            tool_tap(x, y);
            return;
        }
        if (on_handle(x, y)) {
            drag = DRAG_RESIZE;
        } else {
            sel = item_at(x, y);
            if (sel < 0)
                return;
            drag = DRAG_PENDING;
        }
        drag_id = id;
        drag_x0 = x;
        drag_y0 = y;
        drag_from = lay->keys[sel];
        return;
    }
    if (drag == DRAG_NONE || id != drag_id)
        return;
    int dx = x - drag_x0, dy = y - drag_y0;
    if (type == SDL_FINGERMOTION) {
        if (drag == DRAG_PENDING && dx * dx + dy * dy > DRAG_SLOP * DRAG_SLOP)
            drag = DRAG_MOVE;
        if (drag == DRAG_MOVE)
            move_to(drag_from.x + dx, drag_from.y + dy, true);
        else if (drag == DRAG_RESIZE)
            resize_to(drag_from.w + dx, drag_from.h + dy, true);
        return;
    }
    drag = DRAG_NONE;
}

void vpad_screen_handle_event(const SDL_Event *e) {
    int x, y;
    switch (e->type) {
    case SDL_FINGERDOWN:
    case SDL_FINGERUP:
    case SDL_FINGERMOTION: {
        int ww, wh;
        gfx_window_size(&ww, &wh);
        gfx_window_to_screen(e->tfinger.x * ww, e->tfinger.y * wh, &x, &y);
        pointer(e->tfinger.fingerId, e->type, x, y);
        break;
    }
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
        if (e->button.which != SDL_TOUCH_MOUSEID && e->button.button == SDL_BUTTON_LEFT) {
            gfx_window_to_screen(e->button.x, e->button.y, &x, &y);
            pointer(-1, e->type == SDL_MOUSEBUTTONDOWN ? SDL_FINGERDOWN : SDL_FINGERUP, x, y);
        }
        break;
    case SDL_MOUSEMOTION:
        if (e->motion.which != SDL_TOUCH_MOUSEID && (e->motion.state & SDL_BUTTON_LMASK)) {
            gfx_window_to_screen(e->motion.x, e->motion.y, &x, &y);
            pointer(-1, SDL_FINGERMOTION, x, y);
        }
        break;
    default:
        break;
    }
}

// ---------------------------------------------------------------------------

bool vpad_screen_update(void) {
    if (done || input_pressed(BTN_B) || input_pressed(BTN_PLUS) || input_pressed(BTN_MINUS)) {
        drag = DRAG_NONE;
        return false;
    }
    if (drag != DRAG_NONE)
        return true;

    // Tay cầm: L / R chọn phím, D-pad di chuyển, X / Y to / nhỏ, A ẩn / hiện
    if (input_pressed(BTN_R))
        sel = (sel + 1) % VP_COUNT;
    if (input_pressed(BTN_L))
        sel = sel <= 0 ? VP_COUNT - 1 : sel - 1;
    if (sel < 0)
        return true;
    VpadKey *k = &lay->keys[sel];
    int dx = input_pressed(BTN_RIGHT) ? MOVE_STEP : input_pressed(BTN_LEFT) ? -MOVE_STEP : 0;
    int dy = input_pressed(BTN_DOWN) ? MOVE_STEP : input_pressed(BTN_UP) ? -MOVE_STEP : 0;
    if (dx || dy)
        move_to(k->x + dx, k->y + dy, false);
    int ds = input_pressed(BTN_X) ? SIZE_STEP : input_pressed(BTN_Y) ? -SIZE_STEP : 0;
    if (ds) {
        // Giữ tâm phím
        int cx = k->x + k->w / 2, cy = k->y + k->h / 2;
        int w = clampi(k->w + ds, VPAD_SIZE_MIN, SCREEN_W), h = clampi(k->h + ds, VPAD_SIZE_MIN, SCREEN_H);
        k->x = clampi(cx - w / 2, 0, SCREEN_W - w);
        k->y = clampi(cy - h / 2, 0, SCREEN_H - h);
        resize_to(w, h, false);
    }
    if (input_pressed(BTN_A))
        k->hidden = !k->hidden;
    return true;
}

static void outline(int x, int y, int w, int h, int t, SDL_Color c) {
    gfx_fill_rect(x - t, y - t, w + 2 * t, t, c);
    gfx_fill_rect(x - t, y + h, w + 2 * t, t, c);
    gfx_fill_rect(x - t, y, t, h, c);
    gfx_fill_rect(x + w, y, t, h, c);
}

static void draw_button(Tool t, const char *label, bool enabled, bool on) {
    SDL_Rect r;
    tool_rect(t, &r);
    gfx_fill_round_rect(r.x, r.y, r.w, r.h, 10, on ? COL_ACCENT : COL_BTN);
    FontId f = gfx_text_width(FONT_NORMAL, label) <= r.w - 12 ? FONT_NORMAL : FONT_SMALL;
    gfx_text(f, r.x + r.w / 2, r.y + (r.h - gfx_font_height(f)) / 2, r.w - 8, ALIGN_CENTER,
             enabled ? COL_TEXT : COL_OFF, label);
}

static void draw_value(Tool t, const char *text, bool enabled) {
    SDL_Rect r;
    tool_rect(t, &r);
    FontId f = gfx_text_width(FONT_NORMAL, text) <= r.w - 8 ? FONT_NORMAL : FONT_SMALL;
    gfx_text(f, r.x + r.w / 2, r.y + (r.h - gfx_font_height(f)) / 2, r.w - 4, ALIGN_CENTER,
             enabled ? COL_TEXT : COL_OFF, text);
}

static void draw_bar(void) {
    SDL_Rect bar;
    bar_rect(&bar);
    gfx_fill_rect(bar.x, bar.y, bar.w, bar.h, COL_BAR);
    gfx_fill_rect(bar.x, bar.y == 0 ? bar.h - 2 : bar.y, bar.w, 2, COL_ACCENT);

    char buf[64];
    if (sel >= 0) {
        vpad_item_name(sel, buf, sizeof(buf));
        gfx_text(FONT_NORMAL, NAME_X, bar.y + (BAR_H - gfx_font_height(FONT_NORMAL)) / 2, NAME_W, ALIGN_LEFT,
                 COL_ACCENT, buf);
    } else {
        gfx_text_wrapped(FONT_SMALL, NAME_X, bar.y + (BAR_H - 2 * gfx_font_height(FONT_SMALL)) / 2, NAME_W, COL_DIM,
                         tr(S_VPAD_PICK));
    }
    bool hidden = sel >= 0 && lay->keys[sel].hidden;
    draw_button(TB_HIDE, tr(hidden ? S_VPAD_SHOW : S_VPAD_HIDE), sel >= 0, hidden);
    bool round = can_round();
    draw_button(TB_RADIUS_DOWN, "-", round, false);
    snprintf(buf, sizeof(buf), tr(S_VPAD_RADIUS), round ? lay->keys[sel].r : 0);
    draw_value(TB_RADIUS, buf, round);
    draw_button(TB_RADIUS_UP, "+", round, false);
    draw_button(TB_OPACITY_DOWN, "-", lay->opacity > VPAD_OPACITY_MIN, false);
    snprintf(buf, sizeof(buf), tr(S_VPAD_OPACITY), lay->opacity);
    draw_value(TB_OPACITY, buf, true);
    draw_button(TB_OPACITY_UP, "+", lay->opacity < VPAD_OPACITY_MAX, false);
    snprintf(buf, sizeof(buf), tr(S_VPAD_SNAP), tr(lay->snap ? S_ON : S_OFF));
    draw_button(TB_SNAP, buf, true, lay->snap);
    draw_button(TB_RESET, tr(S_VPAD_RESET), true, reset_until && !SDL_TICKS_PASSED(SDL_GetTicks(), reset_until));
    draw_button(TB_DONE, tr(S_VPAD_DONE), true, false);
}

void vpad_screen_draw(void) {
    gfx_clear(COL_BG);

    // Cả màn hình Switch là chỗ đặt phím (không phụ thuộc cỡ màn hình của ứng dụng); hướng dẫn mờ ở giữa
    char buf[96];
    int hint_w = 420;
    gfx_text_wrapped(FONT_SMALL, (SCREEN_W - hint_w) / 2, SCREEN_H / 2 - 60, hint_w, COL_OFF, tr(S_VPAD_EDIT_HELP));

    for (int i = 0; i < VP_COUNT; i++)
        vpad_draw_item(lay, i, false, 0, 0, lay->keys[i].hidden ? 70 : 255);

    if (sel >= 0) {
        const VpadKey *k = &lay->keys[sel];
        outline(k->x, k->y, k->w, k->h, 3, COL_ACCENT);
        gfx_fill_circle(k->x + k->w, k->y + k->h, 14, COL_ACCENT);
        gfx_fill_circle(k->x + k->w, k->y + k->h, 6, COL_TEXT);
        if (drag == DRAG_MOVE || drag == DRAG_RESIZE) {
            // Toạ độ / kích thước đang kéo
            snprintf(buf, sizeof(buf), drag == DRAG_MOVE ? "%d, %d" : "%d x %d", drag == DRAG_MOVE ? k->x : k->w,
                     drag == DRAG_MOVE ? k->y : k->h);
            int tw = gfx_text_width(FONT_SMALL, buf) + 16, th = gfx_font_height(FONT_SMALL) + 6;
            int tx = clampi(k->x + k->w / 2 - tw / 2, 0, SCREEN_W - tw);
            int ty = k->y - th - 8 >= 0 ? k->y - th - 8 : k->y + k->h + 8;
            gfx_fill_round_rect(tx, ty, tw, th, 8, COL_BTN);
            gfx_text(FONT_SMALL, tx + tw / 2, ty + 3, 0, ALIGN_CENTER, COL_TEXT, buf);
        }
    }

    if (drag != DRAG_MOVE && drag != DRAG_RESIZE)
        draw_bar();

    if (reset_until && !SDL_TICKS_PASSED(SDL_GetTicks(), reset_until)) {
        const char *msg = tr(S_VPAD_RESET_CONFIRM);
        int tw = gfx_text_width(FONT_NORMAL, msg) + 80, th = 56;
        gfx_fill_round_rect((SCREEN_W - tw) / 2, (SCREEN_H - th) / 2, tw, th, 12, RGB(0x30, 0x30, 0x30));
        gfx_text(FONT_NORMAL, SCREEN_W / 2, (SCREEN_H - gfx_font_height(FONT_NORMAL)) / 2, 0, ALIGN_CENTER,
                 RGB(0xff, 0xc1, 0x4d), msg);
    }
}
