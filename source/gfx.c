#include "gfx.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL_ttf.h>

#include "platform.h"

// Cache texture của chữ đã render. Direct-mapped theo hash, trùng slot thì ghi đè.
#define TEXT_CACHE_SIZE 256

typedef struct {
    Uint32 hash;
    char *text;
    FontId font;
    Uint32 color;
    SDL_Texture *tex;
    int w, h;
} TextCacheEntry;

static const int font_sizes[FONT_COUNT] = {
    [FONT_SMALL]  = 20,
    [FONT_NORMAL] = 26,
    [FONT_LARGE]  = 38,
};

// Cỡ chữ trong icon tự vẽ
static int label_size(FontId font) {
    return font_sizes[font] * 2 / 3;
}

static SDL_Window *window;
static SDL_Renderer *renderer;
static TTF_Font *fonts[FONT_COUNT];
static TTF_Font *icon_fonts[FONT_COUNT];   // NintendoExt, NULL trên desktop
static TTF_Font *label_fonts[FONT_COUNT];  // chữ nhỏ trong icon tự vẽ
static TextCacheEntry text_cache[TEXT_CACHE_SIZE];
static SDL_Texture *circle_tex;             // hình tròn trắng khử răng cưa, cho góc bo và hình tròn

bool gfx_init(const char *title) {
    window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              SCREEN_W, SCREEN_H, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window) {
        printf("SDL_CreateWindow: %s\n", SDL_GetError());
        return false;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        printf("SDL_CreateRenderer: %s\n", SDL_GetError());
        return false;
    }
    // Luôn vẽ theo toạ độ 1280x720, SDL tự scale theo kích thước cửa sổ
    SDL_RenderSetLogicalSize(renderer, SCREEN_W, SCREEN_H);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    if (TTF_Init() < 0) {
        printf("TTF_Init: %s\n", TTF_GetError());
        return false;
    }
    for (int i = 0; i < FONT_COUNT; i++) {
        fonts[i] = platform_open_font(font_sizes[i]);
        if (!fonts[i]) {
            printf("Khong mo duoc font size %d: %s\n", font_sizes[i], TTF_GetError());
            return false;
        }
        icon_fonts[i] = platform_open_icon_font(font_sizes[i]);
        label_fonts[i] = platform_open_font(label_size(i));
    }
    return true;
}

void gfx_exit(void) {
    for (int i = 0; i < TEXT_CACHE_SIZE; i++) {
        if (text_cache[i].tex)
            SDL_DestroyTexture(text_cache[i].tex);
        free(text_cache[i].text);
    }
    memset(text_cache, 0, sizeof(text_cache));
    if (circle_tex)
        SDL_DestroyTexture(circle_tex);
    circle_tex = NULL;

    for (int i = 0; i < FONT_COUNT; i++) {
        if (fonts[i])
            TTF_CloseFont(fonts[i]);
        if (icon_fonts[i])
            TTF_CloseFont(icon_fonts[i]);
        if (label_fonts[i])
            TTF_CloseFont(label_fonts[i]);
        fonts[i] = icon_fonts[i] = label_fonts[i] = NULL;
    }
    if (TTF_WasInit())
        TTF_Quit();

    if (renderer)
        SDL_DestroyRenderer(renderer);
    if (window)
        SDL_DestroyWindow(window);
    renderer = NULL;
    window = NULL;
}

SDL_Renderer *gfx_renderer(void) {
    return renderer;
}

void gfx_clear(SDL_Color c) {
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
    SDL_RenderClear(renderer);
}

void gfx_present(void) {
    SDL_RenderPresent(renderer);
}

void gfx_fill_rect(int x, int y, int w, int h, SDL_Color c) {
    SDL_Rect r = { x, y, w, h };
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
    SDL_RenderFillRect(renderer, &r);
}

#define CIRCLE_TEX 256

static SDL_Texture *circle(void) {
    if (circle_tex)
        return circle_tex;
    static uint32_t px[CIRCLE_TEX * CIRCLE_TEX];
    float r = CIRCLE_TEX / 2.0f;
    for (int y = 0; y < CIRCLE_TEX; y++) {
        for (int x = 0; x < CIRCLE_TEX; x++) {
            float dx = x + 0.5f - r, dy = y + 0.5f - r;
            float cov = r - sqrtf(dx * dx + dy * dy) + 0.5f;
            cov = cov < 0 ? 0 : cov > 1 ? 1 : cov;
            px[y * CIRCLE_TEX + x] = ((uint32_t)(cov * 255 + 0.5f) << 24) | 0xFFFFFF;
        }
    }
    circle_tex = gfx_texture_argb(px, CIRCLE_TEX, CIRCLE_TEX);
    if (circle_tex)
        SDL_SetTextureScaleMode(circle_tex, SDL_ScaleModeLinear);
    return circle_tex;
}

