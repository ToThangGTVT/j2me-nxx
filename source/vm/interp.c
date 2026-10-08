// Trình thông dịch bytecode
#include "vm_internal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aot.h"
#include "opcodes.h"

#define U2(p) ((uint16_t)(((p)[0] << 8) | (p)[1]))
#define S2(p) ((int16_t)U2(p))
#define S4(p) ((int32_t)(((uint32_t)(p)[0] << 24) | ((uint32_t)(p)[1] << 16) | ((uint32_t)(p)[2] << 8) | (uint32_t)(p)[3]))

static const char *S_init_name;
static Object *oom_error;

// Cache gọi qua interface: (lớp thật của object, method đã resolve) -> method sẽ chạy.
// Bổ sung cho cache 1 lớp trong constant pool: chỗ gọi gặp xen kẽ nhiều lớp không phải tìm
// lại method theo tên mỗi lần đổi lớp. Bảng băm địa chỉ mở, chỉ thêm, xoá khi tắt VM.
typedef struct {
    Class *cls;
    Method *rm;
    Method *target;
} IfaceEntry;

static IfaceEntry *iface_tab;
static uint32_t iface_cap, iface_used;

static uint32_t iface_hash(const Class *c, const Method *rm) {
    uint64_t h = (uint64_t)(uintptr_t)c * 0x9E3779B97F4A7C15ull ^ (uint64_t)(uintptr_t)rm * 0xC2B2AE3D27D4EB4Full;
    return (uint32_t)(h >> 32);
}

static Method *iface_find(const Class *c, const Method *rm) {
    if (!iface_tab)
        return NULL;
    for (uint32_t i = iface_hash(c, rm) & (iface_cap - 1);; i = (i + 1) & (iface_cap - 1)) {
        IfaceEntry *e = &iface_tab[i];
        if (!e->cls)
            return NULL;
        if (e->cls == c && e->rm == rm)
            return e->target;
    }
}

static void iface_put(Class *c, Method *rm, Method *target) {
    if ((iface_used + 1) * 2 > iface_cap) {
        uint32_t cap = iface_cap ? iface_cap * 2 : 256;
        IfaceEntry *tab = calloc(cap, sizeof(IfaceEntry));
        if (!tab)
            return;     // hết bộ nhớ: lần sau tìm theo tên như cũ
        for (uint32_t k = 0; k < iface_cap; k++) {
            IfaceEntry *o = &iface_tab[k];
            if (!o->cls)
                continue;
            uint32_t i = iface_hash(o->cls, o->rm) & (cap - 1);
            while (tab[i].cls)
                i = (i + 1) & (cap - 1);
            tab[i] = *o;
        }
        free(iface_tab);
        iface_tab = tab;
        iface_cap = cap;
    }
    uint32_t i = iface_hash(c, rm) & (iface_cap - 1);
    while (iface_tab[i].cls)
        i = (i + 1) & (iface_cap - 1);
    iface_tab[i] = (IfaceEntry){ c, rm, target };
    iface_used++;
}

void interp_reset(void) {
    S_init_name = NULL;
    oom_error = NULL;
    free(iface_tab);
    iface_tab = NULL;
    iface_cap = iface_used = 0;
}

// ---------------------------------------------------------------------------
// Resolve constant pool

static Class *resolve_class(VMThread *t, Class *cur, uint16_t idx) {
    CPEntry *e = &cur->cp[idx];
    if (e->resolved)
        return e->cls;
    const char *name = cp_class_name(cur, idx);
    if (!name) {
        throw_new(t, "java/lang/ClassFormatError", "bad class index");
        return NULL;
    }
    Class *c = class_load(t, name);
    if (c) {
        e->cls = c;
        e->resolved = 1;
    }
    return c;
}

static void name_and_type(Class *cur, uint16_t nat, const char **name, const char **desc) {
    CPEntry *e = &cur->cp[nat];
    *name = cur->cp[e->a].utf8;
    *desc = cur->cp[e->b].utf8;
}

