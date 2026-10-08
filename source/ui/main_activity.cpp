#include "ui/main_activity.hpp"

#include <cstring>

#include "ui/screen_activity.hpp"
#include "ui/settings_activity.hpp"
#include "ui/update_activity.hpp"
#include "ui/upload_activity.hpp"

using namespace brls::literals;

namespace ui
{

// 7 dòng vừa một màn hình
#define ROW_H     70
#define ICON_SIZE 50

static MainActivity* instance;

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

// Dòng phụ: nhà phát hành, phiên bản, thư mục con
static std::string subtitle(const GameEntry* g)
{
    const char* slash = strrchr(g->name, '/');
    std::string folder = slash ? std::string(g->name, slash - g->name) + "/" : "";
    if (!g->info_loaded)
        return "...";
    if (!g->valid)
        return T(S_NO_MIDLET);
    std::string s = g->vendor;
    if (g->version[0])
        s += (s.empty() ? "v" : "  -  v") + std::string(g->version);
    if (!folder.empty())
        s += (s.empty() ? "" : "  -  ") + folder;
    return s;
}

// Icon của ứng dụng: ảnh trong JAR (phóng nguyên lần cho icon điểm ảnh nhỏ),
// không có icon thì ô màu theo tên + chữ cái đầu
class GameIcon : public brls::View
{
  public:
    GameIcon()
    {
        this->setDimensions(ICON_SIZE, ICON_SIZE);
    }

    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
              brls::FrameContext* ctx) override
    {
        if (!entry)
            return;
        GameEntry* g = entry;
        if (g->icon && !g->icon_img)
            g->icon_img = gfx_image_argb(g->icon, g->icon_w, g->icon_h, false);
        if (g->icon_img)
        {
            int big = g->icon_w > g->icon_h ? g->icon_w : g->icon_h;
            float s = width / big >= 1 ? (float)(int)(width / big) : width / big;
            float w = g->icon_w * s, h = g->icon_h * s;
            float ix = x + (width - w) / 2, iy = y + (height - h) / 2;
            nvgBeginPath(vg);
            nvgRect(vg, ix, iy, w, h);
            nvgFillPaint(vg, nvgImagePattern(vg, ix, iy, w, h, 0, g->icon_img, 1.0f));
            nvgFill(vg);
            return;
        }
        uint32_t hsh = 2166136261u;
        for (const char* p = g->title; *p; p++)
            hsh = (hsh ^ (uint8_t)*p) * 16777619u;
        nvgBeginPath(vg);
        nvgRoundedRect(vg, x, y, width, height, 8);
        nvgFillColor(vg, nvgRGB(0x40 + (hsh & 0x3f), 0x40 + ((hsh >> 8) & 0x3f), 0x60 + ((hsh >> 16) & 0x3f)));
        nvgFill(vg);
        char letter[2] = { g->title[0] ? g->title[0] : '?', '\0' };
        if (letter[0] >= 'a' && letter[0] <= 'z')
            letter[0] -= 32;
        nvgFontFaceId(vg, brls::Application::getDefaultFont());
        nvgFontSize(vg, height * 0.55f);
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(vg, nvgRGB(0xee, 0xee, 0xee));
        nvgText(vg, x + width / 2, y + height / 2, letter, nullptr);
    }

    GameEntry* entry = nullptr;
};

class GameCell : public brls::RecyclerCell
{
  public:
    GameCell()
    {
        this->setFocusable(true);
        this->setAxis(brls::Axis::ROW);
        this->setAlignItems(brls::AlignItems::CENTER);
        this->setHeight(ROW_H);
        this->setPadding(0, 24, 0, 16);

        icon = new GameIcon();
        icon->setMargins(0, 18, 0, 0);
        this->addView(icon);

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

        size = make_label("", 17, color_dim(), false);
        size->setMargins(0, 0, 0, 20);
        this->addView(size);

        this->registerAction(T(S_ACT_OPEN), brls::BUTTON_A, [](brls::View* v)
            {
                instance->open_entry(((GameCell*)v)->getIndexPath().row);
                return true; }, false, false, brls::SOUND_CLICK);
        bind_action(this, T(S_ACT_OPTIONS), brls::BUTTON_BACK, [](brls::View* v)
            {
                instance->open_options(((GameCell*)v)->getIndexPath().row);
                return true; });
        bind_action(this, T(S_ACT_DELETE), brls::BUTTON_LB, [](brls::View* v)
            {
                instance->confirm_delete(((GameCell*)v)->getIndexPath().row);
                return true; });
    }

