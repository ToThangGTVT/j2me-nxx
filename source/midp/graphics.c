// Native cho Graphics / Image / Font: vẽ phần mềm lên mảng int[] ARGB của Image
#include "midp.h"

#include <math.h>
#include <png.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>
#include <SDL_ttf.h>

#include "../platform.h"
#include "../third_party/stb_image.h"
#include "../vm/vm.h"

// ---------------------------------------------------------------------------
// Field slot (tra lười lần đầu)

static bool slots_ready;
static int G_img, G_transX, G_transY, G_clipX, G_clipY, G_clipW, G_clipH, G_color, G_font, G_stroke;
static int I_pixels, I_width, I_height, I_mutable, I_opaque;
static int F_key;

static void init_slots(void) {
    if (slots_ready)
        return;
    const char *G = "javax/microedition/lcdui/Graphics";
    G_img = field_slot(G, "img", "Ljavax/microedition/lcdui/Image;");
    G_transX = field_slot(G, "transX", "I");
    G_transY = field_slot(G, "transY", "I");
    G_clipX = field_slot(G, "clipX", "I");
    G_clipY = field_slot(G, "clipY", "I");
    G_clipW = field_slot(G, "clipW", "I");
    G_clipH = field_slot(G, "clipH", "I");
    G_color = field_slot(G, "color", "I");
    G_font = field_slot(G, "font", "Ljavax/microedition/lcdui/Font;");
    G_stroke = field_slot(G, "stroke", "I");
    const char *I = "javax/microedition/lcdui/Image";
    I_pixels = field_slot(I, "pixels", "[I");
    I_width = field_slot(I, "width", "I");
    I_height = field_slot(I, "height", "I");
    I_mutable = field_slot(I, "mutable", "Z");
    I_opaque = field_slot(I, "opaque", "Z");
    F_key = field_slot("javax/microedition/lcdui/Font", "key", "I");
    slots_ready = true;
}

// ---------------------------------------------------------------------------
// Lớp chữ nét cao. Khi bật chữ mịn, chữ vẽ lên ảnh cỡ màn hình được vẽ thêm một bản ở độ phân giải
// k lần (k = hệ số phóng ra màn hình). Ảnh gốc vẫn y như cũ (getRGB, va chạm...); lúc đẩy ra màn hình,
// pixel nào vẫn còn là chữ đã vẽ thì lấy khối k x k nét cao thay vì phóng to pixel thấp.
// Pixel p lấy từ hi khi valid[p] và tag[p] == giá trị pixel hiện tại. Lệnh vẽ khác đè lên thì xoá valid;
// so tag để bắt cả chỗ ghi thẳng vào mảng pixel từ Java (DirectGraphics, M3G...).

#define MAX_LAYERS 3

typedef struct {
    Object *arr;                // mảng pixel của ảnh (GC không dời object)
    int w, h;
    uint32_t *hi;               // (w*k) x (h*k)
    uint32_t *tag;
    uint8_t *valid;
    int bx0, by0, bx1, by1;     // khung bao các pixel valid; rỗng khi bx0 >= bx1
    unsigned use;
} TextLayer;

static int hires_k;             // < 2 = tắt
static int layer_w, layer_h;    // chỉ ảnh đúng cỡ màn hình mới có lớp chữ
static TextLayer layers[MAX_LAYERS];
static unsigned layer_clock;

static TextLayer *layer_find(Object *arr) {
    if (hires_k < 2 || !arr)
        return NULL;
    for (int i = 0; i < MAX_LAYERS; i++) {
        if (layers[i].arr == arr) {
            layers[i].use = ++layer_clock;
            return &layers[i];
        }
    }
    return NULL;
}

static void layer_free(TextLayer *L) {
    free(L->hi);
    free(L->tag);
    free(L->valid);
    memset(L, 0, sizeof(*L));
}

// Lấy lớp trống hoặc lớp lâu không dùng nhất
static TextLayer *layer_create(Object *arr, int w, int h) {
    if (hires_k < 2 || !arr || w != layer_w || h != layer_h)
        return NULL;
    TextLayer *L = &layers[0];
    for (int i = 1; i < MAX_LAYERS && L->arr; i++) {
        if (!layers[i].arr || layers[i].use < L->use)
            L = &layers[i];
    }
    if (!L->hi) {
        int k = hires_k;
        L->hi = malloc((size_t)w * k * h * k * 4);
        L->tag = malloc((size_t)w * h * 4);
        L->valid = malloc((size_t)w * h);
        if (!L->hi || !L->tag || !L->valid) {
            layer_free(L);
            return NULL;
        }
    }
    memset(L->valid, 0, (size_t)w * h);
    L->arr = arr;
    L->w = w;
    L->h = h;
    L->bx0 = L->by0 = L->bx1 = L->by1 = 0;
    L->use = ++layer_clock;
    return L;
}

static void layer_grow(TextLayer *L, int x0, int y0, int x1, int y1) {
    if (L->bx0 >= L->bx1) {
        L->bx0 = x0; L->by0 = y0; L->bx1 = x1; L->by1 = y1;
        return;
    }
    if (x0 < L->bx0) L->bx0 = x0;
    if (y0 < L->by0) L->by0 = y0;
    if (x1 > L->bx1) L->bx1 = x1;
    if (y1 > L->by1) L->by1 = y1;
}

// Vùng [x0, x1) x [y0, y1) bị vẽ đè: về lại pixel thường
static void layer_clear(TextLayer *L, int x0, int y0, int x1, int y1) {
    if (x0 < L->bx0) x0 = L->bx0;
    if (y0 < L->by0) y0 = L->by0;
    if (x1 > L->bx1) x1 = L->bx1;
    if (y1 > L->by1) y1 = L->by1;
    for (int y = y0; y < y1 && x0 < x1; y++)
        memset(L->valid + y * L->w + x0, 0, (size_t)(x1 - x0));
}

static void fill_block(TextLayer *L, int x, int y, uint32_t v) {
    int k = hires_k, hw = L->w * k;
    uint32_t *o = L->hi + (size_t)y * k * hw + x * k;
    for (int dy = 0; dy < k; dy++, o += hw)
        for (int dx = 0; dx < k; dx++)
            o[dx] = v;
}

typedef struct {
    Object *img;
    TextLayer *layer;           // lớp chữ nét cao của ảnh (nếu có)
    uint32_t *px;
    int w, h;
    int cx0, cy0, cx1, cy1;     // vùng clip [cx0, cx1) x [cy0, cy1)
    int tx, ty;
    uint32_t color;
    bool dotted;
} Ctx;

// Ghi pixel có thể không đục vào ảnh: bỏ cờ opaque
static inline void mark_maybe_transparent(Ctx *c) {
    FIELD_I(c->img, I_opaque) = 0;
}

