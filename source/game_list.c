#include "game_list.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#define MAX_GAMES 1024

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

void game_list_scan(GameList *list, const char *dir) {
    game_list_free(list);
    list->items = calloc(MAX_GAMES, sizeof(GameEntry));
    if (!list->items)
        return;

    DIR *d = opendir(dir);
    if (d) {
        struct dirent *ent;
        while ((ent = readdir(d)) && list->count < MAX_GAMES) {
            if (!has_jar_ext(ent->d_name))
                continue;
            GameEntry *g = &list->items[list->count++];
            snprintf(g->name, sizeof(g->name), "%s", ent->d_name);
            snprintf(g->path, sizeof(g->path), "%s/%s", dir, ent->d_name);

            struct stat st;
            g->size = stat(g->path, &st) == 0 ? (long)st.st_size : 0;
        }
        closedir(d);
    }

    if (list->count == 0) {
        list->demo = true;
        int n = sizeof(demo_names) / sizeof(demo_names[0]);
        for (int i = 0; i < n; i++) {
            snprintf(list->items[i].name, sizeof(list->items[i].name), "%s", demo_names[i]);
            list->items[i].size = -1;
        }
        list->count = n;
    }

    qsort(list->items, list->count, sizeof(GameEntry), cmp_entries);
}

void game_list_id(const GameEntry *g, char *out, size_t size) {
    snprintf(out, size, "%s", g->name);
    size_t n = strlen(out);
    if (n > 4 && strcasecmp(out + n - 4, ".jar") == 0)
        out[n - 4] = '\0';
}

void game_list_free(GameList *list) {
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->demo = false;
}
