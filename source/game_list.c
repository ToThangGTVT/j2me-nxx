#include "game_list.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#include "manifest.h"
#include "third_party/stb_image.h"
#include "vm/zip.h"

#define MAX_GAMES 2048
#define MAX_DEPTH 3

static const char *demo_names[] = {
    "Bounce Tales.jar", "Snake Xenzia.jar", "Space Impact.jar",
    "Gunbound.jar", "Ninja School Online.jar", "Ngoc Rong Online.jar",
    "Avatar.jar", "Army Online.jar", "Asphalt 4.jar", "Prince of Persia.jar",
    "Diamond Rush.jar", "Bubble Bash.jar", "Brick Breaker Revolution.jar",
    "Tetris.jar", "Zuma.jar", "Need for Speed Most Wanted.jar",
    "Assassin's Creed.jar", "Real Football 2009.jar", "Gangstar 2.jar",
    "Block Breaker Deluxe.jar", "Midnight Pool.jar", "Opera Mini 4.jar",
    "Ultimate Spider-Man.jar", "Rally Pro Contest.jar", "Bomberman.jar",
    "Sky Force.jar", "Contra 4.jar", "Metal Slug Mobile.jar",
    "Pac-Man.jar", "Crash Nitro Kart.jar", "Doom RPG.jar",
    "Wolfenstein RPG.jar", "Orcs & Elves.jar", "Townsmen 6.jar",
    "Heroes Lore.jar", "Age of Empires III.jar", "Worms 2008.jar",
    "Galaxy on Fire.jar", "Siberian Strike.jar", "Hero of Sparta.jar",
    "Call of Duty 4.jar", "Splinter Cell.jar", "Lumines.jar",
};

static int cmp_entries(const void *a, const void *b) {
    return strcasecmp(((const GameEntry *)a)->name, ((const GameEntry *)b)->name);
}

static bool has_jar_ext(const char *name) {
    size_t n = strlen(name);
    return n > 4 && strcasecmp(name + n - 4, ".jar") == 0;
}

static void base_title(GameEntry *g) {
    const char *s = strrchr(g->name, '/');
    snprintf(g->title, sizeof(g->title), "%s", s ? s + 1 : g->name);
    size_t n = strlen(g->title);
    if (n > 4 && strcasecmp(g->title + n - 4, ".jar") == 0)
        g->title[n - 4] = '\0';
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
        if (!has_jar_ext(ent->d_name))
            continue;
        GameEntry *g = &list->items[list->count++];
        memset(g, 0, sizeof(*g));
        snprintf(g->name, sizeof(g->name), "%s", child_rel);
        snprintf(g->path, sizeof(g->path), "%s", full);
        g->size = (long)st.st_size;
        base_title(g);
    }
    closedir(d);
}

void game_list_scan(GameList *list, const char *dir) {
    game_list_free(list);
    list->items = calloc(MAX_GAMES, sizeof(GameEntry));
    if (!list->items)
        return;

    // Tạo sẵn thư mục để người dùng biết chép game vào đâu
    mkdir(dir, 0777);
    scan_dir(list, dir, "", 0);

    if (list->count == 0) {
        list->demo = true;
        int n = sizeof(demo_names) / sizeof(demo_names[0]);
        for (int i = 0; i < n; i++) {
            GameEntry *g = &list->items[i];
            snprintf(g->name, sizeof(g->name), "%s", demo_names[i]);
            g->size = -1;
            g->info_loaded = true;
            g->valid = true;
            base_title(g);
        }
        list->count = n;
    }

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
    ZipFile *z = zip_open_file_lazy(g->path);
    if (!z)
        return;
    Manifest m = { 0 };
    manifest_load(&m, g->path, z);

    char cls[256];
    g->valid = manifest_midlet_field(&m, 2, cls, sizeof(cls));
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
    list->demo = false;
}
