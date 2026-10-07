// Màn hình gửi game từ điện thoại: mã QR dẫn tới trang tải lên, tiến độ nhận file
#pragma once

#include <stdbool.h>

void upload_screen_open(const char *games_dir);
// false = đóng màn hình (gọi upload_screen_close)
bool upload_screen_update(void);
void upload_screen_draw(void);
// Tắt server; trả về số file đã nhận
int upload_screen_close(void);
