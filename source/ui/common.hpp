// Phần dùng chung của giao diện borealis: gọi sang code C, chuỗi theo ngôn ngữ, màu, sự kiện SDL
#pragma once

#include <SDL.h>
#include <SDL_ttf.h>

#include <borealis.hpp>
#include <functional>
#include <string>

extern "C" {
#include "crash.h"
#include "emu.h"
#include "game_list.h"
#include "gfx.h"
#include "input.h"
#include "keybind.h"
#include "keymap.h"
#include "lang.h"
#include "platform.h"
#include "qr.h"
#include "settings.h"
#include "update.h"
#include "upload.h"
#include "video_screen.h"
#include "vpad.h"
#include "vpad_screen.h"

bool aot_available(void);
}

namespace ui
{

// Chuỗi giao diện (lang.c) dạng std::string; trf giống printf
std::string T(StrId id);
std::string trf(int id, ...);

// Màu riêng của app (phần còn lại theo theme tối của borealis)
NVGcolor color_accent();
NVGcolor color_dim();
NVGcolor color_warn();
NVGcolor color_ok();

// Thông báo nhỏ góc màn hình
void notify(const std::string& text);

// Sự kiện SDL thô (bàn phím, joystick, chạm) cho các màn hình tự xử lý phím (chạy game, video...).
// borealis không chuyển các sự kiện này ra ngoài nên bắt bằng SDL_AddEventWatch.
using SdlEventHandler = std::function<void(const SDL_Event*)>;
void sdl_events_init();
// Switch: borealis đọc tay cầm bằng libnx, không ai bơm hàng đợi SDL: gọi mỗi vòng lặp
void sdl_events_pump();
// Màn hình đang nhận sự kiện (nullptr = không ai)
void sdl_events_set_handler(SdlEventHandler handler);

// Đặt chữ khi khác chữ đang hiện (tránh dựng lại bố cục mỗi khung hình)
void set_text(brls::Label* label, const std::string& text);

brls::Label* make_label(const std::string& text, float size, NVGcolor color, bool wrap = true);

// Gán hành động cho nút tay cầm; bản desktop gán thêm phím tắt bàn phím giống input.c
// (S = X, A = Y, Q = L, W = R, Tab = -, = là +)
brls::ActionIdentifier bind_action(brls::View* view, const std::string& hint, brls::ControllerButton button,
                            const brls::ActionListener& listener, bool hidden = false);

} // namespace ui
