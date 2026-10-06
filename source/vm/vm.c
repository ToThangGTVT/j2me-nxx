// Khởi động / tắt VM, log, tiện ích chung
#include "vm_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static VMHost host;
static char last_error[512];

int FS_String_value = -1, FS_String_offset = -1, FS_String_count = -1;
int FS_Thread_vmThread = -1, FS_Thread_target = -1, FS_Thread_name = -1;
int FS_Throwable_detailMessage = -1, FS_Throwable_trace = -1;
int FS_Class_vmClass = -1;

const VMHost *vm_host(void) {
    return &host;
}

static FILE *prof_file;
static jlong prof_start;

void vm_prof_open(void *file) {
    prof_file = file;
    prof_start = vm_time_ms();
}

bool vm_prof_on(void) {
    return prof_file != NULL;
}

void vm_prof_log(const char *fmt, ...) {
    if (!prof_file)
        return;
    jlong t = vm_time_ms() - prof_start;
    fprintf(prof_file, "[%4lld.%03lld] ", (long long)(t / 1000), (long long)(t % 1000));
    va_list ap;
    va_start(ap, fmt);
    vfprintf(prof_file, fmt, ap);
    va_end(ap);
    fputc('\n', prof_file);
}

void vm_prof_flush(void) {
    if (prof_file)
        fflush(prof_file);
}

void vm_log(const char *fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    printf("[vm] %s\n", buf);
    fflush(stdout);
    if (host.log)
        host.log(buf);
}

const char *vm_last_error(void) {
    return last_error;
}

static void set_error(const char *msg) {
    snprintf(last_error, sizeof(last_error), "%s", msg);
    vm_log("%s", msg);
}

// Giờ thực lấy 1 lần lúc đầu, sau đó cộng theo đồng hồ đơn điệu: CLOCK_REALTIME
// trên Switch không đủ chính xác tới mili giây, game đo khung hình sẽ bị giật
jlong vm_time_ms(void) {
    static jlong base_real = -1, base_mono;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    jlong mono = (jlong)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
    if (base_real < 0) {
        struct timespec rt;
        clock_gettime(CLOCK_REALTIME, &rt);
        base_real = (jlong)rt.tv_sec * 1000 + rt.tv_nsec / 1000000;
        base_mono = mono;
    }
    return base_real + (mono - base_mono);
}

int field_slot(const char *cls, const char *name, const char *desc) {
    Class *c = class_load(NULL, cls);
    if (!c)
        return -1;
    Field *f = class_find_field(c, name, desc);
    if (!f || (f->access & ACC_STATIC)) {
        vm_log("Khong tim thay field %s.%s", cls, name);
        return -1;
    }
    return f->slot;
}

void field_slots_init(void) {
    FS_Class_vmClass = field_slot("java/lang/Class", "vmClass", "J");
    FS_String_value = field_slot("java/lang/String", "value", "[C");
    FS_String_offset = field_slot("java/lang/String", "offset", "I");
    FS_String_count = field_slot("java/lang/String", "count", "I");
    FS_Throwable_detailMessage = field_slot("java/lang/Throwable", "detailMessage", "Ljava/lang/String;");
    FS_Throwable_trace = field_slot("java/lang/Throwable", "vmTrace", "Ljava/lang/Object;");
    FS_Thread_vmThread = field_slot("java/lang/Thread", "vmThread", "J");
    FS_Thread_target = field_slot("java/lang/Thread", "target", "Ljava/lang/Runnable;");
    FS_Thread_name = field_slot("java/lang/Thread", "name", "Ljava/lang/String;");
}

bool vm_init(const VMHost *h) {
    host = *h;
    last_error[0] = '\0';
    natives_lang_init();
    jstring_init();

    if (!class_bootstrap()) {
        set_error("Khong nap duoc java.lang.Object/Class/String tu thu vien he thong");
        return false;
    }
    field_slots_init();
    if (FS_String_value < 0 || FS_Class_vmClass < 0 || FS_Thread_vmThread < 0) {
        set_error("Thu vien he thong thieu field can thiet");
        return false;
    }
    // Nạp sẵn các lớp hay dùng để lỗi thiếu lớp lộ ra sớm
    static const char *preload[] = {
        "java/lang/Thread", "java/lang/Throwable", "java/lang/NullPointerException",
        "java/lang/ArrayIndexOutOfBoundsException", "java/lang/ArithmeticException",
        "java/lang/ClassCastException", "java/lang/OutOfMemoryError",
    };
    for (size_t i = 0; i < sizeof(preload) / sizeof(preload[0]); i++) {
        if (!class_load(NULL, preload[i])) {
            char msg[128];
            snprintf(msg, sizeof(msg), "Thieu lop he thong %s", preload[i]);
            set_error(msg);
            return false;
        }
    }
    return true;
}

void vm_shutdown(void) {
    interp_reset();
    thread_free_all();
    heap_free_all();
    jstring_free_all();
    class_free_all();
}

VMThread *vm_spawn_static(const char *cls, const char *name, const char *desc, const Value *args, int nargs) {
    Class *c = class_load(NULL, cls);
    if (!c) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Khong tim thay lop %s", cls);
        set_error(msg);
        return NULL;
    }
    Method *m = class_find_method(c, name, desc);
    if (!m || !(m->access & ACC_STATIC) || m->arg_slots != nargs) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Khong tim thay method %s.%s%s", cls, name, desc);
        set_error(msg);
        return NULL;
    }
    VMThread *t = thread_new(NULL);
    if (!t)
        return NULL;
    // VMThread Java tương ứng được tạo lười trong Thread.currentThread()
    if (!thread_push_frame(t, m, args)) {
        thread_terminate(t);
        return NULL;
    }
    t->state = TS_RUNNABLE;
    // <clinit> của lớp chạy trước method
    if (c->state != CLASS_INITIALIZED)
        class_ensure_init(t, c);
    return t;
}

bool vm_call_static(const char *cls, const char *name, const char *desc, Value *args, Value *ret) {
    Class *c = class_load(NULL, cls);
    if (!c)
        return false;
    Method *m = class_find_method(c, name, desc);
    if (!m)
        return false;
    VMThread *t = vm_spawn_static(cls, name, desc, args, m->arg_slots);
    if (!t)
        return false;
    while (t->state != TS_TERMINATED) {
        if (!vm_run(50))
            break;
    }
    if (ret)
        *ret = t->result;
    return t->uncaught == NULL;
}
