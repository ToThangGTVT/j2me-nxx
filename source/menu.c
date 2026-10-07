#include "menu.h"

#include <stdio.h>
#include <string.h>

#include "gfx.h"
#include "input.h"
#include "lang.h"
#include "update.h"

#define HEADER_H    80
#define FOOTER_H    72
#define LIST_X      40
#define LIST_W      (SCREEN_W - 2 * LIST_X)
#define LIST_TOP    136
#define ROW_H       66
#define ICON_SIZE   48
#define INFO_PER_FRAME 3    // số JAR đọc thông tin mỗi frame (giữ menu mượt)
#define LIST_ROWS   ((SCREEN_H - FOOTER_H - 12 - LIST_TOP) / ROW_H)
#define SCROLLBAR_W 6

#define COL_BG       RGB(0x24, 0x26, 0x2b)
#define COL_BAR      RGB(0x18, 0x19, 0x1d)
#define COL_ACCENT   RGB(0x00, 0xb4, 0xe6)
#define COL_TEXT     RGB(0xee, 0xee, 0xee)
#define COL_DIM      RGB(0x9a, 0x9f, 0xa8)
#define COL_ROW_SEL  RGB(0x33, 0x3a, 0x48)
#define COL_TRACK    RGB(0x34, 0x37, 0x3e)
#define COL_WARN     RGB(0xff, 0xc1, 0x4d)

static void format_size(long size, char *out, size_t len) {
    if (size < 0)
        size = 0;
    if (size >= 1024 * 1024)
        snprintf(out, len, "%.1f MB", size / (1024.0 * 1024.0));
    else
        snprintf(out, len, "%ld KB", (size + 1023) / 1024);
}

MenuAction menu_update(Menu *m, GameList *list) {
    if (m->picking) {
        GameEntry *g = &list->items[m->cursor];
        int n = g->midlet_count;
        if (input_pressed(BTN_B) || input_pressed(BTN_PLUS)) {
            m->picking = false;
        } else if (input_pressed(BTN_DOWN)) {
            m->pick = (m->pick + 1) % n;
        } else if (input_pressed(BTN_UP)) {
            m->pick = (m->pick + n - 1) % n;
        } else if (input_pressed(BTN_A)) {
            m->picking = false;
            m->midlet = m->pick + 1;
            return MENU_LAUNCH;
        }
        return MENU_NONE;
    }
    if (input_pressed(BTN_PLUS))
        return MENU_QUIT;
    if (input_pressed(BTN_Y))
        return MENU_RESCAN;
    if (input_pressed(BTN_X))
        return MENU_SETTINGS;
    if (input_pressed(BTN_B) && update_available())
        return MENU_UPDATE;
    if (input_pressed(BTN_MINUS) && list->count > 0)
        return MENU_GAME_OPTIONS;
    if (list->count == 0)
        return MENU_NONE;

    int prev = m->cursor;
    bool paging = false;

    if (input_pressed(BTN_DOWN))
        m->cursor++;
    if (input_pressed(BTN_UP))
        m->cursor--;
    if (input_pressed(BTN_RIGHT) || input_pressed(BTN_R)) {
        m->cursor += LIST_ROWS;
        paging = true;
    }
    if (input_pressed(BTN_LEFT) || input_pressed(BTN_L)) {
        m->cursor -= LIST_ROWS;
        paging = true;
    }

    // Up/Down quay vòng, lật trang thì dừng ở đầu/cuối
    if (m->cursor < 0)
        m->cursor = paging ? 0 : list->count - 1;
    else if (m->cursor >= list->count)
        m->cursor = paging ? list->count - 1 : 0;

    if (m->cursor != prev)
        m->status[0] = '\0';

    if (m->cursor < m->scroll)
        m->scroll = m->cursor;
    else if (m->cursor >= m->scroll + LIST_ROWS)
        m->scroll = m->cursor - LIST_ROWS + 1;

    if (input_pressed(BTN_A)) {
        GameEntry *g = &list->items[m->cursor];
        game_list_load_info(g);
        m->midlet = 1;
        if (!g->video && g->midlet_count > 1) {
            m->picking = true;
            m->pick = 0;
            return MENU_NONE;
        }
        return MENU_LAUNCH;
    }
    return MENU_NONE;
}

static const char *line_subtitle(void) {
    static char buf[160];
    snprintf(buf, sizeof(buf), "v" APP_VERSION_STR "  -  %s", tr(S_APP_SUBTITLE));
    return buf;
}

