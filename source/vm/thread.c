// Green thread, monitor và scheduler
#include "vm_internal.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

// Số lệnh bytecode mỗi lượt của 1 thread
#define TIME_SLICE 4000

static VMThread *threads;
static VMThread *current;
static int next_id = 1;

VMThread *thread_current(void) { return current; }
void thread_set_current(VMThread *t) { current = t; }
VMThread *thread_list(void) { return threads; }

VMThread *thread_new(Object *jthread) {
    VMThread *t = calloc(1, sizeof(VMThread));
    if (!t)
        return NULL;
    t->stack = calloc(THREAD_STACK_SLOTS, sizeof(Value));
    t->frames = calloc(THREAD_MAX_FRAMES, sizeof(Frame));
    if (!t->stack || !t->frames) {
        free(t->stack);
        free(t->frames);
        free(t);
        return NULL;
    }
    t->id = next_id++;
    t->jthread = jthread;
    t->state = TS_NEW;
    if (jthread && FS_Thread_vmThread >= 0)
        OBJ_FIELDS(jthread)[FS_Thread_vmThread].j = (jlong)(intptr_t)t;

    // Thêm vào cuối danh sách để xoay vòng công bằng
    VMThread **pp = &threads;
    while (*pp)
        pp = &(*pp)->next;
    *pp = t;
    return t;
}

static void wake_matching(ThreadState from, Object *o, ThreadState to, bool all) {
    for (VMThread *t = threads; t; t = t->next) {
        if (t->state == from && t->wait_obj == o) {
            t->state = to;
            if (to == TS_RUNNABLE)
                t->wait_obj = NULL;
            if (!all)
                return;
        }
    }
}

void thread_terminate(VMThread *t) {
    if (t->state == TS_TERMINATED)
        return;
    // Nhả mọi monitor còn giữ (thread chết giữa chừng)
    while (t->frame_count > 0) {
        Frame *f = &t->frames[t->frame_count - 1];
        if (f->sync_obj)
            monitor_exit(t, f->sync_obj);
        if (f->clinit_of) {
            f->clinit_of->state = CLASS_ERROR;
            f->clinit_of->init_thread = NULL;
        }
        t->frame_count--;
    }
    t->state = TS_TERMINATED;
    if (t->jthread) {
        if (FS_Thread_vmThread >= 0)
            OBJ_FIELDS(t->jthread)[FS_Thread_vmThread].j = 0;
        // Thread.join() chờ trên chính object VMThread
        wake_matching(TS_WAITING, t->jthread, TS_REACQUIRE, true);
    }
    thread_wake_waiters();
}

void thread_wake_waiters(void) {
    for (VMThread *t = threads; t; t = t->next) {
        if (t->state == TS_WAIT_INIT)
            t->state = TS_RUNNABLE;
    }
}

// ---------------------------------------------------------------------------
// Monitor

static Monitor *get_monitor(Object *o) {
    if (!o->monitor)
        o->monitor = calloc(1, sizeof(Monitor));
    return o->monitor;
}

bool monitor_enter(VMThread *t, Object *o) {
    Monitor *mon = get_monitor(o);
    if (!mon->owner || mon->owner->state == TS_TERMINATED) {
        mon->owner = t;
        mon->count = 1;
        return true;
    }
    if (mon->owner == t) {
        mon->count++;
        return true;
    }
    t->state = TS_BLOCKED;
    t->wait_obj = o;
    return false;
}

static void monitor_release(Object *o) {
    Monitor *mon = o->monitor;
    mon->owner = NULL;
    mon->count = 0;
    wake_matching(TS_BLOCKED, o, TS_RUNNABLE, true);
}

bool monitor_exit(VMThread *t, Object *o) {
    Monitor *mon = o->monitor;
    if (!mon || mon->owner != t)
        return false;
    if (--mon->count == 0)
        monitor_release(o);
    return true;
}

int monitor_wait(VMThread *t, Object *o, jlong ms) {
    Monitor *mon = o->monitor;
    if (!mon || mon->owner != t)
        return -1;
    t->saved_count = mon->count;
    monitor_release(o);
    t->state = TS_WAITING;
    t->wait_obj = o;
    t->wake_time = ms > 0 ? vm_time_ms() + ms : 0;
    return 0;
}

