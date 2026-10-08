#include "vpad.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "gfx.h"
#include "lang.h"
#include "midp/midp.h"

#define INSET       3       // phím vẽ lùi vào: các phím hít sát nhau vẫn có khe
#define STICK_DEAD  0.25f   // vùng chết của cần điều khiển, theo bán kính
#define KNOB        0.42f   // bán kính núm, theo bán kính cần
#define MAX_TOUCH   10
#define SUB_OVERLAP 6

#define COL_KEY     RGB(0x30, 0x34, 0x3c)
#define COL_KNOB    RGB(0x5a, 0x60, 0x6c)
#define COL_DOWN    RGB(0x00, 0xb4, 0xe6)
#define COL_TEXT    RGB(0xee, 0xee, 0xee)
#define COL_DIM     RGB(0x9a, 0x9f, 0xa8)

static const struct {
    const char *id;
    int code;
    const char *label, *sub;    // chữ trên phím, chữ nhỏ bên dưới (phím số)
    VpadKey def;                // bố cục Nokia mặc định
} items[VP_COUNT] = {
    // Bên trái: phím mềm trái trên cần điều khiển
    [VP_STICK]      = { "stick", 0, "", "", { 50, 330, 270, 270, 0, false } },
    [VP_SOFT_LEFT]  = { "soft_left", MIDP_KEY_SOFT_LEFT, "", "", { 50, 220, 150, 70, 24, false } },
    // Bên phải: phím mềm phải, hàng Xoá + Fire, bàn phím số 3 x 4, hít liền một khối
    [VP_SOFT_RIGHT] = { "soft_right", MIDP_KEY_SOFT_RIGHT, "", "", { 1100, 220, 150, 70, 24, false } },
    [VP_CLEAR]      = { "clear", MIDP_KEY_CLEAR, "C", "", { 950, 290, 100, 80, 14, false } },
    [VP_FIRE]       = { "fire", MIDP_KEY_FIRE, "OK", "", { 1050, 290, 200, 80, 14, false } },
    [VP_1]          = { "1", '1', "1", "", { 950, 370, 100, 80, 14, false } },
    [VP_2]          = { "2", '2', "2", "abc", { 1050, 370, 100, 80, 14, false } },
    [VP_3]          = { "3", '3', "3", "def", { 1150, 370, 100, 80, 14, false } },
    [VP_4]          = { "4", '4', "4", "ghi", { 950, 450, 100, 80, 14, false } },
    [VP_5]          = { "5", '5', "5", "jkl", { 1050, 450, 100, 80, 14, false } },
    [VP_6]          = { "6", '6', "6", "mno", { 1150, 450, 100, 80, 14, false } },
    [VP_7]          = { "7", '7', "7", "pqrs", { 950, 530, 100, 80, 14, false } },
    [VP_8]          = { "8", '8', "8", "tuv", { 1050, 530, 100, 80, 14, false } },
    [VP_9]          = { "9", '9', "9", "wxyz", { 1150, 530, 100, 80, 14, false } },
    [VP_STAR]       = { "star", MIDP_KEY_STAR, "*", "", { 950, 610, 100, 80, 14, false } },
    [VP_0]          = { "0", '0', "0", "", { 1050, 610, 100, 80, 14, false } },
    [VP_POUND]      = { "pound", MIDP_KEY_POUND, "#", "", { 1150, 610, 100, 80, 14, false } },
};

void vpad_layout_default(VpadLayout *l) {
    l->style = VPAD_STYLE_NOKIA;
    l->opacity = 60;
    l->snap = true;
    for (int i = 0; i < VP_COUNT; i++)
        l->keys[i] = items[i].def;
}

bool vpad_key_changed(const VpadLayout *l, VpadItem i) {
    const VpadKey *a = &l->keys[i], *b = &items[i].def;
    return a->x != b->x || a->y != b->y || a->w != b->w || a->h != b->h || a->r != b->r || a->hidden != b->hidden;
}

bool vpad_layout_changed(const VpadLayout *l) {
    for (int i = 0; i < VP_COUNT; i++) {
        if (vpad_key_changed(l, i))
            return true;
    }
    return l->opacity != 60 || !l->snap;
}

bool vpad_key_valid(const VpadKey *k) {
    return k->w >= VPAD_SIZE_MIN && k->h >= VPAD_SIZE_MIN && k->x >= 0 && k->y >= 0 && k->x + k->w <= SCREEN_W &&
           k->y + k->h <= SCREEN_H && k->r >= 0;
}

const char *vpad_style_name(int style) {
    (void)style;
    return "Nokia";
}

const char *vpad_item_id(VpadItem i) {
    return i >= 0 && i < VP_COUNT ? items[i].id : "";
}

int vpad_item_from_id(const char *id) {
    for (int i = 0; i < VP_COUNT; i++) {
        if (strcmp(items[i].id, id) == 0)
            return i;
    }
    return -1;
}

