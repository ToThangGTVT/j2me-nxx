#include "settings.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "platform.h"

const int SETTINGS_FPS_CHOICES[] = { 0, 15, 20, 30, 60 };
const int SETTINGS_FPS_CHOICE_COUNT = sizeof(SETTINGS_FPS_CHOICES) / sizeof(SETTINGS_FPS_CHOICES[0]);

static Settings current = {
    .fps_limit = 0,
};

Settings *settings(void) {
    return &current;
}

static void settings_path(char *out, size_t size) {
    snprintf(out, size, "%s/settings.ini", platform_data_dir());
}

void settings_load(void) {
    char path[512];
    settings_path(path, sizeof(path));
    FILE *f = fopen(path, "r");
    if (!f)
        return;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        int v;
        if (sscanf(line, "fps_limit=%d", &v) == 1 && v >= 0 && v <= 240)
            current.fps_limit = v;
    }
    fclose(f);
}

bool settings_save(void) {
    mkdir(platform_data_dir(), 0777);
    char path[512];
    settings_path(path, sizeof(path));
    FILE *f = fopen(path, "w");
    if (!f)
        return false;
    fprintf(f, "fps_limit=%d\n", current.fps_limit);
    return fclose(f) == 0;
}
