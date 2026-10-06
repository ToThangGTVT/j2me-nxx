// Native của java.lang / java.util
#include "vm_internal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef __SWITCH__
#include <switch.h>
#endif

#define ARG_L(n) (args[n].l)
#define ARG_I(n) (args[n].i)
#define ARG_J(n) (args[n].j)
#define ARG_D(n) (args[n].d)
#define ARG_F(n) (args[n].f)

// ---------------------------------------------------------------------------
// System properties

typedef struct Prop {
    struct Prop *next;
    char *key, *value;
} Prop;

static Prop *props;

void vm_set_property(const char *key, const char *value) {
    for (Prop *p = props; p; p = p->next) {
        if (strcmp(p->key, key) == 0) {
            free(p->value);
            p->value = value ? strdup(value) : NULL;
            return;
        }
    }
    Prop *p = malloc(sizeof(Prop));
    p->key = strdup(key);
    p->value = value ? strdup(value) : NULL;
    p->next = props;
    props = p;
}

const char *vm_get_property(const char *key) {
    for (Prop *p = props; p; p = p->next) {
        if (strcmp(p->key, key) == 0)
            return p->value;
    }
    return NULL;
}

// ---------------------------------------------------------------------------
// java.lang.Object

static NativeResult Object_getClass(VMThread *t, Value *args, Value *ret) {
    ret->l = class_mirror(t, ARG_L(0)->cls);
    return ret->l ? NATIVE_OK : NATIVE_EXCEPTION;
}

static NativeResult Object_hashCode(VMThread *t, Value *args, Value *ret) {
    (void)t;
    ret->i = (jint)ARG_L(0)->hash;
    return NATIVE_OK;
}

static NativeResult Object_clone(VMThread *t, Value *args, Value *ret) {
    Object *o = ARG_L(0);
    Class *c = o->cls;
    Object *copy;
    if (c->is_array) {
        copy = heap_alloc_array(t, c, ARRAY_LEN(o));
        if (copy)
            memcpy(ARRAY_DATA(copy, uint8_t), ARRAY_DATA(o, uint8_t), (size_t)ARRAY_LEN(o) * c->elem_size);
    } else {
        copy = heap_alloc_object(t, c);
        if (copy)
            memcpy(OBJ_FIELDS(copy), OBJ_FIELDS(o), (size_t)c->instance_slots * sizeof(Value));
    }
    if (!copy)
        return NATIVE_EXCEPTION;
    ret->l = copy;
    return NATIVE_OK;
}

static NativeResult Object_notify(VMThread *t, Value *args, Value *ret) {
    (void)ret;
    if (!monitor_notify(t, ARG_L(0), false)) {
        throw_new(t, "java/lang/IllegalMonitorStateException", NULL);
        return NATIVE_EXCEPTION;
    }
    return NATIVE_OK;
}

static NativeResult Object_notifyAll(VMThread *t, Value *args, Value *ret) {
    (void)ret;
    if (!monitor_notify(t, ARG_L(0), true)) {
        throw_new(t, "java/lang/IllegalMonitorStateException", NULL);
        return NATIVE_EXCEPTION;
    }
    return NATIVE_OK;
}

static NativeResult Object_wait(VMThread *t, Value *args, Value *ret) {
    (void)ret;
    if (t->interrupted) {
        t->interrupted = false;
        throw_new(t, "java/lang/InterruptedException", NULL);
        return NATIVE_EXCEPTION;
    }
    jlong ms = ARG_J(1);
    if (ms < 0) {
        throw_new(t, "java/lang/IllegalArgumentException", "timeout < 0");
        return NATIVE_EXCEPTION;
    }
    if (monitor_wait(t, ARG_L(0), ms) < 0) {
        throw_new(t, "java/lang/IllegalMonitorStateException", NULL);
        return NATIVE_EXCEPTION;
    }
    return NATIVE_OK;
}

// ---------------------------------------------------------------------------
// java.lang.Class

static Object *class_name_string(VMThread *t, Class *c) {
    char *buf = strdup(c->name);
    for (char *p = buf; *p; p++) {
        if (*p == '/')
            *p = '.';
    }
    Object *s = jstring_new_utf8(t, buf);
    free(buf);
    return s;
}

