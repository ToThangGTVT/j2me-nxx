// Green thread, monitor và scheduler
#include "vm_internal.h"

#include <stdio.h>
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

// Khi bật log đo hiệu năng: lấy mẫu method đang chạy sau mỗi lượt, in các chỗ tốn CPU nhất mỗi 5 giây
#define PROF_SLOTS 256
typedef struct {
    int tid;
    Method *m[3];   // đỉnh stack và 2 frame gọi nó
    int count;
} ProfSample;
static ProfSample prof_tab[PROF_SLOTS];
static int prof_total;
static jlong prof_last;

static void prof_record(VMThread *t) {
    if (t->frame_count == 0)
        return;
    ProfSample key = { t->id, { NULL, NULL, NULL }, 0 };
    for (int i = 0; i < 3 && i < t->frame_count; i++)
        key.m[i] = t->frames[t->frame_count - 1 - i].m;
    uint32_t h = (uint32_t)key.tid * 31u + (uint32_t)((uintptr_t)key.m[0] >> 4) * 17u +
                 (uint32_t)((uintptr_t)key.m[1] >> 4);
    for (int i = 0; i < PROF_SLOTS; i++) {
        ProfSample *e = &prof_tab[(h + i) % PROF_SLOTS];
        if (e->count == 0) {
            *e = key;
        } else if (e->tid != key.tid || memcmp(e->m, key.m, sizeof(key.m)) != 0) {
            continue;
        }
        e->count++;
        prof_total++;
        return;
    }
}

static int prof_cmp(const void *a, const void *b) {
    return ((const ProfSample *)b)->count - ((const ProfSample *)a)->count;
}

static void prof_dump(void) {
    if (prof_total == 0)
        return;
    qsort(prof_tab, PROF_SLOTS, sizeof(ProfSample), prof_cmp);
    vm_prof_log("--- %d lượt ---", prof_total);
    for (int i = 0; i < 8 && prof_tab[i].count; i++) {
        ProfSample *e = &prof_tab[i];
        char line[512];
        int n = snprintf(line, sizeof(line), "%3d%% T%d", e->count * 100 / prof_total, e->tid);
        for (int k = 0; k < 3 && e->m[k] && n < (int)sizeof(line); k++)
            n += snprintf(line + n, sizeof(line) - n, " %s %s.%s", k ? "<" : "", e->m[k]->owner->name,
                          e->m[k]->name);
        vm_prof_log("%s", line);
    }
    memset(prof_tab, 0, sizeof(prof_tab));
    prof_total = 0;
}

// Trạng thái từng thread + 3 frame trên cùng: tìm game treo (VM rảnh mà không vẽ)
static void prof_dump_threads(void) {
    static const char *names[] = { "new", "run", "sleep", "blocked", "wait", "reacquire", "event", "init", "dead" };
    for (VMThread *t = threads; t; t = t->next) {
        char line[512];
        int n = snprintf(line, sizeof(line), "T%d %s", t->id, names[t->state]);
        if (t->wait_obj && n < (int)sizeof(line))
            n += snprintf(line + n, sizeof(line) - n, " on %s", t->wait_obj->cls->name);
        for (int k = 0; k < 3 && k < t->frame_count && n < (int)sizeof(line); k++) {
            Frame *f = &t->frames[t->frame_count - 1 - k];
            n += snprintf(line + n, sizeof(line) - n, " %s %s.%s:%d", k ? "<" : "|", f->m->owner->name, f->m->name,
                          method_line(f->m, f->pc));
        }
        vm_prof_log("%s", line);
    }
}

static jlong idle_ms;

jlong vm_take_idle_ms(void) {
    jlong v = idle_ms;
    idle_ms = 0;
    return v;
}

bool vm_run(int budget_ms) {
    bool prof_on = vm_prof_on();
    if (prof_on && vm_time_ms() - prof_last >= 5000) {
        prof_dump();
        prof_last = vm_time_ms();
    }
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
            if (prof_on)
                prof_record(t);
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
                idle_ms += vm_time_ms() - now;
            }
        }
    }
}

static volatile int *preempt_flag;

void vm_set_preempt_flag(volatile int *flag) {
    preempt_flag = flag;
}

VMRunResult vm_run_slice(int budget_ms) {
    if (vm_prof_on() && vm_time_ms() - prof_last >= 5000) {
        prof_dump();
        prof_dump_threads();
        prof_last = vm_time_ms();
    }
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
            if (vm_prof_on())
                prof_record(t);
            current = NULL;
            ran = true;
            if (preempt_flag && *preempt_flag)
                break;
        }
        reap_terminated();
        if (!threads)
            return VM_RUN_DEAD;
        if (!ran)
            return VM_RUN_IDLE;
        if ((preempt_flag && *preempt_flag) || vm_time_ms() >= deadline)
            return VM_RUN_BUSY;
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