    void set(GameEntry* g)
    {
        // Ngôn ngữ có thể đã đổi từ lúc tạo ô
        this->updateActionHint(brls::BUTTON_A, T(S_ACT_OPEN));
        this->updateActionHint(brls::BUTTON_BACK, T(S_ACT_OPTIONS));
        this->updateActionHint(brls::BUTTON_LB, T(S_ACT_DELETE));
        icon->entry = g;
        title->setText(g->title);
        sub->setText(subtitle(g));
        sub->setTextColor(g->info_loaded && !g->valid ? color_warn() : color_dim());
        size->setText(format_size(g->size));
    }

    void onFocusGained() override
    {
        brls::RecyclerCell::onFocusGained();
        if (instance)
            instance->focused = this->getIndexPath().row;
    }

  private:
    GameIcon* icon;
    brls::Label *title, *sub, *size;
};

class GameDataSource : public brls::RecyclerDataSource
{
  public:
    explicit GameDataSource(MainActivity* owner)
        : owner(owner)
    {
    }

    int numberOfRows(brls::RecyclerFrame* recycler, int section) override
    {
        return owner->list.count;
    }

    brls::RecyclerCell* cellForRow(brls::RecyclerFrame* recycler, brls::IndexPath index) override
    {
        auto* cell   = (GameCell*)recycler->dequeueReusableCell("game");
        GameEntry* g = &owner->list.items[index.row];
        if (!g->info_loaded)
            game_list_load_info(g);
        cell->set(g);
        return cell;
    }

    float heightForRow(brls::RecyclerFrame* recycler, brls::IndexPath index) override
    {
        return ROW_H;
    }

    void didSelectRowAt(brls::RecyclerFrame* recycler, brls::IndexPath index) override
    {
        owner->open_entry(index.row);
    }

  private:
    MainActivity* owner;
};

MainActivity::MainActivity()
{
    instance = this;
}

MainActivity::~MainActivity()
{
    free_icons();
    game_list_free(&list);
    if (instance == this)
        instance = nullptr;
}

void MainActivity::open()
{
    brls::Application::pushActivity(new MainActivity());
}

MainActivity* MainActivity::get()
{
    return instance;
}

brls::View* MainActivity::createContentView()
{
    body = new brls::Box(brls::Axis::COLUMN);
    body->setGrow(1.0f);

    folder_label = make_label("", 17, color_dim(), false);
    folder_label->setMargins(14, 50, 6, 50);
    body->addView(folder_label);

    recycler = new brls::RecyclerFrame();
    recycler->setGrow(1.0f);
    recycler->setPadding(4, 40, 8, 40);
    recycler->estimatedRowHeight = ROW_H;
    recycler->setScrollingBehavior(brls::ScrollingBehavior::CENTERED);
    recycler->registerCell("game", []()
        { return new GameCell(); });
    body->addView(recycler);

    build_empty();

    frame = new brls::AppletFrame(body);
    frame->setTitle("J2ME-NXX");
    // Cạnh tiêu đề: phiên bản và mô tả app
    if (brls::View* title = frame->getView("brls/applet_frame/title_label"))
    {
        subtitle_label = make_label("", 18, color_dim(), false);
        subtitle_label->setMargins(0, 0, 0, 16);
        subtitle_label->setVerticalAlign(brls::VerticalAlign::BOTTOM);
        subtitle_label->setAlignSelf(brls::AlignSelf::FLEX_END);
        subtitle_label->setMarginBottom(12);
        ((brls::Box*)title->getParent())->addView(subtitle_label);
    }

    // Góc phải đầu trang: nhãn có bản mới + số ứng dụng
    auto* right = new brls::Box(brls::Axis::ROW);
    right->setAlignItems(brls::AlignItems::CENTER);
    badge = new brls::Box(brls::Axis::ROW);
    badge->setBackgroundColor(color_accent());
    badge->setCornerRadius(6);
    badge->setPadding(6, 14, 6, 14);
    badge->setMargins(0, 24, 0, 0);
    badge->setVisibility(brls::Visibility::GONE);
    badge_label = make_label("", 18, nvgRGB(0x18, 0x19, 0x1d), false);
    badge->addView(badge_label);
    count_label = make_label("", 22, color_dim(), false);
    right->addView(badge);
    right->addView(count_label);
    brls::View* hint_box = frame->getView("brls/applet_frame/hint_box");
    if (hint_box)
        ((brls::Box*)hint_box)->addView(right);
    return frame;
}

void MainActivity::build_empty()
{
    empty = new brls::Box(brls::Axis::COLUMN);
    empty->setGrow(1.0f);
    empty->setAlignItems(brls::AlignItems::CENTER);
    empty->setJustifyContent(brls::JustifyContent::CENTER);
    empty->setPadding(0, 80, 40, 80);
    // Phải nhận được focus thì các phím (X, Y, R...) mới chạy khi danh sách rỗng
    empty->setFocusable(true);
    empty->setHideHighlight(true);
    empty->setVisibility(brls::Visibility::GONE);
    body->addView(empty);
}

