// Có bản mới: ghi chú phát hành, tải về, tiến độ, khởi động lại
#pragma once

#include "ui/common.hpp"

namespace ui
{

class ProgressBar : public brls::View
{
  public:
    ProgressBar();
    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
              brls::FrameContext* ctx) override;

    float value = -1;   // 0..1, < 0 = chưa biết (vạch chạy qua lại)
};

class UpdateActivity : public brls::Activity
{
  public:
    ~UpdateActivity() override;
    brls::View* createContentView() override;
    void onContentAvailable() override;

    static void open();

  private:
    brls::AppletFrame* frame = nullptr;
    brls::Label *title = nullptr, *sub = nullptr, *error = nullptr, *notes = nullptr, *info = nullptr;
    brls::ScrollingFrame* notes_scroll = nullptr;
    brls::Box* progress_box = nullptr;
    ProgressBar* bar = nullptr;
    brls::Label *bytes = nullptr, *percent = nullptr, *speed = nullptr;
    brls::VoidEvent::Subscription loop;
    int shown_state = -1;
    bool closing    = false;    // đã bấm huỷ, chờ luồng tải dừng

    void refresh();
    void set_actions(UpdateState s);
};

} // namespace ui
