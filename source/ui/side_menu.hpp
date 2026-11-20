// Menu bật ra từ cạnh phải: gom các hành động (thay cho thanh gợi ý nút dưới cùng đã hết chỗ)
#pragma once

#include <vector>

#include "ui/common.hpp"

namespace ui
{

struct MenuItem
{
    std::string title;
    std::string key;            // nút tắt hiện bên phải (ICON_...), "" nếu không có
    std::function<void()> run;  // chạy sau khi menu đã đóng
    bool warn = false;          // hành động xoá: chữ màu cảnh báo
};

struct MenuSection
{
    std::string title;
    std::vector<MenuItem> items;
};

class SideMenu : public brls::Box
{
  public:
    SideMenu(const std::string& title, std::vector<MenuSection> sections);

    static void open(const std::string& title, std::vector<MenuSection> sections);

    bool isTranslucent() override { return true; }
    void show(std::function<void(void)> cb, bool animate, float animationDuration) override;
    void hide(std::function<void(void)> cb, bool animated, float animationDuration) override;

  private:
    brls::Box* panel = nullptr;
    brls::Animatable slide = 0;
    std::vector<MenuSection> sections;

    void close(std::function<void()> then);
};

} // namespace ui
