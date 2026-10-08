// Khai báo dùng chung giữa các file trong source/vm (không public cho phần còn lại)
#pragma once

#include "vm.h"

typedef void (*MarkFn)(Object *o);

// heap.c
void heap_mark(Object *o);
void heap_gc_if_needed(void);

// class.c
void class_mark_roots(MarkFn mark);
void class_free_all(void);
Class *class_object(void);          // java/lang/Object
Class *class_string(void);          // java/lang/String
Class *class_class(void);           // java/lang/Class
bool class_bootstrap(void);
const char *cp_class_name(Class *c, uint16_t idx);

// jstring.c
void jstring_init(void);
void jstring_mark_roots(MarkFn mark);
void jstring_free_all(void);

// thread.c
void thread_mark_roots(MarkFn mark);
void thread_free_all(void);
void thread_set_current(VMThread *t);
VMThread *thread_main(void);

// interp.c
// Kiểm tra exception t->exception với frame hiện tại; trả về true nếu tìm được handler
bool interp_handle_exception(VMThread *t);
// Method sẽ chạy khi gọi rm qua interface trên object lớp cls (cache trong e), NULL nếu không có
Method *interp_find_virtual(CPEntry *e, Class *cls, Method *rm);
void throwable_fill_trace(VMThread *t, Object *ex);
void interp_reset(void);
void format_trace(Object *trace, char *out, size_t size);

// native.c
void native_free_all(void);

// Field slot cache cho các lớp hệ thống
extern int FS_String_value, FS_String_offset, FS_String_count;
extern int FS_Thread_vmThread, FS_Thread_target, FS_Thread_name;
extern int FS_Throwable_detailMessage, FS_Throwable_trace;
extern int FS_Class_vmClass;
void field_slots_init(void);

// interp.c: lỗi Java không ai bắt gần nhất (vm_last_uncaught)
extern char vm_uncaught_text[4096];
