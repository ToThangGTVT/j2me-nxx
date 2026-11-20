#include "ui/main_activity.hpp"

#include <cstring>

#include "ui/backup_activity.hpp"
#include "ui/screen_activity.hpp"
#include "ui/settings_activity.hpp"
#include "ui/side_menu.hpp"
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
                instance->open_entry(instance->index_of(((GameCell*)v)->getIndexPath().row));
                return true; }, false, false, brls::SOUND_CLICK);
        // Nút tắt, gợi ý ẩn: các hành động nằm trong Menu (X), cạnh mỗi mục có ghi nút tắt
        bind_action(this, T(S_ACT_OPTIONS), brls::BUTTON_BACK, [](brls::View* v)
            {
                instance->open_options(instance->index_of(((GameCell*)v)->getIndexPath().row));
                return true; }, true);
        bind_action(this, T(S_ACT_DELETE), brls::BUTTON_LB, [](brls::View* v)
            {
                instance->confirm_delete(instance->index_of(((GameCell*)v)->getIndexPath().row));
                return true; }, true);
        bind_action(this, T(S_ACT_GROUPS), brls::BUTTON_LT, [](brls::View* v)
            {
                instance->open_groups(instance->index_of(((GameCell*)v)->getIndexPath().row));
                return true; }, true);
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
        {
            instance->focused = this->getIndexPath().row;
        }
    }

  private:
    GameIcon* icon;
    brls::Label *title, *sub, *size;
};

// Dấu cộng vẽ bằng hai vạch: chữ "+" cỡ to bị font đẩy lệch khỏi giữa dòng
class PlusIcon : public brls::View
{
  public:
    PlusIcon()
    {
        this->setDimensions(18, 18);
    }

    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
              brls::FrameContext* ctx) override
    {
        float t = 3.0f;
        nvgBeginPath(vg);
        nvgRoundedRect(vg, x, y + (height - t) / 2, width, t, t / 2);
        nvgRoundedRect(vg, x + (width - t) / 2, y, t, height, t / 2);
        nvgFillColor(vg, a(color_accent()));
        nvgFill(vg);
    }
};

// Một tab ở hàng trên: Tất cả, một nhóm, hoặc "+" thêm nhóm. Chọn được bằng tay cầm (lên từ danh sách)
class TabButton : public brls::Box
{
  public:
    explicit TabButton(int t)
        : t(t)
    {
        this->setAxis(brls::Axis::COLUMN);
        this->setFocusable(true);
        this->setPadding(4, 16, 0, 16);
        this->setMarginRight(6);
        // Không bóp tab lại: hàng tab cuộn ngang khi quá dài
        this->setCornerRadius(6);
        auto* text = new brls::Box(brls::Axis::ROW);
        text->setAlignItems(brls::AlignItems::CENTER);
        text->setAlignSelf(brls::AlignSelf::CENTER);
        text->setHeight(30);
        // Tab "+": dấu cộng to bên trái chữ
        plus = new PlusIcon();
        plus->setMarginRight(10);
        // Căn theo giữa chữ hoa (hộp chữ có chừa chỗ dấu tiếng Việt phía trên nên tâm hộp thấp hơn)
        plus->setMarginBottom(6);
        plus->setVisibility(brls::Visibility::GONE);
        text->addView(plus);
        label = make_label("", 22, color_dim(), false);
        text->addView(label);
        this->addView(text);
        line = new brls::Box();
        line->setHeight(4);
        line->setCornerRadius(2);
        line->setMarginTop(8);
        line->setAlignSelf(brls::AlignSelf::STRETCH);
        this->addView(line);
        this->addGestureRecognizer(new brls::TapGestureRecognizer(this, [this]()
            { instance->tab_pressed(this->t); }));
    }

    void onFocusGained() override
    {
        brls::Box::onFocusGained();
        if (instance)
            instance->tab_focused(t);
    }

    int t;
    PlusIcon* plus;
    brls::Label* label;
    brls::Box* line;
};

// Hàng tab: đi lên từ danh sách thì vào đúng tab đang mở
class TabRow : public brls::Box
{
  public:
    TabRow()
        : brls::Box(brls::Axis::ROW)
    {
    }