static bool ctx_init(Object *g, Ctx *c) {
    init_slots();
    Object *img = FIELD_L(g, G_img);
    Object *arr = FIELD_L(img, I_pixels);
    c->img = img;
    c->layer = layer_find(arr);
    c->px = ARRAY_DATA(arr, uint32_t);
    c->w = FIELD_I(img, I_width);
    c->h = FIELD_I(img, I_height);
    c->tx = FIELD_I(g, G_transX);
    c->ty = FIELD_I(g, G_transY);
    c->cx0 = FIELD_I(g, G_clipX);
    c->cy0 = FIELD_I(g, G_clipY);
    c->cx1 = c->cx0 + FIELD_I(g, G_clipW);
    c->cy1 = c->cy0 + FIELD_I(g, G_clipH);
    if (c->cx0 < 0) c->cx0 = 0;
    if (c->cy0 < 0) c->cy0 = 0;
    if (c->cx1 > c->w) c->cx1 = c->w;
    if (c->cy1 > c->h) c->cy1 = c->h;
    c->color = (uint32_t)FIELD_I(g, G_color);
    c->dotted = FIELD_I(g, G_stroke) != 0;
    return c->cx0 < c->cx1 && c->cy0 < c->cy1;
}

static inline uint32_t blend(uint32_t dst, uint32_t src) {
    uint32_t sa = src >> 24;
    if (sa == 255)
        return src;
    if (sa == 0)
        return dst;
    uint32_t da = dst >> 24;
    uint32_t ia = 255 - sa;
    if (da == 255) {
        uint32_t r = (((src >> 16) & 0xff) * sa + ((dst >> 16) & 0xff) * ia) / 255;
        uint32_t g = (((src >> 8) & 0xff) * sa + ((dst >> 8) & 0xff) * ia) / 255;
        uint32_t b = ((src & 0xff) * sa + (dst & 0xff) * ia) / 255;
        return 0xff000000u | (r << 16) | (g << 8) | b;
    }
    uint32_t oa = sa + da * ia / 255;
    if (oa == 0)
        return 0;
    uint32_t dw = da * ia / 255;
    uint32_t r = (((src >> 16) & 0xff) * sa + ((dst >> 16) & 0xff) * dw) / oa;
    uint32_t g = (((src >> 8) & 0xff) * sa + ((dst >> 8) & 0xff) * dw) / oa;
    uint32_t b = ((src & 0xff) * sa + (dst & 0xff) * dw) / oa;
    return (oa << 24) | (r << 16) | (g << 8) | b;
}

// Toạ độ tuyệt đối (đã cộng translate)
static inline void plot(Ctx *c, int x, int y) {
    if (x < c->cx0 || x >= c->cx1 || y < c->cy0 || y >= c->cy1)
        return;
    uint32_t *p = &c->px[y * c->w + x];
    *p = blend(*p, c->color);
    if (c->layer)
        layer_clear(c->layer, x, y, x + 1, y + 1);
}

static void hspan(Ctx *c, int x0, int x1, int y) {
    if (y < c->cy0 || y >= c->cy1)
        return;
    if (x0 < c->cx0) x0 = c->cx0;
    if (x1 > c->cx1) x1 = c->cx1;
    if (x0 >= x1)
        return;
    if (c->layer)
        layer_clear(c->layer, x0, y, x1, y + 1);
    uint32_t *p = &c->px[y * c->w + x0];
    if ((c->color >> 24) == 255) {
        for (int x = x0; x < x1; x++)
            *p++ = c->color;
    } else {
        for (int x = x0; x < x1; x++, p++)
            *p = blend(*p, c->color);
    }
}

