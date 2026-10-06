#include "gfx.h"

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

static SDL_Window *window;
static SDL_Renderer *renderer;
static TTF_Font *fonts[FONT_COUNT];
static TextCacheEntry text_cache[TEXT_CACHE_SIZE];

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

    for (int i = 0; i < FONT_COUNT; i++) {
        if (fonts[i])
            TTF_CloseFont(fonts[i]);
        fonts[i] = NULL;
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

int gfx_font_height(FontId font) {
    return TTF_FontHeight(fonts[font]);
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

static TextCacheEntry *text_get(FontId font, SDL_Color c, const char *text) {
    Uint32 color = pack_color(c);
    Uint32 hash = text_hash(text, font, color);
    TextCacheEntry *e = &text_cache[hash % TEXT_CACHE_SIZE];

    if (e->tex && e->hash == hash && e->font == font && e->color == color && strcmp(e->text, text) == 0)
        return e;

    SDL_Surface *surf = TTF_RenderUTF8_Blended(fonts[font], text, c);
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
    SDL_RenderCopy(renderer, e->tex, &src, &dst);
    return w;
}