static Field *resolve_field(VMThread *t, Class *cur, uint16_t idx) {
    CPEntry *e = &cur->cp[idx];
    if (e->resolved)
        return e->field;
    Class *c = resolve_class(t, cur, e->a);
    if (!c)
        return NULL;
    const char *name, *desc;
    name_and_type(cur, e->b, &name, &desc);
    Field *f = class_find_field(c, name, desc);
    if (!f) {
        char msg[256];
        snprintf(msg, sizeof(msg), "%s.%s %s", c->name, name, desc);
        throw_new(t, "java/lang/NoSuchFieldError", msg);
        return NULL;
    }
    e->field = f;
    e->resolved = 1;
    return f;
}

static Method *resolve_method(VMThread *t, Class *cur, uint16_t idx) {
    CPEntry *e = &cur->cp[idx];
    if (e->resolved)
        return e->method;
    Class *c = resolve_class(t, cur, e->a);
    if (!c)
        return NULL;
    const char *name, *desc;
    name_and_type(cur, e->b, &name, &desc);
    Method *m = class_find_method(c, name, desc);
    if (!m) {
        char msg[300];
        snprintf(msg, sizeof(msg), "%s.%s%s", c->name, name, desc);
        throw_new(t, "java/lang/NoSuchMethodError", msg);
        return NULL;
    }
    e->method = m;
    e->resolved = 1;
    return m;
}

static Object *resolve_string(VMThread *t, Class *cur, uint16_t idx) {
    CPEntry *e = &cur->cp[idx];
    if (!e->resolved) {
        e->str = jstring_intern_utf8(t, cur->cp[e->a].utf8);
        if (!e->str)
            return NULL;
        e->resolved = 1;
    }
    return e->str;
}

// ---------------------------------------------------------------------------
// Exception

static Object *capture_trace(VMThread *t) {
    int n = t->frame_count;
    Object *arr = heap_new_prim_array(NULL, 'J', n * 2);
    if (!arr)
        return NULL;
    jlong *d = ARRAY_DATA(arr, jlong);
    for (int i = 0; i < n; i++) {
        Frame *f = &t->frames[n - 1 - i];
        d[i * 2] = (jlong)(intptr_t)f->m;
        d[i * 2 + 1] = f->m->code && f->pc ? (jlong)(f->pc - f->m->code) : -1;
    }
    return arr;
}

void throwable_fill_trace(VMThread *t, Object *ex) {
    if (FS_Throwable_trace >= 0)
        OBJ_FIELDS(ex)[FS_Throwable_trace].l = capture_trace(t);
}

void throw_object(VMThread *t, Object *ex) {
    t->exception = ex;
}

void throw_new(VMThread *t, const char *cls_name, const char *msg) {
    if (!t)
        return;
    Object *saved = t->exception;
    t->exception = NULL;
    Class *c = class_load(t, cls_name);
    if (!c) {
        t->exception = NULL;
        c = class_load(t, "java/lang/Error");
    }
    t->exception = saved;
    if (!c) {
        vm_log("Khong nap duoc lop exception %s (%s)", cls_name, msg ? msg : "");
        thread_terminate(t);
        return;
    }
    Object *ex = heap_alloc_object(NULL, c);
    if (!ex) {
        // Hết bộ nhớ: dùng lại 1 object lỗi cấp sẵn (nếu có)
        t->exception = oom_error;
        if (!oom_error)
            thread_terminate(t);
        return;
    }
    if (msg && FS_Throwable_detailMessage >= 0)
        OBJ_FIELDS(ex)[FS_Throwable_detailMessage].l = jstring_new_utf8(NULL, msg);
    throwable_fill_trace(t, ex);
    t->exception = ex;
}

void throw_null(VMThread *t) {
    throw_new(t, "java/lang/NullPointerException", NULL);
}

static void java_name(const char *internal, char *out, size_t size) {
    size_t i = 0;
    for (; internal[i] && i + 1 < size; i++)
        out[i] = internal[i] == '/' ? '.' : internal[i];
    out[i] = '\0';
}

