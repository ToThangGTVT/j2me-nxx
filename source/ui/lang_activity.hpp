// Mở app lần đầu: chọn ngôn ngữ (chữ trên màn hình luôn là tiếng Anh: người dùng chưa chọn)
#pragma once

#include "ui/common.hpp"

namespace ui
{

class LangActivity : public brls::Activity
{
  public:
    brls::View* createContentView() override;

    static void open();
};

} // namespace ui