static NativeResult Class_forName(VMThread *t, Value *args, Value *ret) {
    Object *name = ARG_L(0);
    if (!name) {
        throw_null(t);
        return NATIVE_EXCEPTION;
    }
    char *buf = jstring_to_utf8(name);
    for (char *p = buf; *p; p++) {
        if (*p == '.')
            *p = '/';
    }
    Class *c = class_load(t, buf);
    if (!c) {
        t->exception = NULL;
        throw_new(t, "java/lang/ClassNotFoundException", buf);
        free(buf);
        return NATIVE_EXCEPTION;
    }
    free(buf);
    if (c->state != CLASS_INITIALIZED && !class_ensure_init(t, c))
        return t->exception ? NATIVE_EXCEPTION : NATIVE_RETRY;
    ret->l = class_mirror(t, c);
    return NATIVE_OK;
}

static NativeResult Class_allocInstance(VMThread *t, Value *args, Value *ret) {
    Class *c = class_from_mirror(ARG_L(0));
    if (c->is_array || (c->access & (ACC_INTERFACE | ACC_ABSTRACT))) {
        throw_new(t, "java/lang/InstantiationException", c->name);
        return NATIVE_EXCEPTION;
    }
    if (c->state != CLASS_INITIALIZED && !class_ensure_init(t, c))
        return t->exception ? NATIVE_EXCEPTION : NATIVE_RETRY;
    ret->l = heap_alloc_object(t, c);
    return ret->l ? NATIVE_OK : NATIVE_EXCEPTION;
}

static NativeResult Class_initInstance(VMThread *t, Value *args, Value *ret) {
    (void)ret;
    Class *c = class_from_mirror(ARG_L(0));
    Method *init = class_find_declared_method(c, intern_cstr("<init>"), intern_cstr("()V"));
    if (!init) {
        throw_new(t, "java/lang/InstantiationException", c->name);
        return NATIVE_EXCEPTION;
    }
    t->invoke_method = init;
    t->invoke_args[0].l = ARG_L(1);
    return NATIVE_INVOKE;
}

static NativeResult Class_isInstance(VMThread *t, Value *args, Value *ret) {
    (void)t;
    Object *o = ARG_L(1);
    ret->i = o && class_instance_of(o->cls, class_from_mirror(ARG_L(0)));
    return NATIVE_OK;
}

static NativeResult Class_isAssignableFrom(VMThread *t, Value *args, Value *ret) {
    if (!ARG_L(1)) {
        throw_null(t);
        return NATIVE_EXCEPTION;
    }
    ret->i = class_instance_of(class_from_mirror(ARG_L(1)), class_from_mirror(ARG_L(0)));
    return NATIVE_OK;
}

static NativeResult Class_isInterface(VMThread *t, Value *args, Value *ret) {
    (void)t;
    ret->i = (class_from_mirror(ARG_L(0))->access & ACC_INTERFACE) != 0;
    return NATIVE_OK;
}

static NativeResult Class_isArray(VMThread *t, Value *args, Value *ret) {
    (void)t;
    ret->i = class_from_mirror(ARG_L(0))->is_array;
    return NATIVE_OK;
}

static NativeResult Class_getName(VMThread *t, Value *args, Value *ret) {
    ret->l = class_name_string(t, class_from_mirror(ARG_L(0)));
    return ret->l ? NATIVE_OK : NATIVE_EXCEPTION;
}

static NativeResult Class_getResourceData(VMThread *t, Value *args, Value *ret) {
    Object *name = ARG_L(0);
    if (!name) {
        ret->l = NULL;
        return NATIVE_OK;
    }
    char *n = jstring_to_utf8(name);
    size_t size = 0;
    uint8_t *data = vm_host()->read_resource ? vm_host()->read_resource(n, &size) : NULL;
    free(n);
    if (!data) {
        ret->l = NULL;
        return NATIVE_OK;
    }
    Object *arr = heap_new_prim_array(t, 'B', (jint)size);
    if (arr)
        memcpy(ARRAY_DATA(arr, uint8_t), data, size);
    free(data);
    ret->l = arr;
    return arr ? NATIVE_OK : NATIVE_EXCEPTION;
}

