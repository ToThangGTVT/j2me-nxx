// Phiên chạy 1 game J2ME: nạp JAR, khởi động VM + MIDP, nhận phím, vẽ màn hình
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <SDL.h>

// game_id: khoá cho save RMS / tuỳ chọn riêng (NULL = tên file JAR)
bool emu_start(const char *jar_path, const char *game_id, char *err, size_t err_size);
void emu_stop(void);
bool emu_running(void);

void emu_handle_event(const SDL_Event *e);
// Chạy VM trong 1 frame. Trả về false khi game đã thoát (gọi emu_stop sau đó)
bool emu_update(void);
void emu_draw(void);

// Lý do thoát gần nhất (rỗng nếu thoát bình thường)
const char *emu_exit_message(void);
