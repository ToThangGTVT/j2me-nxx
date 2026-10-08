// Nhận ứng dụng / video từ điện thoại qua Wi-Fi: mã QR tới trang tải lên, tiến độ nhận file
#pragma once

#include "ui/common.hpp"

namespace ui
{

class ProgressBar;

class UploadActivity : public brls::Activity
{
  public:
    // closed(received): số file đã nhận, gọi sau khi đóng
    UploadActivity(const std::string& dir, std::function<void(int)> closed);
    ~UploadActivity() override;

    brls::View* createContentView() override;
    void onContentAvailable() override;

    static void open(const std::string& dir, std::function<void(int)> closed);

  private:
    std::string dir;
    std::function<void(int)> closed;
    bool server_on = false;
    char url[96]   = "";
    char error[256] = "";
    brls::Label *status = nullptr, *bytes = nullptr, *net_error = nullptr;
    ProgressBar* bar    = nullptr;
    brls::VoidEvent::Subscription loop;

    void refresh();
};

} // namespace ui