static void draw_header(const GameList *list, const char *games_dir) {
    gfx_fill_rect(0, 0, SCREEN_W, HEADER_H, COL_BAR);
    gfx_fill_rect(0, HEADER_H, SCREEN_W, 2, COL_ACCENT);

    int title_y = (HEADER_H - gfx_font_height(FONT_LARGE)) / 2;
    int w = gfx_text(FONT_LARGE, LIST_X, title_y, 0, ALIGN_LEFT, COL_TEXT, "J2ME-NXX");
    gfx_text(FONT_SMALL, LIST_X + w + 14, title_y + gfx_font_height(FONT_LARGE) - gfx_font_height(FONT_SMALL) - 4,
             0, ALIGN_LEFT, COL_DIM, line_subtitle());

    char count[32];
    snprintf(count, sizeof(count), tr(S_GAME_COUNT), list->count);
    int cw = gfx_text(FONT_NORMAL, SCREEN_W - LIST_X, (HEADER_H - gfx_font_height(FONT_NORMAL)) / 2, 0, ALIGN_RIGHT,
                      COL_DIM, count);
    if (update_available()) {
        // Nhãn "có bản mới" bên trái số game
        char badge[64];
        snprintf(badge, sizeof(badge), tr(S_UPDATE_BADGE), update_latest_version());
        int bw = gfx_text_width(FONT_SMALL, badge) + 32, bh = gfx_font_height(FONT_SMALL) + 14;
        int bx = SCREEN_W - LIST_X - cw - 28 - bw, by = (HEADER_H - bh) / 2;
        gfx_fill_rect(bx, by, bw, bh, COL_ACCENT);
        gfx_text(FONT_SMALL, bx + bw / 2, by + 7, 0, ALIGN_CENTER, COL_BAR, badge);
    }

    char line[600];
    snprintf(line, sizeof(line), tr(S_FOLDER), games_dir);
    gfx_text(FONT_SMALL, LIST_X, HEADER_H + 18, LIST_W, ALIGN_LEFT, COL_DIM, line);
}

// Chưa có game: hướng dẫn chép .jar vào thư mục games
static void draw_empty(const char *games_dir) {
    int y = LIST_TOP + 40;
    gfx_text(FONT_LARGE, SCREEN_W / 2, y, 0, ALIGN_CENTER, COL_TEXT, tr(S_EMPTY_TITLE));
    y += gfx_font_height(FONT_LARGE) + 28;
    gfx_text(FONT_NORMAL, SCREEN_W / 2, y, LIST_W, ALIGN_CENTER, COL_DIM, tr(S_EMPTY_COPY));
    y += gfx_font_height(FONT_NORMAL) + 14;

    char dir[600];
    snprintf(dir, sizeof(dir), "%s/", games_dir);
    gfx_text(FONT_LARGE, SCREEN_W / 2, y, LIST_W, ALIGN_CENTER, COL_ACCENT, dir);
    y += gfx_font_height(FONT_LARGE) + 28;

    gfx_text(FONT_NORMAL, SCREEN_W / 2, y, LIST_W, ALIGN_CENTER, COL_DIM, tr(S_EMPTY_SUBDIR));
    y += gfx_font_height(FONT_NORMAL) + 10;
    gfx_text(FONT_NORMAL, SCREEN_W / 2, y, LIST_W, ALIGN_CENTER, COL_DIM, tr(S_EMPTY_VIDEO));
    y += gfx_font_height(FONT_NORMAL) + 28;
    gfx_text(FONT_NORMAL, SCREEN_W / 2, y, LIST_W, ALIGN_CENTER, COL_WARN, tr(S_EMPTY_RESCAN));
}

// Video: ô tối có hình tam giác "phát"
static void draw_video_icon(int x, int y) {
    gfx_fill_rect(x, y, ICON_SIZE, ICON_SIZE, RGB(0x30, 0x34, 0x3c));
    int h = ICON_SIZE / 2, x0 = x + ICON_SIZE / 2 - h / 3, y0 = y + (ICON_SIZE - h) / 2;
    for (int i = 0; i < h; i++) {
        int w = (i < h / 2 ? i : h - 1 - i) * 2 * 2 / 3 + 1;
        gfx_fill_rect(x0, y0 + i, w, 1, COL_ACCENT);
    }
}

