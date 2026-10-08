#include "lang.h"

#include <string.h>

static Lang current = LANG_VI;

extern const char *const lang_strings_vi[S_COUNT];
extern const char *const lang_strings_en[S_COUNT];

static const char *const *const strings[LANG_COUNT] = {
    [LANG_VI] = lang_strings_vi,
    [LANG_EN] = lang_strings_en,
};

void lang_set(Lang l) {
    if (l >= 0 && l < LANG_COUNT)
        current = l;
}

Lang lang_get(void) {
    return current;
}

const char *lang_code(Lang l) {
    return l == LANG_EN ? "en" : "vi";
}

const char *lang_name(Lang l) {
    return l == LANG_EN ? "English" : "Tiếng Việt";
}

Lang lang_from_code(const char *code) {
    return code && strncmp(code, "en", 2) == 0 ? LANG_EN : LANG_VI;
}

const char *tr(StrId id) {
    const char *s = id >= 0 && id < S_COUNT ? strings[current][id] : NULL;
    return s ? s : "?";
}
