// Máy ảo Java tối giản cho J2ME (CLDC 1.1)
//
// - Mỗi slot (local / operand stack / field) là 1 Value 8 byte.
//   long/double chiếm 2 slot trên stack và local như spec (giá trị nằm ở slot đầu).
// - Field của object: mỗi field 1 slot, kể cả long/double.
// - Green thread: mọi Java thread chạy trên 1 thread của host, scheduler xoay vòng.
// - GC mark-sweep: field/array/static quét chính xác, stack quét bảo thủ.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct Class Class;
typedef struct Object Object;
typedef struct Method Method;
typedef struct Field Field;
typedef struct VMThread VMThread;
typedef struct Monitor Monitor;

typedef int8_t   jbyte;
typedef uint16_t jchar;
typedef int16_t  jshort;
typedef int32_t  jint;
typedef int64_t  jlong;
typedef float    jfloat;
typedef double   jdouble;

typedef union {
    jint i;
    jlong j;
    jfloat f;
    jdouble d;
    Object *l;
} Value;

struct Object {
    Class *cls;
    Monitor *monitor;
    uint32_t hash;
    uint32_t marked;
};

typedef struct {
    Object hdr;
    jint length;
    jint pad;
    uint8_t data[];
} Array;

#define OBJ_FIELDS(o)       ((Value *)((Object *)(o) + 1))
#define ARRAY_LEN(a)        (((Array *)(a))->length)
#define ARRAY_DATA(a, T)    ((T *)((Array *)(a))->data)

// ---------------------------------------------------------------------------
// Class

enum {
    ACC_PUBLIC       = 0x0001,
    ACC_PRIVATE      = 0x0002,
    ACC_PROTECTED    = 0x0004,
    ACC_STATIC       = 0x0008,
    ACC_FINAL        = 0x0010,
    ACC_SYNCHRONIZED = 0x0020,
    ACC_SUPER        = 0x0020,
    ACC_NATIVE       = 0x0100,
    ACC_INTERFACE    = 0x0200,
    ACC_ABSTRACT     = 0x0400,
};

enum {
    CONST_Utf8 = 1,
    CONST_Integer = 3,
    CONST_Float = 4,
    CONST_Long = 5,
    CONST_Double = 6,
    CONST_Class = 7,
    CONST_String = 8,
    CONST_Fieldref = 9,
    CONST_Methodref = 10,
    CONST_InterfaceMethodref = 11,
    CONST_NameAndType = 12,
};

typedef struct {
    uint8_t tag;
    uint8_t resolved;
    uint16_t a, b;              // chỉ số thô trong constant pool
    union {
        jint i;
        jfloat f;
        jlong j;
        jdouble d;
        const char *utf8;       // đã intern
        Object *str;            // CONST_String đã resolve
        Class *cls;             // CONST_Class đã resolve
        Field *field;
        Method *method;
    };
    // cache cho invokeinterface / invokevirtual trên interface
    Class *cache_cls;
    Method *cache_method;
} CPEntry;

struct Field {
    Class *owner;
    const char *name;           // intern
    const char *desc;           // intern
    uint16_t access;
    uint16_t const_index;       // ConstantValue attribute (static final)
    int slot;                   // chỉ số trong OBJ_FIELDS hoặc owner->statics
    bool is_ref;
};

typedef struct {
    uint16_t start_pc, end_pc, handler_pc, catch_type;
} ExceptionEntry;

typedef struct {
    uint16_t start_pc, line;
} LineEntry;

typedef enum {
    NATIVE_OK,          // xong, kết quả ở *ret
    NATIVE_EXCEPTION,   // đã đặt t->exception
    NATIVE_RETRY,       // chưa xong (thread bị block / đã đẩy frame <clinit>), chạy lại lệnh sau
    NATIVE_INVOKE,      // native nhờ interpreter gọi t->invoke_method với t->invoke_args
} NativeResult;

typedef NativeResult (*NativeFn)(VMThread *t, Value *args, Value *ret);

struct Method {
    Class *owner;
    const char *name;           // intern
    const char *desc;           // intern
    uint16_t access;
    uint16_t max_stack;
    uint16_t max_locals;
    uint16_t exc_count;
    uint16_t line_count;
    uint32_t code_len;
    uint8_t *code;
    ExceptionEntry *exc;
    LineEntry *lines;
    int arg_slots;              // số slot tham số, tính cả this
    char ret_type;              // 'V' 'I' 'J' 'F' 'D' 'L' (ref, gồm cả mảng) 'Z' 'B' 'C' 'S'
    int vtable_index;           // -1 nếu không phải phương thức ảo
    NativeFn native;
};