static void line(Ctx *c, int x0, int y0, int x1, int y1) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    int n = 0;
    for (;;) {
        if (!c->dotted || (n++ % 6) < 3)
            plot(c, x0, y0);
        if (x0 == x1 && y0 == y1)
            break;
        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

// ---------------------------------------------------------------------------
// Hình cơ bản

static NativeResult G_drawLine(VMThread *t, Value *args, Value *ret) {
    (void)t; (void)ret;
    Ctx c;
    if (ctx_init(args[0].l, &c))
        line(&c, args[1].i + c.tx, args[2].i + c.ty, args[3].i + c.tx, args[4].i + c.ty);
    return NATIVE_OK;
}

static NativeResult G_fillRect(VMThread *t, Value *args, Value *ret) {
    (void)t; (void)ret;
    Ctx c;
    if (!ctx_init(args[0].l, &c))
        return NATIVE_OK;
    int x = args[1].i + c.tx, y = args[2].i + c.ty, w = args[3].i, h = args[4].i;
    if (w <= 0 || h <= 0)
        return NATIVE_OK;
    int y0 = y < c.cy0 ? c.cy0 : y;
    int y1 = y + h > c.cy1 ? c.cy1 : y + h;
    for (int yy = y0; yy < y1; yy++)
        hspan(&c, x, x + w, yy);
    return NATIVE_OK;
}

static void fill_triangle(Ctx *c, int x1, int y1, int x2, int y2, int x3, int y3) {
    // Sắp theo y
    if (y1 > y2) { int tx = x1, ty = y1; x1 = x2; y1 = y2; x2 = tx; y2 = ty; }
    if (y1 > y3) { int tx = x1, ty = y1; x1 = x3; y1 = y3; x3 = tx; y3 = ty; }
    if (y2 > y3) { int tx = x2, ty = y2; x2 = x3; y2 = y3; x3 = tx; y3 = ty; }
    if (y1 == y3) {
        int mn = x1 < x2 ? x1 : x2, mx = x1 > x2 ? x1 : x2;
        if (x3 < mn) mn = x3;
        if (x3 > mx) mx = x3;
        hspan(c, mn, mx + 1, y1);
        return;
    }
    for (int y = y1; y <= y3; y++) {
        double xa = x1 + (double)(x3 - x1) * (y - y1) / (y3 - y1);
        double xb;
        if (y < y2 || (y == y2 && y2 != y1))
            xb = y2 == y1 ? x2 : x1 + (double)(x2 - x1) * (y - y1) / (y2 - y1);
        else
            xb = y3 == y2 ? x3 : x2 + (double)(x3 - x2) * (y - y2) / (y3 - y2);
        if (xa > xb) { double tmp = xa; xa = xb; xb = tmp; }
        hspan(c, (int)lround(xa), (int)lround(xb) + 1, y);
    }
}

static NativeResult G_fillTriangle(VMThread *t, Value *args, Value *ret) {
    (void)t; (void)ret;
    Ctx c;
    if (ctx_init(args[0].l, &c))
        fill_triangle(&c, args[1].i + c.tx, args[2].i + c.ty, args[3].i + c.tx, args[4].i + c.ty,
                      args[5].i + c.tx, args[6].i + c.ty);
    return NATIVE_OK;
}

// Góc tính theo độ, ngược chiều kim đồng hồ từ hướng 3 giờ
static bool angle_in(double a, int start, int arc) {
    if (arc >= 360 || arc <= -360)
        return true;
    if (arc < 0) {
        start += arc;
        arc = -arc;
    }
    double d = fmod(a - start, 360.0);
    if (d < 0)
        d += 360.0;
    return d <= arc;
}

static void fill_arc(Ctx *c, int x, int y, int w, int h, int start, int arc) {
    if (w <= 0 || h <= 0)
        return;
    double rx = w / 2.0, ry = h / 2.0;
    double cx = x + rx, cy = y + ry;
    for (int py = y; py < y + h; py++) {
        if (py < c->cy0 || py >= c->cy1)
            continue;
        double dy = (cy - (py + 0.5)) / ry;
        if (dy * dy > 1)
            continue;
        double half = rx * sqrt(1 - dy * dy);
        int xa = (int)ceil(cx - half - 0.5), xb = (int)floor(cx + half - 0.5);
        if (arc >= 360 || arc <= -360) {
            hspan(c, xa, xb + 1, py);
            continue;
        }
        for (int px = xa; px <= xb; px++) {
            double a = atan2(cy - (py + 0.5), (px + 0.5) - cx) * 180.0 / M_PI;
            if (angle_in(a, start, arc))
                plot(c, px, py);
        }
    }
}

static void draw_arc(Ctx *c, int x, int y, int w, int h, int start, int arc) {
    if (w < 0 || h < 0)
        return;
    if (arc < 0) {
        start += arc;
        arc = -arc;
    }
    if (arc > 360)
        arc = 360;
    double rx = w / 2.0, ry = h / 2.0;
    double cx = x + rx, cy = y + ry;
    int steps = (int)((rx + ry) * 2 * M_PI * arc / 360.0) + 4;
    int px = 0, py = 0;
    for (int i = 0; i <= steps; i++) {
        double a = (start + (double)arc * i / steps) * M_PI / 180.0;
        int nx = (int)lround(cx + rx * cos(a)), ny = (int)lround(cy - ry * sin(a));
        if (i > 0)
            line(c, px, py, nx, ny);
        px = nx;
        py = ny;
    }
}

static NativeResult G_fillArc(VMThread *t, Value *args, Value *ret) {
    (void)t; (void)ret;
    Ctx c;
    if (ctx_init(args[0].l, &c))
        fill_arc(&c, args[1].i + c.tx, args[2].i + c.ty, args[3].i, args[4].i, args[5].i, args[6].i);
    return NATIVE_OK;
}

static NativeResult G_drawArc(VMThread *t, Value *args, Value *ret) {
    (void)t; (void)ret;
    Ctx c;
    if (ctx_init(args[0].l, &c))
        draw_arc(&c, args[1].i + c.tx, args[2].i + c.ty, args[3].i, args[4].i, args[5].i, args[6].i);
    return NATIVE_OK;
}

static NativeResult G_fillRoundRect(VMThread *t, Value *args, Value *ret) {
    (void)t; (void)ret;
    Ctx c;
    if (!ctx_init(args[0].l, &c))
        return NATIVE_OK;
    int x = args[1].i + c.tx, y = args[2].i + c.ty, w = args[3].i, h = args[4].i;
    double rx = abs(args[5].i) / 2.0, ry = abs(args[6].i) / 2.0;
    if (w <= 0 || h <= 0)
        return NATIVE_OK;
    if (rx > w / 2.0) rx = w / 2.0;
    if (ry > h / 2.0) ry = h / 2.0;
    for (int row = 0; row < h; row++) {
        double inset = 0;
        double dy = -1;
        if (row < ry)
            dy = (ry - row - 0.5) / ry;
        else if (row >= h - ry)
            dy = (row + 0.5 - (h - ry)) / ry;
        if (dy >= 0 && ry > 0) {
            if (dy > 1) dy = 1;
            inset = rx - rx * sqrt(1 - dy * dy);
        }
        hspan(&c, x + (int)lround(inset), x + w - (int)lround(inset), y + row);
    }
    return NATIVE_OK;
}

static NativeResult G_drawRoundRect(VMThread *t, Value *args, Value *ret) {
    (void)t; (void)ret;
    Ctx c;
    if (!ctx_init(args[0].l, &c))
        return NATIVE_OK;
    int x = args[1].i + c.tx, y = args[2].i + c.ty, w = args[3].i, h = args[4].i;
    int aw = abs(args[5].i), ah = abs(args[6].i);
    if (w < 0 || h < 0)
        return NATIVE_OK;
    if (aw > w) aw = w;
    if (ah > h) ah = h;
    int rx = aw / 2, ry = ah / 2;
    line(&c, x + rx, y, x + w - rx, y);
    line(&c, x + rx, y + h, x + w - rx, y + h);
    line(&c, x, y + ry, x, y + h - ry);
    line(&c, x + w, y + ry, x + w, y + h - ry);
    draw_arc(&c, x, y, aw, ah, 90, 90);
    draw_arc(&c, x + w - aw, y, aw, ah, 0, 90);
    draw_arc(&c, x, y + h - ah, aw, ah, 180, 90);
    draw_arc(&c, x + w - aw, y + h - ah, aw, ah, 270, 90);
    return NATIVE_OK;
}

// ---------------------------------------------------------------------------
// Ảnh

static NativeResult G_drawRGB(VMThread *t, Value *args, Value *ret) {
    (void)ret;
    Object *rgb = args[1].l;
    jint off = args[2].i, scan = args[3].i, x = args[4].i, y = args[5].i, w = args[6].i, h = args[7].i;
    bool alpha = args[8].i != 0;
    if (!rgb) {
        throw_null(t);
        return NATIVE_EXCEPTION;
    }
    if (w <= 0 || h <= 0)
        return NATIVE_OK;
    int64_t first = (int64_t)off, last = (int64_t)off + (int64_t)(h - 1) * scan + (w - 1);
    int64_t lo = first < last ? first : last, hi = first < last ? last : first;
    if (lo < 0 || hi >= ARRAY_LEN(rgb) || off < 0) {
        throw_new(t, "java/lang/ArrayIndexOutOfBoundsException", "drawRGB");
        return NATIVE_EXCEPTION;
    }
    Ctx c;
    if (!ctx_init(args[0].l, &c))
        return NATIVE_OK;
    x += c.tx;
    y += c.ty;
    const uint32_t *src = ARRAY_DATA(rgb, uint32_t);
    if (alpha && !FIELD_I(c.img, I_opaque))
        mark_maybe_transparent(&c);
    int r0 = y < c.cy0 ? c.cy0 - y : 0, r1 = y + h > c.cy1 ? c.cy1 - y : h;
    int k0 = x < c.cx0 ? c.cx0 - x : 0, k1 = x + w > c.cx1 ? c.cx1 - x : w;
    if (c.layer)
        layer_clear(c.layer, x + k0, y + r0, x + k1, y + r1);
    for (int r = r0; r < r1; r++) {
        const uint32_t *s = src + off + (int64_t)r * scan;
        uint32_t *d = c.px + (y + r) * c.w + x;
        for (int k = k0; k < k1; k++)
            d[k] = alpha ? blend(d[k], s[k]) : (s[k] | 0xff000000u);
    }
    return NATIVE_OK;
}

// Sau khi chép ảnh có lớp chữ (sl, mảng sp) sang ảnh đích không lật/xoay: chép theo các khối nét cao
// còn đúng, chỗ khác về pixel thường. (ox, oy) = toạ độ nguồn - toạ độ đích.
static void layer_copy(TextLayer *dl, const uint32_t *dp, TextLayer *sl, const uint32_t *sp, int ox, int oy,
                       int x0, int y0, int x1, int y1) {
    int k = hires_k, dhw = dl->w * k, shw = sl->w * k;
    int vx0 = x1, vy0 = y1, vx1 = x0, vy1 = y0;
    for (int y = y0; y < y1; y++) {
        int sy = y + oy;
        bool row_in = sy >= sl->by0 && sy < sl->by1;
        for (int x = x0; x < x1; x++) {
            int sx = x + ox;
            int si = sy * sl->w + sx, di = y * dl->w + x;
            if (row_in && sx >= sl->bx0 && sx < sl->bx1 && sl->valid[si] && sl->tag[si] == sp[si] &&
                dp[di] == sp[si]) {
                const uint32_t *s = sl->hi + (size_t)sy * k * shw + sx * k;
                uint32_t *d = dl->hi + (size_t)y * k * dhw + x * k;
                for (int dy = 0; dy < k; dy++, s += shw, d += dhw)
                    memcpy(d, s, (size_t)k * 4);
                dl->valid[di] = 1;
                dl->tag[di] = dp[di];
                if (x < vx0) vx0 = x;
                if (x >= vx1) vx1 = x + 1;
                if (y < vy0) vy0 = y;
                if (y >= vy1) vy1 = y + 1;
            } else {
                dl->valid[di] = 0;
            }
        }
    }
    if (vx0 < vx1)
        layer_grow(dl, vx0, vy0, vx1, vy1);
}

static NativeResult G_drawRegionImpl(VMThread *t, Value *args, Value *ret) {
    (void)ret;
    Object *g = args[0].l, *src = args[1].l;
    if (!src) {
        throw_null(t);
        return NATIVE_EXCEPTION;
    }
    jint sx = args[2].i, sy = args[3].i, w = args[4].i, h = args[5].i, tr = args[6].i;
    jint dx = args[7].i, dy = args[8].i;
    bool copy = args[9].i != 0;
    Ctx c;
    if (!ctx_init(g, &c) || w <= 0 || h <= 0)
        return NATIVE_OK;
    int sw = FIELD_I(src, I_width);
    const uint32_t *sp = ARRAY_DATA(FIELD_L(src, I_pixels), uint32_t);
    dx += c.tx;
    dy += c.ty;
    bool swap = (tr & 4) != 0;
    int dw = swap ? h : w, dh = swap ? w : h;

    int y0 = dy < c.cy0 ? c.cy0 : dy, y1 = dy + dh > c.cy1 ? c.cy1 : dy + dh;
    int x0 = dx < c.cx0 ? c.cx0 : dx, x1 = dx + dw > c.cx1 ? c.cx1 : dx + dw;
    if (x0 >= x1 || y0 >= y1)
        return NATIVE_OK;

    bool src_opaque = FIELD_I(src, I_opaque) != 0;
    if (copy && !src_opaque)
        mark_maybe_transparent(&c);
    // Ảnh nguồn có chữ nét cao (vd bộ đệm riêng của game vẽ ra màn hình): đích cũng cần lớp chữ
    TextLayer *sl = tr == 0 ? layer_find(FIELD_L(src, I_pixels)) : NULL;
    if (sl && (sl->bx0 >= sl->bx1 || sl == c.layer))
        sl = NULL;
    if (sl && !c.layer)
        c.layer = layer_create(FIELD_L(c.img, I_pixels), c.w, c.h);
    if (c.layer && !sl)
        layer_clear(c.layer, x0, y0, x1, y1);
    if (tr == 0) {
        for (int y = y0; y < y1; y++) {
            const uint32_t *s = sp + (sy + y - dy) * sw + sx + (x0 - dx);
            uint32_t *d = c.px + y * c.w + x0;
            int n = x1 - x0;
            // Ảnh đục: chép thẳng cả hàng
            if (copy || src_opaque) {
                memcpy(d, s, (size_t)n * 4);
                continue;
            }
            for (int i = 0; i < n; i++) {
                uint32_t p = s[i];
                uint32_t a = p >> 24;
                if (a == 255)
                    d[i] = p;
                else if (a)
                    d[i] = blend(d[i], p);
            }
        }
        if (sl && c.layer)
            layer_copy(c.layer, c.px, sl, sp, sx - dx, sy - dy, x0, y0, x1, y1);
        return NATIVE_OK;
    }

    for (int y = y0; y < y1; y++) {
        int ly = y - dy;
        uint32_t *d = c.px + y * c.w;
        for (int x = x0; x < x1; x++) {
            int lx = x - dx;
            int px, py;
            switch (tr) {
            case 2: px = w - 1 - lx; py = ly; break;            // MIRROR
            case 1: px = lx; py = h - 1 - ly; break;            // MIRROR_ROT180
            case 3: px = w - 1 - lx; py = h - 1 - ly; break;    // ROT180
            case 5: px = ly; py = h - 1 - lx; break;            // ROT90
            case 6: px = w - 1 - ly; py = lx; break;            // ROT270
            case 7: px = w - 1 - ly; py = h - 1 - lx; break;    // MIRROR_ROT90
            case 4: px = ly; py = lx; break;                    // MIRROR_ROT270
            default: px = lx; py = ly; break;
            }
            uint32_t p = sp[(sy + py) * sw + sx + px];
            if (copy || src_opaque)
                d[x] = p;
            else if ((p >> 24) == 255)
                d[x] = p;
            else if (p >> 24)
                d[x] = blend(d[x], p);
        }
    }
    return NATIVE_OK;
}

typedef struct {
    const uint8_t *data;
    size_t size, pos;
} PngReader;

static void png_read_mem(png_structp png, png_bytep out, png_size_t len) {
    PngReader *r = png_get_io_ptr(png);
    if (r->pos + len > r->size)
        png_error(png, "eof");
    memcpy(out, r->data + r->pos, len);
    r->pos += len;
}

static void png_warn(png_structp png, png_const_charp msg) {
    (void)png;
    (void)msg;
}

static uint32_t *decode_png(const uint8_t *data, size_t size, int *out_w, int *out_h) {
    if (size < 8 || png_sig_cmp(data, 0, 8))
        return NULL;
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, png_warn);
    if (!png)
        return NULL;
    png_infop info = png_create_info_struct(png);
    uint32_t *volatile pixels = NULL;
    png_bytep *volatile rows = NULL;
    if (!info || setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, info ? &info : NULL, NULL);
        free(pixels);
        free(rows);
        return NULL;
    }
    PngReader r = { data, size, 0 };
    png_set_read_fn(png, &r, png_read_mem);
    // Nhiều file PNG trong game J2ME bị sai CRC: bỏ qua
    png_set_crc_action(png, PNG_CRC_QUIET_USE, PNG_CRC_QUIET_USE);
    png_read_info(png, info);

    png_uint_32 w, h;
    int depth, color;
    png_get_IHDR(png, info, &w, &h, &depth, &color, NULL, NULL, NULL);
    if (w == 0 || h == 0 || w > 8192 || h > 8192)
        png_error(png, "size");

    png_set_expand(png);
    png_set_strip_16(png);
    if (color == PNG_COLOR_TYPE_GRAY || color == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png);
    if (!(color & PNG_COLOR_MASK_ALPHA) && !png_get_valid(png, info, PNG_INFO_tRNS))
        png_set_add_alpha(png, 0xff, PNG_FILLER_AFTER);
    png_set_bgr(png);
    png_set_interlace_handling(png);
    png_read_update_info(png, info);

    pixels = malloc((size_t)w * h * 4);
    rows = malloc(sizeof(png_bytep) * h);
    for (png_uint_32 i = 0; i < h; i++)
        rows[i] = (png_bytep)(pixels + (size_t)i * w);
    png_read_image(png, rows);
    png_destroy_read_struct(&png, &info, NULL);
    free(rows);
    *out_w = (int)w;
    *out_h = (int)h;
    return pixels;
}