void vpad_item_name(VpadItem i, char *out, size_t size) {
    switch (i) {
    case VP_STICK:      snprintf(out, size, "%s", tr(S_VPAD_STICK)); break;
    case VP_SOFT_LEFT:  snprintf(out, size, "%s", tr(S_HELP_SOFT_LEFT)); break;
    case VP_SOFT_RIGHT: snprintf(out, size, "%s", tr(S_HELP_SOFT_RIGHT)); break;
    case VP_CLEAR:      snprintf(out, size, "%s", tr(S_KEY_CLEAR)); break;
    case VP_FIRE:       snprintf(out, size, "Fire (OK)"); break;
    default:            snprintf(out, size, tr(S_VPAD_KEY_FMT), items[i].label); break;
    }
}

int vpad_item_code(VpadItem i) {
    return i >= 0 && i < VP_COUNT ? items[i].code : 0;
}

static SDL_Color with_alpha(SDL_Color c, int a) {
    c.a = (Uint8)(a < 0 ? 0 : a > 255 ? 255 : a);
    return c;
}

void vpad_draw_item(const VpadLayout *l, VpadItem i, bool down, int kx, int ky, Uint8 alpha) {
    const VpadKey *k = &l->keys[i];
    int a = l->opacity * 255 / 100 * alpha / 255;
    int fill = a * 200 / 255;
    if (i == VP_STICK) {
        int cx = k->x + k->w / 2, cy = k->y + k->h / 2, r = k->w / 2;
        gfx_fill_circle(cx, cy, r, with_alpha(COL_KEY, fill));
        // Chấm chỉ 4 hướng
        int d = r * 78 / 100, dot = r / 18 + 2;
        gfx_fill_circle(cx, cy - d, dot, with_alpha(COL_DIM, a));
        gfx_fill_circle(cx, cy + d, dot, with_alpha(COL_DIM, a));
        gfx_fill_circle(cx - d, cy, dot, with_alpha(COL_DIM, a));
        gfx_fill_circle(cx + d, cy, dot, with_alpha(COL_DIM, a));
        gfx_fill_circle(cx + kx, cy + ky, (int)(r * KNOB), with_alpha(down ? COL_DOWN : COL_KNOB, a));
        return;
    }
    int x = k->x + INSET, y = k->y + INSET, w = k->w - 2 * INSET, h = k->h - 2 * INSET;
    if (w <= 0 || h <= 0)
        return;
    int r = k->r - INSET < 0 ? 0 : k->r - INSET;
    gfx_fill_round_rect(x, y, w, h, r, with_alpha(down ? COL_DOWN : COL_KEY, down ? a : fill));
    SDL_Color text = with_alpha(COL_TEXT, a);
    if (i == VP_SOFT_LEFT || i == VP_SOFT_RIGHT) {
        // Phím mềm: vạch ngang như trên điện thoại Nokia
        int bw = w * 2 / 5 > 48 ? 48 : w * 2 / 5;
        gfx_fill_round_rect(x + (w - bw) / 2, y + h / 2 - 3, bw, 6, 3, text);
        return;
    }
    const char *sub = items[i].sub;
    FontId f = gfx_font_height(FONT_LARGE) <= h - 4 ? FONT_LARGE : gfx_font_height(FONT_NORMAL) <= h ? FONT_NORMAL
                                                                                                   : FONT_SMALL;
    int fh = gfx_font_height(f), sh = gfx_font_height(FONT_SMALL);
    // Chữ nhỏ dưới số chỉ khi phím đủ cao
    // Dòng chữ nhỏ đè lên phần trống dưới chân số (SUB_OVERLAP px) cho cụm gọn
    bool show_sub = *sub && f == FONT_LARGE && fh + sh - SUB_OVERLAP <= h;
    int ty = show_sub ? y + (h - (fh + sh - SUB_OVERLAP)) / 2 : y + (h - fh) / 2;
    gfx_text(f, x + w / 2, ty, w - 4, ALIGN_CENTER, text, items[i].label);
    if (show_sub)
        gfx_text(FONT_SMALL, x + w / 2, ty + fh - SUB_OVERLAP, w - 4, ALIGN_CENTER, with_alpha(COL_DIM, a), sub);
}

// ---------------------------------------------------------------------------
// Khi chạy ứng dụng

enum {
    DIR_UP = 1,
    DIR_DOWN = 2,
    DIR_LEFT = 4,
    DIR_RIGHT = 8,
};

typedef struct {
    bool active;
    SDL_FingerID id;
    int item;
} Touch;

static bool enabled;
static VpadLayout lay;
static int dir_codes[4];
static VpadKeyFn key_fn;
static Touch touches[MAX_TOUCH];
static int stick_dirs;          // các hướng đang bấm (DIR_*)
static int knob_x, knob_y;      // độ lệch của núm so với tâm cần

void vpad_start(bool on, const VpadLayout *l, const int dirs[4], VpadKeyFn key) {
    memset(touches, 0, sizeof(touches));
    stick_dirs = 0;
    knob_x = knob_y = 0;
    enabled = on;
    lay = *l;
    memcpy(dir_codes, dirs, sizeof(dir_codes));
    key_fn = key;
}