static void draw_icon(GameEntry *g, int x, int y) {
    if (g->video) {
        draw_video_icon(x, y);
        return;
    }
    if (g->icon && !g->icon_tex)
        g->icon_tex = gfx_texture_argb(g->icon, g->icon_w, g->icon_h);
    if (g->icon_tex) {
        // Giữ tỉ lệ, phóng to số nguyên lần cho icon pixel nhỏ
        int s = ICON_SIZE / (g->icon_w > g->icon_h ? g->icon_w : g->icon_h);
        int w = s >= 1 ? g->icon_w * s : ICON_SIZE;
        int h = s >= 1 ? g->icon_h * s : ICON_SIZE * g->icon_h / g->icon_w;
        gfx_draw_texture(g->icon_tex, x + (ICON_SIZE - w) / 2, y + (ICON_SIZE - h) / 2, w, h);
        return;
    }
    // Không có icon: ô màu theo tên + chữ cái đầu
    uint32_t hsh = 2166136261u;
    for (const char *p = g->title; *p; p++)
        hsh = (hsh ^ (uint8_t)*p) * 16777619u;
    gfx_fill_rect(x, y, ICON_SIZE, ICON_SIZE, RGB(0x40 + (hsh & 0x3f), 0x40 + ((hsh >> 8) & 0x3f), 0x60 + ((hsh >> 16) & 0x3f)));
    char letter[2] = { g->title[0] ? g->title[0] : '?', '\0' };
    if (letter[0] >= 'a' && letter[0] <= 'z')
        letter[0] -= 32;
    gfx_text(FONT_LARGE, x + ICON_SIZE / 2, y + (ICON_SIZE - gfx_font_height(FONT_LARGE)) / 2, 0, ALIGN_CENTER,
             COL_TEXT, letter);
}

static void draw_list(const Menu *m, GameList *list) {
    int row_w = LIST_W - SCROLLBAR_W - 12;
    int loaded = 0;

    for (int row = 0; row < LIST_ROWS; row++) {
        int idx = m->scroll + row;
        if (idx >= list->count)
            break;

        GameEntry *g = &list->items[idx];
        if (!g->info_loaded && (loaded < INFO_PER_FRAME || idx == m->cursor)) {
            game_list_load_info(g);
            loaded++;
        }
        int y = LIST_TOP + row * ROW_H;
        bool sel = idx == m->cursor;

        if (sel) {
            gfx_fill_rect(LIST_X, y, row_w, ROW_H, COL_ROW_SEL);
            gfx_fill_rect(LIST_X, y, 6, ROW_H, COL_ACCENT);
        } else if (row > 0) {
            gfx_fill_rect(LIST_X + 20, y, row_w - 40, 1, COL_TRACK);
        }

        int x = LIST_X + 24;
        draw_icon(g, x, y + (ROW_H - ICON_SIZE) / 2);
        x += ICON_SIZE + 20;

        char size[16];
        format_size(g->size, size, sizeof(size));
        int size_w = gfx_text(FONT_SMALL, LIST_X + row_w - 24, y + (ROW_H - gfx_font_height(FONT_SMALL)) / 2, 0,
                              ALIGN_RIGHT, COL_DIM, size);

        int text_w = LIST_X + row_w - 24 - size_w - 24 - x;
        int title_h = gfx_font_height(FONT_NORMAL), sub_h = gfx_font_height(FONT_SMALL);
        int ty = y + (ROW_H - title_h - sub_h - 2) / 2;
        gfx_text(FONT_NORMAL, x, ty, text_w, ALIGN_LEFT, COL_TEXT, g->title);

        // Dòng phụ: nhà phát hành, phiên bản, thư mục con
        char sub[256] = "";
        if (!g->info_loaded) {
            snprintf(sub, sizeof(sub), "...");
        } else if (g->video) {
            const char *slash = strrchr(g->name, '/');
            if (slash)
                snprintf(sub, sizeof(sub), "%s  -  %.*s/", tr(S_VIDEO_TAG), (int)(slash - g->name), g->name);
            else
                snprintf(sub, sizeof(sub), "%s", tr(S_VIDEO_TAG));
        } else if (!g->valid) {
            snprintf(sub, sizeof(sub), "%s", tr(S_NO_MIDLET));
        } else {
            const char *slash = strrchr(g->name, '/');
            char folder[128] = "";
            if (slash)
                snprintf(folder, sizeof(folder), "%.*s/", (int)(slash - g->name), g->name);
            snprintf(sub, sizeof(sub), "%s%s%s%s%s", g->vendor, g->vendor[0] && g->version[0] ? "  -  v" : "",
                     g->version, folder[0] && (g->vendor[0] || g->version[0]) ? "  -  " : "", folder);
        }
        gfx_text(FONT_SMALL, x, ty + title_h + 2, text_w, ALIGN_LEFT, g->info_loaded && !g->valid ? COL_WARN : COL_DIM,
                 sub);
    }

    // Thanh cuộn
    if (list->count > LIST_ROWS) {
        int track_x = LIST_X + LIST_W - SCROLLBAR_W;
        int track_h = LIST_ROWS * ROW_H;
        int thumb_h = track_h * LIST_ROWS / list->count;
        if (thumb_h < 24)
            thumb_h = 24;
        int thumb_y = LIST_TOP + (track_h - thumb_h) * m->scroll / (list->count - LIST_ROWS);

        gfx_fill_rect(track_x, LIST_TOP, SCROLLBAR_W, track_h, COL_TRACK);
        gfx_fill_rect(track_x, thumb_y, SCROLLBAR_W, thumb_h, COL_ACCENT);
    }
}

