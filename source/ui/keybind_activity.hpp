// Ánh xạ phím: chọn phím điện thoại cho từng nút Switch
#pragma once

#include "ui/common.hpp"

namespace ui
{

class KeybindActivity : public brls::Activity
{
  public:
    // binds: bảng đang chỉnh (BIND_COUNT phần tử), sửa trực tiếp.
    // global: bảng của cài đặt chung khi đang chỉnh tuỳ chọn riêng (binds có thể là BIND_INHERIT), nullptr khi
    // đang chỉnh cài đặt chung. done: gọi mỗi khi bảng đổi
    KeybindActivity(int* binds, const int* global, const std::string& subtitle, std::function<void()> done);

    brls::View* createContentView() override;

    static void open(int* binds, const int* global, const std::string& subtitle, std::function<void()> done);

  private:
    int* binds;
    const int* global;
    std::string subtitle;
    std::function<void()> done;
    std::vector<brls::SelectorCell*> cells;

    std::vector<std::string> choices(int b);
    int selection(int b);
    void refresh();
};

} // namespace ui
