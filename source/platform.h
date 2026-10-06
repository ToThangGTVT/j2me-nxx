// Phần khác nhau giữa Switch và desktop (font, đường dẫn, service của libnx)
#pragma once

#include <stdbool.h>
#include <SDL_ttf.h>

bool platform_init(void);
void platform_exit(void);

// Thư mục chứa file .jar
const char *platform_games_dir(void);

// Thư mục dữ liệu của app (log, save RMS)
const char *platform_data_dir(void);

// Bàn phím ảo: trả về chuỗi UTF-8 malloc, NULL nếu huỷ / không hỗ trợ
char *platform_keyboard(const char *title, const char *text, int max_len, int type);

// Luồng chạy VM. Switch: ghim vào nhân CPU 1 (luồng chính ở nhân 0) để chạy song song thật sự
typedef struct PlatformThread PlatformThread;
PlatformThread *platform_thread_start(int (*fn)(void *), void *arg);
void platform_thread_join(PlatformThread *t);

// Font hệ thống: shared font của Switch, hoặc font có sẵn trên desktop
TTF_Font *platform_open_font(int ptsize);
