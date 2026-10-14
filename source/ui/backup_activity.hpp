// Quản lý backup: danh sách ứng dụng -> các backup của một ứng dụng (tạo, áp dụng, xoá),
// chia sẻ backup với điện thoại qua mã QR
#pragma once

#include <vector>

#include "ui/common.hpp"

namespace ui
{

class ListCell;
class ListSource;
class ProgressBar;

// Màn hình danh sách một cột: dòng trống khi chưa có gì, hành động theo dòng đang chọn
class ListActivity : public brls::Activity
{
  public:
    ~ListActivity() override;
    brls::View* createContentView() override;
    void onContentAvailable() override;

    virtual int row_count()                    = 0;
    virtual void fill(ListCell* cell, int row) = 0;
    // A / L trên một dòng
    virtual void row_action(brls::ControllerButton button, int row) = 0;
    // Gợi ý nút của từng dòng (A và L)
    virtual std::string hint_a() = 0;
    virtual std::string hint_l() { return ""; }

    int focused = 0;

  protected:
    brls::AppletFrame* frame      = nullptr;
    brls::Box* body               = nullptr;
    brls::Label* header           = nullptr;
    brls::RecyclerFrame* recycler = nullptr;
    brls::Box* empty              = nullptr;
    ListSource* source            = nullptr;

    virtual std::string title() = 0;
    virtual void register_actions() = 0;
    // Chữ khi danh sách rỗng
    virtual void build_empty() = 0;
    virtual void reload() = 0;
    // Nạp lại dữ liệu rồi dựng lại danh sách; keep: giữ con trỏ ở gần chỗ cũ
    void refresh(bool keep);
};

class BackupActivity : public ListActivity
{
  public:
    // closed(changed): gọi sau khi đóng, changed = có thể đã thêm file game (áp dụng / nhận backup)
    explicit BackupActivity(const std::string& select_id, std::function<void()> closed);
    ~BackupActivity() override;

    static void open(const std::string& select_id, std::function<void()> closed);

    int row_count() override;
    void fill(ListCell* cell, int row) override;
    void row_action(brls::ControllerButton button, int row) override;
    std::string hint_a() override;

  protected:
    std::string title() override;
    void register_actions() override;
    void build_empty() override;
    void reload() override;

  private:
    struct Entry
    {
        std::string id, title, game;    // game: file game tương đối, "" nếu không còn
        int list_index = -1;            // vị trí trong list, -1 nếu chỉ còn backup
        int count      = 0;
        std::string latest;
    };
    GameList list = {};
    std::vector<Entry> entries;
    std::string select_id;
    std::function<void()> closed;

    void create_backup(int row);
};

class GameBackupActivity : public ListActivity
{
  public:
    GameBackupActivity(const std::string& id, const std::string& title, const std::string& game,
                       std::function<void()> changed);
    ~GameBackupActivity() override;

    int row_count() override;
    void fill(ListCell* cell, int row) override;
    void row_action(brls::ControllerButton button, int row) override;
    std::string hint_a() override;
    std::string hint_l() override;

  protected:
    std::string title() override;
    void register_actions() override;
    void build_empty() override;
    void reload() override;

  private:
    std::string id, game_title, game;
    std::function<void()> changed;
    BackupList backups = {};

    void confirm_apply(int row);
    void confirm_delete(int row);
};

// Mã QR tới trang backup trên điện thoại: tải về / gửi lên
class ShareActivity : public brls::Activity
{
  public:
    explicit ShareActivity(std::function<void(int)> closed);
    ~ShareActivity() override;

    brls::View* createContentView() override;
    void onContentAvailable() override;

    // closed(received): số backup nhận từ điện thoại
    static void open(std::function<void(int)> closed);

  private:
    std::function<void(int)> closed;
    bool server_on = false;
    char url[96]    = "";
    char error[256] = "";
    brls::Label *status = nullptr, *bytes = nullptr, *net_error = nullptr;
    ProgressBar* bar    = nullptr;
    brls::VoidEvent::Subscription loop;

    void refresh();
};

} // namespace ui