static NativeResult Image_decode0(VMThread *t, Value *args, Value *ret) {
    init_slots();
    Object *self = args[0].l, *data = args[1].l;
    jint off = args[2].i, len = args[3].i;
    if (!data || off < 0 || len < 0 || off + len > ARRAY_LEN(data)) {
        ret->i = 0;
        return NATIVE_OK;
    }
    int w = 0, h = 0;
    const uint8_t *bytes = ARRAY_DATA(data, uint8_t) + off;
    uint32_t *px = decode_png(bytes, (size_t)len, &w, &h);
    if (!px) {
        // JPEG / GIF / BMP (hoặc PNG mà libpng từ chối)
        int comp;
        uint8_t *rgba = stbi_load_from_memory(bytes, len, &w, &h, &comp, 4);
        if (rgba) {
            px = malloc((size_t)w * h * 4);
            for (int i = 0; i < w * h; i++)
                px[i] = ((uint32_t)rgba[i * 4 + 3] << 24) | ((uint32_t)rgba[i * 4] << 16) |
                        ((uint32_t)rgba[i * 4 + 1] << 8) | rgba[i * 4 + 2];
            stbi_image_free(rgba);
        }
    }
    if (!px) {
        const uint8_t *d = ARRAY_DATA(data, uint8_t) + off;
        vm_log("Khong doc duoc anh (%d byte, dau %02x %02x %02x %02x)", len, len > 0 ? d[0] : 0,
               len > 1 ? d[1] : 0, len > 2 ? d[2] : 0, len > 3 ? d[3] : 0);
        ret->i = 0;
        return NATIVE_OK;
    }
    Object *arr = heap_new_prim_array(t, 'I', w * h);
    if (!arr) {
        free(px);
        return NATIVE_EXCEPTION;
    }
    memcpy(ARRAY_DATA(arr, uint32_t), px, (size_t)w * h * 4);
    free(px);
    FIELD_L(self, I_pixels) = arr;
    FIELD_I(self, I_width) = w;
    FIELD_I(self, I_height) = h;
    FIELD_I(self, I_mutable) = 0;
    // Kiểm tra ảnh có pixel trong suốt không
    int opaque = 1;
    const uint32_t *pp = ARRAY_DATA(arr, uint32_t);
    for (int i = 0; i < w * h; i++) {
        if ((pp[i] >> 24) != 255) {
            opaque = 0;
            break;
        }
    }
    FIELD_I(self, I_opaque) = opaque;
    ret->i = 1;
    return NATIVE_OK;
}

