#include "update_screen.h"

#include <stdio.h>
#include <string.h>

#include "gfx.h"
#include "input.h"
#include "lang.h"
#include "update.h"

#define BOX_W   900
#define BOX_H   520
#define PAD     40
#define BAR_H   28

#define COL_SHADE   ((SDL_Color){ 0, 0, 0, 170 })
#define COL_BOX     RGB(0x24, 0x26, 0x2b)
#define COL_BAR     RGB(0x18, 0x19, 0x1d)
#define COL_ACCENT  RGB(0x00, 0xb4, 0xe6)
#define COL_TEXT    RGB(0xee, 0xee, 0xee)
#define COL_DIM     RGB(0x9a, 0x9f, 0xa8)
#define COL_TRACK   RGB(0x3a, 0x3e, 0x47)
#define COL_WARN    RGB(0xff, 0xc1, 0x4d)
#define COL_OK      RGB(0x5c, 0xd6, 0x7a)

static bool closing;    // đã bấm huỷ, chờ luồng tải dừng

void update_screen_open(void) {
    closing = false;
}

bool update_screen_update(bool *quit) {
    *quit = false;
    UpdateState s = update_state();
    bool a = input_pressed(BTN_A), b = input_pressed(BTN_B);
    switch (s) {
    case UPDATE_DOWNLOADING:
        if (b) {
            update_cancel();
            closing = true;
        }
        return true;
    case UPDATE_DONE:
        if (a) {
#ifdef __SWITCH__
            // Khởi động lại vào bản mới; loader không hỗ trợ thì thoát để người dùng mở lại
            update_restart();
            *quit = true;
#endif
            return false;
        }
        return !b;
    case UPDATE_FAILED:
        if (closing)
            return false;   // vừa huỷ
        if (a)
            update_download();
        return !b;
    case UPDATE_AVAILABLE:
        if (closing)
            return false;   // vừa huỷ tải
        if (a)
            update_download();
        return !b;
    default:
        return false;
    }
}

static void format_mb(char *out, size_t size, int64_t bytes) {
    snprintf(out, size, "%.1f MB", bytes / (1024.0 * 1024.0));
}

