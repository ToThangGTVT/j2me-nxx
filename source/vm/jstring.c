// java.lang.String: tạo, intern, chuyển đổi UTF-8
#include "vm_internal.h"

#include <stdlib.h>
#include <string.h>

typedef struct InternStr {
    struct InternStr *next;
    uint32_t hash;
    Object *str;
} InternStr;

#define BUCKETS 2048
static InternStr *table[BUCKETS];

void jstring_init(void) {
}

int jstring_length(Object *s) {
    return FIELD_I(s, FS_String_count);
}

const jchar *jstring_chars(Object *s) {
    Object *v = FIELD_L(s, FS_String_value);
    return ARRAY_DATA(v, jchar) + FIELD_I(s, FS_String_offset);
}

Object *jstring_new_chars(VMThread *t, const jchar *chars, int len) {
    Object *arr = heap_new_prim_array(t, 'C', len);
    if (!arr)
        return NULL;
    if (len)
        memcpy(ARRAY_DATA(arr, jchar), chars, (size_t)len * 2);
    Object *s = heap_alloc_object(t, class_string());
    if (!s)
        return NULL;
    FIELD_L(s, FS_String_value) = arr;
    FIELD_I(s, FS_String_offset) = 0;
    FIELD_I(s, FS_String_count) = len;
    return s;
}

// Giải mã UTF-8 (chuẩn hoặc modified) sang UTF-16. Trả về số jchar.
static int decode_utf8(const char *s, jchar *out) {
    const uint8_t *p = (const uint8_t *)s;
    int n = 0;
    while (*p) {
        uint32_t c = *p;
        if (c < 0x80) {
            p++;
        } else if ((c & 0xE0) == 0xC0 && p[1]) {
            c = ((c & 0x1F) << 6) | (p[1] & 0x3F);
            p += 2;
        } else if ((c & 0xF0) == 0xE0 && p[1] && p[2]) {
            c = ((c & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
            p += 3;
        } else if ((c & 0xF8) == 0xF0 && p[1] && p[2] && p[3]) {
            c = ((c & 0x07) << 18) | ((p[1] & 0x3F) << 12) | ((p[2] & 0x3F) << 6) | (p[3] & 0x3F);
            p += 4;
            if (out) {
                c -= 0x10000;
                out[n] = (jchar)(0xD800 + (c >> 10));
                out[n + 1] = (jchar)(0xDC00 + (c & 0x3FF));
            }
            n += 2;
            continue;
        } else {
            c = '?';
            p++;
        }
        if (out)
            out[n] = (jchar)c;
        n++;
    }
    return n;
}

Object *jstring_new_utf8(VMThread *t, const char *s) {
    int len = decode_utf8(s, NULL);
    jchar stack_buf[256];
    jchar *buf = len <= 256 ? stack_buf : malloc((size_t)len * 2);
    decode_utf8(s, buf);
    Object *r = jstring_new_chars(t, buf, len);
    if (buf != stack_buf)
        free(buf);
    return r;
}

static uint32_t chars_hash(const jchar *c, int len) {
    uint32_t h = 0;
    for (int i = 0; i < len; i++)
        h = 31 * h + c[i];
    return h;
}

static Object *intern_lookup(const jchar *c, int len, uint32_t h) {
    for (InternStr *e = table[h % BUCKETS]; e; e = e->next) {
        if (e->hash == h && jstring_length(e->str) == len &&
            memcmp(jstring_chars(e->str), c, (size_t)len * 2) == 0)
            return e->str;
    }
    return NULL;
}

static void intern_insert(Object *s, uint32_t h) {
    InternStr *e = malloc(sizeof(InternStr));
    e->hash = h;
    e->str = s;
    e->next = table[h % BUCKETS];
    table[h % BUCKETS] = e;
}

Object *jstring_intern(VMThread *t, Object *s) {
    (void)t;
    int len = jstring_length(s);
    const jchar *c = jstring_chars(s);
    uint32_t h = chars_hash(c, len);
    Object *found = intern_lookup(c, len, h);
    if (found)
        return found;
    intern_insert(s, h);
    return s;
}

Object *jstring_intern_utf8(VMThread *t, const char *utf8) {
    int len = decode_utf8(utf8, NULL);
    jchar stack_buf[256];
    jchar *buf = len <= 256 ? stack_buf : malloc((size_t)len * 2);
    decode_utf8(utf8, buf);
    uint32_t h = chars_hash(buf, len);
    Object *s = intern_lookup(buf, len, h);
    if (!s) {
        s = jstring_new_chars(t, buf, len);
        if (s)
            intern_insert(s, h);
    }
    if (buf != stack_buf)
        free(buf);
    return s;
}

char *jstring_to_utf8(Object *s) {
    int len = jstring_length(s);
    const jchar *c = jstring_chars(s);
    char *out = malloc((size_t)len * 3 + 1);
    size_t n = 0;
    for (int i = 0; i < len; i++) {
        uint32_t ch = c[i];
        if (ch >= 0xD800 && ch < 0xDC00 && i + 1 < len && c[i + 1] >= 0xDC00 && c[i + 1] < 0xE000) {
            ch = 0x10000 + ((ch - 0xD800) << 10) + (c[i + 1] - 0xDC00);
            i++;
        }
        if (ch < 0x80) {
            out[n++] = (char)ch;
        } else if (ch < 0x800) {
            out[n++] = (char)(0xC0 | (ch >> 6));
            out[n++] = (char)(0x80 | (ch & 0x3F));
        } else if (ch < 0x10000) {
            out[n++] = (char)(0xE0 | (ch >> 12));
            out[n++] = (char)(0x80 | ((ch >> 6) & 0x3F));
            out[n++] = (char)(0x80 | (ch & 0x3F));
        } else {
            out = realloc(out, n + 4 + (size_t)(len - i) * 3 + 1);
            out[n++] = (char)(0xF0 | (ch >> 18));
            out[n++] = (char)(0x80 | ((ch >> 12) & 0x3F));
            out[n++] = (char)(0x80 | ((ch >> 6) & 0x3F));
            out[n++] = (char)(0x80 | (ch & 0x3F));
        }
    }
    out[n] = '\0';
    return out;
}

char *jstring_to_cstr(Object *s, char *buf, size_t size) {
    if (!size)
        return buf;
    if (!s) {
        buf[0] = '\0';
        return buf;
    }
    char *u = jstring_to_utf8(s);
    strncpy(buf, u, size - 1);
    buf[size - 1] = '\0';
    free(u);
    return buf;
}

void jstring_mark_roots(MarkFn mark) {
    for (int i = 0; i < BUCKETS; i++) {
        for (InternStr *e = table[i]; e; e = e->next)
            mark(e->str);
    }
}

void jstring_free_all(void) {
    for (int i = 0; i < BUCKETS; i++) {
        InternStr *e = table[i];
        while (e) {
            InternStr *n = e->next;
            free(e);
            e = n;
        }
        table[i] = NULL;
    }
}
