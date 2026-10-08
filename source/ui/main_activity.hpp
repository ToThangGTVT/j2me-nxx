// Màn hình chính: danh sách ứng dụng (.jar) và video trong thư mục games.
// Vẽ và xử lý phím y như giao diện cũ (menu.c, vẽ bằng gfx.h trên NanoVG); phím đến từ action của borealis.
#pragma once

#include "ui/common.hpp"

extern "C" {
#include "menu.h"
}

namespace ui
{

class MainActivity : public brls::Activity
{
  public:
    MainActivity();
    ~MainActivity() override;

    brls::View* createContentView() override;
    void onContentAvailable() override;

    static void open();
    static MainActivity* get();
    // Mỗi vòng lặp: hỏi cập nhật khi có bản mới
    static void tick();
    // Trước khi thoát app: giải phóng ảnh icon
    static void shutdown();
    // Dòng thông báo ở thanh dưới (giữ tới khi di chuyển con trỏ); không có màn hình chính thì hiện thông báo nổi
    static void set_status(const std::string& text);

    // game_id: khoá cho save / tuỳ chọn riêng (NULL = tên file)
    static bool launch_game(const char* path, const char* game_id, int midlet);
    static bool play_video(const char* path, const char* title);

    // Quét lại thư mục games; keep: giữ con trỏ ở gần chỗ cũ
    void rescan(bool keep);
    // Ngôn ngữ đổi (chữ vẽ lại mỗi khung hình nên không cần dựng lại gì)
    void relabel() { }

    // Một lần bấm nút / chạm đã được menu.c xử lý
    void handle(MenuAction action);

    GameList list = {};
    Menu menu     = {};
};

} // namespace ui