void update_screen_draw(void) {
    gfx_fill_rect(0, 0, SCREEN_W, SCREEN_H, COL_SHADE);
    int x = (SCREEN_W - BOX_W) / 2, y = (SCREEN_H - BOX_H) / 2;
    gfx_fill_rect(x, y, BOX_W, BOX_H, COL_BOX);
    gfx_fill_rect(x, y, BOX_W, 4, COL_ACCENT);
    int cx = x + PAD, cw = BOX_W - 2 * PAD, cy = y + PAD;
    int footer_y = y + BOX_H - 64;
    gfx_fill_rect(x, footer_y, BOX_W, 64, COL_BAR);

    UpdateState s = update_state();
    const char *ver = update_latest_version();
    char line[256];
    const char *hints = "";

    switch (s) {
    case UPDATE_AVAILABLE:
    case UPDATE_FAILED: {
        snprintf(line, sizeof(line), tr(S_UPDATE_TITLE), ver);
        gfx_text(FONT_LARGE, cx, cy, cw, ALIGN_LEFT, COL_TEXT, line);
        cy += gfx_font_height(FONT_LARGE) + 6;
        snprintf(line, sizeof(line), tr(S_UPDATE_CURRENT), APP_VERSION_STR);
        gfx_text(FONT_SMALL, cx, cy, cw, ALIGN_LEFT, COL_DIM, line);
        cy += gfx_font_height(FONT_SMALL) + 24;
        if (s == UPDATE_FAILED) {
            char err[192];
            update_error(err, sizeof(err));
            snprintf(line, sizeof(line), tr(S_UPDATE_FAILED), err);
            cy += gfx_text_wrapped(FONT_NORMAL, cx, cy, cw, COL_WARN, line) + 16;
            hints = tr(S_UPDATE_FAILED_HINTS);
        } else {
            hints = tr(S_UPDATE_PROMPT_HINTS);
        }
        // Ghi chú phát hành: cắt bớt cho vừa khung
        const char *notes = update_notes();
        if (notes[0]) {
            int bottom = footer_y - 16;
            SDL_Rect clip = { cx, cy, cw, bottom - cy };
            SDL_RenderSetClipRect(gfx_renderer(), &clip);
            for (const char *p = notes; *p && cy < bottom;) {
                const char *nl = strchr(p, '\n');
                size_t n = nl ? (size_t)(nl - p) : strlen(p);
                char row[512];
                snprintf(row, sizeof(row), "%.*s", (int)n, p);
                if (row[0])
                    cy += gfx_text_wrapped(FONT_SMALL, cx, cy, cw, COL_DIM, row) + 4;
                else
                    cy += gfx_font_height(FONT_SMALL) / 2;
                p += n + (nl ? 1 : 0);
            }
            SDL_RenderSetClipRect(gfx_renderer(), NULL);
        }
        break;
    }
    case UPDATE_DOWNLOADING: {
        snprintf(line, sizeof(line), tr(closing ? S_UPDATE_CANCELLING : S_UPDATE_DOWNLOADING), ver);
        gfx_text(FONT_LARGE, cx, cy, cw, ALIGN_LEFT, COL_TEXT, line);
        cy += gfx_font_height(FONT_LARGE) + 60;

        int64_t done, total;
        int bps;
        update_progress(&done, &total, &bps);
        gfx_fill_rect(cx, cy, cw, BAR_H, COL_TRACK);
        int pct = total > 0 ? (int)(done * 100 / total) : -1;
        if (total > 0) {
            gfx_fill_rect(cx, cy, (int)(cw * (double)done / total), BAR_H, COL_ACCENT);
        } else {
            // Chưa biết độ dài: vạch chạy qua lại
            int seg = cw / 5, pos = (int)(SDL_GetTicks() / 4 % (Uint32)(2 * (cw - seg)));
            if (pos > cw - seg)
                pos = 2 * (cw - seg) - pos;
            gfx_fill_rect(cx + pos, cy, seg, BAR_H, COL_ACCENT);
        }
        cy += BAR_H + 20;

        char a[32], b[32];
        format_mb(a, sizeof(a), done);
        if (total > 0) {
            format_mb(b, sizeof(b), total);
            snprintf(line, sizeof(line), "%s / %s", a, b);
        } else {
            snprintf(line, sizeof(line), "%s", a);
        }
        gfx_text(FONT_NORMAL, cx, cy, 0, ALIGN_LEFT, COL_TEXT, line);
        if (pct >= 0) {
            snprintf(line, sizeof(line), "%d%%", pct);
            gfx_text(FONT_LARGE, cx + cw, cy - 6, 0, ALIGN_RIGHT, COL_ACCENT, line);
        }
        cy += gfx_font_height(FONT_NORMAL) + 8;
        if (bps > 0) {
            if (bps >= 1024 * 1024)
                snprintf(line, sizeof(line), "%.1f MB/s", bps / (1024.0 * 1024.0));
            else
                snprintf(line, sizeof(line), "%d KB/s", bps / 1024);
            if (total > 0 && done < total) {
                int sec = (int)((total - done) / bps);
                char eta[48];
                snprintf(eta, sizeof(eta), tr(S_UPDATE_ETA), sec / 60, sec % 60);
                snprintf(line + strlen(line), sizeof(line) - strlen(line), "  -  %s", eta);
            }
            gfx_text(FONT_SMALL, cx, cy, 0, ALIGN_LEFT, COL_DIM, line);
        }
        cy += gfx_font_height(FONT_SMALL) + 30;
        gfx_text_wrapped(FONT_SMALL, cx, cy, cw, COL_DIM, tr(S_UPDATE_KEEP_OPEN));
        hints = closing ? "" : tr(S_UPDATE_CANCEL_HINT);
        break;
    }
    case UPDATE_DONE:
        snprintf(line, sizeof(line), tr(S_UPDATE_DONE), ver);
        gfx_text(FONT_LARGE, cx, cy, cw, ALIGN_LEFT, COL_OK, line);
        cy += gfx_font_height(FONT_LARGE) + 24;
#ifdef __SWITCH__
        gfx_text_wrapped(FONT_NORMAL, cx, cy, cw, COL_TEXT, tr(S_UPDATE_DONE_INFO));
        hints = tr(S_UPDATE_DONE_HINTS);
#else
        gfx_text_wrapped(FONT_NORMAL, cx, cy, cw, COL_TEXT, tr(S_UPDATE_DONE_DESKTOP));
        hints = tr(S_UPDATE_CLOSE_HINT);
#endif
        break;
    default:
        break;
    }
    gfx_text(FONT_NORMAL, x + BOX_W - PAD, footer_y + (64 - gfx_font_height(FONT_NORMAL)) / 2, 0, ALIGN_RIGHT,
             COL_TEXT, hints);
}
