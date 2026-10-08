// Trình thông dịch bytecode
#include "vm_internal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "opcodes.h"

#define U2(p) ((uint16_t)(((p)[0] << 8) | (p)[1]))
#define S2(p) ((int16_t)U2(p))
#define S4(p) ((int32_t)(((uint32_t)(p)[0] << 24) | ((uint32_t)(p)[1] << 16) | ((uint32_t)(p)[2] << 8) | (uint32_t)(p)[3]))

static const char *S_init_name;
static Object *oom_error;

void interp_reset(void) {
    S_init_name = NULL;
    oom_error = NULL;
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

static Method *lookup_virtual(VMThread *t, CPEntry *e, Class *cls, Method *rm) {
    if (e->cache_cls == cls)
        return e->cache_method;
    Method *m = class_find_interface_method(cls, rm->name, rm->desc);
    if (!m) {
        char msg[300];
        snprintf(msg, sizeof(msg), "%s.%s%s", cls->name, rm->name, rm->desc);
        throw_new(t, "java/lang/AbstractMethodError", msg);
        return NULL;
    }
    e->cache_cls = cls;
    e->cache_method = m;
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

void interp_run(VMThread *t, int budget) {
    if (!S_init_name)
        S_init_name = intern_cstr("<init>");

    Frame *f;
    Method *m;
    uint8_t *pc, *insn;
    uint8_t op;
    Value *sp, *locals;
    CPEntry *cp;
    Method *target;             // invoke: method sẽ gọi, số slot tham số, độ dài lệnh
    int nargs, len;

#define LOAD()                                  \
    do {                                        \
        f = &t->frames[t->frame_count - 1];     \
        m = f->m;                               \
        pc = f->pc;                             \
        sp = f->sp;                             \
        locals = f->locals;                     \
        cp = m->owner->cp;                      \
    } while (0)
#define SAVE_AT(p)      do { f->pc = (p); f->sp = sp; } while (0)
#define THROW(cls, msg) do { SAVE_AT(insn); throw_new(t, cls, msg); goto exception; } while (0)
#define CASE(x)         case x: lbl_##x:
#define CHECK_NULL(o)   do { if (!(o)) THROW("java/lang/NullPointerException", NULL); } while (0)
#define PUSHI(v)        ((sp++)->i = (v))
#define PUSHF(v)        ((sp++)->f = (v))
#define PUSHL(v)        ((sp++)->l = (v))
#define PUSHJ(v)        do { sp->j = (v); sp += 2; } while (0)
#define PUSHD(v)        do { sp->d = (v); sp += 2; } while (0)
#define POPI()          ((--sp)->i)
#define POPF()          ((--sp)->f)
#define POPL()          ((--sp)->l)
#define POPJ()          ((sp -= 2)->j)
#define POPD()          ((sp -= 2)->d)
// Lệnh có thể phải chạy lại (class init / block): lưu trạng thái rồi thoát hoặc chạy tiếp frame mới
#define RETRY()                                                 \
    do {                                                        \
        SAVE_AT(insn);                                          \
        if (t->exception)                                       \
            goto exception;                                     \
        if (t->frame_count > 0 && &t->frames[t->frame_count - 1] != f) \
            f->retry = 1;                                       \
        if (t->state != TS_RUNNABLE)                            \
            return;                                             \
        LOAD();                                                 \
        TICK();                                                 \
        goto next;                                              \
    } while (0)
// Ngân sách chỉ trừ ở nhánh lùi và lúc vào method: đoạn code chạy thẳng luôn hữu hạn,
// nên vẫn chắc chắn trả lại quyền cho scheduler mà không tốn 1 phép đếm mỗi lệnh
#define TICK()          do { if (--budget <= 0) { SAVE_AT(pc); return; } } while (0)
#define BRANCH(off)     do { jint _o = (off); pc += _o; if (_o <= 0) TICK(); } while (0)
#define ARRAY_CHECK(arr, idx)                                                   \
    do {                                                                        \
        CHECK_NULL(arr);                                                        \
        if ((uint32_t)(idx) >= (uint32_t)ARRAY_LEN(arr)) {                      \
            char _m[32];                                                        \
            snprintf(_m, sizeof(_m), "%d", (int)(idx));                         \
            THROW("java/lang/ArrayIndexOutOfBoundsException", _m);              \
        }                                                                       \
    } while (0)

    static const void *dispatch[256];
    if (!dispatch[OP_NOP]) {
        for (int i = 0; i < 256; i++)
            dispatch[i] = &&lbl_default;
#define SET(x) dispatch[x] = &&lbl_##x;
        SET(OP_NOP) SET(OP_ACONST_NULL) SET(OP_ICONST_M1) SET(OP_ICONST_0)
        SET(OP_ICONST_1) SET(OP_ICONST_2) SET(OP_ICONST_3) SET(OP_ICONST_4)
        SET(OP_ICONST_5) SET(OP_LCONST_0) SET(OP_LCONST_1) SET(OP_FCONST_0)
        SET(OP_FCONST_1) SET(OP_FCONST_2) SET(OP_DCONST_0) SET(OP_DCONST_1)
        SET(OP_BIPUSH) SET(OP_SIPUSH) SET(OP_LDC) SET(OP_LDC_W)
        SET(OP_LDC2_W) SET(OP_ILOAD) SET(OP_FLOAD) SET(OP_ALOAD)
        SET(OP_LLOAD) SET(OP_DLOAD) SET(OP_ILOAD_0) SET(OP_ILOAD_1)
        SET(OP_ILOAD_2) SET(OP_ILOAD_3) SET(OP_LLOAD_0) SET(OP_LLOAD_1)
        SET(OP_LLOAD_2) SET(OP_LLOAD_3) SET(OP_FLOAD_0) SET(OP_FLOAD_1)
        SET(OP_FLOAD_2) SET(OP_FLOAD_3) SET(OP_DLOAD_0) SET(OP_DLOAD_1)
        SET(OP_DLOAD_2) SET(OP_DLOAD_3) SET(OP_ALOAD_0) SET(OP_ALOAD_1)
        SET(OP_ALOAD_2) SET(OP_ALOAD_3) SET(OP_ISTORE) SET(OP_FSTORE)
        SET(OP_ASTORE) SET(OP_LSTORE) SET(OP_DSTORE) SET(OP_ISTORE_0)
        SET(OP_ISTORE_1) SET(OP_ISTORE_2) SET(OP_ISTORE_3) SET(OP_LSTORE_0)
        SET(OP_LSTORE_1) SET(OP_LSTORE_2) SET(OP_LSTORE_3) SET(OP_FSTORE_0)
        SET(OP_FSTORE_1) SET(OP_FSTORE_2) SET(OP_FSTORE_3) SET(OP_DSTORE_0)
        SET(OP_DSTORE_1) SET(OP_DSTORE_2) SET(OP_DSTORE_3) SET(OP_ASTORE_0)
        SET(OP_ASTORE_1) SET(OP_ASTORE_2) SET(OP_ASTORE_3) SET(OP_IALOAD)
        SET(OP_FALOAD) SET(OP_LALOAD) SET(OP_DALOAD) SET(OP_AALOAD)
        SET(OP_BALOAD) SET(OP_CALOAD) SET(OP_SALOAD) SET(OP_IASTORE)
        SET(OP_FASTORE) SET(OP_LASTORE) SET(OP_DASTORE) SET(OP_AASTORE)
        SET(OP_BASTORE) SET(OP_CASTORE) SET(OP_SASTORE) SET(OP_POP)
        SET(OP_POP2) SET(OP_DUP) SET(OP_DUP_X1) SET(OP_DUP_X2)
        SET(OP_DUP2) SET(OP_DUP2_X1) SET(OP_DUP2_X2) SET(OP_SWAP)
        SET(OP_IADD) SET(OP_ISUB) SET(OP_IMUL) SET(OP_IDIV)
        SET(OP_IREM) SET(OP_INEG) SET(OP_ISHL) SET(OP_ISHR)
        SET(OP_IUSHR) SET(OP_IAND) SET(OP_IOR) SET(OP_IXOR)
        SET(OP_LADD) SET(OP_LSUB) SET(OP_LMUL) SET(OP_LDIV)
        SET(OP_LREM) SET(OP_LNEG) SET(OP_LSHL) SET(OP_LSHR)
        SET(OP_LUSHR) SET(OP_LAND) SET(OP_LOR) SET(OP_LXOR)
        SET(OP_FADD) SET(OP_FSUB) SET(OP_FMUL) SET(OP_FDIV)
        SET(OP_FREM) SET(OP_FNEG) SET(OP_DADD) SET(OP_DSUB)
        SET(OP_DMUL) SET(OP_DDIV) SET(OP_DREM) SET(OP_DNEG)
        SET(OP_IINC) SET(OP_I2L) SET(OP_I2F) SET(OP_I2D)
        SET(OP_L2I) SET(OP_L2F) SET(OP_L2D) SET(OP_F2I)
        SET(OP_F2L) SET(OP_F2D) SET(OP_D2I) SET(OP_D2L)
        SET(OP_D2F) SET(OP_I2B) SET(OP_I2C) SET(OP_I2S)
        SET(OP_LCMP) SET(OP_FCMPL) SET(OP_FCMPG) SET(OP_DCMPL)
        SET(OP_DCMPG) SET(OP_IFEQ) SET(OP_IFNE) SET(OP_IFLT)
        SET(OP_IFGE) SET(OP_IFGT) SET(OP_IFLE) SET(OP_IF_ICMPEQ)
        SET(OP_IF_ICMPNE) SET(OP_IF_ICMPLT) SET(OP_IF_ICMPGE) SET(OP_IF_ICMPGT)
        SET(OP_IF_ICMPLE) SET(OP_IF_ACMPEQ) SET(OP_IF_ACMPNE) SET(OP_IFNULL)
        SET(OP_IFNONNULL) SET(OP_GOTO) SET(OP_GOTO_W) SET(OP_JSR)
        SET(OP_JSR_W) SET(OP_RET) SET(OP_TABLESWITCH) SET(OP_LOOKUPSWITCH)
        SET(OP_IRETURN) SET(OP_FRETURN) SET(OP_ARETURN) SET(OP_LRETURN)
        SET(OP_DRETURN) SET(OP_RETURN) SET(OP_GETSTATIC) SET(OP_PUTSTATIC)
        SET(OP_GETFIELD) SET(OP_PUTFIELD) SET(OP_INVOKEVIRTUAL) SET(OP_INVOKESPECIAL)
        SET(OP_INVOKESTATIC) SET(OP_INVOKEINTERFACE) SET(OP_NEW) SET(OP_NEWARRAY)
        SET(OP_ANEWARRAY) SET(OP_MULTIANEWARRAY) SET(OP_ARRAYLENGTH) SET(OP_ATHROW)
        SET(OP_CHECKCAST) SET(OP_INSTANCEOF) SET(OP_MONITORENTER) SET(OP_MONITOREXIT)
        SET(OP_WIDE)
        SET(OP_GETFIELD_Q) SET(OP_GETFIELD2_Q) SET(OP_PUTFIELD_Q) SET(OP_PUTFIELD2_Q)
        SET(OP_GETSTATIC_Q) SET(OP_GETSTATIC2_Q) SET(OP_PUTSTATIC_Q) SET(OP_PUTSTATIC2_Q)
        SET(OP_INVOKEVIRTUAL_Q) SET(OP_INVOKESPECIAL_Q) SET(OP_INVOKESTATIC_Q)
        SET(OP_NEW_Q) SET(OP_CHECKCAST_Q) SET(OP_INSTANCEOF_Q)
#undef SET
    }

    if (t->frame_count == 0)
        return;
    LOAD();

    if (t->exception)
        goto exception;

next:
    for (;;) {
        insn = pc;
        op = *pc;
        // Nhảy thẳng tới nhãn của lệnh (computed goto của GCC / Clang): mỗi lệnh có lệnh nhảy
        // riêng nên CPU đoán đích đúng hơn 1 bảng nhảy chung. switch chỉ còn để đặt nhãn case.
        goto *dispatch[op];
        switch (op) {
        CASE(OP_NOP) pc++; break;
        CASE(OP_ACONST_NULL) PUSHL(NULL); pc++; break;
        CASE(OP_ICONST_M1) CASE(OP_ICONST_0) CASE(OP_ICONST_1) CASE(OP_ICONST_2)
        CASE(OP_ICONST_3) CASE(OP_ICONST_4) CASE(OP_ICONST_5)
            PUSHI(op - OP_ICONST_0); pc++; break;
        CASE(OP_LCONST_0) CASE(OP_LCONST_1) PUSHJ(op - OP_LCONST_0); pc++; break;
        CASE(OP_FCONST_0) CASE(OP_FCONST_1) CASE(OP_FCONST_2) PUSHF((float)(op - OP_FCONST_0)); pc++; break;
        CASE(OP_DCONST_0) CASE(OP_DCONST_1) PUSHD((double)(op - OP_DCONST_0)); pc++; break;
        CASE(OP_BIPUSH) PUSHI((int8_t)pc[1]); pc += 2; break;
        CASE(OP_SIPUSH) PUSHI(S2(pc + 1)); pc += 3; break;

        CASE(OP_LDC) CASE(OP_LDC_W) {
            uint16_t idx = op == OP_LDC ? pc[1] : U2(pc + 1);
            CPEntry *e = &cp[idx];
            switch (e->tag) {
            case CONST_Integer: PUSHI(e->i); break;
            case CONST_Float: PUSHF(e->f); break;
            case CONST_String: {
                SAVE_AT(insn);
                Object *s = resolve_string(t, m->owner, idx);
                if (!s)
                    goto exception;
                PUSHL(s);
                break;
            }
            case CONST_Class: {
                SAVE_AT(insn);
                Class *c = resolve_class(t, m->owner, idx);
                if (!c)
                    goto exception;
                PUSHL(class_mirror(t, c));
                break;
            }
            default:
                THROW("java/lang/ClassFormatError", "ldc");
            }
            pc += op == OP_LDC ? 2 : 3;
            break;
        }
        CASE(OP_LDC2_W) {
            CPEntry *e = &cp[U2(pc + 1)];
            if (e->tag == CONST_Long)
                PUSHJ(e->j);
            else
                PUSHD(e->d);
            pc += 3;
            break;
        }

        // --- load / store
        CASE(OP_ILOAD) CASE(OP_FLOAD) CASE(OP_ALOAD) *sp++ = locals[pc[1]]; pc += 2; break;
        CASE(OP_LLOAD) CASE(OP_DLOAD) sp[0] = locals[pc[1]]; sp += 2; pc += 2; break;
        CASE(OP_ILOAD_0) CASE(OP_ILOAD_1) CASE(OP_ILOAD_2) CASE(OP_ILOAD_3)
            *sp++ = locals[op - OP_ILOAD_0]; pc++; break;
        CASE(OP_LLOAD_0) CASE(OP_LLOAD_1) CASE(OP_LLOAD_2) CASE(OP_LLOAD_3)
            sp[0] = locals[op - OP_LLOAD_0]; sp += 2; pc++; break;
        CASE(OP_FLOAD_0) CASE(OP_FLOAD_1) CASE(OP_FLOAD_2) CASE(OP_FLOAD_3)
            *sp++ = locals[op - OP_FLOAD_0]; pc++; break;
        CASE(OP_DLOAD_0) CASE(OP_DLOAD_1) CASE(OP_DLOAD_2) CASE(OP_DLOAD_3)
            sp[0] = locals[op - OP_DLOAD_0]; sp += 2; pc++; break;
        CASE(OP_ALOAD_0) CASE(OP_ALOAD_1) CASE(OP_ALOAD_2) CASE(OP_ALOAD_3)
            *sp++ = locals[op - OP_ALOAD_0]; pc++; break;

        CASE(OP_ISTORE) CASE(OP_FSTORE) CASE(OP_ASTORE) locals[pc[1]] = *--sp; pc += 2; break;
        CASE(OP_LSTORE) CASE(OP_DSTORE) sp -= 2; locals[pc[1]] = sp[0]; pc += 2; break;
        CASE(OP_ISTORE_0) CASE(OP_ISTORE_1) CASE(OP_ISTORE_2) CASE(OP_ISTORE_3)
            locals[op - OP_ISTORE_0] = *--sp; pc++; break;
        CASE(OP_LSTORE_0) CASE(OP_LSTORE_1) CASE(OP_LSTORE_2) CASE(OP_LSTORE_3)
            sp -= 2; locals[op - OP_LSTORE_0] = sp[0]; pc++; break;
        CASE(OP_FSTORE_0) CASE(OP_FSTORE_1) CASE(OP_FSTORE_2) CASE(OP_FSTORE_3)
            locals[op - OP_FSTORE_0] = *--sp; pc++; break;
        CASE(OP_DSTORE_0) CASE(OP_DSTORE_1) CASE(OP_DSTORE_2) CASE(OP_DSTORE_3)
            sp -= 2; locals[op - OP_DSTORE_0] = sp[0]; pc++; break;
        CASE(OP_ASTORE_0) CASE(OP_ASTORE_1) CASE(OP_ASTORE_2) CASE(OP_ASTORE_3)
            locals[op - OP_ASTORE_0] = *--sp; pc++; break;

        // --- array load
        CASE(OP_IALOAD) CASE(OP_FALOAD) {
            jint i = POPI(); Object *a = POPL();
            ARRAY_CHECK(a, i);
            sp->i = ARRAY_DATA(a, jint)[i]; sp++;
            pc++; break;
        }
        CASE(OP_LALOAD) CASE(OP_DALOAD) {
            jint i = POPI(); Object *a = POPL();
            ARRAY_CHECK(a, i);
            sp->j = ARRAY_DATA(a, jlong)[i]; sp += 2;
            pc++; break;
        }
        CASE(OP_AALOAD) {
            jint i = POPI(); Object *a = POPL();
            ARRAY_CHECK(a, i);
            PUSHL(ARRAY_DATA(a, Object *)[i]);
            pc++; break;
        }
        CASE(OP_BALOAD) {
            jint i = POPI(); Object *a = POPL();
            ARRAY_CHECK(a, i);
            PUSHI(ARRAY_DATA(a, jbyte)[i]);
            pc++; break;
        }
        CASE(OP_CALOAD) {
            jint i = POPI(); Object *a = POPL();
            ARRAY_CHECK(a, i);
            PUSHI(ARRAY_DATA(a, jchar)[i]);
            pc++; break;
        }
        CASE(OP_SALOAD) {
            jint i = POPI(); Object *a = POPL();
            ARRAY_CHECK(a, i);
            PUSHI(ARRAY_DATA(a, jshort)[i]);
            pc++; break;
        }

        // --- array store
        CASE(OP_IASTORE) CASE(OP_FASTORE) {
            jint v = POPI(); jint i = POPI(); Object *a = POPL();
            ARRAY_CHECK(a, i);
            ARRAY_DATA(a, jint)[i] = v;
            pc++; break;
        }
        CASE(OP_LASTORE) CASE(OP_DASTORE) {
            jlong v = POPJ(); jint i = POPI(); Object *a = POPL();
            ARRAY_CHECK(a, i);
            ARRAY_DATA(a, jlong)[i] = v;
            pc++; break;
        }
        CASE(OP_AASTORE) {
            Object *v = POPL(); jint i = POPI(); Object *a = POPL();
            ARRAY_CHECK(a, i);
            if (v && a->cls->component && !class_instance_of(v->cls, a->cls->component))
                THROW("java/lang/ArrayStoreException", v->cls->name);
            ARRAY_DATA(a, Object *)[i] = v;
            pc++; break;
        }
        CASE(OP_BASTORE) {
            jint v = POPI(); jint i = POPI(); Object *a = POPL();
            ARRAY_CHECK(a, i);
            ARRAY_DATA(a, jbyte)[i] = (jbyte)v;
            pc++; break;
        }
        CASE(OP_CASTORE) CASE(OP_SASTORE) {
            jint v = POPI(); jint i = POPI(); Object *a = POPL();
            ARRAY_CHECK(a, i);
            ARRAY_DATA(a, jchar)[i] = (jchar)v;
            pc++; break;
        }

        // --- stack
        CASE(OP_POP) sp--; pc++; break;
        CASE(OP_POP2) sp -= 2; pc++; break;
        CASE(OP_DUP) sp[0] = sp[-1]; sp++; pc++; break;
        CASE(OP_DUP_X1) {
            Value v1 = sp[-1], v2 = sp[-2];
            sp[-2] = v1; sp[-1] = v2; sp[0] = v1; sp++;
            pc++; break;
        }
        CASE(OP_DUP_X2) {
            Value v1 = sp[-1], v2 = sp[-2], v3 = sp[-3];
            sp[-3] = v1; sp[-2] = v3; sp[-1] = v2; sp[0] = v1; sp++;
            pc++; break;
        }
        CASE(OP_DUP2) sp[0] = sp[-2]; sp[1] = sp[-1]; sp += 2; pc++; break;
        CASE(OP_DUP2_X1) {
            Value v1 = sp[-1], v2 = sp[-2], v3 = sp[-3];
            sp[-3] = v2; sp[-2] = v1; sp[-1] = v3; sp[0] = v2; sp[1] = v1; sp += 2;
            pc++; break;
        }
        CASE(OP_DUP2_X2) {
            Value v1 = sp[-1], v2 = sp[-2], v3 = sp[-3], v4 = sp[-4];
            sp[-4] = v2; sp[-3] = v1; sp[-2] = v4; sp[-1] = v3; sp[0] = v2; sp[1] = v1; sp += 2;
            pc++; break;
        }
        CASE(OP_SWAP) { Value v = sp[-1]; sp[-1] = sp[-2]; sp[-2] = v; pc++; break; }

        // --- int
        CASE(OP_IADD) { jint b = POPI(); sp[-1].i = (jint)((uint32_t)sp[-1].i + (uint32_t)b); pc++; break; }
        CASE(OP_ISUB) { jint b = POPI(); sp[-1].i = (jint)((uint32_t)sp[-1].i - (uint32_t)b); pc++; break; }
        CASE(OP_IMUL) { jint b = POPI(); sp[-1].i = (jint)((uint32_t)sp[-1].i * (uint32_t)b); pc++; break; }
        CASE(OP_IDIV) {
            jint b = POPI();
            if (b == 0) { sp++; THROW("java/lang/ArithmeticException", "/ by zero"); }
            jint a = sp[-1].i;
            sp[-1].i = (a == INT32_MIN && b == -1) ? a : a / b;
            pc++; break;
        }
        CASE(OP_IREM) {
            jint b = POPI();
            if (b == 0) { sp++; THROW("java/lang/ArithmeticException", "/ by zero"); }
            jint a = sp[-1].i;
            sp[-1].i = (b == -1) ? 0 : a % b;
            pc++; break;
        }
        CASE(OP_INEG) sp[-1].i = (jint)(0u - (uint32_t)sp[-1].i); pc++; break;
        CASE(OP_ISHL) { jint s = POPI(); sp[-1].i = (jint)((uint32_t)sp[-1].i << (s & 31)); pc++; break; }
        CASE(OP_ISHR) { jint s = POPI(); sp[-1].i = sp[-1].i >> (s & 31); pc++; break; }
        CASE(OP_IUSHR) { jint s = POPI(); sp[-1].i = (jint)((uint32_t)sp[-1].i >> (s & 31)); pc++; break; }
        CASE(OP_IAND) { jint b = POPI(); sp[-1].i &= b; pc++; break; }
        CASE(OP_IOR)  { jint b = POPI(); sp[-1].i |= b; pc++; break; }
        CASE(OP_IXOR) { jint b = POPI(); sp[-1].i ^= b; pc++; break; }

        // --- long
        CASE(OP_LADD) { jlong b = POPJ(); sp[-2].j = (jlong)((uint64_t)sp[-2].j + (uint64_t)b); pc++; break; }
        CASE(OP_LSUB) { jlong b = POPJ(); sp[-2].j = (jlong)((uint64_t)sp[-2].j - (uint64_t)b); pc++; break; }
        CASE(OP_LMUL) { jlong b = POPJ(); sp[-2].j = (jlong)((uint64_t)sp[-2].j * (uint64_t)b); pc++; break; }
        CASE(OP_LDIV) {
            jlong b = POPJ();
            if (b == 0) { sp += 2; THROW("java/lang/ArithmeticException", "/ by zero"); }
            jlong a = sp[-2].j;
            sp[-2].j = (a == INT64_MIN && b == -1) ? a : a / b;
            pc++; break;
        }
        CASE(OP_LREM) {
            jlong b = POPJ();
            if (b == 0) { sp += 2; THROW("java/lang/ArithmeticException", "/ by zero"); }
            jlong a = sp[-2].j;
            sp[-2].j = (b == -1) ? 0 : a % b;
            pc++; break;
        }
        CASE(OP_LNEG) sp[-2].j = (jlong)(0ull - (uint64_t)sp[-2].j); pc++; break;
        CASE(OP_LSHL) { jint s = POPI(); sp[-2].j = (jlong)((uint64_t)sp[-2].j << (s & 63)); pc++; break; }
        CASE(OP_LSHR) { jint s = POPI(); sp[-2].j = sp[-2].j >> (s & 63); pc++; break; }
        CASE(OP_LUSHR) { jint s = POPI(); sp[-2].j = (jlong)((uint64_t)sp[-2].j >> (s & 63)); pc++; break; }
        CASE(OP_LAND) { jlong b = POPJ(); sp[-2].j &= b; pc++; break; }
        CASE(OP_LOR)  { jlong b = POPJ(); sp[-2].j |= b; pc++; break; }
        CASE(OP_LXOR) { jlong b = POPJ(); sp[-2].j ^= b; pc++; break; }

        // --- float / double
        CASE(OP_FADD) { jfloat b = POPF(); sp[-1].f += b; pc++; break; }
        CASE(OP_FSUB) { jfloat b = POPF(); sp[-1].f -= b; pc++; break; }
        CASE(OP_FMUL) { jfloat b = POPF(); sp[-1].f *= b; pc++; break; }
        CASE(OP_FDIV) { jfloat b = POPF(); sp[-1].f /= b; pc++; break; }
        CASE(OP_FREM) { jfloat b = POPF(); sp[-1].f = fmodf(sp[-1].f, b); pc++; break; }
        CASE(OP_FNEG) sp[-1].f = -sp[-1].f; pc++; break;
        CASE(OP_DADD) { jdouble b = POPD(); sp[-2].d += b; pc++; break; }
        CASE(OP_DSUB) { jdouble b = POPD(); sp[-2].d -= b; pc++; break; }
        CASE(OP_DMUL) { jdouble b = POPD(); sp[-2].d *= b; pc++; break; }
        CASE(OP_DDIV) { jdouble b = POPD(); sp[-2].d /= b; pc++; break; }
        CASE(OP_DREM) { jdouble b = POPD(); sp[-2].d = fmod(sp[-2].d, b); pc++; break; }
        CASE(OP_DNEG) sp[-2].d = -sp[-2].d; pc++; break;

        CASE(OP_IINC) locals[pc[1]].i = (jint)((uint32_t)locals[pc[1]].i + (uint32_t)(int8_t)pc[2]); pc += 3; break;

        // --- conversion
        CASE(OP_I2L) { jint v = POPI(); PUSHJ(v); pc++; break; }
        CASE(OP_I2F) sp[-1].f = (jfloat)sp[-1].i; pc++; break;
        CASE(OP_I2D) { jint v = POPI(); PUSHD(v); pc++; break; }
        CASE(OP_L2I) { jlong v = POPJ(); PUSHI((jint)v); pc++; break; }
        CASE(OP_L2F) { jlong v = POPJ(); PUSHF((jfloat)v); pc++; break; }
        CASE(OP_L2D) sp[-2].d = (jdouble)sp[-2].j; pc++; break;
        CASE(OP_F2I) sp[-1].i = f2i(sp[-1].f); pc++; break;
        CASE(OP_F2L) { jfloat v = POPF(); PUSHJ(f2l(v)); pc++; break; }
        CASE(OP_F2D) { jfloat v = POPF(); PUSHD(v); pc++; break; }
        CASE(OP_D2I) { jdouble v = POPD(); PUSHI(f2i(v)); pc++; break; }
        CASE(OP_D2L) sp[-2].j = f2l(sp[-2].d); pc++; break;
        CASE(OP_D2F) { jdouble v = POPD(); PUSHF((jfloat)v); pc++; break; }
        CASE(OP_I2B) sp[-1].i = (jbyte)sp[-1].i; pc++; break;
        CASE(OP_I2C) sp[-1].i = (jchar)sp[-1].i; pc++; break;
        CASE(OP_I2S) sp[-1].i = (jshort)sp[-1].i; pc++; break;

        // --- compare
        CASE(OP_LCMP) {
            jlong b = POPJ(), a = POPJ();
            PUSHI(a > b ? 1 : a < b ? -1 : 0);
            pc++; break;
        }
        CASE(OP_FCMPL) CASE(OP_FCMPG) {
            jfloat b = POPF(), a = POPF();
            if (isnan(a) || isnan(b))
                PUSHI(op == OP_FCMPG ? 1 : -1);
            else
                PUSHI(a > b ? 1 : a < b ? -1 : 0);
            pc++; break;
        }
        CASE(OP_DCMPL) CASE(OP_DCMPG) {
            jdouble b = POPD(), a = POPD();
            if (isnan(a) || isnan(b))
                PUSHI(op == OP_DCMPG ? 1 : -1);
            else
                PUSHI(a > b ? 1 : a < b ? -1 : 0);
            pc++; break;
        }

#define BRANCH_IF(cond) do { if (cond) BRANCH(S2(pc + 1)); else pc += 3; } while (0)
        CASE(OP_IFEQ) { jint v = POPI(); BRANCH_IF(v == 0); break; }
        CASE(OP_IFNE) { jint v = POPI(); BRANCH_IF(v != 0); break; }
        CASE(OP_IFLT) { jint v = POPI(); BRANCH_IF(v < 0); break; }
        CASE(OP_IFGE) { jint v = POPI(); BRANCH_IF(v >= 0); break; }
        CASE(OP_IFGT) { jint v = POPI(); BRANCH_IF(v > 0); break; }
        CASE(OP_IFLE) { jint v = POPI(); BRANCH_IF(v <= 0); break; }
        CASE(OP_IF_ICMPEQ) { jint b = POPI(), a = POPI(); BRANCH_IF(a == b); break; }
        CASE(OP_IF_ICMPNE) { jint b = POPI(), a = POPI(); BRANCH_IF(a != b); break; }
        CASE(OP_IF_ICMPLT) { jint b = POPI(), a = POPI(); BRANCH_IF(a < b); break; }
        CASE(OP_IF_ICMPGE) { jint b = POPI(), a = POPI(); BRANCH_IF(a >= b); break; }
        CASE(OP_IF_ICMPGT) { jint b = POPI(), a = POPI(); BRANCH_IF(a > b); break; }
        CASE(OP_IF_ICMPLE) { jint b = POPI(), a = POPI(); BRANCH_IF(a <= b); break; }
        CASE(OP_IF_ACMPEQ) { Object *b = POPL(), *a = POPL(); BRANCH_IF(a == b); break; }
        CASE(OP_IF_ACMPNE) { Object *b = POPL(), *a = POPL(); BRANCH_IF(a != b); break; }
        CASE(OP_IFNULL) { Object *a = POPL(); BRANCH_IF(a == NULL); break; }
        CASE(OP_IFNONNULL) { Object *a = POPL(); BRANCH_IF(a != NULL); break; }
        CASE(OP_GOTO) BRANCH(S2(pc + 1)); break;
        CASE(OP_GOTO_W) BRANCH(S4(pc + 1)); break;
        CASE(OP_JSR) PUSHI((jint)(pc + 3 - m->code)); BRANCH(S2(pc + 1)); break;
        CASE(OP_JSR_W) PUSHI((jint)(pc + 5 - m->code)); BRANCH(S4(pc + 1)); break;
        CASE(OP_RET) pc = m->code + locals[pc[1]].i; TICK(); break;

        CASE(OP_TABLESWITCH) {
            uint8_t *p = m->code + (((pc - m->code) + 4) & ~3);
            jint def = S4(p), lo = S4(p + 4), hi = S4(p + 8);
            jint key = POPI();
            if (key < lo || key > hi)
                BRANCH(def);
            else
                BRANCH(S4(p + 12 + (key - lo) * 4));
            break;
        }
        CASE(OP_LOOKUPSWITCH) {
            uint8_t *p = m->code + (((pc - m->code) + 4) & ~3);
            jint def = S4(p), n = S4(p + 4);
            jint key = POPI();
            jint off = def;
            // Bảng đã sắp xếp: tìm nhị phân
            jint lo = 0, hi = n - 1;
            while (lo <= hi) {
                jint mid = (lo + hi) >> 1;
                jint k = S4(p + 8 + mid * 8);
                if (k == key) {
                    off = S4(p + 12 + mid * 8);
                    break;
                }
                if (k < key)
                    lo = mid + 1;
                else
                    hi = mid - 1;
            }
            BRANCH(off);
            break;
        }

        // --- return
        CASE(OP_IRETURN) CASE(OP_FRETURN) CASE(OP_ARETURN)
        CASE(OP_LRETURN) CASE(OP_DRETURN) CASE(OP_RETURN) {
            int slots = (op == OP_RETURN) ? 0 : (op == OP_LRETURN || op == OP_DRETURN) ? 2 : 1;
            Value ret = slots ? sp[-slots] : (Value){ .j = 0 };
            frame_pop(t);
            if (t->frame_count == 0) {
                t->result = ret;
                thread_terminate(t);
                return;
            }
            LOAD();
            if (slots) {
                sp[0] = ret;
                sp += slots;
            }
            break;
        }

        // --- field
        CASE(OP_GETSTATIC) CASE(OP_PUTSTATIC) {
            SAVE_AT(insn);
            Field *fl = resolve_field(t, m->owner, U2(pc + 1));
            if (!fl)
                goto exception;
            if (fl->owner->state != CLASS_INITIALIZED && !class_ensure_init(t, fl->owner))
                RETRY();
            Value *v = &fl->owner->statics[fl->slot];
            bool wide = fl->desc[0] == 'J' || fl->desc[0] == 'D';
            // Lớp đang chạy <clinit> trên chính thread này thì chưa viết đè: thread khác phải chờ
            if (fl->owner->state == CLASS_INITIALIZED)
                *insn = op == OP_GETSTATIC ? (wide ? OP_GETSTATIC2_Q : OP_GETSTATIC_Q)
                                           : (wide ? OP_PUTSTATIC2_Q : OP_PUTSTATIC_Q);
            if (op == OP_GETSTATIC) {
                sp[0] = *v;
                sp += wide ? 2 : 1;
            } else {
                sp -= wide ? 2 : 1;
                *v = sp[0];
            }
            pc += 3;
            break;
        }
        CASE(OP_GETFIELD) CASE(OP_PUTFIELD) {
            SAVE_AT(insn);
            Field *fl = resolve_field(t, m->owner, U2(pc + 1));
            if (!fl)
                goto exception;
            if (fl->slot > 0xffff)
                THROW("java/lang/InternalError", "too many fields");
            bool wide = fl->desc[0] == 'J' || fl->desc[0] == 'D';
            pc[1] = (uint8_t)(fl->slot >> 8);
            pc[2] = (uint8_t)fl->slot;
            *pc = op == OP_GETFIELD ? (wide ? OP_GETFIELD2_Q : OP_GETFIELD_Q)
                                    : (wide ? OP_PUTFIELD2_Q : OP_PUTFIELD_Q);
            goto *dispatch[*pc];
        }
        CASE(OP_GETFIELD_Q) {
            Object *o = sp[-1].l;
            CHECK_NULL(o);
            sp[-1] = OBJ_FIELDS(o)[U2(pc + 1)];
            pc += 3;
            break;
        }
        CASE(OP_GETFIELD2_Q) {
            Object *o = sp[-1].l;
            CHECK_NULL(o);
            sp[-1] = OBJ_FIELDS(o)[U2(pc + 1)];
            sp++;
            pc += 3;
            break;
        }
        CASE(OP_PUTFIELD_Q) {
            Object *o = sp[-2].l;
            CHECK_NULL(o);
            OBJ_FIELDS(o)[U2(pc + 1)] = sp[-1];
            sp -= 2;
            pc += 3;
            break;
        }
        CASE(OP_PUTFIELD2_Q) {
            Object *o = sp[-3].l;
            CHECK_NULL(o);
            OBJ_FIELDS(o)[U2(pc + 1)] = sp[-2];
            sp -= 3;
            pc += 3;
            break;
        }
        CASE(OP_GETSTATIC_Q) {
            Field *fl = cp[U2(pc + 1)].field;
            *sp++ = fl->owner->statics[fl->slot];
            pc += 3;
            break;
        }
        CASE(OP_GETSTATIC2_Q) {
            Field *fl = cp[U2(pc + 1)].field;
            sp[0] = fl->owner->statics[fl->slot];
            sp += 2;
            pc += 3;
            break;
        }
        CASE(OP_PUTSTATIC_Q) {
            Field *fl = cp[U2(pc + 1)].field;
            fl->owner->statics[fl->slot] = *--sp;
            pc += 3;
            break;
        }
        CASE(OP_PUTSTATIC2_Q) {
            Field *fl = cp[U2(pc + 1)].field;
            sp -= 2;
            fl->owner->statics[fl->slot] = sp[0];
            pc += 3;
            break;
        }

        // --- invoke
        CASE(OP_INVOKEVIRTUAL) CASE(OP_INVOKESPECIAL) CASE(OP_INVOKESTATIC) CASE(OP_INVOKEINTERFACE) {
            uint16_t idx = U2(pc + 1);
            CPEntry *e = &cp[idx];
            Method *rm = e->resolved ? e->method : NULL;
            if (!rm) {
                SAVE_AT(insn);
                if (!(rm = resolve_method(t, m->owner, idx)))
                    goto exception;
            }
            len = op == OP_INVOKEINTERFACE ? 5 : 3;
            nargs = rm->arg_slots;
            target = rm;

            if (op == OP_INVOKESTATIC) {
                if (rm->owner->state != CLASS_INITIALIZED && !class_ensure_init(t, rm->owner))
                    RETRY();
                if (rm->owner->state == CLASS_INITIALIZED)
                    *insn = OP_INVOKESTATIC_Q;
            } else {
                Object *obj = sp[-nargs].l;
                CHECK_NULL(obj);
                if (op == OP_INVOKEVIRTUAL) {
                    if (rm->vtable_index >= 0 && !(rm->access & ACC_PRIVATE)) {
                        target = obj->cls->vtable[rm->vtable_index];
                        *insn = OP_INVOKEVIRTUAL_Q;
                    } else if (!(rm->access & ACC_PRIVATE)) {
                        SAVE_AT(insn);
                        if (!(target = lookup_virtual(t, e, obj->cls, rm)))
                            goto exception;
                    } else {
                        *insn = OP_INVOKESPECIAL_Q;
                    }
                } else if (op == OP_INVOKEINTERFACE) {
                    if (e->cache_cls == obj->cls) {
                        target = e->cache_method;
                    } else {
                        SAVE_AT(insn);
                        if (!(target = lookup_virtual(t, e, obj->cls, rm)))
                            goto exception;
                    }
                } else if (rm->name != S_init_name && !(rm->access & ACC_PRIVATE) &&
                           rm->vtable_index >= 0 && (m->owner->access & ACC_SUPER) &&
                           m->owner->super && rm->owner != m->owner &&
                           class_is_subclass(m->owner, rm->owner)) {
                    // invokespecial gọi super.method(): tìm từ lớp cha của lớp hiện tại
                    target = m->owner->super->vtable[rm->vtable_index];
                } else {
                    *insn = OP_INVOKESPECIAL_Q;
                }
            }
            goto invoke;
        }
        CASE(OP_INVOKEVIRTUAL_Q) {
            Method *rm = cp[U2(pc + 1)].method;
            nargs = rm->arg_slots;
            len = 3;
            Object *obj = sp[-nargs].l;
            CHECK_NULL(obj);
            target = obj->cls->vtable[rm->vtable_index];
            goto invoke;
        }
        CASE(OP_INVOKESPECIAL_Q) {
            target = cp[U2(pc + 1)].method;
            nargs = target->arg_slots;
            len = 3;
            CHECK_NULL(sp[-nargs].l);
            goto invoke;
        }
        CASE(OP_INVOKESTATIC_Q) {
            target = cp[U2(pc + 1)].method;
            nargs = target->arg_slots;
            len = 3;
            goto invoke;
        }
        invoke: {
            if (target->access & ACC_ABSTRACT) {
                char msg[300];
                snprintf(msg, sizeof(msg), "%s.%s%s", target->owner->name, target->name, target->desc);
                THROW("java/lang/AbstractMethodError", msg);
            }

            if (target->access & ACC_NATIVE) {
                if (!target->native) {
                    char msg[300];
                    snprintf(msg, sizeof(msg), "%s.%s%s", target->owner->name, target->name, target->desc);
                    THROW("java/lang/UnsatisfiedLinkError", msg);
                }
                SAVE_AT(insn);
                Value *args = sp - nargs;
                Value ret;
                ret.j = 0;
                int fc = t->frame_count;
                NativeResult r = target->native(t, args, &ret);
                switch (r) {
                case NATIVE_OK:
                    sp = args;
                    if (target->ret_type == 'J' || target->ret_type == 'D') {
                        sp[0] = ret;
                        sp += 2;
                    } else if (target->ret_type != 'V') {
                        *sp++ = ret;
                    }
                    pc += len;
                    if (t->state != TS_RUNNABLE) {
                        SAVE_AT(pc);
                        return;
                    }
                    break;
                case NATIVE_EXCEPTION:
                    goto exception;
                case NATIVE_RETRY:
                    if (t->frame_count != fc)
                        f->retry = 1;
                    if (t->exception)
                        goto exception;
                    if (t->state != TS_RUNNABLE)
                        return;
                    LOAD();
                    TICK();
                    break;
                case NATIVE_INVOKE: {
                    sp = args;
                    f->retry = 0;
                    SAVE_AT(pc + len);
                    Method *im = t->invoke_method;
                    if (!thread_push_frame(t, im, t->invoke_args)) {
                        f->pc = insn;
                        goto exception;
                    }
                    LOAD();
                    break;
                }
                }
                break;
            }

            Object *sync = NULL;
            if (target->access & ACC_SYNCHRONIZED) {
                sync = (target->access & ACC_STATIC) ? class_mirror(t, target->owner) : sp[-nargs].l;
                if (!monitor_enter(t, sync)) {
                    SAVE_AT(insn);
                    return;
                }
            }
            sp -= nargs;
            f->retry = 0;
            SAVE_AT(pc + len);
            if (!thread_push_frame(t, target, sp)) {
                if (sync)
                    monitor_exit(t, sync);
                f->sp = sp + nargs;
                f->pc = insn;
                goto exception;
            }
            t->frames[t->frame_count - 1].sync_obj = sync;
            LOAD();
            TICK();
            break;
        }

        // --- object
        CASE(OP_NEW) {
            SAVE_AT(insn);
            Class *c = resolve_class(t, m->owner, U2(pc + 1));
            if (!c)
                goto exception;
            if (c->access & (ACC_INTERFACE | ACC_ABSTRACT))
                THROW("java/lang/InstantiationError", c->name);
            if (c->state != CLASS_INITIALIZED && !class_ensure_init(t, c))
                RETRY();
            if (c->state == CLASS_INITIALIZED)
                *insn = OP_NEW_Q;
            Object *o = heap_alloc_object(t, c);
            if (!o)
                goto exception;
            PUSHL(o);
            pc += 3;
            break;
        }
        CASE(OP_NEW_Q) {
            SAVE_AT(insn);
            Object *o = heap_alloc_object(t, cp[U2(pc + 1)].cls);
            if (!o)
                goto exception;
            PUSHL(o);
            pc += 3;
            break;
        }
        CASE(OP_NEWARRAY) {
            static const char types[] = { 0, 0, 0, 0, 'Z', 'C', 'F', 'D', 'B', 'S', 'I', 'J' };
            jint n = POPI();
            uint8_t at = pc[1];
            if (at < 4 || at > 11)
                THROW("java/lang/ClassFormatError", "newarray");
            SAVE_AT(insn);
            Object *a = heap_alloc_array(t, class_prim_array(types[at]), n);
            if (!a)
                goto exception;
            PUSHL(a);
            pc += 2;
            break;
        }
        CASE(OP_ANEWARRAY) {
            SAVE_AT(insn);
            Class *c = resolve_class(t, m->owner, U2(pc + 1));
            if (!c)
                goto exception;
            Class *ac = class_array_of(t, c);
            if (!ac)
                goto exception;
            jint n = sp[-1].i;
            Object *a = heap_alloc_array(t, ac, n);
            if (!a)
                goto exception;
            sp[-1].l = a;
            pc += 3;
            break;
        }
        CASE(OP_MULTIANEWARRAY) {
            SAVE_AT(insn);
            Class *c = resolve_class(t, m->owner, U2(pc + 1));
            if (!c)
                goto exception;
            int dims = pc[3];
            jint counts[256];
            for (int i = 0; i < dims; i++)
                counts[i] = sp[-dims + i].i;
            Object *a = new_multi_array(t, c, dims, counts);
            if (!a)
                goto exception;
            sp -= dims;
            PUSHL(a);
            pc += 4;
            break;
        }
        CASE(OP_ARRAYLENGTH) {
            Object *a = sp[-1].l;
            CHECK_NULL(a);
            sp[-1].i = ARRAY_LEN(a);
            pc++;
            break;
        }
        CASE(OP_ATHROW) {
            Object *ex = sp[-1].l;
            CHECK_NULL(ex);
            SAVE_AT(insn);
            t->exception = ex;
            goto exception;
        }
        CASE(OP_CHECKCAST) {
            Object *o = sp[-1].l;
            if (o) {
                SAVE_AT(insn);
                Class *c = resolve_class(t, m->owner, U2(pc + 1));
                if (!c)
                    goto exception;
                *insn = OP_CHECKCAST_Q;
                goto *dispatch[OP_CHECKCAST_Q];
            }
            pc += 3;
            break;
        }
        CASE(OP_CHECKCAST_Q) {
            Object *o = sp[-1].l;
            Class *c = cp[U2(pc + 1)].cls;
            if (o && o->cls != c && !class_instance_of(o->cls, c)) {
                char msg[300];
                snprintf(msg, sizeof(msg), "%s cannot be cast to %s", o->cls->name, c->name);
                THROW("java/lang/ClassCastException", msg);
            }
            pc += 3;
            break;
        }
        CASE(OP_INSTANCEOF) {
            Object *o = sp[-1].l;
            if (o) {
                SAVE_AT(insn);
                Class *c = resolve_class(t, m->owner, U2(pc + 1));
                if (!c)
                    goto exception;
                *insn = OP_INSTANCEOF_Q;
                goto *dispatch[OP_INSTANCEOF_Q];
            }
            sp[-1].i = 0;
            pc += 3;
            break;
        }
        CASE(OP_INSTANCEOF_Q) {
            Object *o = sp[-1].l;
            Class *c = cp[U2(pc + 1)].cls;
            sp[-1].i = o && (o->cls == c || class_instance_of(o->cls, c)) ? 1 : 0;
            pc += 3;
            break;
        }
        CASE(OP_MONITORENTER) {
            Object *o = sp[-1].l;
            CHECK_NULL(o);
            if (!monitor_enter(t, o)) {
                SAVE_AT(insn);
                return;
            }
            sp--;
            pc++;
            break;
        }
        CASE(OP_MONITOREXIT) {
            Object *o = sp[-1].l;
            CHECK_NULL(o);
            if (!monitor_exit(t, o))
                THROW("java/lang/IllegalMonitorStateException", NULL);
            sp--;
            pc++;
            break;
        }

        CASE(OP_WIDE) {
            uint8_t wop = pc[1];
            uint16_t idx = U2(pc + 2);
            switch (wop) {
            case OP_ILOAD: case OP_FLOAD: case OP_ALOAD: *sp++ = locals[idx]; break;
            case OP_LLOAD: case OP_DLOAD: sp[0] = locals[idx]; sp += 2; break;
            case OP_ISTORE: case OP_FSTORE: case OP_ASTORE: locals[idx] = *--sp; break;
            case OP_LSTORE: case OP_DSTORE: sp -= 2; locals[idx] = sp[0]; break;
            case OP_RET: pc = m->code + locals[idx].i; TICK(); goto next_insn;
            case OP_IINC:
                locals[idx].i = (jint)((uint32_t)locals[idx].i + (uint32_t)(int32_t)S2(pc + 4));
                pc += 6;
                goto next_insn;
            default:
                THROW("java/lang/ClassFormatError", "wide");
            }
            pc += 4;
            break;
        }

        default: lbl_default: {
            char msg[64];
            snprintf(msg, sizeof(msg), "opcode 0x%02x", op);
            THROW("java/lang/InternalError", msg);
        }
        }
    next_insn:;
    }

exception:
    if (!t->exception)
        throw_new(t, "java/lang/InternalError", "exception without object");
    if (interp_handle_exception(t)) {
        LOAD();
        TICK();
        goto next;
    }
    // Không ai bắt: kết thúc thread
    {
        Object *ex = t->exception;
        vm_log("Uncaught exception trong thread %d:", t->id);
        exception_describe(t, ex);
        int n = snprintf(vm_uncaught_text, sizeof(vm_uncaught_text), "Thread %d: ", t->id);
        if (n > 0 && (size_t)n < sizeof(vm_uncaught_text))
            exception_text(ex, vm_uncaught_text + n, sizeof(vm_uncaught_text) - (size_t)n);
        t->exception = NULL;
        t->uncaught = ex;
        thread_terminate(t);
    }
}