// ---------------------------------------------------------------------------
// Font & chữ

// Cỡ font (pt) cho SMALL / MEDIUM / LARGE
static const int font_pt[3] = { 11, 13, 16 };
// Chữ mịn (khử răng cưa) cho ứng dụng nhiều chữ như Opera Mini; tắt thì vẽ chữ điểm ảnh như điện thoại thật
static bool smooth_text;
static int font_scale = 100;        // % so với cỡ gốc
// Font hệ thống làm font chính (font nhúng chỉ bù ký tự thiếu); tắt thì ngược lại
static bool system_font;

// Mỗi font J2ME là một chuỗi font: ký tự nào font đầu không có thì lấy ở font sau (chữ Trung/Nhật/Hàn
// trong font hệ thống, chữ Việt trong font nhúng...). Font sau chỉ mở khi cần tới.
#define FONT_EMBEDDED (-1)
#define MAX_CHAIN 16

typedef struct {
    TTF_Font *f[MAX_CHAIN];
    uint8_t state[MAX_CHAIN];       // 0 chưa mở, 1 đã mở, 2 không có
} FontChain;

static FontChain chains[12];
static FontChain chains_hi[12];     // cỡ x hires_k cho lớp chữ nét cao
static int chain_src[MAX_CHAIN];    // nguồn của từng vị trí: FONT_EMBEDDED hoặc chỉ số font hệ thống
static int chain_len;

void midp_graphics_set_text_style(bool smooth, int scale_pct, bool sys_font, int hires, int screen_w,
                                  int screen_h) {
    smooth_text = smooth;
    font_scale = scale_pct > 0 ? scale_pct : 100;
    system_font = sys_font;
    hires_k = smooth && hires >= 2 ? hires : 0;
    layer_w = screen_w;
    layer_h = screen_h;

    int n = platform_system_font_count();
    if (n > MAX_CHAIN - 1)
        n = MAX_CHAIN - 1;
    chain_len = 0;
    if (!system_font)
        chain_src[chain_len++] = FONT_EMBEDDED;
    for (int i = 0; i < n; i++)
        chain_src[chain_len++] = i;
    if (system_font)
        chain_src[chain_len++] = FONT_EMBEDDED;
}