// Một phần tư hình tròn (qx, qy: 0 = nửa trái / trên, 1 = nửa phải / dưới) vào ô r x r
static void quarter(SDL_Texture *t, int x, int y, int r, int qx, int qy) {
    int h = CIRCLE_TEX / 2;
    SDL_Rect src = { qx * h, qy * h, h, h };
    SDL_Rect d = { x, y, r, r };
    SDL_RenderCopy(renderer, t, &src, &d);
}

void gfx_fill_round_rect(int x, int y, int w, int h, int r, SDL_Color c) {
    if (r > w / 2)
        r = w / 2;
    if (r > h / 2)
        r = h / 2;
    SDL_Texture *t = r > 0 ? circle() : NULL;
    if (!t) {
        gfx_fill_rect(x, y, w, h, c);
        return;
    }
    SDL_SetTextureColorMod(t, c.r, c.g, c.b);
    SDL_SetTextureAlphaMod(t, c.a);
    quarter(t, x, y, r, 0, 0);
    quarter(t, x + w - r, y, r, 1, 0);
    quarter(t, x, y + h - r, r, 0, 1);
    quarter(t, x + w - r, y + h - r, r, 1, 1);
    // Các mảnh không chồng nhau để màu trong suốt đều
    gfx_fill_rect(x + r, y, w - 2 * r, h, c);
    gfx_fill_rect(x, y + r, r, h - 2 * r, c);
    gfx_fill_rect(x + w - r, y + r, r, h - 2 * r, c);
}

void gfx_fill_circle(int cx, int cy, int r, SDL_Color c) {
    SDL_Texture *t = circle();
    if (!t)
        return;
    SDL_SetTextureColorMod(t, c.r, c.g, c.b);
    SDL_SetTextureAlphaMod(t, c.a);
    SDL_Rect d = { cx - r, cy - r, 2 * r, 2 * r };
    SDL_RenderCopy(renderer, t, NULL, &d);
}

SDL_Texture *gfx_texture_argb(const uint32_t *pixels, int w, int h) {
    SDL_Texture *t = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, w, h);
    if (!t)
        return NULL;
    SDL_UpdateTexture(t, NULL, pixels, w * 4);
    SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    return t;
}

void gfx_draw_texture(SDL_Texture *tex, int x, int y, int w, int h) {
    SDL_Rect r = { x, y, w, h };
    SDL_RenderCopy(renderer, tex, NULL, &r);
}

int gfx_font_height(FontId font) {
    return TTF_FontHeight(fonts[font]);
}

// Icon nút: ký tự vùng riêng U+E000..U+F8FF (3 byte UTF-8), trả về 0 nếu không phải
static Uint16 icon_at(const char *s) {
    const unsigned char *u = (const unsigned char *)s;
    if ((u[0] & 0xF0) != 0xE0 || (u[1] & 0xC0) != 0x80 || (u[2] & 0xC0) != 0x80)
        return 0;
    Uint16 cp = (Uint16)(((u[0] & 0x0F) << 12) | ((u[1] & 0x3F) << 6) | (u[2] & 0x3F));
    return cp >= 0xE000 && cp <= 0xF8FF ? cp : 0;
}

#define ICON_BYTES 3

// Độ dài đoạn chữ thường tính từ s, tới icon kế tiếp hoặc hết chuỗi
static size_t plain_len(const char *s) {
    size_t n = 0;
    while (s[n] && !icon_at(s + n))
        n++;
    return n;
}

static bool has_icon(const char *s) {
    return s[plain_len(s)] != '\0';
}

static bool icon_in_font(FontId font, Uint16 cp) {
    return icon_fonts[font] && TTF_GlyphIsProvided(icon_fonts[font], cp);
}

// Chữ trong icon tự vẽ (khi máy không có font NintendoExt)
static const char *icon_label(Uint16 cp) {
    switch (cp) {
    case 0xE0E0: return "A";
    case 0xE0E1: return "B";
    case 0xE0E2: return "X";
    case 0xE0E3: return "Y";
    case 0xE0A4: return "L";
    case 0xE0A5: return "R";
    case 0xE0A6: return "ZL";
    case 0xE0A7: return "ZR";
    case 0xE0B5: return "+";
    case 0xE0B6: return "-";
    case 0xE079: return "^";
    case 0xE07A: return "v";
    case 0xE07B: return "<";
    case 0xE07C: return ">";
    default:     return "?";
    }
}

// Icon tự vẽ: viên tròn (hoặc viên thuốc nếu chữ dài) màu chữ, chữ khoét rỗng ở giữa
#define ICON_PAD 2

