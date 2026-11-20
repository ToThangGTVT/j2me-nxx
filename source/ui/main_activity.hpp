// Màn hình chính: danh sách ứng dụng (.jar) trong thư mục games
#pragma once

#include "ui/common.hpp"

extern "C" {
#include "groups.h"
}

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
    int focused   = 0;      // dòng đang chọn trong tab hiện tại

    // Tab: 0 tất cả, 1.. nhóm thứ tab - 1 (groups.h)
    int tab = 0;
    std::vector<int> rows;          // dòng -> vị trí trong list theo tab hiện tại
    std::vector<std::string> ids;   // theo vị trí trong list: game_list_id
    std::vector<class TabButton*> tabs;

    // Vị trí trong list của một dòng, -1 nếu không có
    int index_of(int row) const { return row >= 0 && row < (int)rows.size() ? rows[row] : -1; }

  private:
    brls::AppletFrame* frame     = nullptr;
    brls::Box* body              = nullptr;
    brls::RecyclerFrame* recycler = nullptr;
    brls::Box* empty             = nullptr;
    brls::Label* count_label     = nullptr;
    brls::Label* subtitle_label  = nullptr;
    brls::Box* badge             = nullptr;
    brls::Label* badge_label     = nullptr;
    class TabScroll* tab_scroll  = nullptr;
    int tab_add                  = 0;   // vị trí tab "+"
    std::vector<int> tab_focus   = std::vector<int>(GROUP_MAX + 1, 0);
    GameDataSource* source       = nullptr;
    std::vector<brls::ActionIdentifier> actions;
    bool update_shown            = false;   // đang hiện nhãn có bản mới

    void build_empty();
    void register_actions();
    void refresh_header();
    void build_tabs();
    void refresh_tabs();
    // Dựng lại rows theo tab; keep: giữ con trỏ ở gần chỗ cũ; focus: đưa focus về danh sách
    void apply_filter(bool keep, bool focus);
    void fill_empty();
    void focus_list();
    void focus_tab(int t);
    bool in_tab_row();
    int group_size(int g);

  public:
    void open_entry(int index);
    void open_options(int index);
    void confirm_delete(int index);
    void open_groups(int index);
    void switch_tab(int to, bool focus);
    void tab_focused(int t);
    void tab_pressed(int t);
    void new_group(int index);
    void rename_group(int g);
    void confirm_delete_group(int g);
    void open_menu();
    // Cuộn hàng tab tới tab đang chọn (khi focus không ở hàng tab)
    void scroll_to_tab(bool animated);
    void do_rescan();
    void do_upload();
    void do_backup();
    void free_icons();
};

} // namespace ui