static TTF_Font *open_font(int src, int key, int mul) {
    int pt = (font_pt[key / 4] * font_scale + 50) / 100;
    pt = (pt < 6 ? 6 : pt) * mul;
    TTF_Font *f = src == FONT_EMBEDDED ? platform_open_font(pt) : platform_open_system_font(src, pt);
    if (f) {
        int style = TTF_STYLE_NORMAL;
        if (key & 1)
            style |= TTF_STYLE_BOLD;
        if (key & 2)
            style |= TTF_STYLE_ITALIC;
        TTF_SetFontStyle(f, style);
        if (!smooth_text) {
            // Chữ điểm ảnh: căn lưới đơn sắc, không khử răng cưa
            TTF_SetFontHinting(f, TTF_HINTING_MONO);
        } else {
            // Hinting nhẹ: giữ dáng chữ tròn như thiết kế
#ifdef TTF_HINTING_LIGHT_SUBPIXEL
            TTF_SetFontHinting(f, mul > 1 ? TTF_HINTING_LIGHT_SUBPIXEL : TTF_HINTING_LIGHT);
#else
            TTF_SetFontHinting(f, TTF_HINTING_LIGHT);
#endif
        }
    }
    return f;
}

static int font_key(int key) {
    return key < 0 || key >= 12 ? 4 : key;
}

static FontChain *get_chain(int key, bool hi) {
    return &(hi ? chains_hi : chains)[font_key(key)];
}

// Font thứ i trong chuỗi (mở lười); NULL nếu không có
static TTF_Font *chain_font(FontChain *ch, int key, bool hi, int i) {
    if (ch->state[i] == 0) {
        ch->f[i] = open_font(chain_src[i], font_key(key), hi ? hires_k : 1);
        ch->state[i] = ch->f[i] ? 1 : 2;
    }
    return ch->f[i];
}

// Font chính (đầu tiên mở được): dùng cho chiều cao, đường chân chữ
static int chain_primary(FontChain *ch, int key, bool hi) {
    for (int i = 0; i < chain_len; i++) {
        if (chain_font(ch, key, hi, i))
            return i;
    }
    return -1;
}

static TTF_Font *get_font(int key) {
    FontChain *ch = get_chain(key, false);
    int i = chain_primary(ch, key, false);
    return i < 0 ? NULL : ch->f[i];
}

static uint32_t utf8_next(const char **p) {
    const uint8_t *s = (const uint8_t *)*p;
    uint32_t c = s[0];
    int n = c < 0x80 ? 0 : c >= 0xf0 ? 3 : c >= 0xe0 ? 2 : c >= 0xc0 ? 1 : -1;
    if (n < 0) {
        *p += 1;
        return 0xfffd;
    }
    c &= n ? 0x3f >> n : 0x7f;
    for (int i = 1; i <= n; i++) {
        if ((s[i] & 0xc0) != 0x80) {
            *p += i;
            return 0xfffd;
        }
        c = (c << 6) | (s[i] & 0x3f);
    }
    *p += n + 1;
    return c;
}

// Đoạn chuỗi vẽ cùng một font
typedef struct {
    int start, end;
    int font;
} TextRun;

#define MAX_RUNS 64

// Chia chuỗi theo font có ký tự. Ký tự font đang dùng cũng có thì giữ font đó (đỡ đổi font giữa chừng);
// không font nào có thì dùng font chính. Trả về số đoạn.
static int split_runs(FontChain *ch, int key, bool hi, const char *utf8, TextRun *runs) {
    int primary = chain_primary(ch, key, hi);
    if (primary < 0)
        return 0;
    int n = 0, cur = -1;
    const char *p = utf8;
    while (*p) {
        int at = (int)(p - utf8);
        uint32_t cp = utf8_next(&p);
        int font = cur;
        if (font < 0 || !TTF_GlyphIsProvided32(ch->f[font], cp)) {
            font = primary;
            for (int i = 0; i < chain_len; i++) {
                TTF_Font *f = chain_font(ch, key, hi, i);
                if (f && TTF_GlyphIsProvided32(f, cp)) {
                    font = i;
                    break;
                }
            }
        }
        if (n > 0 && (font == cur || n == MAX_RUNS)) {
            runs[n - 1].end = (int)(p - utf8);
            continue;
        }
        runs[n++] = (TextRun){ at, (int)(p - utf8), font };
        cur = font;
    }
    return n;
}

// Độ rộng chuỗi (pixel), tính theo từng đoạn font
static int text_width(int key, const char *utf8) {
    FontChain *ch = get_chain(key, false);
    TextRun runs[MAX_RUNS];
    int n = split_runs(ch, key, false, utf8, runs);
    int total = 0, h = 0;
    if (n == 1)
        return TTF_SizeUTF8(ch->f[runs[0].font], utf8, &total, &h) == 0 ? total : 0;
    char buf[1024];
    for (int i = 0; i < n; i++) {
        int len = runs[i].end - runs[i].start;
        const char *s = utf8 + runs[i].start;
        char *tmp = len < (int)sizeof(buf) ? buf : malloc((size_t)len + 1);
        if (!tmp)
            continue;
        memcpy(tmp, s, (size_t)len);
        tmp[len] = '\0';
        int w = 0;
        if (TTF_SizeUTF8(ch->f[runs[i].font], tmp, &w, &h) == 0)
            total += w;
        if (tmp != buf)
            free(tmp);
    }
    return total;
}

// Cache mặt nạ alpha của chuỗi đã render
#define TEXT_CACHE 256

typedef struct {
    uint32_t hash;
    int key;
    char *text;
    int w, h;
    int base;                       // hàng của đường chân chữ trong mặt nạ
    uint8_t *alpha;
} TextMask;

static TextMask text_cache[TEXT_CACHE];
static TextMask text_cache_hi[TEXT_CACHE];

static uint32_t text_hash(const char *s, int key) {
    uint32_t h = 2166136261u ^ (uint32_t)key;
    for (; *s; s++)
        h = (h ^ (uint8_t)*s) * 16777619u;
    return h;
}

static SDL_Surface *render_run(TTF_Font *f, const char *utf8) {
    SDL_Color white = { 255, 255, 255, 255 };
    SDL_Surface *s = smooth_text ? TTF_RenderUTF8_Blended(f, utf8, white) : TTF_RenderUTF8_Solid(f, utf8, white);
    if (!s)
        return NULL;
    SDL_Surface *conv = SDL_ConvertSurfaceFormat(s, SDL_PIXELFORMAT_ARGB8888, 0);
    SDL_FreeSurface(s);
    return conv;
}

