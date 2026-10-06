#include "vm_internal.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

// Ngưỡng cấp phát giữa 2 lần GC
#define GC_MIN_THRESHOLD    (4u * 1024 * 1024)
// Dung lượng "máy" báo cho game qua Runtime.totalMemory()
#define REPORTED_TOTAL      (16u * 1024 * 1024)

static Object **objs;
static size_t obj_count, obj_cap;

// Hash set địa chỉ object (để quét stack bảo thủ)
static Object **set;
static size_t set_cap, set_used;
#define SET_TOMB ((Object *)1)

static size_t used_bytes;
static size_t alloc_since_gc;
static size_t gc_threshold = GC_MIN_THRESHOLD;
static bool gc_requested;
static uint32_t next_hash = 0x9e3779b9u;

static Object ***roots;
static size_t root_count, root_cap;

static Object **mark_stack;
static size_t mark_top, mark_cap;

static size_t ptr_hash(const void *p) {
    uintptr_t x = (uintptr_t)p;
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33;
    return (size_t)x;
}

static void set_insert_raw(Object *o) {
    size_t mask = set_cap - 1;
    size_t i = ptr_hash(o) & mask;
    while (set[i] && set[i] != SET_TOMB)
        i = (i + 1) & mask;
    if (!set[i])
        set_used++;
    set[i] = o;
}

static void set_rebuild(size_t min_cap) {
    size_t cap = 1024;
    while (cap < min_cap * 2)
        cap <<= 1;
    free(set);
    set = calloc(cap, sizeof(Object *));
    set_cap = cap;
    set_used = 0;
    for (size_t i = 0; i < obj_count; i++)
        set_insert_raw(objs[i]);
}

bool heap_is_object(const void *p) {
    if (!p || !set_cap || ((uintptr_t)p & 7))
        return false;
    size_t mask = set_cap - 1;
    size_t i = ptr_hash(p) & mask;
    while (set[i]) {
        if (set[i] == p)
            return true;
        i = (i + 1) & mask;
    }
    return false;
}

static Object *alloc_raw(VMThread *t, Class *c, size_t size) {
    if (obj_count == obj_cap) {
        size_t cap = obj_cap ? obj_cap * 2 : 4096;
        Object **n = realloc(objs, cap * sizeof(Object *));
        if (!n)
            goto oom;
        objs = n;
        obj_cap = cap;
    }
    if ((set_used + 1) * 2 > set_cap)
        set_rebuild(obj_count + 1);

    Object *o = calloc(1, size);
    if (!o)
        goto oom;
    o->cls = c;
    next_hash = next_hash * 1103515245u + 12345u;
    o->hash = next_hash >> 1;
    objs[obj_count++] = o;
    set_insert_raw(o);

    used_bytes += size;
    alloc_since_gc += size;
    if (alloc_since_gc > gc_threshold)
        gc_requested = true;
    return o;

oom:
    gc_requested = true;
    if (t)
        throw_new(t, "java/lang/OutOfMemoryError", NULL);
    return NULL;
}

Object *heap_alloc_object(VMThread *t, Class *c) {
    return alloc_raw(t, c, sizeof(Object) + (size_t)c->instance_slots * sizeof(Value));
}

Object *heap_alloc_array(VMThread *t, Class *array_cls, jint len) {
    if (len < 0) {
        if (t)
            throw_new(t, "java/lang/NegativeArraySizeException", NULL);
        return NULL;
    }
    size_t size = sizeof(Array) + (size_t)len * array_cls->elem_size;
    if (size > 64u * 1024 * 1024) {
        if (t)
            throw_new(t, "java/lang/OutOfMemoryError", "array too large");
        return NULL;
    }
    Array *a = (Array *)alloc_raw(t, array_cls, size);
    if (a)
        a->length = len;
    return (Object *)a;
}

Object *heap_new_prim_array(VMThread *t, char type, jint len) {
    return heap_alloc_array(t, class_prim_array(type), len);
}

