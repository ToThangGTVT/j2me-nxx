// Vùng nhớ thực thi cho mã máy của chế độ AOT: cấp 1 lần, mỗi phiên VM dùng lại từ đầu
#include "aot.h"

#include <stdlib.h>
#include <string.h>

#define AOT_MEM_SIZE (32u * 1024 * 1024)

static size_t used;

#if defined(__SWITCH__) && defined(__aarch64__)
// Switch: libnx Jit. Kiểu CodeMemory có 2 địa chỉ cho cùng vùng nhớ (ghi / chạy) nên ghi thêm
// mã lúc nào cũng được; kiểu cũ (SetProcessMemoryPermission) phải đổi quyền mỗi lần ghi.
#include <switch.h>

static Jit jit;
static bool ready;

bool aot_mem_exit(void) {
    if (!ready)
        return false;
    ready = false;
    used = 0;
    jitClose(&jit);
    return true;
}

static void exit_hook(void) {
    aot_mem_exit();
}

bool aot_mem_init(void) {
    if (ready)
        return true;
    if (R_FAILED(jitCreate(&jit, AOT_MEM_SIZE)))
        return false;
    if (R_FAILED(jitTransitionToExecutable(&jit))) {
        jitClose(&jit);
        return false;
    }
    ready = true;
    // Vùng này lấy từ heap và bị khoá cho mã máy: không trả thì .nro nạp sau (hbmenu, bản vừa cập nhật)
    // dùng lại heap đó sẽ sập
    static bool hooked;
    if (!hooked)
        hooked = atexit(exit_hook) == 0;
    return true;
}

void *aot_mem_put(const void *code, size_t size) {
    if (!ready || used + size > AOT_MEM_SIZE)
        return NULL;
    size_t off = used;
    used = (used + size + 15) & ~(size_t)15;
    if (jit.type != JitType_CodeMemory && R_FAILED(jitTransitionToWritable(&jit)))
        return NULL;
    uint8_t *rw = (uint8_t *)jitGetRwAddr(&jit) + off;
    uint8_t *rx = (uint8_t *)jitGetRxAddr(&jit) + off;
    memcpy(rw, code, size);
    armDCacheFlush(rw, size);
    if (jit.type != JitType_CodeMemory && R_FAILED(jitTransitionToExecutable(&jit)))
        return NULL;
    armICacheInvalidate(rx, size);
    return rx;
}

#elif defined(__aarch64__) && (defined(__APPLE__) || defined(__linux__))
// macOS (Apple Silicon): MAP_JIT, tắt bảo vệ ghi của thread trong lúc chép.
// Linux ARM64: mmap đọc / ghi / chạy.
#include <sys/mman.h>
#if defined(__APPLE__)
#include <libkern/OSCacheControl.h>
#include <pthread.h>
#endif

static uint8_t *base;

bool aot_mem_exit(void) {
    if (!base)
        return false;
    munmap(base, AOT_MEM_SIZE);
    base = NULL;
    used = 0;
    return true;
}

bool aot_mem_init(void) {
    if (base)
        return true;
    int flags = MAP_PRIVATE | MAP_ANON;
#if defined(__APPLE__)
    flags |= MAP_JIT;
#endif
    void *p = mmap(NULL, AOT_MEM_SIZE, PROT_READ | PROT_WRITE | PROT_EXEC, flags, -1, 0);
    if (p == MAP_FAILED)
        return false;
    base = p;
    return true;
}

void *aot_mem_put(const void *code, size_t size) {
    if (!base || used + size > AOT_MEM_SIZE)
        return NULL;
    uint8_t *p = base + used;
    used = (used + size + 15) & ~(size_t)15;
#if defined(__APPLE__)
    pthread_jit_write_protect_np(0);
    memcpy(p, code, size);
    pthread_jit_write_protect_np(1);
    sys_icache_invalidate(p, size);
#else
    memcpy(p, code, size);
    __builtin___clear_cache((char *)p, (char *)p + size);
#endif
    return p;
}

#else
// Không phải ARM64: không có chế độ AOT
bool aot_mem_init(void) {
    return false;
}

bool aot_mem_exit(void) {
    return false;
}

void *aot_mem_put(const void *code, size_t size) {
    (void)code;
    (void)size;
    return NULL;
}
#endif

void aot_mem_reset(void) {
    used = 0;
}

size_t aot_mem_used(void) {
    return used;
}
