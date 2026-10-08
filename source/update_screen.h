// Màn hình cập nhật: hỏi tải bản mới, thanh tiến trình, báo xong / lỗi
#pragma once

#include <stdbool.h>

void update_screen_open(void);
// false = đóng màn hình. *quit = true khi app cần thoát (khởi động lại vào bản mới)
bool update_screen_update(bool *quit);
void update_screen_draw(void);