void MainActivity::onContentAvailable()
{
    source = new GameDataSource(this);
    recycler->setDataSource(source);
    register_actions();
    rescan(false);
}

void MainActivity::onResume()
{
    refresh_header();
}

void MainActivity::register_actions()
{
    for (auto id : actions)
        frame->unregisterAction(id);
    actions.clear();
    actions.push_back(bind_action(frame, T(S_SETTINGS), brls::BUTTON_X, [](brls::View*)
        {
            SettingsActivity::open_global();
            return true; }));
    actions.push_back(bind_action(frame, T(S_ACT_RESCAN), brls::BUTTON_Y, [this](brls::View*)
        {
            rescan(false);
            notify(trf(S_RESCANNED, list.count));
            return true; }));
    actions.push_back(bind_action(frame, T(S_ACT_UPLOAD), brls::BUTTON_RB, [](brls::View*)
        {
            UploadActivity::open(platform_games_dir(), [](int received)
                {
                    if (received > 0 && instance)
                    {
                        instance->rescan(true);
                        notify(trf(S_UPLOAD_DONE_STATUS, received));
                    } });
            return true; }));
    actions.push_back(bind_action(frame, T(S_ACT_UPDATE), brls::BUTTON_B, [](brls::View*)
        {
            if (update_available())
                UpdateActivity::open();
            return true; }, !update_available()));
    actions.push_back(bind_action(frame, "hints/exit"_i18n, brls::BUTTON_START, [](brls::View*)
        {
            brls::Application::quit();
            return true; }));
}

void MainActivity::relabel()
{
    register_actions();
    rescan(true);
}

