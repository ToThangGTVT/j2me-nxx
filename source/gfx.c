#include "gfx.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <glad/glad.h>
#include <nanovg.h>

// Có trong nanovg_gl.h (bản GL3 borealis dùng); khai báo lại để khỏi phải định nghĩa NANOVG_GL3 ở đây
GLuint nvglImageHandleGL3(NVGcontext *ctx, int image);

// Cỡ chữ của NanoVG tính theo chiều cao dòng (ascent - descent), lớn hơn cỡ pt của SDL_ttf cũ ~1.17 lần
static const float font_sizes[FONT_COUNT] = {
    [FONT_SMALL]  = 23,
    [FONT_NORMAL] = 30,
    [FONT_LARGE]  = 44,
};

static NVGcontext *vg;
static int font_face = -1;
// Ma trận toạ độ 1280x720 -> điểm ảnh của khung hình, và nghịch đảo của nó
static float xform[6], inv_xform[6];
static GLuint blit_fbo[2];

void gfx_init(struct NVGcontext *ctx, int font) {
    vg = ctx;
    font_face = font;
    nvgTransformIdentity(xform);
    nvgTransformIdentity(inv_xform);
}

void gfx_begin(float x, float y, float w, float h) {
    nvgSave(vg);
    float s = w / SCREEN_W < h / SCREEN_H ? w / SCREEN_W : h / SCREEN_H;
    nvgTranslate(vg, x + (w - SCREEN_W * s) / 2, y + (h - SCREEN_H * s) / 2);
    nvgScale(vg, s, s);
    nvgCurrentTransform(vg, xform);
    nvgTransformInverse(inv_xform, xform);
}

void gfx_end(void) {
    nvgRestore(vg);
}

void gfx_window_size(int *w, int *h) {
    SDL_Window *win = SDL_GL_GetCurrentWindow();
    *w = SCREEN_W;
    *h = SCREEN_H;
    if (win)
        SDL_GetWindowSize(win, w, h);
}

void gfx_window_to_screen(float wx, float wy, int *sx, int *sy) {
    // Điểm của SDL -> điểm ảnh khung hình (màn hình Retina: nhiều điểm ảnh hơn)
    SDL_Window *win = SDL_GL_GetCurrentWindow();
    if (win) {
        int ww, wh, pw, ph;
        SDL_GetWindowSize(win, &ww, &wh);
        SDL_GL_GetDrawableSize(win, &pw, &ph);
        if (ww > 0 && wh > 0) {
            wx = wx * pw / ww;
            wy = wy * ph / wh;
        }
    }
    float x, y;
    nvgTransformPoint(&x, &y, inv_xform, wx, wy);
    *sx = (int)floorf(x);
    *sy = (int)floorf(y);
}

static NVGcolor color(SDL_Color c) {
    return nvgRGBA(c.r, c.g, c.b, c.a);
}

void gfx_clear(SDL_Color c) {
    gfx_fill_rect(0, 0, SCREEN_W, SCREEN_H, c);
}

void gfx_fill_rect(int x, int y, int w, int h, SDL_Color c) {
    if (w <= 0 || h <= 0)
        return;
    // Hình chữ nhật thẳng: tắt khử răng cưa để cạnh sắc, các ô liền nhau không hở
    nvgShapeAntiAlias(vg, 0);
    nvgBeginPath(vg);
    nvgRect(vg, x, y, w, h);
    nvgFillColor(vg, color(c));
    nvgFill(vg);
    nvgShapeAntiAlias(vg, 1);
}

void gfx_fill_round_rect(int x, int y, int w, int h, int r, SDL_Color c) {
    if (w <= 0 || h <= 0)
        return;
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, w, h, r);
    nvgFillColor(vg, color(c));
    nvgFill(vg);
}

void gfx_fill_circle(int cx, int cy, int r, SDL_Color c) {
    nvgBeginPath(vg);
    nvgCircle(vg, cx, cy, r);
    nvgFillColor(vg, color(c));
    nvgFill(vg);
}

void gfx_clip(int x, int y, int w, int h) {
    nvgScissor(vg, x, y, w, h);
}

void gfx_unclip(void) {
    nvgResetScissor(vg);
}

static void set_font(FontId font) {
    nvgFontFaceId(vg, font_face);
    nvgFontSize(vg, font_sizes[font]);
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
    nvgTextLetterSpacing(vg, 0);
}

