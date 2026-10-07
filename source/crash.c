#include "crash.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <SDL.h>

#include "platform.h"
#include "vm/vm.h"

#ifdef __SWITCH__
#include <switch.h>
#else
#include <execinfo.h>
#include <signal.h>
#include <unistd.h>
#endif

#define RING_SIZE (48 * 1024)   // log gần nhất giữ trong RAM

static char ring[RING_SIZE];
static size_t ring_pos;
static bool ring_full;
static SDL_SpinLock ring_lock;
static char game_jar[512], game_cls[256];
static char crash_dir[512];

void crash_log(const char *line) {
    SDL_AtomicLock(&ring_lock);
    for (int pass = 0; pass < 2; pass++) {
        const char *s = pass ? "\n" : line;
        for (; *s; s++) {
            ring[ring_pos++] = *s;
            if (ring_pos == RING_SIZE) {
                ring_pos = 0;
                ring_full = true;
            }
        }
    }
    SDL_AtomicUnlock(&ring_lock);
}

void crash_set_game(const char *jar_path, const char *midlet_class) {
    snprintf(game_jar, sizeof(game_jar), "%s", jar_path ? jar_path : "");
    snprintf(game_cls, sizeof(game_cls), "%s", midlet_class ? midlet_class : "");
}

// Không khoá: có thể gọi từ bộ bắt crash khi luồng khác đang giữ khoá
static void write_ring(FILE *f) {
    if (ring_full) {
        // Bỏ dòng đầu bị cắt dở
        size_t i = ring_pos;
        while (i < RING_SIZE && ring[i] != '\n')
            i++;
        if (i < RING_SIZE)
            fwrite(ring + i + 1, 1, RING_SIZE - i - 1, f);
    }
    fwrite(ring, 1, ring_pos, f);
}

// Mở file báo cáo mới; name nhận tên file
static FILE *open_report(char *name, size_t name_size) {
    if (!crash_dir[0])
        return NULL;
    mkdir(platform_data_dir(), 0777);
    mkdir(crash_dir, 0777);
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char stamp[32] = "unknown";
    if (tm)
        strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", tm);
    char path[700];
    for (int i = 1; i < 100; i++) {
        if (i == 1)
            snprintf(name, name_size, "crash-%s.txt", stamp);
        else
            snprintf(name, name_size, "crash-%s-%d.txt", stamp, i);
        snprintf(path, sizeof(path), "%s/%s", crash_dir, name);
        struct stat st;
        if (stat(path, &st) != 0)
            return fopen(path, "w");
    }
    return NULL;
}

static void write_header(FILE *f, const char *title) {
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char when[64] = "?";
    if (tm)
        strftime(when, sizeof(when), "%Y-%m-%d %H:%M:%S", tm);
#ifdef __SWITCH__
    const char *plat = "Nintendo Switch";
#else
    const char *plat = "desktop";
#endif
    fprintf(f, "J2ME-NXX v" APP_VERSION_STR " - bao cao crash\n");
    fprintf(f, "Thoi gian: %s\nNen tang: %s\n", when, plat);
    if (game_jar[0])
        fprintf(f, "Game: %s (%s)\n", game_jar, game_cls);
    else
        fprintf(f, "Game: (khong co, dang o danh sach game)\n");
    fprintf(f, "Loi: %s\n\n", title);
}

static void write_footer(FILE *f) {
    fprintf(f, "\n--- Log gan nhat ---\n");
    write_ring(f);
}

// Ghi tên file crash để lần mở app sau báo cho người dùng
static void write_marker(const char *name) {
    char path[600];
    snprintf(path, sizeof(path), "%s/last.txt", crash_dir);
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "%s\n", name);
        fclose(f);
    }
}

bool crash_write_report(const char *title, const char *detail, char *name, size_t name_size) {
    FILE *f = open_report(name, name_size);
    if (!f)
        return false;
    write_header(f, title);
    if (detail && detail[0])
        fprintf(f, "%s\n", detail);
    write_footer(f);
    return fclose(f) == 0;
}

bool crash_take_previous(char *name, size_t name_size) {
    char path[600];
    snprintf(path, sizeof(path), "%s/last.txt", crash_dir);
    FILE *f = fopen(path, "r");
    if (!f)
        return false;
    bool ok = fgets(name, (int)name_size, f) != NULL;
    fclose(f);
    remove(path);
    if (ok)
        name[strcspn(name, "\r\n")] = 0;
    return ok && name[0];
}

// ---------------------------------------------------------------------------
// App sập

static void write_java_state(FILE *f) {
    static char java[4096];
    vm_describe_current(java, sizeof(java));
    fprintf(f, "\n--- Java (luong VM) ---\n%s", java);
}

#ifdef __SWITCH__

// libnx gọi __libnx_exception_handler khi có exception (đọc/ghi sai địa chỉ...) trên stack riêng này
alignas(16) u8 __nx_exception_stack[0x8000];
u64 __nx_exception_stack_size = sizeof(__nx_exception_stack);
u32 __nx_exception_ignoredebug = 1;

extern char __start__[];    // địa chỉ nạp của .nro; offset = địa chỉ - __start__ dùng với addr2line trên .elf

