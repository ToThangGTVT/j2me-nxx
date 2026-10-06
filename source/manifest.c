#include "manifest.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "vm/zip.h"

static char *trim(char *s) {
    while (*s && isspace((unsigned char)*s))
        s++;
    char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1]))
        *--e = '\0';
    return s;
}

static void set_prop(Manifest *m, const char *key, const char *value, bool override) {
    for (int i = 0; i < m->count; i++) {
        if (strcmp(m->props[i].key, key) == 0) {
            if (override) {
                free(m->props[i].value);
                m->props[i].value = strdup(value);
            }
            return;
        }
    }
    m->props = realloc(m->props, sizeof(ManifestProp) * (m->count + 1));
    m->props[m->count].key = strdup(key);
    m->props[m->count].value = strdup(value);
    m->count++;
}

// Dòng bắt đầu bằng dấu cách là phần nối của dòng trước
void manifest_parse(Manifest *m, const char *text, bool override) {
    char *buf = strdup(text);
    char *key = NULL;
    char *value = NULL;
    char *save = NULL;
    for (char *line = strtok_r(buf, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
        size_t len = strlen(line);
        if (len && line[len - 1] == '\r')
            line[--len] = '\0';
        if (line[0] == ' ' && value) {
            size_t vl = strlen(value);
            value = realloc(value, vl + len);
            memcpy(value + vl, line + 1, len);
            continue;
        }
        if (key) {
            set_prop(m, key, trim(value), override);
            free(key);
            free(value);
            key = value = NULL;
        }
        char *colon = strchr(line, ':');
        if (!colon)
            continue;
        *colon = '\0';
        key = strdup(trim(line));
        value = strdup(colon + 1);
    }
    if (key) {
        set_prop(m, key, trim(value), override);
        free(key);
        free(value);
    }
    free(buf);
}

const char *manifest_get(const Manifest *m, const char *key) {
    for (int i = 0; i < m->count; i++) {
        if (strcmp(m->props[i].key, key) == 0)
            return m->props[i].value;
    }
    for (int i = 0; i < m->count; i++) {
        if (strcasecmp(m->props[i].key, key) == 0)
            return m->props[i].value;
    }
    return NULL;
}

void manifest_free(Manifest *m) {
    for (int i = 0; i < m->count; i++) {
        free(m->props[i].key);
        free(m->props[i].value);
    }
    free(m->props);
    m->props = NULL;
    m->count = 0;
}

void manifest_load(Manifest *m, const char *jar_path, ZipFile *zip) {
    char jad[600];
    snprintf(jad, sizeof(jad), "%s", jar_path);
    char *dot = strrchr(jad, '.');
    if (dot && strlen(dot) == 4) {
        strcpy(dot, ".jad");
        FILE *f = fopen(jad, "rb");
        if (f) {
            char *buf = calloc(1, 65536);
            fread(buf, 1, 65535, f);
            fclose(f);
            manifest_parse(m, buf, true);
            free(buf);
        }
    }
    size_t size = 0;
    char *mf = (char *)zip_read(zip, "META-INF/MANIFEST.MF", &size);
    if (mf) {
        manifest_parse(m, mf, false);
        free(mf);
    }
}

bool manifest_midlet_field(const Manifest *m, int field, char *out, size_t size) {
    return manifest_midlet_entry(m, 1, field, out, size);
}

int manifest_midlet_count(const Manifest *m) {
    int n = 0;
    char key[32];
    for (;;) {
        snprintf(key, sizeof(key), "MIDlet-%d", n + 1);
        if (!manifest_get(m, key))
            return n;
        n++;
    }
}

bool manifest_midlet_entry(const Manifest *m, int index, int field, char *out, size_t size) {
    char key[32];
    snprintf(key, sizeof(key), "MIDlet-%d", index);
    const char *v = manifest_get(m, key);
    if (!v)
        return false;
    for (int i = 0; i < field; i++) {
        v = strchr(v, ',');
        if (!v)
            return false;
        v++;
    }
    const char *end = strchr(v, ',');
    size_t len = end ? (size_t)(end - v) : strlen(v);
    char buf[256];
    if (len >= sizeof(buf))
        len = sizeof(buf) - 1;
    memcpy(buf, v, len);
    buf[len] = '\0';
    char *t = trim(buf);
    if (!*t)
        return false;
    snprintf(out, size, "%s", t);
    return true;
}