static TextMask *get_mask(int key, const char *utf8, bool hi) {
    uint32_t h = text_hash(utf8, key);
    TextMask *m = &(hi ? text_cache_hi : text_cache)[h % TEXT_CACHE];
    if (m->text && m->hash == h && m->key == key && strcmp(m->text, utf8) == 0)
        return m;

    FontChain *ch = get_chain(key, hi);
    TextRun runs[MAX_RUNS];
    int n = split_runs(ch, key, hi, utf8, runs);
    if (n == 0)
        return NULL;
    TTF_Font *primary = ch->f[chain_primary(ch, key, hi)];

    // Render từng đoạn rồi xếp nối nhau, cùng đường chân chữ
    SDL_Surface *surf[MAX_RUNS];
    int asc[MAX_RUNS];
    int base = TTF_FontAscent(primary), below = TTF_FontHeight(primary) - base, w = 0;
    char buf[1024];
    for (int i = 0; i < n; i++) {
        TTF_Font *f = ch->f[runs[i].font];
        if (n == 1) {
            surf[i] = render_run(f, utf8);
        } else {
            int len = runs[i].end - runs[i].start;
            char *tmp = len < (int)sizeof(buf) ? buf : malloc((size_t)len + 1);
            surf[i] = NULL;
            if (!tmp)
                continue;
            memcpy(tmp, utf8 + runs[i].start, (size_t)len);
            tmp[len] = '\0';
            surf[i] = render_run(f, tmp);
            if (tmp != buf)
                free(tmp);
        }
        if (!surf[i])
            continue;
        asc[i] = TTF_FontAscent(f);
        if (asc[i] > base)
            base = asc[i];
        if (surf[i]->h - asc[i] > below)
            below = surf[i]->h - asc[i];
        w += surf[i]->w;
    }
    int mh = base + below;
    uint8_t *alpha = w > 0 && mh > 0 ? calloc((size_t)w * mh, 1) : NULL;
    int x = 0;
    for (int i = 0; i < n; i++) {
        SDL_Surface *c = surf[i];
        if (!c)
            continue;
        if (alpha) {
            int oy = base - asc[i];
            SDL_LockSurface(c);
            for (int y = 0; y < c->h; y++) {
                const uint32_t *row = (const uint32_t *)((const uint8_t *)c->pixels + y * c->pitch);
                uint8_t *d = alpha + (size_t)(y + oy) * w + x;
                for (int k = 0; k < c->w; k++)
                    d[k] = (uint8_t)(row[k] >> 24);
            }
            SDL_UnlockSurface(c);
        }
        x += c->w;
        SDL_FreeSurface(c);
    }
    if (!alpha)
        return NULL;

    free(m->text);
    free(m->alpha);
    m->hash = h;
    m->key = key;
    m->text = strdup(utf8);
    m->w = w;
    m->h = mh;
    m->base = base;
    m->alpha = alpha;
    return m;
}

static int floor_div(int a, int k) {
    return a >= 0 ? a / k : -((-a + k - 1) / k);
}

// Vẽ bản nét cao của chuỗi vào lớp chữ (trước khi vẽ bản thường); by = đường chân chữ. Trả về vùng pixel
// thấp bị ảnh hưởng qua r[4] để gắn tag sau khi vẽ bản thường; false nếu không vẽ được.
static bool layer_text(TextLayer *L, Ctx *c, int key, const char *utf8, int x, int by, const TextMask *m,
                       int r[4]) {
    TextMask *hm = get_mask(key, utf8, true);
    if (!hm)
        return false;
    int k = hires_k, y = by - m->base;
    // Khớp đường chân chữ của bản thường
    int hx = x * k, hy = by * k - hm->base;
    int x0 = floor_div(hx, k), y0 = floor_div(hy, k);
    int x1 = floor_div(hx + hm->w + k - 1, k), y1 = floor_div(hy + hm->h + k - 1, k);
    if (x < x0) x0 = x;
    if (y < y0) y0 = y;
    if (x + m->w > x1) x1 = x + m->w;
    if (y + m->h > y1) y1 = y + m->h;
    if (x0 < c->cx0) x0 = c->cx0;
    if (y0 < c->cy0) y0 = c->cy0;
    if (x1 > c->cx1) x1 = c->cx1;
    if (y1 > c->cy1) y1 = c->cy1;
    if (x0 >= x1 || y0 >= y1)
        return false;

    // Pixel chưa có bản nét cao (hoặc đã bị ghi đè): lấy nền từ pixel thấp hiện tại
    for (int yy = y0; yy < y1; yy++) {
        for (int xx = x0; xx < x1; xx++) {
            int i = yy * L->w + xx;
            if (!L->valid[i] || L->tag[i] != c->px[i]) {
                fill_block(L, xx, yy, c->px[i]);
                L->valid[i] = 1;
            }
        }
    }

    uint32_t rgb = c->color & 0xffffff, ca = c->color >> 24;
    int hw = L->w * k;
    int ys = y0 * k > hy ? y0 * k : hy, ye = y1 * k < hy + hm->h ? y1 * k : hy + hm->h;
    int xs = x0 * k > hx ? x0 * k : hx, xe = x1 * k < hx + hm->w ? x1 * k : hx + hm->w;
    for (int yy = ys; yy < ye; yy++) {
        const uint8_t *a = hm->alpha + (yy - hy) * hm->w;
        uint32_t *d = L->hi + (size_t)yy * hw;
        for (int xx = xs; xx < xe; xx++) {
            uint32_t al = a[xx - hx] * ca / 255;
            if (al)
                d[xx] = blend(d[xx], (al << 24) | rgb);
        }
    }
    r[0] = x0; r[1] = y0; r[2] = x1; r[3] = y1;
    return true;
}