static int drawn_icon_shape_w(FontId font, Uint16 cp) {
    int d = font_sizes[font] * 9 / 10;
    int lw = 0, lh = 0;
    if (label_fonts[font])
        TTF_SizeUTF8(label_fonts[font], icon_label(cp), &lw, &lh);
    int w = lw + d / 2;
    return w > d ? w : d;
}

static SDL_Surface *render_drawn_icon(FontId font, Uint16 cp, SDL_Color c) {
    int d = font_sizes[font] * 9 / 10;
    int shape_w = drawn_icon_shape_w(font, cp);
    int w = shape_w + 2 * ICON_PAD, h = TTF_FontHeight(fonts[font]);
    SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!surf)
        return NULL;

    // Giữa icon ngang giữa chữ hoa
    float r = d / 2.0f;
    float cy = TTF_FontAscent(fonts[font]) - font_sizes[font] * 0.36f;
    float x0 = ICON_PAD + r, x1 = ICON_PAD + shape_w - r;
    Uint32 *px = surf->pixels;
    int pitch = surf->pitch / 4;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            float fx = x + 0.5f, fy = y + 0.5f;
            float sx = fx < x0 ? x0 : fx > x1 ? x1 : fx;
            float dist = sqrtf((fx - sx) * (fx - sx) + (fy - cy) * (fy - cy)) - r;
            float cov = 0.5f - dist;
            cov = cov < 0 ? 0 : cov > 1 ? 1 : cov;
            Uint32 a = (Uint32)(cov * c.a + 0.5f);
            px[y * pitch + x] = (a << 24) | ((Uint32)c.r << 16) | ((Uint32)c.g << 8) | c.b;
        }
    }

    SDL_Surface *label = label_fonts[font]
        ? TTF_RenderUTF8_Blended(label_fonts[font], icon_label(cp), RGB(255, 255, 255)) : NULL;
    SDL_Surface *lab = label ? SDL_ConvertSurfaceFormat(label, SDL_PIXELFORMAT_ARGB8888, 0) : NULL;
    if (lab) {
        int lx = ICON_PAD + (shape_w - lab->w) / 2;
        int ly = (int)(cy - (TTF_FontAscent(label_fonts[font]) - label_size(font) * 0.36f) + 0.5f);
        const Uint32 *lp = lab->pixels;
        int lpitch = lab->pitch / 4;
        for (int y = 0; y < lab->h; y++) {
            for (int x = 0; x < lab->w; x++) {
                int tx = lx + x, ty = ly + y;
                if (tx < 0 || ty < 0 || tx >= w || ty >= h)
                    continue;
                Uint32 la = lp[y * lpitch + x] >> 24;
                Uint32 p = px[ty * pitch + tx];
                Uint32 a = (p >> 24) * (255 - la) / 255;
                px[ty * pitch + tx] = (a << 24) | (p & 0xFFFFFF);
            }
        }
        SDL_FreeSurface(lab);
    }
    if (label)
        SDL_FreeSurface(label);
    return surf;
}

static int icon_width(FontId font, const char *s, Uint16 cp) {
    if (icon_in_font(font, cp)) {
        char buf[ICON_BYTES + 1];
        memcpy(buf, s, ICON_BYTES);
        buf[ICON_BYTES] = '\0';
        int w = 0, h = 0;
        TTF_SizeUTF8(icon_fonts[font], buf, &w, &h);
        return w;
    }
    return drawn_icon_shape_w(font, cp) + 2 * ICON_PAD;
}

int gfx_text_width(FontId font, const char *text) {
    int total = 0;
    char buf[512];
    while (text && *text) {
        Uint16 cp = icon_at(text);
        if (cp) {
            total += icon_width(font, text, cp);
            text += ICON_BYTES;
            continue;
        }
        size_t n = plain_len(text);
        int w = 0, h = 0;
        if (!text[n]) {
            TTF_SizeUTF8(fonts[font], text, &w, &h);
        } else {
            size_t m = n < sizeof(buf) - 1 ? n : sizeof(buf) - 1;
            memcpy(buf, text, m);
            buf[m] = '\0';
            TTF_SizeUTF8(fonts[font], buf, &w, &h);
        }
        total += w;
        text += n;
    }
    return total;
}

int gfx_text_wrapped(FontId font, int x, int y, int max_w, SDL_Color c, const char *text) {
    int line_h = gfx_font_height(font);
    int dy = 0;
    char line[512];
    const char *p = text;
    while (*p) {
        // Lấy nhiều từ nhất còn vừa max_w
        size_t best = 0, len = 0;
        while (p[len]) {
            size_t next = len;
            while (p[next] == ' ')
                next++;
            while (p[next] && p[next] != ' ')
                next++;
            if (next >= sizeof(line))
                break;
            memcpy(line, p, next);
            line[next] = '\0';
            if (best && gfx_text_width(font, line) > max_w)
                break;
            best = next;
            len = next;
        }
        if (!best)
            break;
        memcpy(line, p, best);
        line[best] = '\0';
        gfx_text(font, x, y + dy, max_w, ALIGN_LEFT, c, line);
        dy += line_h;
        p += best;
        while (*p == ' ')
            p++;
    }
    return dy;
}