int gfx_font_height(FontId font) {
    if (!vg)
        return (int)font_sizes[font];
    set_font(font);
    float lineh = font_sizes[font];
    nvgTextMetrics(vg, NULL, NULL, &lineh);
    return (int)ceilf(lineh);
}

int gfx_text_width(FontId font, const char *text) {
    if (!vg || !text || !*text)
        return 0;
    set_font(font);
    return (int)ceilf(nvgTextBounds(vg, 0, 0, text, NULL, NULL));
}

int gfx_text(FontId font, int x, int y, int max_w, TextAlign align, SDL_Color c, const char *text) {
    if (!text || !*text)
        return 0;
    int w = gfx_text_width(font, text);
    bool clip = max_w > 0 && w > max_w;
    if (clip)
        w = max_w;
    if (align == ALIGN_RIGHT)
        x -= w;
    else if (align == ALIGN_CENTER)
        x -= w / 2;
    if (clip) {
        nvgSave(vg);
        nvgIntersectScissor(vg, x, y, w, gfx_font_height(font));
    }
    set_font(font);
    nvgFillColor(vg, color(c));
    nvgText(vg, x, y, text, NULL);
    if (clip)
        nvgRestore(vg);
    return w;
}

int gfx_text_wrapped(FontId font, int x, int y, int max_w, SDL_Color c, const char *text) {
    // Giãn dòng thêm cho dấu tiếng Việt khỏi chạm dòng trên
    int line_h = gfx_font_height(font) + (int)(font_sizes[font] * 0.2f);
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

int gfx_image_create(int w, int h, bool smooth) {
    if (!vg || w <= 0 || h <= 0)
        return 0;
    return nvgCreateImageRGBA(vg, w, h, smooth ? 0 : NVG_IMAGE_NEAREST, NULL);
}

void gfx_image_update(int img, const uint32_t *argb) {
    if (!img || !argb)
        return;
    int w, h;
    nvgImageSize(vg, img, &w, &h);
    // ARGB8888 trong bộ nhớ (little endian) là B, G, R, A: đưa thẳng lên GPU dạng BGRA, khỏi đổi thứ tự byte
    glBindTexture(GL_TEXTURE_2D, nvglImageHandleGL3(vg, img));
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_BGRA, GL_UNSIGNED_BYTE, argb);
    glBindTexture(GL_TEXTURE_2D, 0);
}

int gfx_image_argb(const uint32_t *argb, int w, int h, bool smooth) {
    int img = gfx_image_create(w, h, smooth);
    gfx_image_update(img, argb);
    return img;
}

void gfx_image_free(int img) {
    if (img && vg)
        nvgDeleteImage(vg, img);
}

void gfx_draw_image_alpha(int img, int x, int y, int w, int h, float alpha) {
    if (!img || w <= 0 || h <= 0)
        return;
    nvgShapeAntiAlias(vg, 0);
    nvgBeginPath(vg);
    nvgRect(vg, x, y, w, h);
    nvgFillPaint(vg, nvgImagePattern(vg, x, y, w, h, 0, img, alpha));
    nvgFill(vg);
    nvgShapeAntiAlias(vg, 1);
}

void gfx_draw_image(int img, int x, int y, int w, int h) {
    gfx_draw_image_alpha(img, x, y, w, h, 1.0f);
}

bool gfx_image_upscale(int src, int dst) {
    if (!src || !dst)
        return false;
    int sw, sh, dw, dh;
    nvgImageSize(vg, src, &sw, &sh);
    nvgImageSize(vg, dst, &dw, &dh);
    if (!blit_fbo[0])
        glGenFramebuffers(2, blit_fbo);
    GLint read_fb = 0, draw_fb = 0;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read_fb);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw_fb);
    GLboolean scissor = glIsEnabled(GL_SCISSOR_TEST);
    if (scissor)
        glDisable(GL_SCISSOR_TEST);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, blit_fbo[0]);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, nvglImageHandleGL3(vg, src), 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, blit_fbo[1]);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, nvglImageHandleGL3(vg, dst), 0);
    bool ok = glCheckFramebufferStatus(GL_READ_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE &&
              glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    if (ok)
        glBlitFramebuffer(0, 0, sw, sh, 0, 0, dw, dh, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)read_fb);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)draw_fb);
    if (scissor)
        glEnable(GL_SCISSOR_TEST);
    return ok;
}