bool monitor_notify(VMThread *t, Object *o, bool all) {
    Monitor *mon = o->monitor;
    if (!mon || mon->owner != t)
        return false;
    wake_matching(TS_WAITING, o, TS_REACQUIRE, all);
    return true;
}

// ---------------------------------------------------------------------------
// Scheduler

static void deliver_interrupt(VMThread *t) {
    t->interrupted = false;
    // Lệnh invoke sleep()/wait() dài 3 byte; lùi lại để bảng exception trỏ đúng lệnh
    if (t->frame_count > 0)
        t->frames[t->frame_count - 1].pc -= 3;
    throw_new(t, "java/lang/InterruptedException", NULL);
}

static void update_state(VMThread *t, jlong now) {
    switch (t->state) {
    case TS_SLEEPING:
        if (t->interrupted) {
            t->state = TS_RUNNABLE;
            deliver_interrupt(t);
        } else if (now >= t->wake_time) {
            t->state = TS_RUNNABLE;
        }
        break;
    case TS_WAITING:
        if (t->interrupted || (t->wake_time && now >= t->wake_time))
            t->state = TS_REACQUIRE;
        break;
    default:
        break;
    }
    if (t->state == TS_REACQUIRE) {
        Object *o = t->wait_obj;
        Monitor *mon = get_monitor(o);
        if (!mon->owner || mon->owner->state == TS_TERMINATED) {
            mon->owner = t;
            mon->count = t->saved_count;
            t->wait_obj = NULL;
            t->state = TS_RUNNABLE;
            if (t->interrupted)
                deliver_interrupt(t);
        }
    }
}

static void free_thread(VMThread *t) {
    free(t->stack);
    free(t->frames);
    free(t);
}

static void reap_terminated(void) {
    VMThread **pp = &threads;
    while (*pp) {
        VMThread *t = *pp;
        if (t->state == TS_TERMINATED) {
            *pp = t->next;
            free_thread(t);
        } else {
            pp = &t->next;
        }
    }
}

bool vm_has_runnable(void) {
    jlong now = vm_time_ms();
    for (VMThread *t = threads; t; t = t->next) {
        update_state(t, now);
        if (t->state == TS_RUNNABLE)
            return true;
    }
    return false;
}

jlong vm_next_wakeup(void) {
    jlong best = -1;
    for (VMThread *t = threads; t; t = t->next) {
        if ((t->state == TS_SLEEPING || (t->state == TS_WAITING && t->wake_time)) &&
            (best < 0 || t->wake_time < best))
            best = t->wake_time;
    }
    return best;
}

bool vm_run(int budget_ms) {
    jlong deadline = vm_time_ms() + budget_ms;
    for (;;) {
        heap_gc_if_needed();
        jlong now = vm_time_ms();
        bool ran = false;
        for (VMThread *t = threads; t; t = t->next) {
            update_state(t, now);
            if (t->state != TS_RUNNABLE)
                continue;
            current = t;
            interp_run(t, TIME_SLICE);
            current = NULL;
            ran = true;
        }
        reap_terminated();
        if (!threads)
            return false;
        now = vm_time_ms();
        if (now >= deadline)
            return true;
        if (!ran) {
            // Không thread nào chạy được: ngủ tới lần thức dậy gần nhất (nếu trong ngân sách)
            jlong next = vm_next_wakeup();
            if (next < 0 || next >= deadline)
                return true;
            if (next > now) {
                struct timespec ts = { 0, (long)(next - now) * 1000000L };
                nanosleep(&ts, NULL);
            }
        }
    }
}

void thread_mark_roots(MarkFn mark) {
    for (VMThread *t = threads; t; t = t->next) {
        mark(t->jthread);
        mark(t->exception);
        mark(t->uncaught);
        mark(t->wait_obj);
        for (int i = 0; i < 8; i++) {
            if (heap_is_object(t->invoke_args[i].l))
                mark(t->invoke_args[i].l);
        }
        if (heap_is_object(t->result.l))
            mark(t->result.l);
        if (t->frame_count == 0)
            continue;
        Value *top = t->frames[t->frame_count - 1].sp;
        for (Value *v = t->stack; v < top; v++) {
            if (heap_is_object(v->l))
                mark(v->l);
        }
    }
}

void thread_free_all(void) {
    while (threads) {
        VMThread *n = threads->next;
        free_thread(threads);
        threads = n;
    }
    current = NULL;
    next_id = 1;
}
