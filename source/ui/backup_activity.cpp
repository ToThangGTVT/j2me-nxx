#include "ui/backup_activity.hpp"

#include <cstring>

#include "ui/update_activity.hpp"

namespace ui
{

#define ROW_H 70

static std::string format_size(long size)
{
    char out[32];
    if (size < 0)
        size = 0;
    if (size >= 1024 * 1024)
        snprintf(out, sizeof(out), "%.1f MB", size / (1024.0 * 1024.0));
    else
        snprintf(out, sizeof(out), "%ld KB", (size + 1023) / 1024);
    return out;
}

// ---- Danh sách chung ----

class ListCell : public brls::RecyclerCell
{
  public:
    explicit ListCell(ListActivity* owner)
        : owner(owner)
    {
        this->setFocusable(true);
        this->setAxis(brls::Axis::ROW);
        this->setAlignItems(brls::AlignItems::CENTER);
        this->setHeight(ROW_H);
        this->setPadding(0, 24, 0, 24);

        auto* text = new brls::Box(brls::Axis::COLUMN);
        text->setGrow(1.0f);
        text->setShrink(1.0f);
        text->setJustifyContent(brls::JustifyContent::CENTER);
        title = make_label("", 23, brls::Application::getTheme()["brls/text"], false);
        sub   = make_label("", 16, color_dim(), false);
        sub->setMarginTop(2);
        text->addView(title);
        text->addView(sub);
        this->addView(text);

        right = make_label("", 17, color_dim(), false);
        right->setMargins(0, 0, 0, 20);
        this->addView(right);

        this->registerAction(owner->hint_a(), brls::BUTTON_A, [](brls::View* v)
            {
                auto* c = (ListCell*)v;
                c->owner->row_action(brls::BUTTON_A, c->getIndexPath().row);
                return true; }, false, false, brls::SOUND_CLICK);
        if (!owner->hint_l().empty())
        {
            bind_action(this, owner->hint_l(), brls::BUTTON_LB, [](brls::View* v)
                {
                    auto* c = (ListCell*)v;
                    c->owner->row_action(brls::BUTTON_LB, c->getIndexPath().row);
                    return true; });
        }
    }

    void set(const std::string& t, const std::string& s, NVGcolor sub_color, const std::string& r)
    {
        title->setText(t);
        sub->setText(s);
        sub->setTextColor(sub_color);
        right->setText(r);
    }

    void onFocusGained() override
    {
        brls::RecyclerCell::onFocusGained();
        owner->focused = this->getIndexPath().row;
    }

    ListActivity* owner;

  private:
    brls::Label *title, *sub, *right;
};

class ListSource : public brls::RecyclerDataSource
{
  public:
    explicit ListSource(ListActivity* owner)
        : owner(owner)
    {
    }

    int numberOfRows(brls::RecyclerFrame* recycler, int section) override
    {
        return owner->row_count();
    }

    brls::RecyclerCell* cellForRow(brls::RecyclerFrame* recycler, brls::IndexPath index) override
    {
        auto* cell = (ListCell*)recycler->dequeueReusableCell("row");
        owner->fill(cell, index.row);
        return cell;
    }

    float heightForRow(brls::RecyclerFrame* recycler, brls::IndexPath index) override
    {
        return ROW_H;
    }

    void didSelectRowAt(brls::RecyclerFrame* recycler, brls::IndexPath index) override
    {
        owner->row_action(brls::BUTTON_A, index.row);
    }