typedef enum {
    CLASS_LOADED,
    CLASS_LINKED,
    CLASS_INITIALIZING,
    CLASS_INITIALIZED,
    CLASS_ERROR,
} ClassState;

struct Class {
    const char *name;           // dạng nội bộ: "java/lang/String", "[I", "[Ljava/lang/Object;"
    Class *super;
    Class **interfaces;
    uint16_t iface_count;
    uint16_t access;
    CPEntry *cp;
    uint16_t cp_count;
    Field *fields;
    uint16_t field_count;
    Method *methods;
    uint16_t method_count;
    const char *source_file;

    int instance_slots;         // tổng slot field instance, tính cả lớp cha
    uint8_t *slot_is_ref;       // [instance_slots]
    Value *statics;
    int static_count;
    Method **vtable;
    int vtable_len;

    ClassState state;
    VMThread *init_thread;
    Object *mirror;             // java.lang.Class

    // Mảng
    bool is_array;
    char elem_type;             // 'Z' 'B' 'C' 'S' 'I' 'J' 'F' 'D' 'L' '['
    int elem_size;
    Class *component;           // NULL với mảng kiểu nguyên thủy
    Class *array_class;         // cache: lớp mảng có component là lớp này

    bool from_game;             // nạp từ JAR game (không phải thư viện hệ thống)
    Class *hash_next;
};

// ---------------------------------------------------------------------------
// VMThread

typedef struct {
    Method *m;
    uint8_t *pc;                // lệnh đang chạy (với caller: lệnh invoke)
    Value *locals;
    Value *sp;
    Value *stack_base;
    Class *clinit_of;           // frame này là <clinit> của lớp nào
    Object *sync_obj;           // monitor cần nhả khi return (method synchronized)
    uint8_t retry;              // pc đang trỏ vào lệnh sẽ chạy lại (không phải lệnh sau invoke)
} Frame;

typedef enum {
    TS_NEW,
    TS_RUNNABLE,
    TS_SLEEPING,
    TS_BLOCKED,                 // chờ monitor (monitorenter / synchronized)
    TS_WAITING,                 // Object.wait()
    TS_REACQUIRE,               // được notify, chờ lấy lại monitor
    TS_WAIT_EVENT,              // chờ sự kiện từ host (Display)
    TS_WAIT_INIT,               // chờ thread khác chạy xong <clinit>
    TS_TERMINATED,
} ThreadState;

#define THREAD_STACK_SLOTS  (32 * 1024)
#define THREAD_MAX_FRAMES   1024

struct VMThread {
    int id;
    Object *jthread;            // java.lang.VMThread
    ThreadState state;
    jlong wake_time;            // ms, cho SLEEPING / WAITING có timeout (0 = không)
    Object *wait_obj;           // object đang chờ (BLOCKED / WAITING / REACQUIRE)
    int saved_count;            // số lần lock monitor trước khi wait()
    bool interrupted;

    Value *stack;
    Frame *frames;
    int frame_count;

    Object *exception;          // exception đang lan truyền
    Object *uncaught;           // exception làm thread chết (để báo lỗi)
    Value result;               // giá trị trả về của frame đầu tiên

    // NATIVE_INVOKE
    Method *invoke_method;
    Value invoke_args[8];

    VMThread *next;
};

struct Monitor {
    VMThread *owner;
    int count;
};

// ---------------------------------------------------------------------------
// API VM

typedef struct {
    // Đọc file trong classpath (thư viện hệ thống hoặc JAR game). Trả về buffer malloc.
    uint8_t *(*read_file)(const char *name, size_t *size, bool *from_game);
    // Đọc file resource từ JAR game (Class.getResourceAsStream)
    uint8_t *(*read_resource)(const char *name, size_t *size);
    void (*log)(const char *msg);
    // System.exit() / MIDlet.notifyDestroyed()
    void (*exit_request)(int status);
} VMHost;

bool vm_init(const VMHost *host);
void vm_shutdown(void);
const VMHost *vm_host(void);

// Chạy các thread tối đa budget_ms. Trả về false khi không còn thread nào sống.
bool vm_run(int budget_ms);
// Tổng thời gian vm_run ngủ chờ (không có thread nào chạy) kể từ lần gọi trước
jlong vm_take_idle_ms(void);
// Thời điểm sớm nhất (ms) có thread thức dậy, hoặc -1 nếu không có thread hẹn giờ
jlong vm_next_wakeup(void);
bool vm_has_runnable(void);
jlong vm_time_ms(void);

void vm_log(const char *fmt, ...);
const char *vm_last_error(void);