// ---------------------------------------------------------------------------
// java.lang.Throwable

static NativeResult Throwable_fillInStackTrace0(VMThread *t, Value *args, Value *ret) {
    (void)ret;
    throwable_fill_trace(t, ARG_L(0));
    return NATIVE_OK;
}

static NativeResult Throwable_printStackTrace0(VMThread *t, Value *args, Value *ret) {
    (void)ret;
    exception_describe(t, ARG_L(0));
    return NATIVE_OK;
}

// ---------------------------------------------------------------------------
// java.lang.VMThread

static VMThread *vm_thread_of(Object *jthread) {
    return (VMThread *)(intptr_t)OBJ_FIELDS(jthread)[FS_Thread_vmThread].j;
}

static NativeResult Thread_start0(VMThread *t, Value *args, Value *ret) {
    (void)ret;
    Object *self = ARG_L(0);
    Method *run = class_find_method(self->cls, "run", "()V");
    VMThread *nt = thread_new(self);
    if (!nt || !run) {
        throw_new(t, "java/lang/OutOfMemoryError", "thread");
        return NATIVE_EXCEPTION;
    }
    Value a;
    a.l = self;
    if (!thread_push_frame(nt, run, &a)) {
        nt->exception = NULL;
        thread_terminate(nt);
        throw_new(t, "java/lang/OutOfMemoryError", "thread stack");
        return NATIVE_EXCEPTION;
    }
    nt->state = TS_RUNNABLE;
    return NATIVE_OK;
}

static NativeResult Thread_currentThread(VMThread *t, Value *args, Value *ret) {
    (void)args;
    if (!t->jthread) {
        Class *c = class_load(t, "java/lang/Thread");
        if (!c)
            return NATIVE_EXCEPTION;
        Object *o = heap_alloc_object(t, c);
        if (!o)
            return NATIVE_EXCEPTION;
        OBJ_FIELDS(o)[FS_Thread_vmThread].j = (jlong)(intptr_t)t;
        if (FS_Thread_name >= 0)
            OBJ_FIELDS(o)[FS_Thread_name].l = jstring_new_utf8(t, t->id == 1 ? "main" : "vm");
        Field *prio = class_find_field(c, "priority", "I");
        if (prio)
            OBJ_FIELDS(o)[prio->slot].i = 5;
        t->jthread = o;
    }
    ret->l = t->jthread;
    return NATIVE_OK;
}

static NativeResult Thread_sleep(VMThread *t, Value *args, Value *ret) {
    (void)ret;
    jlong ms = ARG_J(0);
    if (ms < 0) {
        throw_new(t, "java/lang/IllegalArgumentException", "timeout < 0");
        return NATIVE_EXCEPTION;
    }
    if (t->interrupted) {
        t->interrupted = false;
        throw_new(t, "java/lang/InterruptedException", NULL);
        return NATIVE_EXCEPTION;
    }
    t->state = TS_SLEEPING;
    t->wake_time = vm_time_ms() + ms;
    return NATIVE_OK;
}

static NativeResult Thread_yield(VMThread *t, Value *args, Value *ret) {
    (void)args;
    (void)ret;
    t->state = TS_SLEEPING;
    t->wake_time = vm_time_ms();
    return NATIVE_OK;
}

static NativeResult Thread_activeCount(VMThread *t, Value *args, Value *ret) {
    (void)t;
    (void)args;
    int n = 0;
    for (VMThread *x = thread_list(); x; x = x->next) {
        if (x->state != TS_TERMINATED)
            n++;
    }
    ret->i = n;
    return NATIVE_OK;
}

static NativeResult Thread_isAlive(VMThread *t, Value *args, Value *ret) {
    (void)t;
    VMThread *vt = vm_thread_of(ARG_L(0));
    ret->i = vt && vt->state != TS_TERMINATED;
    return NATIVE_OK;
}

static NativeResult Thread_interrupt0(VMThread *t, Value *args, Value *ret) {
    (void)t;
    (void)ret;
    VMThread *vt = vm_thread_of(ARG_L(0));
    if (vt)
        vt->interrupted = true;
    return NATIVE_OK;
}