void MainActivity::refresh_header()
{
    if (!count_label)
        return;
    count_label->setText(trf(S_GAME_COUNT, list.count));
    if (subtitle_label)
        subtitle_label->setText("v" APP_VERSION_STR "  -  " + T(S_APP_SUBTITLE));
    folder_label->setText(trf(S_FOLDER, platform_games_dir()));
    bool avail = update_available();
    if (avail)
        badge_label->setText(trf(S_UPDATE_NEW, update_latest_version()) + "  " ICON_B);
    badge->setVisibility(avail ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
    if (avail != update_shown)
    {
        update_shown = avail;
        register_actions();
    }
}

void MainActivity::free_icons()
{
    for (int i = 0; i < list.count; i++)
    {
        gfx_image_free(list.items[i].icon_img);
        list.items[i].icon_img = 0;
    }
}

void MainActivity::rescan(bool keep)
{
    int old = focused;
    free_icons();
    game_list_free(&list);
    game_list_scan(&list, platform_games_dir());

    // Danh sách rỗng: hướng dẫn chép ứng dụng vào thư mục games
    empty->clearViews();
    if (list.count == 0)
    {
        empty->addView(make_label(T(S_EMPTY_TITLE), 34, brls::Application::getTheme()["brls/text"]));
        auto add = [this](const std::string& text, float size, NVGcolor color, float top)
        {
            auto* l = make_label(text, size, color);
            l->setHorizontalAlign(brls::HorizontalAlign::CENTER);
            l->setMarginTop(top);
            empty->addView(l);
        };
        add(T(S_EMPTY_COPY), 22, color_dim(), 28);
        add(std::string(platform_games_dir()) + "/", 28, color_accent(), 14);
        add(T(S_EMPTY_SUBDIR), 20, color_dim(), 28);
        add(T(S_EMPTY_RESCAN), 22, color_warn(), 28);
        add(T(S_EMPTY_UPLOAD), 22, color_warn(), 8);
    }
    bool has = list.count > 0;
    recycler->setVisibility(has ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
    empty->setVisibility(has ? brls::Visibility::GONE : brls::Visibility::VISIBLE);
    focused = keep && has ? (old < list.count ? old : list.count - 1) : 0;
    recycler->setDefaultCellFocus(brls::IndexPath(0, focused));
    recycler->reloadData();
    // Lần đầu danh sách chưa được dàn trang (chưa có ô nào): trao focus sau khi vẽ xong khung hình này
    brls::sync([this, has]()
        {
            auto stack = brls::Application::getActivitiesStack();
            if (stack.empty() || stack.back() != this)
                return;
            brls::Application::giveFocus(has ? (brls::View*)recycler : empty); });
    refresh_header();
}

static void launch_entry(GameEntry* g, int midlet)
{
    char id[256];
    game_list_id(g, id, sizeof(id));
    MainActivity::launch_game(g->path, id, midlet);
}

void MainActivity::open_entry(int index)
{
    if (index < 0 || index >= list.count)
        return;
    GameEntry* g = &list.items[index];
    game_list_load_info(g);
    if (g->midlet_count <= 1)
    {
        launch_entry(g, 1);
        return;
    }
    std::vector<std::string> names;
    for (int i = 0; i < g->midlet_count && i < 8; i++)
        names.push_back(g->midlets[i]);
    auto* dropdown = new brls::Dropdown(T(S_PICK_MIDLET_TITLE), names, [this, index](int selected)
        {
            if (selected >= 0 && index < list.count)
            {
                // Chạy sau khi hộp chọn đóng hẳn
                brls::sync([this, index, selected]()
                    { launch_entry(&list.items[index], selected + 1); });
            } });
    brls::Application::pushActivity(new brls::Activity(dropdown));
}

void MainActivity::open_options(int index)
{
    if (index < 0 || index >= list.count)
        return;
    GameEntry* g = &list.items[index];
    char id[256];
    game_list_id(g, id, sizeof(id));
    game_list_load_info(g);
    SettingsActivity::open_game(id, g->title);
}

void MainActivity::confirm_delete(int index)
{
    if (index < 0 || index >= list.count)
        return;
    GameEntry* g = &list.items[index];
    char jad[600];
    bool has_jad = game_list_find_jad(g, jad, sizeof(jad));

    auto* box = new brls::Box(brls::Axis::COLUMN);
    box->setPadding(36, 40, 24, 40);
    box->addView(make_label(T(S_DELETE_GAME), 20, color_warn()));
    auto* name = make_label(g->title, 30, brls::Application::getTheme()["brls/text"]);
    name->setMarginTop(10);
    box->addView(name);
    auto* file = make_label(std::string(g->name) + (has_jad ? " + .jad" : "") + "  -  " + format_size(g->size), 18,
                            color_dim());
    file->setMarginTop(10);
    box->addView(file);
    auto* keep = make_label(T(S_DELETE_KEEP_SAVE), 18, color_dim());
    keep->setMarginTop(16);
    box->addView(keep);

    auto* dialog = new brls::Dialog(box);
    dialog->addButton(T(S_ACT_CANCEL), []() {});
    dialog->addButton(T(S_ACT_DELETE), [this, index]()
        {
            if (index >= list.count)
                return;
            std::string title = list.items[index].title;
            bool ok = game_list_delete(&list.items[index]);
            notify(trf(ok ? S_DELETED : S_DELETE_FAILED, title.c_str()));
            if (ok)
                rescan(true); });
    dialog->open();
}

bool MainActivity::launch_game(const char* path, const char* game_id, int midlet)
{
    char err[256] = "";
    if (!emu_start(path, game_id, midlet, err, sizeof(err)))
    {
        notify(trf(S_ERROR_FMT, err));
        return false;
    }
    ScreenHooks hooks;
    hooks.event  = [](const SDL_Event* e)
    { emu_handle_event(e); };
    hooks.update = []()
    { return emu_update(); };
    hooks.draw   = []()
    { emu_draw(); };
    hooks.closed = []()
    {
        std::string msg = emu_exit_message();
        bool oom        = emu_exit_out_of_memory();
        emu_stop();
        if (oom)
        {
            // Báo rõ bằng hộp thoại sau khi đã về màn hình chính
            brls::sync([]()
                {
                    auto* box = new brls::Box(brls::Axis::COLUMN);
                    box->setPadding(36, 40, 24, 40);
                    box->addView(make_label(T(S_OUT_OF_MEMORY), 28, color_warn()));
                    auto* hint = make_label(T(S_OUT_OF_MEMORY_HINT), 20, brls::Application::getTheme()["brls/text"]);
                    hint->setMarginTop(16);
                    box->addView(hint);
                    auto* dialog = new brls::Dialog(box);
                    dialog->addButton("hints/ok"_i18n, []() {});
                    dialog->open(); });
        }
        else
        {
            notify(msg.empty() ? T(S_GAME_EXITED) : msg);
        }
#ifndef __SWITCH__
        // Kịch bản test (J2ME_NX_QUIT): thoát app luôn khi game kết thúc
        if (SDL_getenv("J2ME_NX_QUIT"))
            brls::Application::quit();
#endif
    };
    ScreenActivity::open(hooks);
    return true;
}

void MainActivity::tick()
{
    if (!instance)
        return;
    // Kiểm tra xong, có bản mới: hỏi 1 lần khi đang ở danh sách
    static bool prompted;
    auto stack = brls::Application::getActivitiesStack();
    bool on_top = !stack.empty() && stack.back() == instance;
    if (instance->update_shown != update_available())
        instance->refresh_header();
    if (!prompted && on_top && update_state() == UPDATE_AVAILABLE)
    {
        prompted = true;
        UpdateActivity::open();
    }
}

void MainActivity::shutdown()
{
    if (instance)
        instance->free_icons();
}

} // namespace ui