void heap_add_root(Object **slot) {
    if (root_count == root_cap) {
        root_cap = root_cap ? root_cap * 2 : 64;
        roots = realloc(roots, root_cap * sizeof(Object **));
    }
    roots[root_count++] = slot;
}

void heap_remove_root(Object **slot) {
    for (size_t i = 0; i < root_count; i++) {
        if (roots[i] == slot) {
            roots[i] = roots[--root_count];
            return;
        }
    }
}

void heap_request_gc(void) {
    gc_requested = true;
}

size_t heap_used(void) {
    return used_bytes;
}

size_t heap_total(void) {
    return used_bytes > REPORTED_TOTAL ? used_bytes + 1024 * 1024 : REPORTED_TOTAL;
}

// ---------------------------------------------------------------------------
// Mark & sweep

void heap_mark(Object *o) {
    if (!o || o->marked)
        return;
    o->marked = 1;
    if (mark_top == mark_cap) {
        mark_cap = mark_cap ? mark_cap * 2 : 4096;
        mark_stack = realloc(mark_stack, mark_cap * sizeof(Object *));
    }
    mark_stack[mark_top++] = o;
}

static void scan_object(Object *o) {
    Class *c = o->cls;
    if (c->is_array) {
        if (c->elem_type == 'L' || c->elem_type == '[') {
            Object **elems = ARRAY_DATA(o, Object *);
            for (jint i = 0; i < ARRAY_LEN(o); i++)
                heap_mark(elems[i]);
        }
        return;
    }
    Value *f = OBJ_FIELDS(o);
    for (int i = 0; i < c->instance_slots; i++) {
        if (c->slot_is_ref[i])
            heap_mark(f[i].l);
    }
    heap_mark(c->mirror);
}

static double now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

void heap_gc(void) {
    gc_requested = false;
    bool prof = vm_prof_on();
    double t0 = prof ? now_ms() : 0;
    size_t before = obj_count;

    for (size_t i = 0; i < root_count; i++)
        heap_mark(*roots[i]);
    class_mark_roots(heap_mark);
    jstring_mark_roots(heap_mark);
    thread_mark_roots(heap_mark);

    while (mark_top > 0)
        scan_object(mark_stack[--mark_top]);

    size_t live = 0;
    used_bytes = 0;
    for (size_t i = 0; i < obj_count; i++) {
        Object *o = objs[i];
        if (o->marked) {
            o->marked = 0;
            objs[live++] = o;
            Class *c = o->cls;
            used_bytes += c->is_array ? sizeof(Array) + (size_t)ARRAY_LEN(o) * c->elem_size
                                      : sizeof(Object) + (size_t)c->instance_slots * sizeof(Value);
        } else {
            free(o->monitor);
            free(o);
        }
    }
    obj_count = live;
    set_rebuild(obj_count);

    alloc_since_gc = 0;
    gc_threshold = used_bytes > GC_MIN_THRESHOLD ? used_bytes : GC_MIN_THRESHOLD;
    if (prof)
        vm_prof_log("gc %.1f ms  objs %zu -> %zu  live %zuK",
                now_ms() - t0, before, obj_count, used_bytes / 1024);
}

void heap_gc_if_needed(void) {
    if (gc_requested)
        heap_gc();
}

void heap_free_all(void) {
    for (size_t i = 0; i < obj_count; i++) {
        free(objs[i]->monitor);
        free(objs[i]);
    }
    free(objs);
    free(set);
    free(roots);
    free(mark_stack);
    objs = NULL;
    set = NULL;
    roots = NULL;
    mark_stack = NULL;
    obj_count = obj_cap = set_cap = set_used = 0;
    root_count = root_cap = mark_top = mark_cap = 0;
    used_bytes = alloc_since_gc = 0;
    gc_threshold = GC_MIN_THRESHOLD;
    gc_requested = false;
}