// ---------------------------------------------------------------------------
// java.lang.Runtime / System

static NativeResult Runtime_freeMemory(VMThread *t, Value *args, Value *ret) {
    (void)t;
    (void)args;
    size_t total = heap_total(), used = heap_used();
    ret->j = used < total ? (jlong)(total - used) : 0;
    return NATIVE_OK;
}

static NativeResult Runtime_totalMemory(VMThread *t, Value *args, Value *ret) {
    (void)t;
    (void)args;
    ret->j = (jlong)heap_total();
    return NATIVE_OK;
}

static NativeResult System_currentTimeMillis(VMThread *t, Value *args, Value *ret) {
    (void)t;
    (void)args;
    ret->j = vm_time_ms();
    return NATIVE_OK;
}

static NativeResult System_arraycopy(VMThread *t, Value *args, Value *ret) {
    (void)ret;
    Object *src = ARG_L(0), *dst = ARG_L(2);
    jint sp = ARG_I(1), dp = ARG_I(3), len = ARG_I(4);
    if (!src || !dst) {
        throw_null(t);
        return NATIVE_EXCEPTION;
    }
    Class *sc = src->cls, *dc = dst->cls;
    if (!sc->is_array || !dc->is_array) {
        throw_new(t, "java/lang/ArrayStoreException", "not an array");
        return NATIVE_EXCEPTION;
    }
    bool sref = sc->elem_type == 'L' || sc->elem_type == '[';
    bool dref = dc->elem_type == 'L' || dc->elem_type == '[';
    if (sref != dref || (!sref && sc->elem_type != dc->elem_type)) {
        throw_new(t, "java/lang/ArrayStoreException", "type mismatch");
        return NATIVE_EXCEPTION;
    }
    if (len < 0 || sp < 0 || dp < 0 || (int64_t)sp + len > ARRAY_LEN(src) || (int64_t)dp + len > ARRAY_LEN(dst)) {
        throw_new(t, "java/lang/ArrayIndexOutOfBoundsException", "arraycopy");
        return NATIVE_EXCEPTION;
    }
    if (len == 0)
        return NATIVE_OK;

    if (sref && sc != dc && !class_instance_of(sc, dc)) {
        // Kiểm tra từng phần tử
        Object **s = ARRAY_DATA(src, Object *) + sp;
        Object **d = ARRAY_DATA(dst, Object *) + dp;
        for (jint i = 0; i < len; i++) {
            if (s[i] && !class_instance_of(s[i]->cls, dc->component)) {
                throw_new(t, "java/lang/ArrayStoreException", NULL);
                return NATIVE_EXCEPTION;
            }
            d[i] = s[i];
        }
        return NATIVE_OK;
    }
    int es = sc->elem_size;
    memmove(ARRAY_DATA(dst, uint8_t) + (size_t)dp * es, ARRAY_DATA(src, uint8_t) + (size_t)sp * es, (size_t)len * es);
    return NATIVE_OK;
}

static NativeResult System_identityHashCode(VMThread *t, Value *args, Value *ret) {
    (void)t;
    ret->i = ARG_L(0) ? (jint)ARG_L(0)->hash : 0;
    return NATIVE_OK;
}

static NativeResult System_getProperty0(VMThread *t, Value *args, Value *ret) {
    char key[256];
    jstring_to_cstr(ARG_L(0), key, sizeof(key));
    const char *v = vm_get_property(key);
    ret->l = v ? jstring_new_utf8(t, v) : NULL;
    return NATIVE_OK;
}

static NativeResult System_exit0(VMThread *t, Value *args, Value *ret) {
    (void)ret;
    vm_log("System.exit(%d)", ARG_I(0));
    if (vm_host()->exit_request)
        vm_host()->exit_request(ARG_I(0));
    thread_terminate(t);
    return NATIVE_OK;
}

static NativeResult System_gc(VMThread *t, Value *args, Value *ret) {
    (void)t;
    (void)args;
    (void)ret;
    heap_request_gc();
    return NATIVE_OK;
}

