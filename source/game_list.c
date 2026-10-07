#include "game_list.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#include "manifest.h"
#include "platform.h"
#include "third_party/stb_image.h"
#include "vm/zip.h"

#define MAX_GAMES 2048
#define MAX_DEPTH 3

static int cmp_entries(const void *a, const void *b) {
    return strcasecmp(((const GameEntry *)a)->name, ((const GameEntry *)b)->name);
}

static bool has_jar_ext(const char *name) {
    size_t n = strlen(name);
    return n > 4 && strcasecmp(name + n - 4, ".jar") == 0;
}

const char *const game_list_video_exts[] = { ".3gp", ".3g2", ".mp4", ".m4v", ".mov", ".avi", ".mkv",
                                              ".webm", ".flv", ".mpg", ".mpeg", ".ts", ".wmv", ".asf" };
const int game_list_video_ext_count = (int)(sizeof(game_list_video_exts) / sizeof(game_list_video_exts[0]));

bool game_list_is_video(const char *name) {
    const char *dot = strrchr(name, '.');
    if (!dot)
        return false;
    for (int i = 0; i < game_list_video_ext_count; i++) {
        if (strcasecmp(dot, game_list_video_exts[i]) == 0)
            return true;
    }
    return false;
}

static void base_title(GameEntry *g) {
    const char *s = strrchr(g->name, '/');
    snprintf(g->title, sizeof(g->title), "%.127s", s ? s + 1 : g->name);
    char *dot = strrchr(g->title, '.');
    if (dot && (strcasecmp(dot, ".jar") == 0 || g->video))
        *dot = '\0';
}

static void scan_dir(GameList *list, const char *root, const char *rel, int depth) {
    char dir_path[600];
    if (rel[0])
        snprintf(dir_path, sizeof(dir_path), "%s/%s", root, rel);
    else
        snprintf(dir_path, sizeof(dir_path), "%s", root);
    DIR *d = opendir(dir_path);
    if (!d)
        return;
    struct dirent *ent;
    while ((ent = readdir(d)) && list->count < MAX_GAMES) {
        if (ent->d_name[0] == '.')
            continue;
        char child_rel[256];
        if (rel[0])
            snprintf(child_rel, sizeof(child_rel), "%s/%s", rel, ent->d_name);
        else
            snprintf(child_rel, sizeof(child_rel), "%s", ent->d_name);
        char full[600];
        snprintf(full, sizeof(full), "%s/%s", root, child_rel);

        struct stat st;
        if (stat(full, &st) != 0)
            continue;
        if (S_ISDIR(st.st_mode)) {
            if (depth < MAX_DEPTH)
                scan_dir(list, root, child_rel, depth + 1);
            continue;
        }
        bool video = game_list_is_video(ent->d_name);
        if (!has_jar_ext(ent->d_name) && !video)
            continue;
        GameEntry *g = &list->items[list->count++];
        memset(g, 0, sizeof(*g));
        g->video = video;
        snprintf(g->name, sizeof(g->name), "%s", child_rel);
        if (strlen(full) >= sizeof(g->path)) {
            list->count--;
            continue;
        }
        snprintf(g->path, sizeof(g->path), "%.511s", full);
        g->size = (long)st.st_size;
        base_title(g);
    }
    closedir(d);
}

// Tạo thư mục và thư mục cha (vd "sdmc:/switch/j2me-nxx" rồi ".../games")
static void make_dirs(const char *dir) {
    char tmp[512];
    snprintf(tmp, sizeof(tmp), "%s", dir);
    char *slash = strrchr(tmp, '/');
    if (slash) {
        *slash = '\0';
        mkdir(tmp, 0777);
    }
    mkdir(dir, 0777);
}

void game_list_scan(GameList *list, const char *dir) {
    game_list_free(list);
    list->items = calloc(MAX_GAMES, sizeof(GameEntry));
    if (!list->items)
        return;

    // Tạo sẵn thư mục để người dùng biết chép game vào đâu
    make_dirs(dir);
    char sf_dir[512];   // chỗ chép file SoundFont .sf2
    snprintf(sf_dir, sizeof(sf_dir), "%s/soundfonts", platform_data_dir());
    mkdir(sf_dir, 0777);
    scan_dir(list, dir, "", 0);

    qsort(list->items, list->count, sizeof(GameEntry), cmp_entries);
}

// Giải mã icon PNG (thường 16-64px) sang ARGB
static void load_icon(GameEntry *g, ZipFile *z, const char *icon_name) {
    size_t size = 0;
    uint8_t *data = zip_read(z, icon_name, &size);
    if (!data)
        return;
    int w, h, comp;
    uint8_t *rgba = stbi_load_from_memory(data, (int)size, &w, &h, &comp, 4);
    free(data);
    if (!rgba)
        return;
    if (w > 0 && h > 0 && w <= 256 && h <= 256) {
        g->icon = malloc((size_t)w * h * 4);
        for (int i = 0; i < w * h; i++)
            g->icon[i] = ((uint32_t)rgba[i * 4 + 3] << 24) | ((uint32_t)rgba[i * 4] << 16) |
                         ((uint32_t)rgba[i * 4 + 1] << 8) | rgba[i * 4 + 2];
        g->icon_w = w;
        g->icon_h = h;
    }
    stbi_image_free(rgba);
}

void game_list_load_info(GameEntry *g) {
    if (g->info_loaded)
        return;
    g->info_loaded = true;
    if (g->video) {
        g->valid = true;
        return;
    }
    ZipFile *z = zip_open_file_lazy(g->path);
    if (!z)
        return;
    Manifest m = { 0 };
    manifest_load(&m, g->path, z);

    char cls[256];
    g->valid = manifest_midlet_field(&m, 2, cls, sizeof(cls));
    g->midlet_count = manifest_midlet_count(&m);
    if (g->midlet_count > 8)
        g->midlet_count = 8;
    for (int i = 0; i < g->midlet_count; i++) {
        if (!manifest_midlet_entry(&m, i + 1, 0, g->midlets[i], sizeof(g->midlets[i])))
            snprintf(g->midlets[i], sizeof(g->midlets[i]), "MIDlet-%d", i + 1);
    }
    const char *v;
    if ((v = manifest_get(&m, "MIDlet-Name")) && *v)
        snprintf(g->title, sizeof(g->title), "%s", v);
    if ((v = manifest_get(&m, "MIDlet-Vendor")))
        snprintf(g->vendor, sizeof(g->vendor), "%s", v);
    if ((v = manifest_get(&m, "MIDlet-Version")))
        snprintf(g->version, sizeof(g->version), "%s", v);

    char icon[256] = "";
    if ((v = manifest_get(&m, "MIDlet-Icon")) && *v)
        snprintf(icon, sizeof(icon), "%s", v);
    else
        manifest_midlet_field(&m, 1, icon, sizeof(icon));
    if (icon[0])
        load_icon(g, z, icon);

    manifest_free(&m);
    zip_close(z);
}

void game_list_id(const GameEntry *g, char *out, size_t size) {
    // "RPG/abc.jar" -> "RPG_abc": tên phẳng để làm tên file / thư mục
    snprintf(out, size, "%s", g->name);
    size_t n = strlen(out);
    if (n > 4 && strcasecmp(out + n - 4, ".jar") == 0)
        out[n - 4] = '\0';
    for (char *p = out; *p; p++) {
        if (*p == '/')
            *p = '_';
    }
}

void game_list_free(GameList *list) {
    if (list->items) {
        for (int i = 0; i < list->count; i++)
            free(list->items[i].icon);
    }
    free(list->items);
    list->items = NULL;
    list->count = 0;
}