  private:
    ListActivity* owner;
};

ListActivity::~ListActivity() = default;

brls::View* ListActivity::createContentView()
{
    body = new brls::Box(brls::Axis::COLUMN);
    body->setGrow(1.0f);

    header = make_label("", 17, color_dim(), false);
    header->setMargins(14, 50, 6, 50);
    body->addView(header);

    recycler = new brls::RecyclerFrame();
    recycler->setGrow(1.0f);
    recycler->setPadding(4, 40, 8, 40);
    recycler->estimatedRowHeight = ROW_H;
    recycler->setScrollingBehavior(brls::ScrollingBehavior::CENTERED);
    recycler->registerCell("row", [this]()
        { return new ListCell(this); });
    body->addView(recycler);

    empty = new brls::Box(brls::Axis::COLUMN);
    empty->setGrow(1.0f);
    empty->setAlignItems(brls::AlignItems::CENTER);
    empty->setJustifyContent(brls::JustifyContent::CENTER);
    empty->setPadding(0, 80, 40, 80);
    // Phải nhận được focus thì các phím mới chạy khi danh sách rỗng
    empty->setFocusable(true);
    empty->setHideHighlight(true);
    empty->setVisibility(brls::Visibility::GONE);
    body->addView(empty);

    frame = new brls::AppletFrame(body);
    frame->setTitle(title());
    return frame;
}

void ListActivity::onContentAvailable()
{
    source = new ListSource(this);
    recycler->setDataSource(source);
    register_actions();
    refresh(false);
}

void ListActivity::refresh(bool keep)
{
    int old = focused;
    reload();
    int count = row_count();
    empty->clearViews();
    if (count == 0)
        build_empty();
    bool has = count > 0;
    recycler->setVisibility(has ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
    empty->setVisibility(has ? brls::Visibility::GONE : brls::Visibility::VISIBLE);
    if (!keep)
        old = 0;
    focused = has ? (old < count ? old : count - 1) : 0;
    recycler->setDefaultCellFocus(brls::IndexPath(0, focused));
    recycler->reloadData();
    // Lần đầu danh sách chưa được dàn trang: trao focus sau khi vẽ xong khung hình này
    brls::sync([this, has]()
        {
            auto stack = brls::Application::getActivitiesStack();
            if (stack.empty() || stack.back() != this)
                return;
            brls::Application::giveFocus(has ? (brls::View*)recycler : empty); });
}

static void add_empty_label(brls::Box* box, const std::string& text, float size, NVGcolor color, float top)
{
    auto* l = make_label(text, size, color);
    l->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    l->setMarginTop(top);
    box->addView(l);
}

// ---- Danh sách ứng dụng ----

BackupActivity::BackupActivity(const std::string& select_id, std::function<void()> closed)
    : select_id(select_id)
    , closed(std::move(closed))
{
}

BackupActivity::~BackupActivity()
{
    game_list_free(&list);
    auto cb = closed;
    if (cb)
        brls::sync([cb]()
            { cb(); });
}

void BackupActivity::open(const std::string& select_id, std::function<void()> closed)
{
    brls::Application::pushActivity(new BackupActivity(select_id, std::move(closed)));
}

std::string BackupActivity::title()
{
    return T(S_BACKUP_TITLE);
}

std::string BackupActivity::hint_a()
{
    return T(S_ACT_OPEN);
}

void BackupActivity::register_actions()
{
    bind_action(frame, T(S_ACT_BACKUP_NOW), brls::BUTTON_Y, [this](brls::View*)
        {
            create_backup(focused);
            return true; });
    bind_action(frame, T(S_ACT_SHARE), brls::BUTTON_RB, [this](brls::View*)
        {
            ShareActivity::open([this](int received)
                {
                    if (received > 0)
                        notify(trf(S_SHARE_RECEIVED_STATUS, received));
                    refresh(true); });
            return true; });
}

void BackupActivity::reload()
{
    game_list_free(&list);
    game_list_scan(&list, platform_games_dir());
    entries.clear();
    for (int i = 0; i < list.count; i++)
    {
        Entry e;
        char id[256];
        game_list_id(&list.items[i], id, sizeof(id));
        e.id         = id;
        e.game       = list.items[i].name;
        e.list_index = i;
        entries.push_back(e);
    }
    // Ứng dụng đã xoá file game nhưng còn backup
    char** ids;
    int n = backup_ids(&ids);
    for (int i = 0; i < n; i++)
    {
        bool found = false;
        for (auto& e : entries)
            found = found || e.id == ids[i];
        if (found)
            continue;
        Entry e;
        e.id = ids[i];
        entries.push_back(e);
    }
    backup_ids_free(ids, n);

    for (auto& e : entries)
    {
        BackupList l;
        backup_list(e.id.c_str(), &l);
        e.count  = l.count;
        e.latest = l.count ? l.items[0].when : "";
        if (e.list_index < 0)
        {
            // Tên ứng dụng lấy từ backup mới nhất
            char path[768];
            BackupMeta meta;
            if (l.count && backup_file_path(e.id.c_str(), l.items[0].file, path, sizeof(path)) &&
                backup_read_meta(path, &meta))
            {
                e.title = meta.title;
                e.game  = meta.game;
            }
            else
            {
                e.title = e.id;
            }
        }
        backup_list_free(&l);
    }
    // Lần đầu: đưa con trỏ tới ứng dụng đang chọn ở màn hình chính
    if (!select_id.empty())
    {
        for (size_t i = 0; i < entries.size(); i++)
        {
            if (entries[i].id == select_id)
                focused = (int)i;
        }
        select_id.clear();
    }
    header->setText(trf(S_GAME_COUNT, (int)entries.size()));
}

int BackupActivity::row_count()
{
    return (int)entries.size();
}

void BackupActivity::fill(ListCell* cell, int row)
{
    Entry& e = entries[row];
    if (e.list_index >= 0)
    {
        GameEntry* g = &list.items[e.list_index];
        if (!g->info_loaded)
            game_list_load_info(g);
        e.title = g->title;
    }
    std::string sub = e.count ? trf(S_BACKUP_COUNT, e.count, e.latest.c_str()) : T(S_BACKUP_NONE);
    if (e.list_index < 0)
        sub += "  -  " + T(S_BACKUP_NO_GAME);
    cell->set(e.title, sub, e.count ? color_dim() : color_warn(), e.game);
    // Ô được dùng lại sau khi đổi màn hình: cập nhật gợi ý
    cell->updateActionHint(brls::BUTTON_A, hint_a());
}

void BackupActivity::row_action(brls::ControllerButton button, int row)
{
    if (button != brls::BUTTON_A || row < 0 || row >= (int)entries.size())
        return;
    Entry& e = entries[row];
    brls::Application::pushActivity(new GameBackupActivity(e.id, e.title, e.game, [this]()
        { refresh(true); }));
}

void BackupActivity::create_backup(int row)
{
    if (row < 0 || row >= (int)entries.size())
        return;
    Entry& e = entries[row];
    char file[64], err[256];
    if (backup_create(e.id.c_str(), e.list_index >= 0 ? e.game.c_str() : nullptr, e.title.c_str(), file,
            sizeof(file), err, sizeof(err)))
        notify(trf(S_BACKUP_CREATED, e.title.c_str()));
    else
        notify(trf(S_ERROR_FMT, err));
    refresh(true);
}

void BackupActivity::build_empty()
{
    add_empty_label(empty, T(S_BACKUP_EMPTY_GAMES), 30, brls::Application::getTheme()["brls/text"], 0);
    add_empty_label(empty, T(S_EMPTY_COPY), 20, color_dim(), 24);
    add_empty_label(empty, std::string(platform_games_dir()) + "/", 24, color_accent(), 12);
}

// ---- Backup của một ứng dụng ----

GameBackupActivity::GameBackupActivity(const std::string& id, const std::string& title, const std::string& game,
                                       std::function<void()> changed)
    : id(id)
    , game_title(title)
    , game(game)
    , changed(std::move(changed))
{
}

GameBackupActivity::~GameBackupActivity()
{
    backup_list_free(&backups);
    auto cb = changed;
    if (cb)
        brls::sync([cb]()
            { cb(); });
}

std::string GameBackupActivity::title()
{
    return game_title;
}

std::string GameBackupActivity::hint_a()
{
    return T(S_ACT_APPLY);
}

std::string GameBackupActivity::hint_l()
{
    return T(S_ACT_DELETE);
}

// File game còn trong thư mục games không (backup mới kèm file game)
static bool game_exists(const std::string& game)
{
    if (game.empty())
        return false;
    std::string path = std::string(platform_games_dir()) + "/" + game;
    FILE* f          = fopen(path.c_str(), "rb");
    if (f)
        fclose(f);
    return f != nullptr;
}

void GameBackupActivity::register_actions()
{
    bind_action(frame, T(S_ACT_BACKUP_NOW), brls::BUTTON_Y, [this](brls::View*)
        {
            char file[64], err[256];
            const char* g = game_exists(game) ? game.c_str() : nullptr;
            if (backup_create(id.c_str(), g, game_title.c_str(), file, sizeof(file), err, sizeof(err)))
                notify(trf(S_BACKUP_CREATED, game_title.c_str()));
            else
                notify(trf(S_ERROR_FMT, err));
            refresh(false);
            return true; });
    bind_action(frame, T(S_ACT_SHARE), brls::BUTTON_RB, [this](brls::View*)
        {
            ShareActivity::open([this](int received)
                {
                    if (received > 0)
                        notify(trf(S_SHARE_RECEIVED_STATUS, received));
                    refresh(true); });
            return true; });
}

void GameBackupActivity::reload()
{
    backup_list_free(&backups);
    backup_list(id.c_str(), &backups);
    header->setText(backups.count ? trf(S_BACKUP_COUNT, backups.count, backups.items[0].when) : T(S_BACKUP_NONE));
}

int GameBackupActivity::row_count()
{
    return backups.count;
}

void GameBackupActivity::fill(ListCell* cell, int row)
{
    const BackupInfo& b = backups.items[row];
    cell->set(b.when, b.file, color_dim(), format_size(b.size));
    cell->updateActionHint(brls::BUTTON_A, hint_a());
}

void GameBackupActivity::row_action(brls::ControllerButton button, int row)
{
    if (row < 0 || row >= backups.count)
        return;
    if (button == brls::BUTTON_A)
        confirm_apply(row);
    else if (button == brls::BUTTON_LB)
        confirm_delete(row);
}

static brls::Box* dialog_box(StrId question, const std::string& name, const std::string& detail)
{
    auto* box = new brls::Box(brls::Axis::COLUMN);
    box->setPadding(36, 40, 24, 40);
    box->addView(make_label(T(question), 20, color_warn()));
    auto* l = make_label(name, 30, brls::Application::getTheme()["brls/text"]);
    l->setMarginTop(10);
    box->addView(l);
    if (!detail.empty())
    {
        auto* d = make_label(detail, 18, color_dim());
        d->setMarginTop(14);
        box->addView(d);
    }
    return box;
}

void GameBackupActivity::confirm_apply(int row)
{
    std::string file = backups.items[row].file;
    std::string when = backups.items[row].when;
    auto* dialog     = new brls::Dialog(dialog_box(S_BACKUP_APPLY_ASK, game_title + "  -  " + when, T(S_BACKUP_APPLY_WARN)));
    dialog->addButton(T(S_ACT_CANCEL), []() {});
    dialog->addButton(T(S_ACT_APPLY), [this, file, when]()
        {
            char saved[64], err[256];
            // Giữ lại dữ liệu hiện tại trước khi thay
            if (backup_has_data(id.c_str()) &&
                !backup_create(id.c_str(), game_exists(game) ? game.c_str() : nullptr, game_title.c_str(), saved,
                    sizeof(saved), err, sizeof(err)))
            {
                notify(trf(S_ERROR_FMT, err));
                return;
            }
            if (backup_apply(id.c_str(), file.c_str(), err, sizeof(err)))
                notify(trf(S_BACKUP_APPLIED, when.c_str()));
            else
                notify(trf(S_ERROR_FMT, err));
            refresh(false); });
    dialog->open();
}

void GameBackupActivity::confirm_delete(int row)
{
    std::string file = backups.items[row].file;
    std::string when = backups.items[row].when;
    auto* dialog     = new brls::Dialog(dialog_box(S_BACKUP_DELETE_ASK, when, format_size(backups.items[row].size)));
    dialog->addButton(T(S_ACT_CANCEL), []() {});
    dialog->addButton(T(S_ACT_DELETE), [this, file, when]()
        {
            if (backup_delete(id.c_str(), file.c_str()))
                notify(trf(S_BACKUP_DELETED, when.c_str()));
            else
                notify(T(S_BACKUP_DELETE_FAILED));
            refresh(true); });
    dialog->open();
}

void GameBackupActivity::build_empty()
{
    add_empty_label(empty, T(S_BACKUP_EMPTY_LIST), 30, brls::Application::getTheme()["brls/text"], 0);
    add_empty_label(empty, T(S_BACKUP_EMPTY_HINT), 22, color_warn(), 24);
}

// ---- Chia sẻ với điện thoại ----

ShareActivity::ShareActivity(std::function<void(int)> closed)
    : closed(std::move(closed))
{
    server_on = share_start(url, sizeof(url), error, sizeof(error));
}

ShareActivity::~ShareActivity()
{
    brls::Application::getRunLoopEvent()->unsubscribe(loop);
    ShareStatus st;
    share_get_status(&st);
    share_stop();
    auto cb      = closed;
    int received = st.received;
    if (cb)
        brls::sync([cb, received]()
            { cb(received); });
}

void ShareActivity::open(std::function<void(int)> closed)
{
    brls::Application::pushActivity(new ShareActivity(std::move(closed)));
}

brls::View* ShareActivity::createContentView()
{
    auto* box = new brls::Box(brls::Axis::ROW);
    box->setPadding(40, 70, 30, 70);
    box->setGrow(1.0f);
    box->setFocusable(true);
    box->setHideHighlight(true);

    if (!server_on)
    {
        net_error = make_label(error, 24, color_warn());
        net_error->setGrow(1.0f);
        box->addView(net_error);
    }
    else
    {
        auto* qr = new QrView(url);
        qr->setMargins(0, 56, 0, 0);
        box->addView(qr);

        auto* col = new brls::Box(brls::Axis::COLUMN);
        col->setGrow(1.0f);
        col->setShrink(1.0f);
        for (StrId step : { S_UPLOAD_STEP1, S_UPLOAD_STEP2, S_SHARE_STEP3 })
        {
            auto* l = make_label(T(step), 22, brls::Application::getTheme()["brls/text"]);
            l->setMarginBottom(12);
            col->addView(l);
        }
        auto* addr = make_label(url, 34, color_accent(), false);
        addr->setMargins(10, 0, 30, 0);
        col->addView(addr);
        status = make_label(T(S_UPLOAD_WAITING), 22, color_dim());
        col->addView(status);
        bar = new ProgressBar();
        bar->setHeight(20);
        bar->setMarginTop(12);
        bar->setVisibility(brls::Visibility::GONE);
        col->addView(bar);
        bytes = make_label("", 18, color_dim(), false);
        bytes->setMarginTop(8);
        bytes->setVisibility(brls::Visibility::GONE);
        col->addView(bytes);
        net_error = make_label("", 20, color_warn());
        net_error->setMarginTop(10);
        col->addView(net_error);
        box->addView(col);
    }

    auto* frame = new brls::AppletFrame(box);
    frame->setTitle(T(S_SHARE_TITLE));
    return frame;
}

void ShareActivity::onContentAvailable()
{
    if (!server_on)
        return;
    loop = brls::Application::getRunLoopEvent()->subscribe([this]()
        { refresh(); });
}

void ShareActivity::refresh()
{
    ShareStatus st;
    share_get_status(&st);
    bool busy = st.current[0] != '\0';
    if (busy)
    {
        set_text(status, trf(st.receiving ? S_UPLOAD_RECEIVING : S_SHARE_SENDING, st.current));
        status->setTextColor(brls::Application::getTheme()["brls/text"]);
        bar->value = st.total > 0 ? (float)((double)st.done / (double)st.total) : -1;
        set_text(bytes, format_mb(st.done) + " / " + format_mb(st.total));
    }
    else if (st.sent + st.received > 0)
    {
        set_text(status, trf(S_SHARE_STATS, st.sent, st.received, st.last));
        status->setTextColor(color_ok());
    }
    else
    {
        set_text(status, T(S_UPLOAD_WAITING));
        status->setTextColor(color_dim());
    }
    brls::Visibility vis = busy ? brls::Visibility::VISIBLE : brls::Visibility::GONE;
    if (bar->getVisibility() != vis)
    {
        bar->setVisibility(vis);
        bytes->setVisibility(vis);
    }
    set_text(net_error, busy ? "" : st.error);
}

} // namespace ui