static NativeResult LogOutputStream_log(VMThread *t, Value *args, Value *ret) {
    (void)t;
    (void)ret;
    Object *b = ARG_L(0);
    jint len = ARG_I(1);
    if (!b || len < 0 || len > ARRAY_LEN(b))
        return NATIVE_OK;
    char *buf = malloc((size_t)len + 1);
    memcpy(buf, ARRAY_DATA(b, char), (size_t)len);
    buf[len] = '\0';
    vm_log("%s%s", ARG_I(2) ? "[err] " : "", buf);
    free(buf);
    return NATIVE_OK;
}

// ---------------------------------------------------------------------------
// java.lang.String / Math / Float / Double

static NativeResult String_intern(VMThread *t, Value *args, Value *ret) {
    ret->l = jstring_intern(t, ARG_L(0));
    return NATIVE_OK;
}

#define MATH1(name, fn)                                                         \
    static NativeResult Math_##name(VMThread *t, Value *args, Value *ret) {       \
        (void)t;                                                                \
        ret->d = fn(ARG_D(0));                                                  \
        return NATIVE_OK;                                                       \
    }
MATH1(sin, sin)
MATH1(cos, cos)
MATH1(tan, tan)
MATH1(asin, asin)
MATH1(acos, acos)
MATH1(atan, atan)
MATH1(sqrt, sqrt)
MATH1(ceil, ceil)
MATH1(floor, floor)
MATH1(exp, exp)
MATH1(log, log)

static NativeResult Math_atan2(VMThread *t, Value *args, Value *ret) {
    (void)t;
    ret->d = atan2(ARG_D(0), ARG_D(2));
    return NATIVE_OK;
}

static NativeResult Math_pow(VMThread *t, Value *args, Value *ret) {
    (void)t;
    ret->d = pow(ARG_D(0), ARG_D(2));
    return NATIVE_OK;
}

static NativeResult Float_floatToIntBits(VMThread *t, Value *args, Value *ret) {
    (void)t;
    float f = ARG_F(0);
    if (isnan(f)) {
        ret->i = 0x7fc00000;
    } else {
        memcpy(&ret->i, &f, 4);
    }
    return NATIVE_OK;
}

static NativeResult Float_intBitsToFloat(VMThread *t, Value *args, Value *ret) {
    (void)t;
    jint i = ARG_I(0);
    memcpy(&ret->f, &i, 4);
    return NATIVE_OK;
}

static NativeResult Double_doubleToLongBits(VMThread *t, Value *args, Value *ret) {
    (void)t;
    double d = ARG_D(0);
    if (isnan(d)) {
        ret->j = 0x7ff8000000000000LL;
    } else {
        memcpy(&ret->j, &d, 8);
    }
    return NATIVE_OK;
}

static NativeResult Double_longBitsToDouble(VMThread *t, Value *args, Value *ret) {
    (void)t;
    jlong j = ARG_J(0);
    memcpy(&ret->d, &j, 8);
    return NATIVE_OK;
}

// Định dạng số thực theo kiểu Java: chữ số ngắn nhất, "1.0", "1.0E10"
static void java_double_str(double d, bool is_float, char *out, size_t size) {
    if (isnan(d)) {
        snprintf(out, size, "NaN");
        return;
    }
    if (isinf(d)) {
        snprintf(out, size, d > 0 ? "Infinity" : "-Infinity");
        return;
    }
    if (d == 0) {
        snprintf(out, size, signbit(d) ? "-0.0" : "0.0");
        return;
    }

    char buf[64];
    int maxp = is_float ? 8 : 16;
    for (int p = 0; p <= maxp; p++) {
        snprintf(buf, sizeof(buf), "%.*e", p, d);
        if (is_float ? (strtof(buf, NULL) == (float)d) : (strtod(buf, NULL) == d))
            break;
    }

    // buf: [-]D[.DDD]e[+-]XX
    bool neg = buf[0] == '-';
    char *p = buf + (neg ? 1 : 0);
    char digits[40];
    int nd = 0;
    while (*p && *p != 'e') {
        if (*p != '.')
            digits[nd++] = *p;
        p++;
    }
    digits[nd] = '\0';
    int exp = atoi(p + 1);
    while (nd > 1 && digits[nd - 1] == '0')
        digits[--nd] = '\0';

    double a = fabs(d);
    size_t pos = 0;
    if (neg)
        out[pos++] = '-';
    if (a >= 1e-3 && a < 1e7) {
        if (exp >= 0) {
            for (int i = 0; i <= exp; i++)
                out[pos++] = i < nd ? digits[i] : '0';
            out[pos++] = '.';
            if (exp + 1 < nd) {
                for (int i = exp + 1; i < nd; i++)
                    out[pos++] = digits[i];
            } else {
                out[pos++] = '0';
            }
        } else {
            out[pos++] = '0';
            out[pos++] = '.';
            for (int i = 0; i < -exp - 1; i++)
                out[pos++] = '0';
            for (int i = 0; i < nd; i++)
                out[pos++] = digits[i];
        }
        out[pos] = '\0';
    } else {
        out[pos++] = digits[0];
        out[pos++] = '.';
        if (nd > 1) {
            for (int i = 1; i < nd; i++)
                out[pos++] = digits[i];
        } else {
            out[pos++] = '0';
        }
        snprintf(out + pos, size - pos, "E%d", exp);
    }
}