void format_trace(Object *trace, char *out, size_t size) {
    out[0] = '\0';
    if (!trace)
        return;
    size_t pos = 0;
    jint n = ARRAY_LEN(trace) / 2;
    jlong *d = ARRAY_DATA(trace, jlong);
    for (jint i = 0; i < n && pos + 1 < size; i++) {
        Method *m = (Method *)(intptr_t)d[i * 2];
        char cname[200];
        java_name(m->owner->name, cname, sizeof(cname));
        int line = d[i * 2 + 1] >= 0 ? method_line(m, m->code + d[i * 2 + 1]) : -1;
        int w;
        if (m->access & ACC_NATIVE)
            w = snprintf(out + pos, size - pos, "\tat %s.%s(Native Method)\n", cname, m->name);
        else if (line >= 0)
            w = snprintf(out + pos, size - pos, "\tat %s.%s(%s:%d)\n", cname, m->name,
                         m->owner->source_file ? m->owner->source_file : "?", line);
        else
            w = snprintf(out + pos, size - pos, "\tat %s.%s(pc %lld)\n", cname, m->name, (long long)d[i * 2 + 1]);
        if (w < 0)
            break;
        pos += (size_t)w;
    }
}

char vm_uncaught_text[4096];

const char *vm_last_uncaught(void) {
    return vm_uncaught_text;
}

// "Lớp: thông điệp" + stack trace
static void exception_text(Object *ex, char *out, size_t size) {
    char cname[200], msg[256] = "";
    java_name(ex->cls->name, cname, sizeof(cname));
    if (FS_Throwable_detailMessage >= 0 && OBJ_FIELDS(ex)[FS_Throwable_detailMessage].l)
        jstring_to_cstr(OBJ_FIELDS(ex)[FS_Throwable_detailMessage].l, msg, sizeof(msg));
    int n = snprintf(out, size, "%s%s%s\n", cname, msg[0] ? ": " : "", msg);
    if (n > 0 && (size_t)n < size)
        format_trace(FS_Throwable_trace >= 0 ? OBJ_FIELDS(ex)[FS_Throwable_trace].l : NULL, out + n, size - (size_t)n);
}

void exception_describe(VMThread *t, Object *ex) {
    (void)t;
    if (!ex)
        return;
    char *text = malloc(8192);
    if (!text)
        return;
    exception_text(ex, text, 8192);
    vm_log("%s", text);
    free(text);
}

void vm_describe_current(char *out, size_t size) {
    VMThread *t = thread_current();
    if (!t || !t->frames) {
        snprintf(out, size, "(khong co thread Java nao dang chay)\n");
        return;
    }
    size_t pos = 0;
    int w = snprintf(out, size, "Java thread %d, %d frame:\n", t->id, t->frame_count);
    pos = w > 0 ? (size_t)w : 0;
    for (int i = t->frame_count - 1; i >= 0 && i >= t->frame_count - 32 && pos + 1 < size; i--) {
        Method *m = t->frames[i].m;
        if (!m || !m->owner)
            continue;
        char cname[200];
        java_name(m->owner->name, cname, sizeof(cname));
        uint8_t *pc = t->frames[i].pc;
        int line = pc && m->code && pc >= m->code && pc < m->code + m->code_len ? method_line(m, pc) : -1;
        if (line >= 0)
            w = snprintf(out + pos, size - pos, "\tat %s.%s%s (%s:%d)\n", cname, m->name, m->desc,
                         m->owner->source_file ? m->owner->source_file : "?", line);
        else
            w = snprintf(out + pos, size - pos, "\tat %s.%s%s\n", cname, m->name, m->desc);
        if (w < 0)
            break;
        pos += (size_t)w;
    }
}

static void frame_pop(VMThread *t) {
    Frame *f = &t->frames[t->frame_count - 1];
    if (f->sync_obj)
        monitor_exit(t, f->sync_obj);
    if (f->clinit_of) {
        f->clinit_of->state = CLASS_INITIALIZED;
        f->clinit_of->init_thread = NULL;
        thread_wake_waiters();
    }
    t->frame_count--;
}