static void draw_footer(const Menu *m, const GameList *list) {
    int y0 = SCREEN_H - FOOTER_H;
    int text_y = y0 + (FOOTER_H - gfx_font_height(FONT_NORMAL)) / 2;

    gfx_fill_rect(0, y0, SCREEN_W, FOOTER_H, COL_BAR);
    gfx_fill_rect(0, y0, SCREEN_W, 1, COL_TRACK);

    if (m->status[0]) {
        gfx_text(FONT_NORMAL, LIST_X, text_y, 440, ALIGN_LEFT, COL_WARN, m->status);
    } else {
        char pos[32];
        snprintf(pos, sizeof(pos), "%d / %d", list->count ? m->cursor + 1 : 0, list->count);
        gfx_text(FONT_NORMAL, LIST_X, text_y, 0, ALIGN_LEFT, COL_DIM, pos);
    }

    gfx_text(FONT_NORMAL, SCREEN_W - LIST_X, text_y, 0, ALIGN_RIGHT, COL_TEXT,
             tr(S_MENU_HINTS));
}

void menu_free_textures(GameList *list) {
    for (int i = 0; i < list->count; i++) {
        if (list->items[i].icon_tex)
            SDL_DestroyTexture(list->items[i].icon_tex);
        list->items[i].icon_tex = NULL;
    }
}

static void draw_picker(const Menu *m, const GameList *list) {
    const GameEntry *g = &list->items[m->cursor];
    int n = g->midlet_count;
    int row_h = 56, w = 640;
    int h = 90 + n * row_h + 20;
    int x = (SCREEN_W - w) / 2, y = (SCREEN_H - h) / 2;
    gfx_fill_rect(0, 0, SCREEN_W, SCREEN_H, (SDL_Color){ 0, 0, 0, 160 });
    gfx_fill_rect(x, y, w, h, COL_BAR);
    gfx_fill_rect(x, y, w, 3, COL_ACCENT);
    gfx_text(FONT_NORMAL, x + 32, y + 24, w - 64, ALIGN_LEFT, COL_TEXT, g->title);
    gfx_text(FONT_SMALL, x + w - 32, y + 30, 0, ALIGN_RIGHT, COL_DIM, tr(S_PICK_MIDLET));
    for (int i = 0; i < n; i++) {
        int ry = y + 80 + i * row_h;
        if (i == m->pick) {
            gfx_fill_rect(x + 16, ry, w - 32, row_h, COL_ROW_SEL);
            gfx_fill_rect(x + 16, ry, 6, row_h, COL_ACCENT);
        }
        gfx_text(FONT_NORMAL, x + 48, ry + (row_h - gfx_font_height(FONT_NORMAL)) / 2, w - 96, ALIGN_LEFT,
                 i == m->pick ? COL_TEXT : COL_DIM, g->midlets[i]);
    }
}

void menu_draw(const Menu *m, GameList *list, const char *games_dir) {
    gfx_clear(COL_BG);
    draw_header(list, games_dir);
    if (list->count == 0)
        draw_empty(games_dir);
    else
        draw_list(m, list);
    draw_footer(m, list);
    if (m->picking)
        draw_picker(m, list);
}
