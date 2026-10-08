#include "settings.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#include "lang.h"
#include "platform.h"

const int SETTINGS_FPS_CHOICES[] = { 0, 15, 20, 30, 60 };
const int SETTINGS_FPS_CHOICE_COUNT = sizeof(SETTINGS_FPS_CHOICES) / sizeof(SETTINGS_FPS_CHOICES[0]);

const int SETTINGS_FONT_SCALE_CHOICES[] = { 75, 100, 125, 150, 175, 200, 250, 300 };
const int SETTINGS_FONT_SCALE_CHOICE_COUNT = sizeof(SETTINGS_FONT_SCALE_CHOICES) / sizeof(SETTINGS_FONT_SCALE_CHOICES[0]);

const ScreenSize SETTINGS_SCREEN_CHOICES[] = {
    { 96, 128 },  { 101, 128 }, { 128, 128 }, { 128, 160 }, { 132, 176 }, { 176, 208 },
    { 176, 220 }, { 208, 208 }, { 240, 240 }, { 240, 320 }, { 240, 400 }, { 320, 320 },
    { 320, 480 }, { 352, 416 }, { 360, 480 }, { 360, 640 }, { 480, 640 }, { 480, 800 },
    { 540, 960 }, { 720, 1280 },
};
const int SETTINGS_SCREEN_CHOICE_COUNT = sizeof(SETTINGS_SCREEN_CHOICES) / sizeof(SETTINGS_SCREEN_CHOICES[0]);

static Settings current = {
    .fps_limit = 0,
    .screen_w = 240,
    .screen_h = 320,
    .lang = LANG_VI,
    .keymap = 0,
    .scale_mode = 0,
    .show_help = false,
    .show_fps = false,
    .smooth_text = false,
    .system_font = false,
    .font_scale = 100,
    .check_update = true,
    .soundfont = "-",       // mặc định tắt: bộ tổng hợp sóng
};

Settings *settings(void) {
    return &current;
}

bool settings_valid_screen(int w, int h) {
    return w >= SCREEN_MIN && h >= SCREEN_MIN && w <= SCREEN_MAX && h <= SCREEN_MAX;
}

bool settings_load(void) {
    keybind_reset(current.keybinds);
    char path[512];
    snprintf(path, sizeof(path), "%s/settings.ini", platform_data_dir());
    FILE *f = fopen(path, "r");
    if (!f)
        return false;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        int v, w, h, b;
        char id[16];
        if (sscanf(line, "fps_limit=%d", &v) == 1 && v >= 0 && v <= 240)
            current.fps_limit = v;
        else if (sscanf(line, "key_%15[a-z_]=%d", id, &v) == 2 && (b = keybind_from_id(id)) >= 0 && keybind_valid(v))
            current.keybinds[b] = v;
        else if (sscanf(line, "screen=%dx%d", &w, &h) == 2 && settings_valid_screen(w, h)) {
            current.screen_w = w;
            current.screen_h = h;
        } else if (strncmp(line, "lang=", 5) == 0)
            current.lang = lang_from_code(line + 5);
        else if (sscanf(line, "show_help=%d", &v) == 1)
            current.show_help = v != 0;
        else if (sscanf(line, "show_fps=%d", &v) == 1)
            current.show_fps = v != 0;
        else if (sscanf(line, "keymap=%d", &v) == 1 && v >= 0 && v < 16)
            current.keymap = v;
        else if (sscanf(line, "scale_mode=%d", &v) == 1 && v >= 0 && v <= 2)
            current.scale_mode = v;
        else if (sscanf(line, "smooth_text=%d", &v) == 1)
            current.smooth_text = v != 0;
        else if (sscanf(line, "system_font=%d", &v) == 1)
            current.system_font = v != 0;
        else if (sscanf(line, "font_scale=%d", &v) == 1 && v >= 50 && v <= 400)
            current.font_scale = v;
        else if (sscanf(line, "check_update=%d", &v) == 1)
            current.check_update = v != 0;
        else if (sscanf(line, "vkb_bubble=%d", &v) == 1)
            current.vkb_bubble = v != 0;
        else if (strncmp(line, "soundfont=", 10) == 0) {
            snprintf(current.soundfont, sizeof(current.soundfont), "%.127s", line + 10);
            current.soundfont[strcspn(current.soundfont, "\r\n")] = 0;
        }
    }
    fclose(f);
    lang_set((Lang)current.lang);
    return true;
}

bool settings_save(void) {
    mkdir(platform_data_dir(), 0777);
    char path[512];
    snprintf(path, sizeof(path), "%s/settings.ini", platform_data_dir());
    FILE *f = fopen(path, "w");
    if (!f)
        return false;
    fprintf(f, "fps_limit=%d\n", current.fps_limit);
    fprintf(f, "screen=%dx%d\n", current.screen_w, current.screen_h);
    fprintf(f, "lang=%s\n", lang_code((Lang)current.lang));
    fprintf(f, "show_help=%d\n", current.show_help ? 1 : 0);
    fprintf(f, "show_fps=%d\n", current.show_fps ? 1 : 0);
    fprintf(f, "keymap=%d\n", current.keymap);
    fprintf(f, "scale_mode=%d\n", current.scale_mode);
    fprintf(f, "smooth_text=%d\n", current.smooth_text ? 1 : 0);
    fprintf(f, "system_font=%d\n", current.system_font ? 1 : 0);
    fprintf(f, "font_scale=%d\n", current.font_scale);
    fprintf(f, "check_update=%d\n", current.check_update ? 1 : 0);
    fprintf(f, "vkb_bubble=%d\n", current.vkb_bubble ? 1 : 0);
    fprintf(f, "soundfont=%s\n", current.soundfont);
    // Chỉ ghi nút đã đổi khác mặc định
    for (int b = 0; b < BIND_COUNT; b++) {
        if (current.keybinds[b] != keybind_default(b))
            fprintf(f, "key_%s=%d\n", keybind_id(b), current.keybinds[b]);
    }
    return fclose(f) == 0;
}

