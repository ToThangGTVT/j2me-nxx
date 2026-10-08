// Lớp vẽ 2D trên NanoVG của borealis: toạ độ logic 1280x720 (giữ tỉ lệ, đặt giữa vùng vẽ),
// hình chữ nhật, chữ, ảnh. Dùng cho các màn hình tự vẽ (chạy game, phím ảo, xem video).
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

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

// Icon nút của Switch: ký tự vùng riêng (UTF-8) trong font NintendoExt (Switch) / switch_icons.ttf (desktop)
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

struct NVGcontext;

// Gọi 1 lần sau khi borealis tạo cửa sổ: context NanoVG và font chữ (id trong fontstash)
void gfx_init(struct NVGcontext *vg, int font);

// Bắt đầu / kết thúc vẽ một màn hình 1280x720 vào vùng (x, y, w, h) của khung hình borealis
void gfx_begin(float x, float y, float w, float h);
void gfx_end(void);
// Đổi toạ độ cửa sổ (điểm của SDL, chuột / chạm) sang toạ độ 1280x720 của màn hình đang vẽ
void gfx_window_to_screen(float wx, float wy, int *sx, int *sy);
// Kích thước cửa sổ theo điểm của SDL (để đổi toạ độ chạm 0..1)
void gfx_window_size(int *w, int *h);

void gfx_clear(SDL_Color c);
void gfx_fill_rect(int x, int y, int w, int h, SDL_Color c);
// Hình chữ nhật bo góc bán kính r / hình tròn, khử răng cưa
void gfx_fill_round_rect(int x, int y, int w, int h, int r, SDL_Color c);
void gfx_fill_circle(int cx, int cy, int r, SDL_Color c);
// Chỉ vẽ trong vùng (x, y, w, h) tới khi gfx_unclip
void gfx_clip(int x, int y, int w, int h);
void gfx_unclip(void);

int gfx_font_height(FontId font);
int gfx_text_width(FontId font, const char *text);
// Vẽ chữ UTF-8, cắt bớt nếu rộng hơn max_w (0 = không giới hạn).
// y là cạnh trên của dòng chữ. Trả về độ rộng đã vẽ.
int gfx_text(FontId font, int x, int y, int max_w, TextAlign align, SDL_Color c, const char *text);
// Vẽ đoạn chữ tự xuống dòng theo từ; trả về chiều cao đã dùng
int gfx_text_wrapped(FontId font, int x, int y, int max_w, SDL_Color c, const char *text);

// Ảnh (id NanoVG, 0 = không có). smooth: lọc tuyến tính khi phóng, không thì giữ điểm ảnh
int gfx_image_create(int w, int h, bool smooth);
// Ghi cả ảnh từ điểm ảnh ARGB8888 (w x h đúng như lúc tạo)
void gfx_image_update(int img, const uint32_t *argb);
int gfx_image_argb(const uint32_t *argb, int w, int h, bool smooth);
void gfx_image_free(int img);
void gfx_draw_image(int img, int x, int y, int w, int h);
void gfx_draw_image_alpha(int img, int x, int y, int w, int h, float alpha);
// Chép src vào dst (lớn gấp nguyên lần) không lọc: dùng cho sharp-bilinear
bool gfx_image_upscale(int src, int dst);

#ifdef __cplusplus
}
#endif
