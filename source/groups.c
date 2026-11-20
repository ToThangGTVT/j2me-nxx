#include "groups.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "lang.h"
#include "platform.h"

// groups.ini:
//   [Tên nhóm]
//   id ứng dụng
//   ...
typedef struct {
    char name[GROUP_NAME_MAX];
    char (*games)[256];
    int count, cap;
} Group;

static Group groups[GROUP_MAX];
static int count;
static bool loaded;

static void groups_path(char *out, size_t size) {
    snprintf(out, size, "%s/groups.ini", platform_data_dir());
}

// Tên hợp lệ: bỏ xuống dòng, dấu [ ], khoảng trắng đầu / cuối, cắt theo ký tự UTF-8; rỗng thì false
static bool clean_name(const char *in, char *out) {
    size_t n = 0;
    for (const unsigned char *p = (const unsigned char *)in; *p;) {
        size_t len = *p < 0x80 ? 1 : *p >= 0xF0 ? 4 : *p >= 0xE0 ? 3 : *p >= 0xC0 ? 2 : 1;
        if (n + len > GROUP_NAME_MAX - 1)
            break;
        bool skip = *p == '\r' || *p == '\n' || *p == '[' || *p == ']' || (*p == ' ' && n == 0);
        for (size_t k = 0; k < len && p[k]; k++) {
            if (!skip)
                out[n++] = (char)p[k];
        }
        for (size_t k = 0; k < len && *p; k++)
            p++;
    }
    while (n > 0 && out[n - 1] == ' ')
        n--;
    out[n] = 0;
    return n > 0;
}

static int find_game(const Group *g, const char *game) {
    for (int i = 0; i < g->count; i++) {
        if (strcmp(g->games[i], game) == 0)
            return i;
    }
    return -1;
}

static void push_game(Group *g, const char *game) {
    if (!*game || find_game(g, game) >= 0)
        return;
    if (g->count == g->cap) {
        int cap = g->cap ? g->cap * 2 : 16;
        void *p = realloc(g->games, (size_t)cap * sizeof(*g->games));
        if (!p)
            return;
        g->games = p;
        g->cap = cap;
    }
    snprintf(g->games[g->count++], sizeof(g->games[0]), "%s", game);
}

static void load(void) {
    if (loaded)
        return;
    loaded = true;
    char path[512];
    groups_path(path, sizeof(path));
    FILE *f = fopen(path, "r");
    if (!f) {
        // Lần đầu: một nhóm Yêu thích, tên theo ngôn ngữ hiện tại
        snprintf(groups[0].name, sizeof(groups[0].name), "%s", tr(S_GROUP_FAVORITES));
        count = 1;
        return;
    }
    char line[300];
    Group *cur = NULL;
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = 0;
        size_t len = strlen(line);
        if (len >= 2 && line[0] == '[' && line[len - 1] == ']') {
            line[len - 1] = 0;
            cur = NULL;
            if (count < GROUP_MAX && clean_name(line + 1, groups[count].name))
                cur = &groups[count++];
        } else if (cur && len) {
            push_game(cur, line);
        }
    }
    fclose(f);
}

static bool save(void) {
    mkdir(platform_data_dir(), 0777);
    char path[512];
    groups_path(path, sizeof(path));
    FILE *f = fopen(path, "w");
    if (!f)
        return false;
    for (int g = 0; g < count; g++) {
        fprintf(f, "[%s]\n", groups[g].name);
        for (int i = 0; i < groups[g].count; i++)
            fprintf(f, "%s\n", groups[g].games[i]);
    }
    return fclose(f) == 0;
}

int group_count(void) {
    load();
    return count;
}

const char *group_name(int g) {
    load();
    return g >= 0 && g < count ? groups[g].name : "";
}

bool group_has(int g, const char *game) {
    load();
    return g >= 0 && g < count && find_game(&groups[g], game) >= 0;
}

bool group_set(int g, const char *game, bool on) {
    load();
    if (g < 0 || g >= count)
        return false;
    Group *gr = &groups[g];
    int i = find_game(gr, game);
    if (on && i < 0)
        push_game(gr, game);
    else if (!on && i >= 0) {
        memmove(gr->games[i], gr->games[i + 1], (size_t)(gr->count - i - 1) * sizeof(*gr->games));
        gr->count--;
    }
    return save();
}

int group_add(const char *name) {
    load();
    char clean[GROUP_NAME_MAX];
    if (count >= GROUP_MAX || !clean_name(name, clean))
        return -1;
    memset(&groups[count], 0, sizeof(groups[count]));
    snprintf(groups[count].name, sizeof(groups[count].name), "%s", clean);
    count++;
    save();
    return count - 1;
}

bool group_rename(int g, const char *name) {
    load();
    char clean[GROUP_NAME_MAX];
    if (g < 0 || g >= count || !clean_name(name, clean))
        return false;
    snprintf(groups[g].name, sizeof(groups[g].name), "%s", clean);
    return save();
}

bool group_delete(int g) {
    load();
    if (g < 0 || g >= count)
        return false;
    free(groups[g].games);
    memmove(&groups[g], &groups[g + 1], (size_t)(count - g - 1) * sizeof(groups[0]));
    count--;
    memset(&groups[count], 0, sizeof(groups[count]));
    return save();
}