    brls::View* getDefaultFocus() override
    {
        if (instance && instance->tab >= 0 && instance->tab < (int)instance->tabs.size() &&
            instance->tabs[instance->tab]->isFocusable())
            return instance->tabs[instance->tab];
        return brls::Box::getDefaultFocus();
    }
};

// Khung cuộn ngang của hàng tab: tab đang chọn luôn nằm trong màn hình
class TabScroll : public brls::HScrollingFrame
{
  public:
    TabScroll()
    {
        this->setScrollingBehavior(brls::ScrollingBehavior::CENTERED);
        this->setScrollingIndicatorVisible(false);
        // Chỉ các tab nhận focus, khung không tự nhận
        this->setFocusable(false);
    }

    // Hết tab ở hai đầu thì không giữ focus lại ở khung
    brls::View* getNextFocus(brls::FocusDirection direction, brls::View* currentView) override
    {
        return brls::Box::getNextFocus(direction, currentView);
    }

    // Bố cục vừa tính lại (đổi tên, thêm / xoá nhóm): cuộn lại tới tab đang chọn
    // (hoãn sang khung sau: cuộn gọi invalidate(), cuộn ngay trong onLayout sẽ đệ quy mãi)
    void onLayout() override
    {
        brls::HScrollingFrame::onLayout();
        if (pending)
            return;
        pending = true;
        brls::sync([this]()
            {
                pending = false;
                if (instance)
                    instance->scroll_to_tab(false); });
    }

    // Đưa tab vào giữa khung (giới hạn ở hai đầu do HScrollingFrame lo)
    void center(brls::View* tab, bool animated)
    {
        float mid = tab->getLocalX() + tab->getWidth() / 2;
        float to  = mid - this->getWidth() / 2;
        float max = row ? row->getWidth() - this->getWidth() : 0;
        to        = to > max ? max : to;
        to        = to < 0 ? 0 : to;
        // Đã đúng chỗ thì thôi (tránh invalidate thừa)
        if (fabsf(to - this->getContentOffsetX()) > 0.5f)
            this->setContentOffsetX(to, animated);
    }

    brls::View* row = nullptr;  // hàng tab bên trong

