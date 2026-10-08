// Màn hình chính: danh sách ứng dụng (.jar) trong thư mục games
#pragma once

#include "ui/common.hpp"

namespace ui
{

class GameDataSource;

class MainActivity : public brls::Activity
{
  public:
    MainActivity();
    ~MainActivity() override;

    brls::View* createContentView() override;
    void onContentAvailable() override;
    void onResume() override;

    static void open();
    static MainActivity* get();
    // Mỗi vòng lặp: hỏi cập nhật khi có bản mới, nhãn "bản mới" ở góc trên
    static void tick();
    // Trước khi thoát app: giải phóng ảnh icon
    static void shutdown();

    // game_id: khoá cho save / tuỳ chọn riêng (NULL = tên file)
    static bool launch_game(const char* path, const char* game_id, int midlet);

    // Quét lại thư mục games; keep: giữ con trỏ ở gần chỗ cũ
    void rescan(bool keep);
    // Ngôn ngữ đổi: dựng lại chữ và gợi ý nút
    void relabel();

    GameList list = {};
    int focused   = 0;

  private:
    brls::AppletFrame* frame     = nullptr;
    brls::Box* body              = nullptr;
    brls::RecyclerFrame* recycler = nullptr;
    brls::Box* empty             = nullptr;
    brls::Label* count_label     = nullptr;
    brls::Label* subtitle_label  = nullptr;
    brls::Box* badge             = nullptr;
    brls::Label* badge_label     = nullptr;
    brls::Label* folder_label    = nullptr;
    GameDataSource* source       = nullptr;
    std::vector<brls::ActionIdentifier> actions;
    bool update_shown            = false;   // đang hiện nhãn có bản mới

    void build_empty();
    void register_actions();
    void refresh_header();

  public:
    void open_entry(int index);
    void open_options(int index);
    void confirm_delete(int index);
    void free_icons();
};

} // namespace ui