static NativeResult Double_toString0(VMThread *t, Value *args, Value *ret) {
    char buf[64];
    java_double_str(ARG_D(0), ARG_I(2) != 0, buf, sizeof(buf));
    ret->l = jstring_new_utf8(t, buf);
    return ret->l ? NATIVE_OK : NATIVE_EXCEPTION;
}

static NativeResult Double_parse0(VMThread *t, Value *args, Value *ret) {
    (void)t;
    char buf[128];
    jstring_to_cstr(ARG_L(0), buf, sizeof(buf));
    char *end;
    double v = strtod(buf, &end);
    if (end == buf) {
        ret->i = 0;
        return NATIVE_OK;
    }
    if (*end == 'f' || *end == 'F' || *end == 'd' || *end == 'D')
        end++;
    if (*end) {
        ret->i = 0;
        return NATIVE_OK;
    }
    ARRAY_DATA(ARG_L(1), jdouble)[0] = v;
    ret->i = 1;
    return NATIVE_OK;
}

// ---------------------------------------------------------------------------
// java.util.TimeZone

static NativeResult TimeZone_getDefaultOffset0(VMThread *t, Value *args, Value *ret) {
    (void)t;
    (void)args;
    int offset = 0;
#ifdef __SWITCH__
    u64 now = 0;
    TimeCalendarTime cal;
    TimeCalendarAdditionalInfo info;
    if (R_SUCCEEDED(timeGetCurrentTime(TimeType_Default, &now)) &&
        R_SUCCEEDED(timeToCalendarTimeWithMyRule(now, &cal, &info)))
        offset = info.offset * 1000;
#else
    time_t now = time(NULL);
    struct tm lt = *localtime(&now);
    struct tm gt = *gmtime(&now);
    gt.tm_isdst = lt.tm_isdst;
    offset = (int)difftime(mktime(&lt), mktime(&gt)) * 1000;
#endif
    ret->i = offset;
    return NATIVE_OK;
}

// ---------------------------------------------------------------------------