static NativeResult G_drawStringImpl(VMThread *t, Value *args, Value *ret) {
    (void)ret;
    Object *g = args[0].l, *str = args[1].l;
    if (!str) {
        throw_null(t);
        return NATIVE_EXCEPTION;
    }
    if (jstring_length(str) == 0)
        return NATIVE_OK;
    Ctx c;
    if (!ctx_init(g, &c))
        return NATIVE_OK;
    int key = FIELD_I(FIELD_L(g, G_font), F_key);
    char *utf8 = jstring_to_utf8(str);
    TextMask *m = get_mask(key, utf8, false);
    if (!m) {
        free(utf8);
        return NATIVE_OK;
    }

    // y là cạnh trên của dòng theo font chính; mặt nạ có thể cao hơn (font dự phòng) nên căn theo chân chữ
    TTF_Font *primary = get_font(key);
    int x = args[2].i + c.tx, by = args[3].i + c.ty + (primary ? TTF_FontAscent(primary) : m->base);
    int y = by - m->base;
    TextLayer *L = NULL;
    int r[4];
    if (hires_k >= 2) {
        L = c.layer ? c.layer : layer_create(FIELD_L(c.img, I_pixels), c.w, c.h);
        if (L && !layer_text(L, &c, key, utf8, x, by, m, r))
            L = NULL;
    }
    free(utf8);

    uint32_t rgb = c.color & 0xffffff;
    uint32_t ca = c.color >> 24;
    int y0 = y < c.cy0 ? c.cy0 : y, y1 = y + m->h > c.cy1 ? c.cy1 : y + m->h;
    int x0 = x < c.cx0 ? c.cx0 : x, x1 = x + m->w > c.cx1 ? c.cx1 : x + m->w;
    for (int yy = y0; yy < y1; yy++) {
        const uint8_t *a = m->alpha + (yy - y) * m->w;
        uint32_t *d = c.px + yy * c.w;
        for (int xx = x0; xx < x1; xx++) {
            uint32_t al = a[xx - x] * ca / 255;
            if (al)
                d[xx] = blend(d[xx], (al << 24) | rgb);
        }
    }

    if (L) {
        for (int yy = r[1]; yy < r[3]; yy++)
            memcpy(L->tag + yy * L->w + r[0], c.px + yy * c.w + r[0], (size_t)(r[2] - r[0]) * 4);
        layer_grow(L, r[0], r[1], r[2], r[3]);
    }
    return NATIVE_OK;
}

// Ghép khung hình nét cao (k lần) của mảng pixel arr vào out; false nếu ảnh không có chữ nét cao
bool midp_text_compose(void *arr, const uint32_t *px, int w, int h, uint32_t *out) {
    TextLayer *L = layer_find(arr);
    if (!L || L->bx0 >= L->bx1 || L->w != w || L->h != h)
        return false;
    int k = hires_k, ow = w * k;
    int vx0 = w, vy0 = h, vx1 = 0, vy1 = 0;
    for (int y = 0; y < h; y++) {
        uint32_t *o = out + (size_t)y * k * ow;
        const uint32_t *s = px + y * w;
        for (int x = 0; x < w; x++)
            for (int dx = 0; dx < k; dx++)
                o[x * k + dx] = s[x];
        for (int dy = 1; dy < k; dy++)
            memcpy(o + dy * ow, o, (size_t)ow * 4);
        if (y < L->by0 || y >= L->by1)
            continue;
        for (int x = L->bx0; x < L->bx1; x++) {
            int i = y * w + x;
            if (!L->valid[i])
                continue;
            if (L->tag[i] != s[x]) {
                L->valid[i] = 0;
                continue;
            }
            const uint32_t *b = L->hi + (size_t)y * k * ow + x * k;
            for (int dy = 0; dy < k; dy++)
                memcpy(o + dy * ow + x * k, b + dy * ow, (size_t)k * 4);
            if (x < vx0) vx0 = x;
            if (x >= vx1) vx1 = x + 1;
            if (y < vy0) vy0 = y;
            if (y >= vy1) vy1 = y + 1;
        }
    }
    // Thu khung bao về các pixel còn đúng
    if (vx0 < vx1) {
        L->bx0 = vx0; L->by0 = vy0; L->bx1 = vx1; L->by1 = vy1;
    } else {
        L->bx0 = L->by0 = L->bx1 = L->by1 = 0;
    }
    return true;
}

static NativeResult Font_height0(VMThread *t, Value *args, Value *ret) {
    (void)t;
    TTF_Font *f = get_font(args[0].i);
    ret->i = f ? TTF_FontHeight(f) : 14;
    return NATIVE_OK;
}

static NativeResult Font_baseline0(VMThread *t, Value *args, Value *ret) {
    (void)t;
    TTF_Font *f = get_font(args[0].i);
    ret->i = f ? TTF_FontAscent(f) : 11;
    return NATIVE_OK;
}

static NativeResult Font_stringWidth0(VMThread *t, Value *args, Value *ret) {
    Object *str = args[1].l;
    if (!str) {
        throw_null(t);
        return NATIVE_EXCEPTION;
    }
    ret->i = 0;
    if (jstring_length(str) == 0)
        return NATIVE_OK;
    char *utf8 = jstring_to_utf8(str);
    ret->i = text_width(args[0].i, utf8);
    free(utf8);
    return NATIVE_OK;
}

// ---------------------------------------------------------------------------

void midp_graphics_register(void) {
    const char *G = "javax/microedition/lcdui/Graphics";
    native_register(G, "drawLine", "(IIII)V", G_drawLine);
    native_register(G, "fillRect", "(IIII)V", G_fillRect);
    native_register(G, "drawRoundRect", "(IIIIII)V", G_drawRoundRect);
    native_register(G, "fillRoundRect", "(IIIIII)V", G_fillRoundRect);
    native_register(G, "fillArc", "(IIIIII)V", G_fillArc);
    native_register(G, "drawArc", "(IIIIII)V", G_drawArc);
    native_register(G, "fillTriangle", "(IIIIII)V", G_fillTriangle);
    native_register(G, "drawStringImpl", "(Ljava/lang/String;II)V", G_drawStringImpl);
    native_register(G, "drawRegionImpl", "(Ljavax/microedition/lcdui/Image;IIIIIIIZ)V", G_drawRegionImpl);
    native_register(G, "drawRGB", "([IIIIIIIZ)V", G_drawRGB);

    native_register("javax/microedition/lcdui/Image", "decode0", "([BII)Z", Image_decode0);

    const char *F = "javax/microedition/lcdui/Font";
    native_register(F, "height0", "(I)I", Font_height0);
    native_register(F, "baseline0", "(I)I", Font_baseline0);
    native_register(F, "stringWidth0", "(ILjava/lang/String;)I", Font_stringWidth0);
}

void midp_graphics_shutdown(void) {
    slots_ready = false;
    for (int i = 0; i < TEXT_CACHE; i++) {
        free(text_cache[i].text);
        free(text_cache[i].alpha);
        free(text_cache_hi[i].text);
        free(text_cache_hi[i].alpha);
    }
    memset(text_cache, 0, sizeof(text_cache));
    memset(text_cache_hi, 0, sizeof(text_cache_hi));
    for (int i = 0; i < 12; i++) {
        for (int j = 0; j < MAX_CHAIN; j++) {
            if (chains[i].f[j])
                TTF_CloseFont(chains[i].f[j]);
            if (chains_hi[i].f[j])
                TTF_CloseFont(chains_hi[i].f[j]);
        }
    }
    memset(chains, 0, sizeof(chains));
    memset(chains_hi, 0, sizeof(chains_hi));
    for (int i = 0; i < MAX_LAYERS; i++)
        layer_free(&layers[i]);
    layer_clock = 0;
}
