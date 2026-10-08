// Lớp vẽ 2D trên SDL2: cửa sổ 1280x720, hình chữ nhật, chữ (có cache texture)
#pragma once

#include <stdbool.h>
#include <SDL.h>

#define SCREEN_W 1280
#define SCREEN_H 720

typedef enum {
    FONT_SMALL,
    FONT_NORMAL,
    FONT_LARGE,
    FONT_COUNT
} FontId;

typedef enum {
    ALIGN_LEFT,
    ALIGN_RIGHT,
    ALIGN_CENTER,
} TextAlign;

#define RGB(r, g, b) ((SDL_Color){ (r), (g), (b), 255 })

// Icon nút của Switch: ký tự vùng riêng (UTF-8) trong font NintendoExt của máy, dùng thẳng trong chuỗi.
// Desktop (không có font này) thì tự vẽ hình tròn có chữ
#define ICON_A      "\xEE\x83\xA0"   // U+E0E0
#define ICON_B      "\xEE\x83\xA1"   // U+E0E1
#define ICON_X      "\xEE\x83\xA2"   // U+E0E2
#define ICON_Y      "\xEE\x83\xA3"   // U+E0E3
#define ICON_L      "\xEE\x82\xA4"   // U+E0A4
#define ICON_R      "\xEE\x82\xA5"   // U+E0A5
#define ICON_ZL     "\xEE\x82\xA6"   // U+E0A6
#define ICON_ZR     "\xEE\x82\xA7"   // U+E0A7
#define ICON_PLUS   "\xEE\x82\xB5"   // U+E0B5
#define ICON_MINUS  "\xEE\x82\xB6"   // U+E0B6
#define ICON_UP     "\xEE\x81\xB9"   // U+E079
#define ICON_DOWN   "\xEE\x81\xBA"   // U+E07A
#define ICON_LEFT   "\xEE\x81\xBB"   // U+E07B
#define ICON_RIGHT  "\xEE\x81\xBC"   // U+E07C

bool gfx_init(const char *title);
void gfx_exit(void);

SDL_Renderer *gfx_renderer(void);

void gfx_clear(SDL_Color c);
void gfx_present(void);
void gfx_fill_rect(int x, int y, int w, int h, SDL_Color c);
// Hình chữ nhật bo góc bán kính r / hình tròn, khử răng cưa, màu trong suốt đều
void gfx_fill_round_rect(int x, int y, int w, int h, int r, SDL_Color c);
void gfx_fill_circle(int cx, int cy, int r, SDL_Color c);

int gfx_font_height(FontId font);
int gfx_text_width(FontId font, const char *text);
// Vẽ đoạn chữ tự xuống dòng theo từ; trả về chiều cao đã dùng
int gfx_text_wrapped(FontId font, int x, int y, int max_w, SDL_Color c, const char *text);

// Texture từ ảnh ARGB (dùng cho icon game)
SDL_Texture *gfx_texture_argb(const uint32_t *pixels, int w, int h);
void gfx_draw_texture(SDL_Texture *tex, int x, int y, int w, int h);

// Vẽ chữ UTF-8, cắt bớt nếu rộng hơn max_w (0 = không giới hạn).
// y là cạnh trên của dòng chữ. Trả về độ rộng đã vẽ.
int gfx_text(FontId font, int x, int y, int max_w, TextAlign align, SDL_Color c, const char *text);
