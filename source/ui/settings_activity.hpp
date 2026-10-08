// Cài đặt chung và tuỳ chọn riêng từng ứng dụng (cùng một màn hình; tuỳ chọn riêng có thêm "Mặc định")
#pragma once

#include "ui/common.hpp"

namespace ui
{

class SettingsActivity : public brls::Activity
{
  public:
    // game_id rỗng: cài đặt chung
    SettingsActivity(const std::string& game_id, const std::string& game_title, int tab = 0);
    ~SettingsActivity() override;

    brls::View* createContentView() override;

    static void open_global();
    static void open_game(const char* id, const char* title);
    // Test desktop: mở thẳng màn hình ánh xạ phím của cài đặt chung
    static void open_keybinds_global();

  private:
    bool game_mode;
    std::string game_id, game_title;
    int start_tab;
    GameSettings game = {};
    bool custom = false;    // đang ở chế độ nhập kích thước tuỳ chỉnh

    // Các mục ẩn / hiện theo lựa chọn khác (tab đang mở; nullptr khi tab khác)
    brls::Box *orient_item = nullptr, *width_item = nullptr, *height_item = nullptr, *smooth_item = nullptr;
    brls::DetailCell *orient_cell = nullptr, *keybind_cell = nullptr, *vpad_layout_cell = nullptr;
    brls::InputNumericCell *width_cell = nullptr, *height_cell = nullptr;
    class RatioPreview* preview = nullptr;

    void save();
    int* cur_w();
    int* cur_h();
    bool is_auto();
    void sync_custom();
    void update_size_items();
    void update_smooth_item();
    std::string keybind_detail();
    std::string vpad_layout_detail();

    brls::View* tab_screen();
    brls::View* tab_controls();
    brls::View* tab_text();
    brls::View* tab_system();

    // Lựa chọn "Mặc định (x) / Bật / Tắt" cho tuỳ chọn riêng, Bật / Tắt cho cài đặt chung
    brls::Box* add_toggle(brls::Box* tab, StrId title, StrId hint, bool* global_value, int* game_value);
};

} // namespace ui
