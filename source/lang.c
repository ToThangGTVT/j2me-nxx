#include "lang.h"

#include <string.h>

static Lang current = LANG_VI;

extern const char *const lang_strings_vi[S_COUNT];
extern const char *const lang_strings_en[S_COUNT];
extern const char *const lang_strings_zh[S_COUNT];
extern const char *const lang_strings_ru[S_COUNT];

static const char *const *const strings[LANG_COUNT] = {
    [LANG_VI] = lang_strings_vi,
    [LANG_EN] = lang_strings_en,
    [LANG_ZH] = lang_strings_zh,
    [LANG_RU] = lang_strings_ru,
};

static const char *const codes[LANG_COUNT] = { "vi", "en", "zh", "ru" };
static const char *const names[LANG_COUNT] = { "Tiếng Việt", "English", "简体中文", "Русский" };

void lang_set(Lang l) {
    if (l >= 0 && l < LANG_COUNT)
        current = l;
}

Lang lang_get(void) {
    return current;
}

const char *lang_code(Lang l) {
    return codes[l >= 0 && l < LANG_COUNT ? l : LANG_VI];
}

const char *lang_name(Lang l) {
    return names[l >= 0 && l < LANG_COUNT ? l : LANG_VI];
}

Lang lang_from_code(const char *code) {
    for (int i = 0; code && i < LANG_COUNT; i++) {
        if (strncmp(code, codes[i], 2) == 0)
            return (Lang)i;
    }
    return LANG_VI;
}

const char *tr(StrId id) {
    const char *s = id >= 0 && id < S_COUNT ? strings[current][id] : NULL;
    return s ? s : "?";
}