static int cmp_name(const void *a, const void *b) {
    return strcasecmp((const char *)a, (const char *)b);
}

int settings_list_soundfonts(char names[][128], int max) {
    char dir[512];
    snprintf(dir, sizeof(dir), "%s/soundfonts", platform_data_dir());
    DIR *d = opendir(dir);
    if (!d)
        return 0;
    int n = 0;
    struct dirent *e;
    while (n < max && (e = readdir(d))) {
        size_t len = strlen(e->d_name);
        if (e->d_name[0] != '.' && len > 4 && len < 128 && strcasecmp(e->d_name + len - 4, ".sf2") == 0)
            snprintf(names[n++], 128, "%s", e->d_name);
    }
    closedir(d);
    qsort(names, (size_t)n, 128, cmp_name);
    return n;
}

SoundFontChoice settings_soundfont(char *out, size_t size) {
    const char *name = current.soundfont;
    char names[SOUNDFONT_MAX][128];
    if (strcmp(name, "-") == 0)
        return SOUNDFONT_OFF;
    if (strcmp(name, "builtin") == 0)
        return SOUNDFONT_BUILTIN;
    if (!*name) {
        if (settings_list_soundfonts(names, SOUNDFONT_MAX) == 0)
            return SOUNDFONT_BUILTIN;
        name = names[0];
    }
    snprintf(out, size, "%s/soundfonts/%s", platform_data_dir(), name);
    struct stat st;
    return stat(out, &st) == 0 ? SOUNDFONT_FILE : SOUNDFONT_BUILTIN;
}

static void game_path(const char *game, char *out, size_t size) {
    snprintf(out, size, "%s/options/%s.ini", platform_data_dir(), game);
}

void game_settings_load(const char *game, GameSettings *out) {
    out->fps_limit = -1;
    out->screen_w = out->screen_h = 0;
    out->keymap = -1;
    out->smooth_text = -1;
    out->system_font = -1;
    out->font_scale = -1;
    for (int b = 0; b < BIND_COUNT; b++)
        out->keybinds[b] = BIND_INHERIT;
    char path[512];
    game_path(game, path, sizeof(path));
    FILE *f = fopen(path, "r");
    if (!f)
        return;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        int v, w, h, b;
        char id[16];
        if (sscanf(line, "fps_limit=%d", &v) == 1 && v >= -1 && v <= 240)
            out->fps_limit = v;
        else if (sscanf(line, "key_%15[a-z_]=%d", id, &v) == 2 && (b = keybind_from_id(id)) >= 0 && keybind_valid(v))
            out->keybinds[b] = v;
        else if (sscanf(line, "keymap=%d", &v) == 1 && v >= -1 && v < 16)
            out->keymap = v;
        else if (sscanf(line, "smooth_text=%d", &v) == 1 && v >= -1 && v <= 1)
            out->smooth_text = v;
        else if (sscanf(line, "system_font=%d", &v) == 1 && v >= -1 && v <= 1)
            out->system_font = v;
        else if (sscanf(line, "font_scale=%d", &v) == 1 && (v == -1 || (v >= 50 && v <= 400)))
            out->font_scale = v;
        else if (sscanf(line, "screen=%dx%d", &w, &h) == 2 && settings_valid_screen(w, h)) {
            out->screen_w = w;
            out->screen_h = h;
        }
    }
    fclose(f);
}

bool game_settings_save(const char *game, const GameSettings *gs) {
    char dir[512];
    mkdir(platform_data_dir(), 0777);
    snprintf(dir, sizeof(dir), "%s/options", platform_data_dir());
    mkdir(dir, 0777);
    char path[512];
    game_path(game, path, sizeof(path));
    // Toàn mặc định thì xoá file cho gọn
    if (gs->fps_limit < 0 && gs->screen_w == 0 && gs->keymap < 0 && gs->smooth_text < 0 &&
        gs->system_font < 0 && gs->font_scale < 0 && keybind_changed(gs->keybinds, true) == 0) {
        remove(path);
        return true;
    }
    FILE *f = fopen(path, "w");
    if (!f)
        return false;
    fprintf(f, "fps_limit=%d\n", gs->fps_limit);
    if (gs->screen_w)
        fprintf(f, "screen=%dx%d\n", gs->screen_w, gs->screen_h);
    fprintf(f, "keymap=%d\n", gs->keymap);
    fprintf(f, "smooth_text=%d\n", gs->smooth_text);
    fprintf(f, "system_font=%d\n", gs->system_font);
    fprintf(f, "font_scale=%d\n", gs->font_scale);
    for (int b = 0; b < BIND_COUNT; b++) {
        if (gs->keybinds[b] != BIND_INHERIT)
            fprintf(f, "key_%s=%d\n", keybind_id(b), gs->keybinds[b]);
    }
    return fclose(f) == 0;
}
