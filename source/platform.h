// Phần khác nhau giữa Switch và desktop (font, đường dẫn, service của libnx)
#pragma once

#include <stdbool.h>
#include <stddef.h>
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

// Mở trang web bằng trình duyệt: Switch dùng trình duyệt có sẵn (chặn tới khi người dùng đóng),
// desktop dùng trình duyệt mặc định
typedef enum {
    OPEN_URL_OK,
    OPEN_URL_NEED_APP,      // Switch: chỉ mở được khi chạy dạng Application (hbmenu full RAM)
    OPEN_URL_FAILED,
} OpenUrlResult;
OpenUrlResult platform_open_url(const char *url);

// Địa chỉ IPv4 của máy trong mạng LAN (để điện thoại kết nối tới). false nếu chưa có mạng
bool platform_local_ip(char *out, size_t size);

// RAM của cả app (byte). total = 0 nếu không biết (desktop)
void platform_mem_usage(size_t *used, size_t *total);

// Font giao diện (Google Sans nhúng); lỗi thì dùng font hệ thống
TTF_Font *platform_open_font(int ptsize);

// Font icon nút (NintendoExt) của Switch; desktop trả về NULL
TTF_Font *platform_open_icon_font(int ptsize);

// Font hệ thống cho chữ trong game, theo thứ tự ưu tiên: Switch dùng shared font các thứ tiếng
// (Nhật/Âu-Mỹ, Trung giản thể, Trung phồn thể, Hàn...), desktop dùng font có sẵn trên máy.
// platform_open_system_font trả NULL nếu font thứ index không có trên máy.
int platform_system_font_count(void);
TTF_Font *platform_open_system_font(int index, int ptsize);