void natives_lang_init(void) {
    static bool done;
    if (done)
        return;
    done = true;

    native_register("java/lang/Object", "getClass", "()Ljava/lang/Class;", Object_getClass);
    native_register("java/lang/Object", "hashCode", "()I", Object_hashCode);
    native_register("java/lang/Object", "clone", "()Ljava/lang/Object;", Object_clone);
    native_register("java/lang/Object", "notify", "()V", Object_notify);
    native_register("java/lang/Object", "notifyAll", "()V", Object_notifyAll);
    native_register("java/lang/Object", "wait", "(J)V", Object_wait);

    native_register("java/lang/Class", "forName", "(Ljava/lang/String;)Ljava/lang/Class;", Class_forName);
    native_register("java/lang/Class", "allocInstance", "()Ljava/lang/Object;", Class_allocInstance);
    native_register("java/lang/Class", "initInstance", "(Ljava/lang/Object;)V", Class_initInstance);
    native_register("java/lang/Class", "isInstance", "(Ljava/lang/Object;)Z", Class_isInstance);
    native_register("java/lang/Class", "isAssignableFrom", "(Ljava/lang/Class;)Z", Class_isAssignableFrom);
    native_register("java/lang/Class", "isInterface", "()Z", Class_isInterface);
    native_register("java/lang/Class", "isArray", "()Z", Class_isArray);
    native_register("java/lang/Class", "getName", "()Ljava/lang/String;", Class_getName);
    native_register("java/lang/Class", "getResourceData", "(Ljava/lang/String;)[B", Class_getResourceData);

    native_register("java/lang/Throwable", "fillInStackTrace0", "()V", Throwable_fillInStackTrace0);
    native_register("java/lang/Throwable", "printStackTrace0", "()V", Throwable_printStackTrace0);

    native_register("java/lang/Thread", "start0", "()V", Thread_start0);
    native_register("java/lang/Thread", "currentThread", "()Ljava/lang/Thread;", Thread_currentThread);
    native_register("java/lang/Thread", "sleep", "(J)V", Thread_sleep);
    native_register("java/lang/Thread", "yield", "()V", Thread_yield);
    native_register("java/lang/Thread", "activeCount", "()I", Thread_activeCount);
    native_register("java/lang/Thread", "isAlive", "()Z", Thread_isAlive);
    native_register("java/lang/Thread", "interrupt0", "()V", Thread_interrupt0);

    native_register("java/lang/Runtime", "freeMemory", "()J", Runtime_freeMemory);
    native_register("java/lang/Runtime", "totalMemory", "()J", Runtime_totalMemory);

    native_register("java/lang/System", "currentTimeMillis", "()J", System_currentTimeMillis);
    native_register("java/lang/System", "arraycopy", "(Ljava/lang/Object;ILjava/lang/Object;II)V", System_arraycopy);
    native_register("java/lang/System", "identityHashCode", "(Ljava/lang/Object;)I", System_identityHashCode);
    native_register("java/lang/System", "getProperty0", "(Ljava/lang/String;)Ljava/lang/String;", System_getProperty0);
    native_register("java/lang/System", "exit0", "(I)V", System_exit0);
    native_register("java/lang/System", "gc", "()V", System_gc);
    native_register("java/lang/LogOutputStream", "log", "([BIZ)V", LogOutputStream_log);

    native_register("java/lang/String", "intern", "()Ljava/lang/String;", String_intern);

    native_register("java/lang/Math", "sin", "(D)D", Math_sin);
    native_register("java/lang/Math", "cos", "(D)D", Math_cos);
    native_register("java/lang/Math", "tan", "(D)D", Math_tan);
    native_register("java/lang/Math", "asin", "(D)D", Math_asin);
    native_register("java/lang/Math", "acos", "(D)D", Math_acos);
    native_register("java/lang/Math", "atan", "(D)D", Math_atan);
    native_register("java/lang/Math", "atan2", "(DD)D", Math_atan2);
    native_register("java/lang/Math", "sqrt", "(D)D", Math_sqrt);
    native_register("java/lang/Math", "ceil", "(D)D", Math_ceil);
    native_register("java/lang/Math", "floor", "(D)D", Math_floor);
    native_register("java/lang/Math", "exp", "(D)D", Math_exp);
    native_register("java/lang/Math", "log", "(D)D", Math_log);
    native_register("java/lang/Math", "pow", "(DD)D", Math_pow);

    native_register("java/lang/Float", "floatToIntBits", "(F)I", Float_floatToIntBits);
    native_register("java/lang/Float", "intBitsToFloat", "(I)F", Float_intBitsToFloat);
    native_register("java/lang/Double", "doubleToLongBits", "(D)J", Double_doubleToLongBits);
    native_register("java/lang/Double", "longBitsToDouble", "(J)D", Double_longBitsToDouble);
    native_register("java/lang/Double", "toString0", "(DZ)Ljava/lang/String;", Double_toString0);
    native_register("java/lang/Double", "parse0", "(Ljava/lang/String;[D)Z", Double_parse0);

    native_register("java/util/TimeZone", "getDefaultOffset0", "()I", TimeZone_getDefaultOffset0);
}
