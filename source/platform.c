#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>

#if defined(__APPLE__) && !defined(__SWITCH__)
#include <mach/mach.h>
#elif defined(__linux__) && !defined(__SWITCH__)
#include <unistd.h>
#endif

#ifdef __SWITCH__
#include <switch.h>

bool platform_init(void) {
    // Mạng cho game online (socket:// và http://); lỗi thì game chỉ không kết nối được
    socketInitializeDefault();
    Result rc = plInitialize(PlServiceType_User);
    if (R_FAILED(rc)) {
        printf("plInitialize failed: 0x%x\n", rc);
        return false;
    }
    return true;
}

void platform_exit(void) {
    plExit();
    socketExit();
}

OpenUrlResult platform_open_url(const char *url) {
    AppletType at = appletGetAppletType();
    if (at != AppletType_Application && at != AppletType_SystemApplication)
        return OPEN_URL_NEED_APP;
    WebCommonConfig cfg;
    WebCommonReply reply;
    Result rc = webPageCreate(&cfg, url);
    // Danh sách URL được phép lấy từ game đang bị chiếm chỗ: cho phép mọi trang http/https
    if (R_SUCCEEDED(rc))
        rc = webConfigSetWhitelist(&cfg, "^http*");
    if (R_SUCCEEDED(rc))
        rc = webConfigShow(&cfg, &reply);
    if (R_FAILED(rc)) {
        printf("web applet: 0x%x\n", rc);
        return OPEN_URL_FAILED;
    }
    return OPEN_URL_OK;
}

void platform_mem_usage(size_t *used, size_t *total) {
    u64 u = 0, t = 0;
    svcGetInfo(&u, InfoType_UsedMemorySize, CUR_PROCESS_HANDLE, 0);
    svcGetInfo(&t, InfoType_TotalMemorySize, CUR_PROCESS_HANDLE, 0);
    *used = (size_t)u;
    *total = (size_t)t;
}

struct PlatformThread {
    Thread th;
    int (*fn)(void *);
    void *arg;
};

static void thread_entry(void *p) {
    PlatformThread *t = p;
    t->fn(t->arg);
}

PlatformThread *platform_thread_start(int (*fn)(void *), void *arg) {
    PlatformThread *t = calloc(1, sizeof(PlatformThread));
    if (!t)
        return NULL;
    t->fn = fn;
    t->arg = arg;
    // Ưu tiên như luồng chính (0x2C), nhân 1, stack 1MB (giải mã ảnh / M3G dùng stack C)
    Result rc = threadCreate(&t->th, thread_entry, t, NULL, 1024 * 1024, 0x2C, 1);
    if (R_FAILED(rc))
        rc = threadCreate(&t->th, thread_entry, t, NULL, 1024 * 1024, 0x2C, -2);
    if (R_FAILED(rc) || R_FAILED(threadStart(&t->th))) {
        printf("threadCreate failed: 0x%x\n", rc);
        free(t);
        return NULL;
    }
    return t;
}

void platform_thread_join(PlatformThread *t) {
    if (!t)
        return;
    threadWaitForExit(&t->th);
    threadClose(&t->th);
    free(t);
}

const char *platform_games_dir(void) {
    return "sdmc:/switch/j2me-nx/games";
}

const char *platform_data_dir(void) {
    return "sdmc:/switch/j2me-nx";
}

char *platform_keyboard(const char *title, const char *text, int max_len, int type) {
    SwkbdConfig kbd;
    if (R_FAILED(swkbdCreate(&kbd, 0)))
        return NULL;
    swkbdConfigMakePresetDefault(&kbd);
    swkbdConfigSetHeaderText(&kbd, title);
    swkbdConfigSetInitialText(&kbd, text);
    if (max_len > 0)
        swkbdConfigSetStringLenMax(&kbd, max_len);
    // TextField.NUMERIC = 2, PHONENUMBER = 3, DECIMAL = 5
    if (type == 2 || type == 3 || type == 5)
        swkbdConfigSetType(&kbd, SwkbdType_NumPad);
    char out[1024] = "";
    Result rc = swkbdShow(&kbd, out, sizeof(out));
    swkbdClose(&kbd);
    return R_SUCCEEDED(rc) ? strdup(out) : NULL;
}

static TTF_Font *open_shared_font(int ptsize) {
    PlFontData font;
    if (R_FAILED(plGetSharedFontByType(&font, PlSharedFontType_Standard)))
        return NULL;
    // Bộ nhớ shared font do pl service giữ, không cần free
    SDL_RWops *rw = SDL_RWFromConstMem(font.address, font.size);
    return TTF_OpenFontRW(rw, 1, ptsize);
}

#else // desktop

bool platform_init(void) {
    return true;
}

void platform_exit(void) {
}

OpenUrlResult platform_open_url(const char *url) {
    // Test tự động: J2ME_NX_NO_BROWSER=1 chỉ in ra, không mở trình duyệt thật
    if (SDL_getenv("J2ME_NX_NO_BROWSER")) {
        printf("open url: %s\n", url);
        return OPEN_URL_OK;
    }
    return SDL_OpenURL(url) == 0 ? OPEN_URL_OK : OPEN_URL_FAILED;
}

void platform_mem_usage(size_t *used, size_t *total) {
    *used = 0;
    *total = 0;
#if defined(__APPLE__)
    struct mach_task_basic_info info;
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &count) == KERN_SUCCESS)
        *used = (size_t)info.resident_size;
#elif defined(__linux__)
    FILE *f = fopen("/proc/self/statm", "r");
    unsigned long pages, rss;
    if (f && fscanf(f, "%lu %lu", &pages, &rss) == 2)
        *used = (size_t)rss * (size_t)sysconf(_SC_PAGESIZE);
    if (f)
        fclose(f);
#endif
}

PlatformThread *platform_thread_start(int (*fn)(void *), void *arg) {
    return (PlatformThread *)SDL_CreateThreadWithStackSize(fn, "vm", 4 * 1024 * 1024, arg);
}

void platform_thread_join(PlatformThread *t) {
    if (t)
        SDL_WaitThread((SDL_Thread *)t, NULL);
}

const char *platform_games_dir(void) {
    const char *dir = SDL_getenv("J2ME_NX_GAMES");
    return dir ? dir : "games";
}

const char *platform_data_dir(void) {
    const char *dir = SDL_getenv("J2ME_NX_DATA");
    return dir ? dir : "data";
}

char *platform_keyboard(const char *title, const char *text, int max_len, int type) {
    // Desktop: chưa có hộp nhập liệu, coi như huỷ
    (void)title;
    (void)text;
    (void)max_len;
    (void)type;
    return NULL;
}

static TTF_Font *open_system_font(int ptsize) {
    static const char *candidates[] = {
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/System/Library/Fonts/Helvetica.ttc",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "C:/Windows/Fonts/arial.ttf",
    };
    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
        TTF_Font *f = TTF_OpenFont(candidates[i], ptsize);
        if (f)
            return f;
    }
    return NULL;
}

#endif

// Font nhúng (Google Sans, có đủ chữ tiếng Việt); lỗi thì dùng font hệ thống
extern const unsigned char ui_font_ttf[];
extern const size_t ui_font_ttf_size;

TTF_Font *platform_open_font(int ptsize) {
    SDL_RWops *rw = SDL_RWFromConstMem(ui_font_ttf, (int)ui_font_ttf_size);
    TTF_Font *f = rw ? TTF_OpenFontRW(rw, 1, ptsize) : NULL;
    if (f)
        return f;
#ifdef __SWITCH__
    return open_shared_font(ptsize);
#else
    return open_system_font(ptsize);
#endif
}