static Uint32 pack_color(SDL_Color c) {
    return ((Uint32)c.r << 24) | ((Uint32)c.g << 16) | ((Uint32)c.b << 8) | c.a;
}

// FNV-1a
static Uint32 text_hash(const char *s, FontId font, Uint32 color) {
    Uint32 h = 2166136261u;
    for (; *s; s++)
        h = (h ^ (unsigned char)*s) * 16777619u;
    h = (h ^ (Uint32)font) * 16777619u;
    h = (h ^ color) * 16777619u;
    return h;
}

// Chuỗi có icon: vẽ từng đoạn (chữ thường bằng font UI, icon bằng NintendoExt hoặc tự vẽ)
// rồi ghép ngang, các đoạn cùng đường chân chữ
static SDL_Surface *render_with_icons(FontId font, SDL_Color c, const char *text) {
    int w = gfx_text_width(font, text), h = TTF_FontHeight(fonts[font]);
    SDL_Surface *out = SDL_CreateRGBSurfaceWithFormat(0, w > 0 ? w : 1, h, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!out)
        return NULL;
    SDL_FillRect(out, NULL, 0);

    int x = 0;
    char buf[512];
    while (*text) {
        Uint16 cp = icon_at(text);
        SDL_Surface *piece = NULL;
        int y = 0;
        size_t n;
        if (cp) {
            n = ICON_BYTES;
            if (icon_in_font(font, cp)) {
                memcpy(buf, text, n);
                buf[n] = '\0';
                piece = TTF_RenderUTF8_Blended(icon_fonts[font], buf, c);
                y = TTF_FontAscent(fonts[font]) - TTF_FontAscent(icon_fonts[font]);
            } else {
                piece = render_drawn_icon(font, cp, c);
            }
        } else {
            n = plain_len(text);
            size_t m = n < sizeof(buf) - 1 ? n : sizeof(buf) - 1;
            memcpy(buf, text, m);
            buf[m] = '\0';
            piece = TTF_RenderUTF8_Blended(fonts[font], buf, c);
        }
        if (piece) {
            SDL_SetSurfaceBlendMode(piece, SDL_BLENDMODE_NONE);
            SDL_Rect dst = { x, y, piece->w, piece->h };
            SDL_BlitSurface(piece, NULL, out, &dst);
            x += piece->w;
            SDL_FreeSurface(piece);
        }
        text += n;
    }
    return out;
}

static TextCacheEntry *text_get(FontId font, SDL_Color c, const char *text) {
    Uint32 color = pack_color(c);
    Uint32 hash = text_hash(text, font, color);
    TextCacheEntry *e = &text_cache[hash % TEXT_CACHE_SIZE];

    if (e->tex && e->hash == hash && e->font == font && e->color == color && strcmp(e->text, text) == 0)
        return e;

    SDL_Surface *surf = has_icon(text) ? render_with_icons(font, c, text)
                                       : TTF_RenderUTF8_Blended(fonts[font], text, c);
    if (!surf)
        return NULL;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
    int w = surf->w, h = surf->h;
    SDL_FreeSurface(surf);
    if (!tex)
        return NULL;

    if (e->tex)
        SDL_DestroyTexture(e->tex);
    free(e->text);
    e->hash = hash;
    e->text = strdup(text);
    e->font = font;
    e->color = color;
    e->tex = tex;
    e->w = w;
    e->h = h;
    return e;
}

int gfx_text(FontId font, int x, int y, int max_w, TextAlign align, SDL_Color c, const char *text) {
    if (!text || !*text)
        return 0;
    // Chữ trong suốt: dùng chung texture chữ đặc, mờ đi bằng alpha mod
    Uint8 alpha = c.a;
    c.a = 255;
    TextCacheEntry *e = text_get(font, c, text);
    if (!e)
        return 0;

    int w = e->w;
    if (max_w > 0 && w > max_w)
        w = max_w;

    if (align == ALIGN_RIGHT)
        x -= w;
    else if (align == ALIGN_CENTER)
        x -= w / 2;

    SDL_Rect src = { 0, 0, w, e->h };
    SDL_Rect dst = { x, y, w, e->h };
    SDL_SetTextureAlphaMod(e->tex, alpha);
    SDL_RenderCopy(renderer, e->tex, &src, &dst);
    return w;
}
