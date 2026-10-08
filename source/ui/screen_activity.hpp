// Màn hình tự vẽ và tự đọc phím (chạy game, xem video, chỉnh phím ảo): code C cũ vẽ qua gfx.h
// trên NanoVG, nhận sự kiện SDL thô. Trong lúc mở, borealis không nhận phím / chạm.
#pragma once

#include "ui/common.hpp"

namespace ui
{

struct ScreenHooks
{
    SdlEventHandler event;              // có thể rỗng
    std::function<bool()> update;       // mỗi vòng lặp; false = đóng màn hình
    std::function<void()> draw;         // vẽ bằng gfx_* (toạ độ 1280x720)
    std::function<void()> closed;       // sau khi đóng (dọn dẹp, báo kết quả)
};

class ScreenActivity : public brls::Activity
{
  public:
    explicit ScreenActivity(ScreenHooks hooks);
    ~ScreenActivity() override;

    brls::View* createContentView() override;
    void onContentAvailable() override;

    // Mở màn hình: chặn phím của borealis tới khi đóng và nhả hết nút
    static void open(ScreenHooks hooks);
    // Gọi mỗi vòng lặp (app.cpp)
    static void tick();
    static bool active();

  private:
    ScreenHooks hooks;
    bool closing = false;

    void close();
};

} // namespace ui
