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
static int I_pixels, I_width, I_height, I_mutable;
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
    F_key = field_slot("javax/microedition/lcdui/Font", "key", "I");
    slots_ready = true;
}

typedef struct {
    uint32_t *px;
    int w, h;
    int cx0, cy0, cx1, cy1;     // vùng clip [cx0, cx1) x [cy0, cy1)
    int tx, ty;
    uint32_t color;
    bool dotted;
} Ctx;

static bool ctx_init(Object *g, Ctx *c) {
    init_slots();
    Object *img = FIELD_L(g, G_img);
    Object *arr = FIELD_L(img, I_pixels);
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
}

static void hspan(Ctx *c, int x0, int x1, int y) {
    if (y < c->cy0 || y >= c->cy1)
        return;
    if (x0 < c->cx0) x0 = c->cx0;
    if (x1 > c->cx1) x1 = c->cx1;
    if (x0 >= x1)
        return;
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
    int r0 = y < c.cy0 ? c.cy0 - y : 0, r1 = y + h > c.cy1 ? c.cy1 - y : h;
    int k0 = x < c.cx0 ? c.cx0 - x : 0, k1 = x + w > c.cx1 ? c.cx1 - x : w;
    for (int r = r0; r < r1; r++) {
        const uint32_t *s = src + off + (int64_t)r * scan;
        uint32_t *d = c.px + (y + r) * c.w + x;
        for (int k = k0; k < k1; k++)
            d[k] = alpha ? blend(d[k], s[k]) : (s[k] | 0xff000000u);
    }
    return NATIVE_OK;
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

    if (tr == 0) {
        for (int y = y0; y < y1; y++) {
            const uint32_t *s = sp + (sy + y - dy) * sw + sx + (x0 - dx);
            uint32_t *d = c.px + y * c.w + x0;
            int n = x1 - x0;
            if (copy) {
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
            if (copy)
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
    ret->i = 1;
    return NATIVE_OK;
}

// ---------------------------------------------------------------------------
// Font & chữ

// Cỡ font (pt) cho SMALL / MEDIUM / LARGE
static const int font_pt[3] = { 11, 13, 16 };
static TTF_Font *fonts[12];

static TTF_Font *get_font(int key) {
    if (key < 0 || key >= 12)
        key = 4;
    if (!fonts[key]) {
        fonts[key] = platform_open_font(font_pt[key / 4]);
        if (fonts[key]) {
            int style = TTF_STYLE_NORMAL;
            if (key & 1)
                style |= TTF_STYLE_BOLD;
            if (key & 2)
                style |= TTF_STYLE_ITALIC;
            TTF_SetFontStyle(fonts[key], style);
        }
    }
    return fonts[key];
}

// Cache mặt nạ alpha của chuỗi đã render
#define TEXT_CACHE 256

typedef struct {
    uint32_t hash;
    int key;
    char *text;
    int w, h;
    uint8_t *alpha;
} TextMask;

static TextMask text_cache[TEXT_CACHE];

static uint32_t text_hash(const char *s, int key) {
    uint32_t h = 2166136261u ^ (uint32_t)key;
    for (; *s; s++)
        h = (h ^ (uint8_t)*s) * 16777619u;
    return h;
}

static TextMask *get_mask(int key, const char *utf8) {
    uint32_t h = text_hash(utf8, key);
    TextMask *m = &text_cache[h % TEXT_CACHE];
    if (m->text && m->hash == h && m->key == key && strcmp(m->text, utf8) == 0)
        return m;

    TTF_Font *f = get_font(key);
    if (!f)
        return NULL;
    SDL_Color white = { 255, 255, 255, 255 };
    SDL_Surface *s = TTF_RenderUTF8_Blended(f, utf8, white);
    if (!s)
        return NULL;
    SDL_Surface *conv = SDL_ConvertSurfaceFormat(s, SDL_PIXELFORMAT_ARGB8888, 0);
    SDL_FreeSurface(s);
    if (!conv)
        return NULL;

    free(m->text);
    free(m->alpha);
    m->hash = h;
    m->key = key;
    m->text = strdup(utf8);
    m->w = conv->w;
    m->h = conv->h;
    m->alpha = malloc((size_t)conv->w * conv->h);
    SDL_LockSurface(conv);
    for (int y = 0; y < conv->h; y++) {
        const uint32_t *row = (const uint32_t *)((const uint8_t *)conv->pixels + y * conv->pitch);
        for (int x = 0; x < conv->w; x++)
            m->alpha[y * conv->w + x] = (uint8_t)(row[x] >> 24);
    }
    SDL_UnlockSurface(conv);
    SDL_FreeSurface(conv);
    return m;
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
    TextMask *m = get_mask(key, utf8);
    free(utf8);
    if (!m)
        return NATIVE_OK;

    int x = args[2].i + c.tx, y = args[3].i + c.ty;
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
    return NATIVE_OK;
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
    TTF_Font *f = get_font(args[0].i);
    char *utf8 = jstring_to_utf8(str);
    int w = 0, h = 0;
    if (f && TTF_SizeUTF8(f, utf8, &w, &h) == 0)
        ret->i = w;
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
    }
    memset(text_cache, 0, sizeof(text_cache));
    for (int i = 0; i < 12; i++) {
        if (fonts[i])
            TTF_CloseFont(fonts[i]);
        fonts[i] = NULL;
    }
}