bool vpad_enabled(void) {
    return enabled;
}

static void send(int code, bool down) {
    if (code && key_fn)
        key_fn(code, down);
}

static void set_dirs(int dirs) {
    for (int b = 0; b < 4; b++) {
        int bit = 1 << b;
        if ((dirs & bit) != (stick_dirs & bit))
            send(dir_codes[b], (dirs & bit) != 0);
    }
    stick_dirs = dirs;
}

// Như stick trái: ra khỏi vùng chết thì bấm hướng theo 8 góc 45 độ (góc chéo bấm 2 hướng)
static void stick_move(int x, int y) {
    const VpadKey *k = &lay.keys[VP_STICK];
    float r = k->w / 2.0f;
    float dx = x - (k->x + r), dy = y - (k->y + r);
    float dist = sqrtf(dx * dx + dy * dy);
    float max = r * (1 - KNOB);
    float s = dist > max ? max / dist : 1;
    knob_x = (int)(dx * s);
    knob_y = (int)(dy * s);
    if (dist < r * STICK_DEAD) {
        set_dirs(0);
        return;
    }
    static const int sectors[8] = {
        DIR_RIGHT, DIR_UP | DIR_RIGHT, DIR_UP, DIR_UP | DIR_LEFT,
        DIR_LEFT, DIR_DOWN | DIR_LEFT, DIR_DOWN, DIR_DOWN | DIR_RIGHT,
    };
    float angle = atan2f(-dy, dx);     // 0 = phải, ngược chiều kim đồng hồ
    int sector = (int)floorf(angle / 0.78539816f + 0.5f);
    set_dirs(sectors[(sector % 8 + 8) % 8]);
}

static bool stick_hit(int x, int y) {
    const VpadKey *k = &lay.keys[VP_STICK];
    float r = k->w / 2.0f, dx = x - (k->x + r), dy = y - (k->y + r);
    float hit = r * 1.15f;      // vùng chạm rộng hơn hình một chút
    return dx * dx + dy * dy <= hit * hit;
}

static int item_at(int x, int y) {
    // Phím vẽ sau nằm trên: tìm từ cuối
    for (int i = VP_COUNT - 1; i >= 0; i--) {
        const VpadKey *k = &lay.keys[i];
        if (k->hidden)
            continue;
        if (i == VP_STICK ? stick_hit(x, y) : x >= k->x && y >= k->y && x < k->x + k->w && y < k->y + k->h)
            return i;
    }
    return -1;
}

static void release_touch(Touch *t) {
    if (t->item == VP_STICK) {
        set_dirs(0);
        knob_x = knob_y = 0;
    } else {
        send(items[t->item].code, false);
    }
    t->active = false;
}

void vpad_release_all(void) {
    for (int i = 0; i < MAX_TOUCH; i++) {
        if (touches[i].active)
            release_touch(&touches[i]);
    }
}

static Touch *find_touch(SDL_FingerID id) {
    for (int i = 0; i < MAX_TOUCH; i++) {
        if (touches[i].active && touches[i].id == id)
            return &touches[i];
    }
    return NULL;
}

static bool stick_held(void) {
    for (int i = 0; i < MAX_TOUCH; i++) {
        if (touches[i].active && touches[i].item == VP_STICK)
            return true;
    }
    return false;
}

bool vpad_pointer(SDL_FingerID id, VkbPointer type, int x, int y) {
    if (!enabled)
        return false;
    Touch *t = find_touch(id);
    if (type == VKB_DOWN) {
        if (t)
            release_touch(t);   // mất sự kiện nhả: coi như chạm mới
        int item = item_at(x, y);
        if (item < 0 || (item == VP_STICK && stick_held()))
            return item >= 0;
        t = NULL;
        for (int i = 0; i < MAX_TOUCH && !t; i++) {
            if (!touches[i].active)
                t = &touches[i];
        }
        if (!t)
            return true;
        t->active = true;
        t->id = id;
        t->item = item;
        if (item == VP_STICK)
            stick_move(x, y);
        else
            send(items[item].code, true);
        return true;
    }
    if (!t)
        return false;
    if (type == VKB_MOVE) {
        // Cần điều khiển đi theo ngón tay dù ra ngoài vòng tròn; phím thường giữ tới khi nhả
        if (t->item == VP_STICK)
            stick_move(x, y);
        return true;
    }
    release_touch(t);
    return true;
}

static bool item_held(int item) {
    for (int i = 0; i < MAX_TOUCH; i++) {
        if (touches[i].active && touches[i].item == item)
            return true;
    }
    return false;
}

void vpad_draw(void) {
    if (!enabled)
        return;
    for (int i = 0; i < VP_COUNT; i++) {
        if (lay.keys[i].hidden)
            continue;
        bool down = i == VP_STICK ? stick_dirs != 0 : item_held(i);
        vpad_draw_item(&lay, i, down, knob_x, knob_y, 255);
    }
}
