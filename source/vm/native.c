// Bảng native method
#include "vm_internal.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *cls, *name, *desc;
    NativeFn fn;
} NativeEntry;

static NativeEntry *entries;
static int count, cap;

void native_register(const char *cls, const char *name, const char *desc, NativeFn fn) {
    for (int i = 0; i < count; i++) {
        NativeEntry *e = &entries[i];
        if (strcmp(e->cls, cls) == 0 && strcmp(e->name, name) == 0 && strcmp(e->desc, desc) == 0) {
            e->fn = fn;
            return;
        }
    }
    if (count == cap) {
        cap = cap ? cap * 2 : 256;
        entries = realloc(entries, (size_t)cap * sizeof(NativeEntry));
    }
    entries[count++] = (NativeEntry){ cls, name, desc, fn };
}

NativeFn native_lookup(const char *cls, const char *name, const char *desc) {
    for (int i = 0; i < count; i++) {
        NativeEntry *e = &entries[i];
        if (strcmp(e->name, name) == 0 && strcmp(e->cls, cls) == 0 && strcmp(e->desc, desc) == 0)
            return e->fn;
    }
    vm_log("Thieu native: %s.%s%s", cls, name, desc);
    return NULL;
}

void native_free_all(void) {
    free(entries);
    entries = NULL;
    count = cap = 0;
}
