// Trình xem video (file trong thư mục games): toàn màn hình, tua, âm lượng
#pragma once

#include <stdbool.h>
#include <stddef.h>

bool video_screen_open(const char *path, const char *title, char *err, size_t err_size);
// false = người dùng thoát
bool video_screen_update(void);
void video_screen_draw(void);
void video_screen_close(void);