bool interp_handle_exception(VMThread *t) {
    Object *ex = t->exception;
    bool first = true;
    while (t->frame_count > 0) {
        Frame *f = &t->frames[t->frame_count - 1];
        Method *m = f->m;
        if (m->code && f->pc) {
            int off = (int)(f->pc - m->code) - ((first || f->retry) ? 0 : 1);
            for (int i = 0; i < m->exc_count; i++) {
                ExceptionEntry *e = &m->exc[i];
                if (off < e->start_pc || off >= e->end_pc)
                    continue;
                if (e->catch_type) {
                    Class *cc;
                    CPEntry *ce = &m->owner->cp[e->catch_type];
                    if (ce->resolved) {
                        cc = ce->cls;
                    } else {
                        cc = class_load(NULL, cp_class_name(m->owner, e->catch_type));
                        if (!cc)
                            continue;
                        ce->cls = cc;
                        ce->resolved = 1;
                    }
                    if (!class_instance_of(ex->cls, cc))
                        continue;
                }
                f->sp = f->stack_base;
                (f->sp++)->l = ex;
                f->pc = m->code + e->handler_pc;
                t->exception = NULL;
                return true;
            }
        }
        if (f->clinit_of) {
            f->clinit_of->state = CLASS_ERROR;
            f->clinit_of->init_thread = NULL;
            f->clinit_of = NULL;
            thread_wake_waiters();
        }
        frame_pop(t);
        first = false;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Frame

bool thread_push_frame(VMThread *t, Method *m, const Value *args) {
    Value *locals = t->frame_count ? t->frames[t->frame_count - 1].sp : t->stack;
    int nlocals = m->max_locals > m->arg_slots ? m->max_locals : m->arg_slots;
    if (t->frame_count >= THREAD_MAX_FRAMES ||
        locals + nlocals + m->max_stack + 4 > t->stack + THREAD_STACK_SLOTS) {
        throw_new(t, "java/lang/StackOverflowError", NULL);
        return false;
    }
    if (args && args != locals)
        memmove(locals, args, (size_t)m->arg_slots * sizeof(Value));
    if (nlocals > m->arg_slots)
        memset(locals + m->arg_slots, 0, (size_t)(nlocals - m->arg_slots) * sizeof(Value));

    Frame *f = &t->frames[t->frame_count++];
    f->m = m;
    f->pc = m->code;
    f->locals = locals;
    f->stack_base = locals + nlocals;
    f->sp = f->stack_base;
    f->clinit_of = NULL;
    f->sync_obj = NULL;
    f->retry = 0;
    return true;
}

static Object *new_multi_array(VMThread *t, Class *c, int dims, const jint *counts) {
    if (counts[0] < 0) {
        throw_new(t, "java/lang/NegativeArraySizeException", NULL);
        return NULL;
    }
    Object *a = heap_alloc_array(t, c, counts[0]);
    if (!a || dims == 1)
        return a;
    Object **elems = ARRAY_DATA(a, Object *);
    for (jint i = 0; i < counts[0]; i++) {
        elems[i] = new_multi_array(t, c->component, dims - 1, counts + 1);
        if (!elems[i] && t->exception)
            return NULL;
    }
    return a;
}

Method *interp_find_virtual(CPEntry *e, Class *cls, Method *rm) {
    if (e->cache_cls == cls)
        return e->cache_method;
    Method *m = iface_find(cls, rm);
    if (!m) {
        m = class_find_interface_method(cls, rm->name, rm->desc);
        if (!m)
            return NULL;
        iface_put(cls, rm, m);
    }
    e->cache_cls = cls;
    e->cache_method = m;
    return m;
}

static Method *lookup_virtual(VMThread *t, CPEntry *e, Class *cls, Method *rm) {
    Method *m = interp_find_virtual(e, cls, rm);
    if (!m) {
        char msg[300];
        snprintf(msg, sizeof(msg), "%s.%s%s", cls->name, rm->name, rm->desc);
        throw_new(t, "java/lang/AbstractMethodError", msg);
    }
    return m;
}

static jint f2i(double v) {
    if (isnan(v))
        return 0;
    if (v >= 2147483647.0)
        return INT32_MAX;
    if (v <= -2147483648.0)
        return INT32_MIN;
    return (jint)v;
}

static jlong f2l(double v) {
    if (isnan(v))
        return 0;
    if (v >= 9223372036854775807.0)
        return INT64_MAX;
    if (v <= -9223372036854775808.0)
        return INT64_MIN;
    return (jlong)v;
}

// ---------------------------------------------------------------------------
// Vòng lặp chính

#define INTERP_FN interp_run_plain
#define INTERP_AOT 0
#include "interp_loop.h"
#undef INTERP_FN
#undef INTERP_AOT

#define INTERP_FN interp_run_aot
#define INTERP_AOT 1
#include "interp_loop.h"
#undef INTERP_FN
#undef INTERP_AOT

void interp_run(VMThread *t, int budget) {
    if (aot_active())
        interp_run_aot(t, budget);
    else
        interp_run_plain(t, budget);
}
