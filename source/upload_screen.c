#include "upload_screen.h"

#include <stdio.h>
#include <string.h>

#include "gfx.h"
#include "input.h"
#include "lang.h"
#include "qr.h"
#include "upload.h"

#define BOX_W   1040
#define BOX_H   520
#define PAD     40
#define QR_PX   340     // cạnh tối đa của mã QR (kể cả viền trắng)
#define QR_QUIET 3      // viền trắng quanh mã, tính theo ô
#define BAR_H   20

#define COL_SHADE   ((SDL_Color){ 0, 0, 0, 170 })
#define COL_BOX     RGB(0x24, 0x26, 0x2b)
#define COL_ACCENT  RGB(0x00, 0xb4, 0xe6)
#define COL_TEXT    RGB(0xee, 0xee, 0xee)
#define COL_DIM     RGB(0x9a, 0x9f, 0xa8)
#define COL_TRACK   RGB(0x3a, 0x3e, 0x47)
#define COL_WARN    RGB(0xff, 0xc1, 0x4d)
#define COL_OK      RGB(0x5c, 0xd6, 0x7a)
#define COL_QR_DARK RGB(0x00, 0x00, 0x00)
#define COL_QR_LITE RGB(0xff, 0xff, 0xff)

static bool server_on;
static char url[96];
static char error[256];
static uint8_t qr[QR_MAX_SIZE * QR_MAX_SIZE];
static int qr_size;

void upload_screen_open(const char *games_dir) {
    error[0] = '\0';
    qr_size = 0;
    server_on = upload_start(games_dir, url, sizeof(url), error, sizeof(error));
    if (server_on)
        qr_size = qr_encode(url, qr);
}

bool upload_screen_update(void) {
    return !input_pressed(BTN_B) && !input_pressed(BTN_PLUS);
}

int upload_screen_close(void) {
    UploadStatus st;
    upload_get_status(&st);
    upload_stop();
    server_on = false;
    return st.received;
}

// Cạnh mã QR khi vẽ (ô nguyên pixel, không quá QR_PX)
static int qr_px(void) {
    int cells = qr_size + 2 * QR_QUIET;
    return QR_PX / cells * cells;
}

static void draw_qr(int x, int y) {
    int cells = qr_size + 2 * QR_QUIET;
    int s = QR_PX / cells;
    gfx_fill_rect(x, y, cells * s, cells * s, COL_QR_LITE);
    int ox = x + QR_QUIET * s, oy = y + QR_QUIET * s;
    // Gộp các ô tối liền nhau trên một hàng thành một hình chữ nhật
    for (int r = 0; r < qr_size; r++) {
        for (int c = 0; c < qr_size;) {
            if (!qr[r * qr_size + c]) {
                c++;
                continue;
            }
            int c0 = c;
            while (c < qr_size && qr[r * qr_size + c])
                c++;
            gfx_fill_rect(ox + c0 * s, oy + r * s, (c - c0) * s, s, COL_QR_DARK);
        }
    }
}

static void format_mb(char *out, size_t size, int64_t bytes) {
    snprintf(out, size, "%.1f MB", bytes / (1024.0 * 1024.0));
}

void upload_screen_draw(void) {
    gfx_fill_rect(0, 0, SCREEN_W, SCREEN_H, COL_SHADE);
    int x = (SCREEN_W - BOX_W) / 2, y = (SCREEN_H - BOX_H) / 2;
    gfx_fill_rect(x, y, BOX_W, BOX_H, COL_BOX);
    gfx_fill_rect(x, y, BOX_W, 4, COL_ACCENT);
    int footer_y = y + BOX_H - 64;

    int cx = x + PAD, cy = y + PAD;
    gfx_text(FONT_LARGE, cx, cy, BOX_W - 2 * PAD, ALIGN_LEFT, COL_TEXT, tr(S_UPLOAD_TITLE));
    cy += gfx_font_height(FONT_LARGE) + 24;

    if (!server_on) {
        gfx_text_wrapped(FONT_NORMAL, cx, cy, BOX_W - 2 * PAD, COL_WARN, error);
    } else {
        draw_qr(cx, cy);
        int tx = cx + qr_px() + 48, tw = x + BOX_W - PAD - tx;
        int ty = cy;
        StrId steps[] = { S_UPLOAD_STEP1, S_UPLOAD_STEP2, S_UPLOAD_STEP3 };
        for (int i = 0; i < 3; i++)
            ty += gfx_text_wrapped(FONT_NORMAL, tx, ty, tw, COL_TEXT, tr(steps[i])) + 10;
        ty += 8;
        gfx_text(FONT_LARGE, tx, ty, tw, ALIGN_LEFT, COL_ACCENT, url);
        ty += gfx_font_height(FONT_LARGE) + 28;

        UploadStatus st;
        upload_get_status(&st);
        char line[256];
        if (st.current[0]) {
            snprintf(line, sizeof(line), tr(S_UPLOAD_RECEIVING), st.current);
            gfx_text(FONT_NORMAL, tx, ty, tw, ALIGN_LEFT, COL_TEXT, line);
            ty += gfx_font_height(FONT_NORMAL) + 10;
            gfx_fill_rect(tx, ty, tw, BAR_H, COL_TRACK);
            if (st.total > 0)
                gfx_fill_rect(tx, ty, (int)(tw * (double)st.done / (double)st.total), BAR_H, COL_ACCENT);
            ty += BAR_H + 8;
            char done[32], total[32];
            format_mb(done, sizeof(done), st.done);
            format_mb(total, sizeof(total), st.total);
            snprintf(line, sizeof(line), "%s / %s", done, total);
            gfx_text(FONT_SMALL, tx, ty, tw, ALIGN_LEFT, COL_DIM, line);
        } else {
            if (st.received > 0) {
                snprintf(line, sizeof(line), tr(S_UPLOAD_RECEIVED), st.received, st.last);
                ty += gfx_text_wrapped(FONT_NORMAL, tx, ty, tw, COL_OK, line);
            } else {
                gfx_text(FONT_NORMAL, tx, ty, tw, ALIGN_LEFT, COL_DIM, tr(S_UPLOAD_WAITING));
                ty += gfx_font_height(FONT_NORMAL);
            }
            if (st.error[0])
                gfx_text_wrapped(FONT_NORMAL, tx, ty + 8, tw, COL_WARN, st.error);
        }
    }

    gfx_text(FONT_NORMAL, x + BOX_W - PAD, footer_y + (64 - gfx_font_height(FONT_NORMAL)) / 2, 0, ALIGN_RIGHT,
             COL_TEXT, tr(S_UPLOAD_HINTS));
}
