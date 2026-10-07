// Trình xem video (file trong thư mục games): toàn màn hình, tua, âm lượng
#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "video_dec.h"

// Mở file video, hoặc nhận VideoDec đã mở sẵn (luồng mạng; màn hình giữ và tự đóng)
bool video_screen_open(const char *path, const char *title, char *err, size_t err_size);
bool video_screen_open_dec(VideoDec *d, const char *title, char *err, size_t err_size);
// Tuỳ chọn mở dùng cho video_screen (tiếng 48kHz stereo, giải mã đa luồng)
void video_screen_options(VDecOptions *opt);
// false = người dùng thoát
bool video_screen_update(void);
void video_screen_draw(void);
void video_screen_close(void);
