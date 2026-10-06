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

bool gfx_init(const char *title);
void gfx_exit(void);

SDL_Renderer *gfx_renderer(void);

void gfx_clear(SDL_Color c);
void gfx_present(void);
void gfx_fill_rect(int x, int y, int w, int h, SDL_Color c);

int gfx_font_height(FontId font);

// Vẽ chữ UTF-8, cắt bớt nếu rộng hơn max_w (0 = không giới hạn).
// y là cạnh trên của dòng chữ. Trả về độ rộng đã vẽ.
int gfx_text(FontId font, int x, int y, int max_w, TextAlign align, SDL_Color c, const char *text);