static const char *exception_name(u32 desc) {
    switch (desc) {
    case ThreadExceptionDesc_InstructionAbort: return "Instruction abort";
    case ThreadExceptionDesc_MisalignedPC: return "Misaligned PC";
    case ThreadExceptionDesc_MisalignedSP: return "Misaligned SP";
    case ThreadExceptionDesc_SError: return "SError";
    case ThreadExceptionDesc_BadSVC: return "Bad SVC";
    case ThreadExceptionDesc_Trap: return "Trap";
    case ThreadExceptionDesc_Other: return "Data abort / khac";
    default: return "?";
    }
}

static void write_addr(FILE *f, const char *label, u64 a) {
    u64 base = (u64)(uintptr_t)__start__;
    if (a >= base && a - base < 0x10000000)
        fprintf(f, "%-5s 0x%016llx  (j2me-nxx.elf + 0x%llx)\n", label, (unsigned long long)a,
                (unsigned long long)(a - base));
    else
        fprintf(f, "%-5s 0x%016llx\n", label, (unsigned long long)a);
}

static bool readable(u64 addr) {
    MemoryInfo mi;
    u32 pi;
    if (R_FAILED(svcQueryMemory(&mi, &pi, addr)))
        return false;
    return (mi.perm & Perm_R) && addr + 16 <= mi.addr + mi.size;
}

void __libnx_exception_handler(ThreadExceptionDump *ctx) {
    static bool busy;
    if (busy)
        return;
    busy = true;
    char name[64], title[128];
    snprintf(title, sizeof(title), "App bi sap - exception 0x%x (%s)", ctx->error_desc,
             exception_name(ctx->error_desc));
    FILE *f = open_report(name, sizeof(name));
    if (!f)
        return;
    write_header(f, title);
    fprintf(f, "Dia chi nap: 0x%llx\n", (unsigned long long)(uintptr_t)__start__);
    write_addr(f, "pc", ctx->pc.x);
    write_addr(f, "lr", ctx->lr.x);
    fprintf(f, "sp    0x%016llx\nfp    0x%016llx\nfar   0x%016llx  (dia chi bi truy cap)\nesr   0x%08x  pstate 0x%08x\n",
            (unsigned long long)ctx->sp.x, (unsigned long long)ctx->fp.x, (unsigned long long)ctx->far.x, ctx->esr,
            ctx->pstate);
    for (int i = 0; i < 29; i++)
        fprintf(f, "x%-2d 0x%016llx%s", i, (unsigned long long)ctx->cpu_gprs[i].x, i % 3 == 2 ? "\n" : "   ");
    fprintf(f, "\n\n--- Backtrace (lan theo frame pointer) ---\n");
    u64 fp = ctx->fp.x;
    for (int i = 0; i < 32 && fp && !(fp & 7) && readable(fp); i++) {
        u64 *frame = (u64 *)(uintptr_t)fp;
        if (!frame[1])
            break;
        char label[8];
        snprintf(label, sizeof(label), "#%d", i);
        write_addr(f, label, frame[1]);
        if (frame[0] <= fp)
            break;
        fp = frame[0];
    }
    fprintf(f, "\nTra ham: aarch64-none-elf-addr2line -f -C -e j2me-nxx.elf <offset>\n");
    write_java_state(f);
    write_footer(f);
    fclose(f);
    write_marker(name);
    // Trả về: libnx báo lỗi cho hệ thống, app đóng như crash bình thường
}

void crash_init(void) {
    snprintf(crash_dir, sizeof(crash_dir), "%s/crash", platform_data_dir());
}

#else

static void on_signal(int sig) {
    static volatile sig_atomic_t busy;
    if (!busy) {
        busy = 1;
        char name[64], title[128];
        snprintf(title, sizeof(title), "App bi sap - signal %d (%s)", sig, strsignal(sig));
        FILE *f = open_report(name, sizeof(name));
        if (f) {
            write_header(f, title);
            fprintf(f, "--- Backtrace ---\n");
            fflush(f);
            void *frames[64];
            int n = backtrace(frames, 64);
            backtrace_symbols_fd(frames, n, fileno(f));
            write_java_state(f);
            write_footer(f);
            fclose(f);
            write_marker(name);
            fprintf(stderr, "Crash: da ghi %s/%s\n", crash_dir, name);
        }
    }
    signal(sig, SIG_DFL);
    raise(sig);
}

void crash_init(void) {
    snprintf(crash_dir, sizeof(crash_dir), "%s/crash", platform_data_dir());
    // Stack riêng để vẫn ghi được khi tràn stack
    static char alt[64 * 1024];
    stack_t ss = { .ss_sp = alt, .ss_size = sizeof(alt), .ss_flags = 0 };
    sigaltstack(&ss, NULL);
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;
    sa.sa_flags = SA_ONSTACK;
    sigemptyset(&sa.sa_mask);
    int sigs[] = { SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT };
    for (size_t i = 0; i < sizeof(sigs) / sizeof(sigs[0]); i++)
        sigaction(sigs[i], &sa, NULL);
}

#endif