  private:
    bool pending = false;
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
        return (int)owner->rows.size();
    }

    brls::RecyclerCell* cellForRow(brls::RecyclerFrame* recycler, brls::IndexPath index) override
    {
        auto* cell   = (GameCell*)recycler->dequeueReusableCell("game");
        int i        = owner->index_of(index.row);
        GameEntry* g = &owner->list.items[i];
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
        owner->open_entry(owner->index_of(index.row));
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

    build_tabs();

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

// Hàng tab trên đầu danh sách: Tất cả (cố định) | các nhóm | +
void MainActivity::build_tabs()
{
    auto* row = new TabRow();
    row->setAlignItems(brls::AlignItems::FLEX_END);
    // Đủ chỗ cho mọi nhóm, ẩn bớt theo số nhóm (không xoá view để khỏi mất focus)
    for (int t = 0; t <= GROUP_MAX + 1; t++)
    {
        auto* b = new TabButton(t);
        tabs.push_back(b);
        row->addView(b);
    }
    tab_add = (int)tabs.size() - 1;

    // Tab nhóm: A về danh sách, L xoá (đổi tên trong Menu); tab +: A thêm nhóm
    for (int t = 0; t < (int)tabs.size(); t++)
    {
        TabButton* b = tabs[t];
        if (t == tab_add)
        {
            b->registerAction(T(S_GROUP_NEW), brls::BUTTON_A, [this](brls::View*)
                {
                    new_group(-1);
                    return true; }, false, false, brls::SOUND_CLICK);
            continue;
        }
        b->registerAction(T(S_ACT_OPEN), brls::BUTTON_A, [this](brls::View*)
            {
                focus_list();
                return true; }, false, false, brls::SOUND_CLICK);
        if (t == 0)
            continue;
        bind_action(b, T(S_GROUP_DELETE), brls::BUTTON_LB, [this, t](brls::View*)
            {
                confirm_delete_group(t - 1);
                return true; }, true);
    }
    tab_scroll = new TabScroll();
    tab_scroll->setMargins(14, 50, 2, 50);
    tab_scroll->setHeight(52);
    tab_scroll->setContentView(row);
    tab_scroll->row = row;
    body->addView(tab_scroll);
}

void MainActivity::refresh_tabs()
{
    if (tabs.empty())
        return;
    int groups = group_count();
    for (int t = 0; t < (int)tabs.size(); t++)
    {
        TabButton* b = tabs[t];
        bool visible = t == tab_add ? groups < GROUP_MAX : t <= groups;
        b->setVisibility(visible ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
        if (!visible)
            continue;
        std::string text;
        if (t == 0)
            text = trf(S_TAB_ALL, list.count);
        else if (t == tab_add)
            text = T(S_GROUP_NEW);
        else
            text = std::string(group_name(t - 1)) + " (" + std::to_string(group_size(t - 1)) + ")";
        set_text(b->label, text);
        bool on = t == tab;
        b->plus->setVisibility(t == tab_add ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
        b->label->setTextColor(on ? brls::Application::getTheme()["brls/text"] : color_dim());
        b->line->setBackgroundColor(on ? color_accent() : nvgRGBA(0, 0, 0, 0));
        // Ngôn ngữ có thể đã đổi
        b->updateActionHint(brls::BUTTON_A, T(t == tab_add ? S_GROUP_NEW : S_ACT_OPEN));
        if (t > 0 && t != tab_add)
            b->updateActionHint(brls::BUTTON_LB, T(S_GROUP_DELETE));
    }
}

void MainActivity::scroll_to_tab(bool animated)
{
    if (tab_scroll && tab >= 0 && tab < (int)tabs.size() && !in_tab_row())
        tab_scroll->center(tabs[tab], animated);
}

int MainActivity::group_size(int g)
{
    int n = 0;
    for (auto& id : ids)
        n += group_has(g, id.c_str());
    return n;
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
    // Thanh dưới chỉ còn A (ô đang chọn), Gửi game và Menu; các nút khác là nút tắt (gợi ý ẩn, ghi trong Menu)
    actions.push_back(bind_action(frame, T(S_ACT_UPLOAD), brls::BUTTON_RB, [this](brls::View*)
        {
            do_upload();
            return true; }));
    actions.push_back(bind_action(frame, T(S_ACT_MENU), brls::BUTTON_X, [this](brls::View*)
        {
            open_menu();
            return true; }));
    actions.push_back(bind_action(frame, T(S_ACT_RESCAN), brls::BUTTON_Y, [this](brls::View*)
        {
            do_rescan();
            return true; }, true));
    actions.push_back(bind_action(frame, T(S_ACT_BACKUP), brls::BUTTON_RT, [this](brls::View*)
        {
            do_backup();
            return true; }, true));
    actions.push_back(bind_action(frame, T(S_ACT_UPDATE), brls::BUTTON_B, [](brls::View*)
        {
            if (update_available())
                UpdateActivity::open();
            return true; }, !update_available()));
    // Đổi tab bằng trái / phải (danh sách dọc không dùng hai hướng này)
    // Đang ở hàng tab thì để borealis tự di chuyển focus giữa các tab
    actions.push_back(frame->registerAction("", brls::BUTTON_NAV_LEFT, [this](brls::View*)
        {
            if (in_tab_row())
                return false;
            switch_tab(tab - 1, true);
            return true; }, true));
    actions.push_back(frame->registerAction("", brls::BUTTON_NAV_RIGHT, [this](brls::View*)
        {
            if (in_tab_row())
                return false;
            switch_tab(tab + 1, true);
            return true; }, true));
    // + vẫn thoát như mọi app Switch
    actions.push_back(bind_action(frame, "hints/exit"_i18n, brls::BUTTON_START, [](brls::View*)
        {
            brls::Application::quit();
            return true; }, true));
}

void MainActivity::do_rescan()
{
    rescan(false);
    notify(trf(S_RESCANNED, list.count));
}

void MainActivity::do_upload()
{
    UploadActivity::open(platform_games_dir(), [](int received)
        {
            app_logf("[upload] dong man hinh Gui game: nhan %d file%s", received,
                received > 0 ? ", quet lai danh sach" : "");
            if (received > 0 && instance)
            {
                instance->rescan(true);
                notify(trf(S_UPLOAD_DONE_STATUS, received));
            } });
}

void MainActivity::do_backup()
{
    // Mở với ứng dụng đang chọn
    char id[256] = "";
    int i        = index_of(focused);
    if (i >= 0 && i < list.count)
        game_list_id(&list.items[i], id, sizeof(id));
    BackupActivity::open(id, []()
        {
            if (instance)
                instance->rescan(true); });
}

// Menu bên phải: ứng dụng đang chọn, nhóm đang mở, các mục chung
void MainActivity::open_menu()
{
    std::vector<MenuSection> sections;
    int i = in_tab_row() ? -1 : index_of(focused);
    if (i >= 0 && i < list.count)
    {
        GameEntry* g = &list.items[i];
        game_list_load_info(g);
        std::string id = ids[i];
        // Chỉ số trong list có thể đổi nếu quét lại: kiểm tra id trước khi chạy
        auto valid = [this, i, id]()
        { return i < list.count && ids[i] == id; };
        sections.push_back({ g->title,
            {
                { T(S_ACT_OPEN), ICON_A, [this, i, valid]()
                    { if (valid()) open_entry(i); } },
                { T(S_ACT_GROUPS), ICON_ZL, [this, i, valid]()
                    { if (valid()) open_groups(i); } },
                { T(S_ACT_OPTIONS), ICON_MINUS, [this, i, valid]()
                    { if (valid()) open_options(i); } },
                { T(S_ACT_BACKUP), ICON_ZR, [this]()
                    { do_backup(); } },
                { T(S_ACT_DELETE), ICON_L, [this, i, valid]()
                    { if (valid()) confirm_delete(i); }, true },
            } });
    }
    if (tab > 0 && tab <= group_count())
    {
        int gr = tab - 1;
        sections.push_back({ group_name(gr),
            {
                { T(S_GROUP_RENAME), "", [this, gr]()
                    { rename_group(gr); } },
                { T(S_GROUP_DELETE), "", [this, gr]()
                    { confirm_delete_group(gr); }, true },
            } });
    }
    std::vector<MenuItem> general = {
        { T(S_GROUP_NEW), "", [this]()
            { new_group(-1); } },
        { T(S_SETTINGS), "", []()
            { SettingsActivity::open_global(); } },
        { T(S_ACT_RESCAN), ICON_Y, [this]()
            { do_rescan(); } },
    };
    if (update_available())
        general.push_back({ T(S_ACT_UPDATE), ICON_B, []()
            { UpdateActivity::open(); } });
    general.push_back({ T(S_ACT_EXIT), ICON_PLUS, []()
        { brls::Application::quit(); } });
    sections.push_back({ T(S_MENU_GENERAL), general });
    SideMenu::open(T(S_ACT_MENU), sections);
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
    refresh_tabs();
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
    free_icons();
    game_list_free(&list);
    game_list_scan(&list, platform_games_dir());
    ids.assign(list.count, "");
    for (int i = 0; i < list.count; i++)
    {
        char id[256];
        game_list_id(&list.items[i], id, sizeof(id));
        ids[i] = id;
    }
    apply_filter(keep, true);
}

// Chữ khi danh sách của tab rỗng: chưa có ứng dụng nào, hoặc nhóm chưa có ứng dụng
void MainActivity::fill_empty()
{
    empty->clearViews();
    auto add = [this](const std::string& text, float size, NVGcolor color, float top)
    {
        auto* l = make_label(text, size, color);
        l->setHorizontalAlign(brls::HorizontalAlign::CENTER);
        l->setMarginTop(top);
        empty->addView(l);
    };
    if (list.count == 0)
    {
        // Hướng dẫn chép ứng dụng vào thư mục games
        empty->addView(make_label(T(S_EMPTY_TITLE), 34, brls::Application::getTheme()["brls/text"]));
        add(T(S_EMPTY_COPY), 22, color_dim(), 28);
        add(std::string(platform_games_dir()) + "/", 28, color_accent(), 14);
        add(T(S_EMPTY_SUBDIR), 20, color_dim(), 28);
        add(T(S_EMPTY_RESCAN), 22, color_warn(), 28);
        add(T(S_EMPTY_UPLOAD), 22, color_warn(), 8);
        return;
    }
    empty->addView(make_label(T(S_GROUP_EMPTY_TITLE), 34, brls::Application::getTheme()["brls/text"]));
    add(T(S_GROUP_EMPTY_HINT), 22, color_dim(), 28);
}

void MainActivity::apply_filter(bool keep, bool focus)
{
    if (tab > group_count())
        tab = group_count();
    int old = focused;
    std::vector<int> prev = rows;
    bool was_list         = !in_tab_row();
    rows.clear();
    for (int i = 0; i < list.count; i++)
    {
        if (tab == 0 || group_has(tab - 1, ids[i].c_str()))
            rows.push_back(i);
    }
    // Chỉ đổi nhóm của một ứng dụng mà danh sách không đổi: giữ nguyên các ô (và focus)
    if (!focus && keep && rows == prev && !rows.empty())
    {
        refresh_header();
        return;
    }
    int n    = (int)rows.size();
    bool has = n > 0;
    if (!has)
        fill_empty();
    recycler->setVisibility(has ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
    empty->setVisibility(has ? brls::Visibility::GONE : brls::Visibility::VISIBLE);
    focused = keep && has ? (old < n ? old : n - 1) : 0;
    recycler->setDefaultCellFocus(brls::IndexPath(0, focused));
    recycler->reloadData();
    refresh_header();
    // Dòng đang chọn vừa bị bỏ khỏi nhóm: ô cũ đã bị dựng lại, đưa focus về danh sách
    if (focus || (keep && was_list && !prev.empty()))
        focus_list();
}

void MainActivity::focus_list()
{
    // Lần đầu danh sách chưa được dàn trang (chưa có ô nào): trao focus sau khi vẽ xong khung hình này
    // Hộp chọn vừa đóng có thể còn trên stack (đang mờ dần) nhưng focus đã trả về màn hình này
    brls::sync([this]()
        {
            brls::View* cur = brls::Application::getCurrentFocus();
            if (cur && cur->getParentActivity() != this)
                return;
            brls::Application::giveFocus(rows.empty() ? empty : (brls::View*)recycler); });
}

// Focus tab t sau khi hàng tab đã cập nhật (tab mới / tab sau khi xoá)
void MainActivity::focus_tab(int t)
{
    brls::sync([this, t]()
        {
            auto stack = brls::Application::getActivitiesStack();
            if (stack.empty() || stack.back() != this || t < 0 || t >= (int)tabs.size())
                return;
            brls::Application::giveFocus(tabs[t]); });
}

bool MainActivity::in_tab_row()
{
    brls::View* v = brls::Application::getCurrentFocus();
    for (auto* b : tabs)
    {
        if (v == b)
            return true;
    }
    return false;
}

void MainActivity::switch_tab(int to, bool focus)
{
    if (to < 0 || to > group_count() || to == tab)
        return;
    if (tab < (int)tab_focus.size())
        tab_focus[tab] = focused;
    tab     = to;
    focused = tab < (int)tab_focus.size() ? tab_focus[tab] : 0;
    apply_filter(true, focus);
    scroll_to_tab(true);
}

void MainActivity::tab_focused(int t)
{
    if (t != tab_add)
        switch_tab(t, false);
}

void MainActivity::tab_pressed(int t)
{
    if (t == tab_add)
        new_group(-1);
    else
        switch_tab(t, true);
}

// Nhập tên nhóm mới; index >= 0: thêm luôn ứng dụng đó vào nhóm
void MainActivity::new_group(int index)
{
    if (group_count() >= GROUP_MAX)
    {
        notify(trf(S_GROUP_FULL, GROUP_MAX));
        return;
    }
    std::string game = index >= 0 && index < list.count ? ids[index] : "";
    brls::Application::getImeManager()->openForText([this, game, index](std::string name)
        {
            int g = group_add(name.c_str());
            if (g < 0)
                return;
            if (!game.empty())
            {
                group_set(g, game.c_str(), true);
                if (index < list.count && ids[index] == game)
                    notify(trf(S_GROUP_ADDED, list.items[index].title, group_name(g)));
                apply_filter(true, false);
                return;
            }
            switch_tab(g + 1, false);
            refresh_tabs();
            focus_tab(g + 1); },
        T(S_GROUP_NEW), T(S_GROUP_NAME_HINT), 24, "");
}

void MainActivity::rename_group(int g)
{
    if (g < 0 || g >= group_count())
        return;
    brls::Application::getImeManager()->openForText([this, g](std::string name)
        {
            if (group_rename(g, name.c_str()))
                refresh_tabs(); },
        T(S_GROUP_RENAME), T(S_GROUP_NAME_HINT), 24, group_name(g));
}

void MainActivity::confirm_delete_group(int g)
{
    if (g < 0 || g >= group_count())
        return;
    auto* box = new brls::Box(brls::Axis::COLUMN);
    box->setPadding(36, 40, 24, 40);
    box->addView(make_label(T(S_GROUP_DELETE), 20, color_warn()));
    auto* name = make_label(group_name(g), 30, brls::Application::getTheme()["brls/text"]);
    name->setMarginTop(10);
    box->addView(name);
    auto* info = make_label(T(S_GROUP_DELETE_INFO), 18, color_dim());
    info->setMarginTop(16);
    box->addView(info);

    auto* dialog = new brls::Dialog(box);
    dialog->addButton(T(S_ACT_CANCEL), []() {});
    dialog->addButton(T(S_ACT_DELETE), [this, g]()
        {
            if (!group_delete(g))
                return;
            tab_focus.assign(GROUP_MAX + 1, 0);
            // Về tab đứng trước nhóm vừa xoá
            tab     = g;
            focused = 0;
            apply_filter(false, false);
            focus_tab(g); });
    dialog->open();
}

// ZL ở một ứng dụng: thêm / bỏ khỏi từng nhóm, hoặc tạo nhóm mới chứa nó
void MainActivity::open_groups(int index)
{
    if (index < 0 || index >= list.count)
        return;
    GameEntry* g = &list.items[index];
    game_list_load_info(g);
    std::vector<std::string> names;
    int count = group_count();
    for (int k = 0; k < count; k++)
        names.push_back(trf(group_has(k, ids[index].c_str()) ? S_GROUP_REMOVE_FROM : S_GROUP_ADD_TO, group_name(k)));
    if (count < GROUP_MAX)
        names.push_back("+ " + T(S_GROUP_NEW));
    std::string game = ids[index];
    auto* dropdown   = new brls::Dropdown(trf(S_GROUP_PICK_TITLE, g->title), names, [this, index, count, game](int selected)
        {
            if (selected < 0)
                return;
            // Chạy sau khi hộp chọn đóng hẳn
            brls::sync([this, index, count, game, selected]()
                {
                    if (index >= list.count || ids[index] != game)
                        return;
                    if (selected >= count)
                    {
                        new_group(index);
                        return;
                    }
                    bool on = !group_has(selected, game.c_str());
                    if (!group_set(selected, game.c_str(), on))
                        app_logf("Khong ghi duoc groups.ini");
                    notify(trf(on ? S_GROUP_ADDED : S_GROUP_REMOVED, list.items[index].title, group_name(selected)));
                    // Trong tab nhóm: dòng vừa bỏ biến mất
                    apply_filter(true, false);
                }); }, -1);
    brls::Application::pushActivity(new brls::Activity(dropdown));
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
    app_logf("Mo game %s (MIDlet %d)", path, midlet);
    if (!emu_start(path, game_id, midlet, err, sizeof(err)))
    {
        app_logf("Khong mo duoc game: %s", err);
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