// System.getProperty
void vm_set_property(const char *key, const char *value);
const char *vm_get_property(const char *key);

// --- string intern (tên lớp / method / descriptor)
const char *intern(const char *s, size_t len);
const char *intern_cstr(const char *s);

// --- class
Class *class_load(VMThread *t, const char *name);       // name dạng nội bộ, đã intern hoặc không
Class *class_find_loaded(const char *name);
Class *class_array_of(VMThread *t, Class *component);   // lớp mảng của component
Class *class_prim_array(char type);                   // "[I" ...
bool class_is_subclass(const Class *sub, const Class *super);
bool class_instance_of(const Class *c, const Class *target);
Field *class_find_field(Class *c, const char *name, const char *desc);
Method *class_find_method(Class *c, const char *name, const char *desc);   // tìm cả lớp cha
Method *class_find_declared_method(Class *c, const char *name, const char *desc);
Method *class_find_interface_method(Class *c, const char *name, const char *desc);
// true nếu đã sẵn sàng, false nếu đã đẩy frame <clinit> / phải chờ (chạy lại lệnh sau)
bool class_ensure_init(VMThread *t, Class *c);
Object *class_mirror(VMThread *t, Class *c);
Class *class_from_mirror(Object *mirror);
int method_line(const Method *m, const uint8_t *pc);

// --- heap
Object *heap_alloc_object(VMThread *t, Class *c);
Object *heap_alloc_array(VMThread *t, Class *array_cls, jint len);
Object *heap_new_prim_array(VMThread *t, char type, jint len);
bool heap_is_object(const void *p);
void heap_add_root(Object **slot);
void heap_remove_root(Object **slot);
void heap_request_gc(void);
void heap_gc(void);
size_t heap_used(void);
size_t heap_total(void);
void heap_free_all(void);

// --- thread
VMThread *thread_new(Object *jthread);
VMThread *thread_current(void);
VMThread *thread_list(void);
void thread_terminate(VMThread *t);
// Đẩy frame mới cho method; args (arg_slots giá trị) được chép vào locals.
bool thread_push_frame(VMThread *t, Method *m, const Value *args);
bool monitor_enter(VMThread *t, Object *o);              // false => thread bị block, thử lại sau
bool monitor_exit(VMThread *t, Object *o);               // false => IllegalMonitorStateException
int monitor_wait(VMThread *t, Object *o, jlong ms);      // 0 ok, -1 lỗi monitor
bool monitor_notify(VMThread *t, Object *o, bool all);
void thread_wake_waiters(void);

// --- interpreter
void interp_run(VMThread *t, int max_instructions);

// --- exception
void throw_new(VMThread *t, const char *cls_name, const char *msg);
void throw_null(VMThread *t);
void throw_object(VMThread *t, Object *ex);
void exception_describe(VMThread *t, Object *ex);       // in ra log

// --- java.lang.String
Object *jstring_new_utf8(VMThread *t, const char *s);
Object *jstring_new_chars(VMThread *t, const jchar *chars, int len);
Object *jstring_intern(VMThread *t, Object *s);
Object *jstring_intern_utf8(VMThread *t, const char *utf8);   // modified UTF-8 từ class file
int jstring_length(Object *s);
const jchar *jstring_chars(Object *s);
char *jstring_to_utf8(Object *s);                     // malloc
char *jstring_to_cstr(Object *s, char *buf, size_t size);

// --- field access theo tên (cache sẵn khi khởi động)
int field_slot(const char *cls, const char *name, const char *desc);
#define FIELD_I(o, slot) (OBJ_FIELDS(o)[slot].i)
#define FIELD_L(o, slot) (OBJ_FIELDS(o)[slot].l)
#define FIELD_J(o, slot) (OBJ_FIELDS(o)[slot].j)

// --- native
void native_register(const char *cls, const char *name, const char *desc, NativeFn fn);
NativeFn native_lookup(const char *cls, const char *name, const char *desc);
void natives_lang_init(void);

// Gọi native với giá trị args: hỗ trợ khi native cần gọi lại method Java (NATIVE_INVOKE)
NativeResult native_invoke(VMThread *t, Method *m, const Value *args, int nargs);

// Chạy method Java đồng bộ trên một thread tạm (chỉ dùng cho khởi động / ít khi)
bool vm_call_static(const char *cls, const char *name, const char *desc, Value *args, Value *ret);
// Tạo Java thread mới chạy static method (args tối đa 4 slot)
VMThread *vm_spawn_static(const char *cls, const char *name, const char *desc, const Value *args, int nargs);
