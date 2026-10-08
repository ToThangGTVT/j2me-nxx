#include "ui/settings_activity.hpp"

#include <cstring>

#include "ui/keybind_activity.hpp"
#include "ui/main_activity.hpp"
#include "ui/screen_activity.hpp"

namespace ui
{

// Khung xem trước tỉ lệ màn hình của ứng dụng
class RatioPreview : public brls::View
{
  public:
    RatioPreview()
    {
        this->setDimensions(240, 360);
    }

    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
              brls::FrameContext* ctx) override
    {
        if (w <= 0 || h <= 0)
            return;
        float s = width / w < height / h ? width / w : height / h;
        float pw = w * s, ph = h * s;
        float px = x + (width - pw) / 2, py = y + (height - ph) / 2;
        nvgBeginPath(vg);
        nvgRoundedRect(vg, px - 3, py - 3, pw + 6, ph + 6, 4);
        nvgFillColor(vg, color_accent());
        nvgFill(vg);
        nvgBeginPath(vg);
        nvgRect(vg, px, py, pw, ph);
        nvgFillColor(vg, nvgRGB(0x10, 0x11, 0x14));
        nvgFill(vg);
        char label[32];
        snprintf(label, sizeof(label), "%dx%d", w, h);
        nvgFontFaceId(vg, brls::Application::getDefaultFont());
        nvgFontSize(vg, 20);
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(vg, color_dim());
        nvgText(vg, px + pw / 2, py + ph / 2, label, nullptr);
    }

    int w = 0, h = 0;
};

static std::string fps_text(int fps)
{
    return fps > 0 ? std::to_string(fps) + " FPS" : T(S_UNLIMITED);
}

static std::string on_off(bool on)
{
    return T(on ? S_ON : S_OFF);
}

// Một mục: ô cài đặt + chú thích nhỏ bên dưới
static brls::Box* add_item(brls::Box* tab, brls::View* cell, const std::string& hint)
{
    auto* item = new brls::Box(brls::Axis::COLUMN);
    item->addView(cell);
    if (!hint.empty())
    {
        auto* l = make_label(hint, 16, color_dim());
        l->setMargins(6, 16, 18, 16);
        item->addView(l);
    }
    else
    {
        item->setMarginBottom(18);
    }
    tab->addView(item);
    return item;
}

static brls::ScrollingFrame* new_tab(brls::Box** content)
{
    auto* scroll = new brls::ScrollingFrame();
    scroll->setScrollingBehavior(brls::ScrollingBehavior::CENTERED);
    auto* box    = new brls::Box(brls::Axis::COLUMN);
    box->setPadding(28, 40, 40, 40);
    scroll->setContentView(box);
    *content = box;
    return scroll;
}

SettingsActivity::SettingsActivity(const std::string& id, const std::string& title, int tab)
    : game_mode(!id.empty())
    , game_id(id)
    , game_title(title)
    , start_tab(tab)
{
    if (game_mode)
        game_settings_load(game_id.c_str(), &game);
    sync_custom();
}

SettingsActivity::~SettingsActivity()
{
    // Vừa bật kiểm tra bản mới: kiểm tra luôn
    if (!game_mode && settings()->check_update && update_state() == UPDATE_IDLE)
        update_check();
}

void SettingsActivity::open_global()
{
    brls::Application::pushActivity(new SettingsActivity("", ""));
}

void SettingsActivity::open_game(const char* id, const char* title)
{
    brls::Application::pushActivity(new SettingsActivity(id, title));
}

void SettingsActivity::open_keybinds_global()
{
    KeybindActivity::open(settings()->keybinds, nullptr, T(S_KEYBIND_GLOBAL), []()
        { settings_save(); });
}

void SettingsActivity::save()
{
    if (game_mode)
        game_settings_save(game_id.c_str(), &game);
    else
        settings_save();
}

int* SettingsActivity::cur_w()
{
    return game_mode ? &game.screen_w : &settings()->screen_w;
}

int* SettingsActivity::cur_h()
{
    return game_mode ? &game.screen_h : &settings()->screen_h;
}

bool SettingsActivity::is_auto()
{
    return game_mode && *cur_w() == 0;
}

// Vị trí trong danh sách cỡ có sẵn (bất kể hướng), -1 nếu không khớp
static int preset_index(int w, int h)
{
    int a = w < h ? w : h, b = w < h ? h : w;
    for (int i = 0; i < SETTINGS_SCREEN_CHOICE_COUNT; i++)
    {
        if (SETTINGS_SCREEN_CHOICES[i].w == a && SETTINGS_SCREEN_CHOICES[i].h == b)
            return i;
    }
    return -1;
}

void SettingsActivity::sync_custom()
{
    custom = !is_auto() && preset_index(*cur_w(), *cur_h()) < 0;
}

static int clamp_size(long v)
{
    return v < SCREEN_MIN ? SCREEN_MIN : v > SCREEN_MAX ? SCREEN_MAX : (int)v;
}

void SettingsActivity::update_size_items()
{
    int w = *cur_w(), h = *cur_h();
    if (orient_item)
        orient_item->setVisibility(is_auto() ? brls::Visibility::GONE : brls::Visibility::VISIBLE);
    if (orient_cell)
    {
        char v[64];
        snprintf(v, sizeof(v), "%s  (%d x %d)", tr(w == h ? S_SQUARE : w < h ? S_PORTRAIT : S_LANDSCAPE), w, h);
        orient_cell->setDetailText(v);
    }
    brls::Visibility custom_vis = custom && !is_auto() ? brls::Visibility::VISIBLE : brls::Visibility::GONE;
    if (width_item)
        width_item->setVisibility(custom_vis);
    if (height_item)
        height_item->setVisibility(custom_vis);
    if (preview)
    {
        preview->w = is_auto() ? settings()->screen_w : w;
        preview->h = is_auto() ? settings()->screen_h : h;
    }
}

void SettingsActivity::update_smooth_item()
{
    // Font hệ thống luôn mịn: ẩn mục chữ mịn
    bool sys = game_mode && game.system_font >= 0 ? game.system_font == 1 : settings()->system_font;
    if (smooth_item)
        smooth_item->setVisibility(sys ? brls::Visibility::GONE : brls::Visibility::VISIBLE);
}

std::string SettingsActivity::keybind_detail()
{
    int changed = keybind_changed(game_mode ? game.keybinds : settings()->keybinds, game_mode);
    if (changed)
        return trf(game_mode ? S_KEYBIND_OWN : S_KEYBIND_CHANGED, changed);
    return T(game_mode ? S_KEYBIND_INHERIT : S_KEYBIND_DEFAULT);
}

std::string SettingsActivity::vpad_layout_detail()
{
    VpadLayout* l = &settings()->vpad_layout;
    if (vpad_layout_changed(l))
        return trf(S_VPAD_CUSTOMIZED, vpad_style_name(l->style));
    return vpad_style_name(l->style);
}

brls::Box* SettingsActivity::add_toggle(brls::Box* tab, StrId title, StrId hint, bool* global_value, int* game_value)
{
    if (!game_mode)
    {
        auto* cell = new brls::BooleanCell();
        cell->init(T(title), *global_value, [this, global_value](bool on)
            {
                *global_value = on;
                save();
                update_smooth_item(); });
        return add_item(tab, cell, T(hint));
    }
    // Mặc định -> Bật -> Tắt
    auto* cell = new brls::SelectorCell();
    std::vector<std::string> data = { trf(S_DEFAULT_FMT, on_off(*global_value).c_str()), T(S_ON), T(S_OFF) };
    int sel                       = *game_value < 0 ? 0 : *game_value == 1 ? 1 : 2;
    cell->init(T(title), data, sel, [this, game_value](int i)
        {
            *game_value = i == 0 ? -1 : i == 1 ? 1 : 0;
            save();
            update_smooth_item(); });
    return add_item(tab, cell, T(hint));
}

brls::View* SettingsActivity::tab_screen()
{
    brls::Box* tab;
    auto* scroll = new_tab(&tab);
    // Cột trái: các mục; cột phải: xem trước tỉ lệ
    tab->setAxis(brls::Axis::ROW);
    tab->setAlignItems(brls::AlignItems::FLEX_START);
    auto* col = new brls::Box(brls::Axis::COLUMN);
    col->setGrow(1.0f);
    col->setShrink(1.0f);
    tab->addView(col);
    preview = new RatioPreview();
    preview->setMargins(20, 0, 0, 40);
    tab->addView(preview);
    Settings* s = settings();

    // Giới hạn FPS: [Mặc định (chỉ ứng dụng)] + các mức
    {
        std::vector<std::string> data;
        if (game_mode)
            data.push_back(trf(S_DEFAULT_FMT, fps_text(s->fps_limit).c_str()));
        int sel = 0;
        int cur = game_mode ? game.fps_limit : s->fps_limit;
        for (int i = 0; i < SETTINGS_FPS_CHOICE_COUNT; i++)
        {
            if (SETTINGS_FPS_CHOICES[i] == cur && !(game_mode && cur < 0))
                sel = (int)data.size();
            data.push_back(fps_text(SETTINGS_FPS_CHOICES[i]));
        }
        auto* cell = new brls::SelectorCell();
        cell->init(T(S_FPS_LIMIT), data, sel, [this](int i)
            {
                int base = game_mode ? 1 : 0;
                int v    = game_mode && i == 0 ? -1 : SETTINGS_FPS_CHOICES[i - base];
                if (game_mode)
                    game.fps_limit = v;
                else
                    settings()->fps_limit = v;
                save(); });
        add_item(col, cell, T(S_FPS_HINT));
    }

    // Kích thước: [Tự động (chỉ ứng dụng)] + các cỡ có sẵn + Tuỳ chỉnh
    {
        std::vector<std::string> data;
        if (game_mode)
            data.push_back(T(S_AUTO));
        for (int i = 0; i < SETTINGS_SCREEN_CHOICE_COUNT; i++)
            data.push_back(std::to_string(SETTINGS_SCREEN_CHOICES[i].w) + " x " +
                           std::to_string(SETTINGS_SCREEN_CHOICES[i].h));
        data.push_back(T(S_CUSTOM));
        int base = game_mode ? 1 : 0;
        int n    = (int)data.size();
        int sel  = is_auto() ? 0 : custom ? n - 1 : base + preset_index(*cur_w(), *cur_h());
        auto* cell = new brls::SelectorCell();
        cell->init(T(game_mode ? S_SCREEN_SIZE : S_SCREEN_SIZE_DEFAULT), data, sel, [this, base, n](int i)
            {
                bool landscape = *cur_w() > *cur_h();
                if (game_mode && i == 0)
                {
                    *cur_w() = *cur_h() = 0;
                    custom   = false;
                }
                else if (i == n - 1)
                {
                    // Tuỳ chỉnh: bắt đầu từ cỡ đang có
                    if (is_auto())
                    {
                        *cur_w() = settings()->screen_w;
                        *cur_h() = settings()->screen_h;
                    }
                    custom = true;
                }
                else
                {
                    ScreenSize sz = SETTINGS_SCREEN_CHOICES[i - base];
                    *cur_w()      = landscape ? sz.h : sz.w;
                    *cur_h()      = landscape ? sz.w : sz.h;
                    custom        = false;
                }
                if (width_cell)
                    width_cell->setValue(*cur_w());
                if (height_cell)
                    height_cell->setValue(*cur_h());
                save();
                update_size_items(); });
        add_item(col, cell, T(game_mode ? S_SCREEN_SIZE_HINT_GAME : S_SCREEN_SIZE_HINT));
    }

    orient_cell = new brls::DetailCell();
    orient_cell->setText(T(S_ORIENTATION));
    orient_cell->registerClickAction([this](brls::View*)
        {
            int w    = *cur_w();
            *cur_w() = *cur_h();
            *cur_h() = w;
            if (width_cell)
                width_cell->setValue(*cur_w());
            if (height_cell)
                height_cell->setValue(*cur_h());
            save();
            update_size_items();
            return true; });
    orient_item = add_item(col, orient_cell, T(S_ORIENT_HINT));

    auto number_cell = [this](StrId title, StrId kb_title, int* (SettingsActivity::*field)())
    {
        auto* cell = new brls::InputNumericCell();
        cell->init(T(title), *(this->*field)(), [this, cell, field](long v)
            {
                int c = clamp_size(v);
                if (*(this->*field)() == c && c == v)
                    return;
                *(this->*field)() = c;
                save();
                update_size_items();
                // Ngoài khoảng cho phép: hiện lại giá trị đã chỉnh
                if (c != v)
                    brls::sync([cell, c]()
                        { cell->setValue(c); }); },
            T(kb_title), 4);
        return cell;
    };
    width_cell  = number_cell(S_WIDTH, S_KB_WIDTH, &SettingsActivity::cur_w);
    width_item  = add_item(col, width_cell, T(S_SIZE_INPUT_HINT));
    height_cell = number_cell(S_HEIGHT, S_KB_HEIGHT, &SettingsActivity::cur_h);
    height_item = add_item(col, height_cell, T(S_SIZE_INPUT_HINT));

    if (!game_mode)
    {
        auto* cell = new brls::SelectorCell();
        cell->init(T(S_SCALE_MODE), { T(S_SCALE_SMOOTH), T(S_SCALE_SHARP), T(S_SCALE_INTEGER) }, s->scale_mode,
            [this](int i)
            {
                settings()->scale_mode = i;
                save(); });
        add_item(col, cell, T(S_SCALE_HINT));
        add_toggle(col, S_SHOW_FPS, S_SHOW_FPS_HINT, &s->show_fps, nullptr);
    }
    update_size_items();
    return scroll;
}

brls::View* SettingsActivity::tab_controls()
{
    brls::Box* tab;
    auto* scroll = new_tab(&tab);
    Settings* s  = settings();

    {
        std::vector<std::string> data;
        if (game_mode)
            data.push_back(trf(S_DEFAULT_FMT, keymap_get(s->keymap)->name));
        for (int i = 0; i < KEYMAP_COUNT; i++)
            data.push_back(keymap_get(i)->name);
        int sel    = game_mode ? game.keymap + 1 : s->keymap;
        auto* cell = new brls::SelectorCell();
        cell->init(T(S_KEYMAP), data, sel < 0 ? 0 : sel, [this](int i)
            {
                if (game_mode)
                    game.keymap = i - 1;
                else
                    settings()->keymap = i;
                save(); });
        add_item(tab, cell, T(S_KEYMAP_HINT));
    }

    keybind_cell = new brls::DetailCell();
    keybind_cell->setText(T(S_KEYBIND));
    keybind_cell->setDetailText(keybind_detail());
    keybind_cell->registerClickAction([this](brls::View*)
        {
            auto done = [this]()
            {
                save();
                if (keybind_cell)
                    keybind_cell->setDetailText(keybind_detail());
            };
            if (game_mode)
                KeybindActivity::open(game.keybinds, settings()->keybinds, game_title, done);
            else
                KeybindActivity::open(settings()->keybinds, nullptr, T(S_KEYBIND_GLOBAL), done);
            return true; });
    add_item(tab, keybind_cell, T(game_mode ? S_KEYBIND_HINT_APP : S_KEYBIND_HINT));

    add_toggle(tab, S_VPAD, game_mode ? S_VPAD_HINT_APP : S_VPAD_HINT, &s->vpad, &game.vpad);

    if (!game_mode)
    {
        vpad_layout_cell = new brls::DetailCell();
        vpad_layout_cell->setText(T(S_VPAD_LAYOUT));
        vpad_layout_cell->setDetailText(vpad_layout_detail());
        vpad_layout_cell->registerClickAction([this](brls::View*)
            {
                vpad_screen_open(&settings()->vpad_layout);
                ScreenHooks hooks;
                hooks.event  = [](const SDL_Event* e)
                { vpad_screen_handle_event(e); };
                hooks.update = []()
                { return vpad_screen_update(); };
                hooks.draw   = []()
                { vpad_screen_draw(); };
                hooks.closed = [this]()
                {
                    settings_save();
                    if (vpad_layout_cell)
                        vpad_layout_cell->setDetailText(vpad_layout_detail());
                };
                ScreenActivity::open(hooks);
                return true; });
        add_item(tab, vpad_layout_cell, T(S_VPAD_LAYOUT_HINT));
        add_toggle(tab, S_VKB_BUBBLE, S_VKB_BUBBLE_HINT, &s->vkb_bubble, nullptr);
        add_toggle(tab, S_SHOW_HELP, S_SHOW_HELP_HINT, &s->show_help, nullptr);
    }
    return scroll;
}

brls::View* SettingsActivity::tab_text()
{
    brls::Box* tab;
    auto* scroll = new_tab(&tab);
    Settings* s  = settings();

    // Cỡ chữ: [Mặc định (chỉ ứng dụng)] + các mức %
    {
        std::vector<std::string> data;
        if (game_mode)
            data.push_back(trf(S_DEFAULT_FMT, (std::to_string(s->font_scale) + "%").c_str()));
        int cur = game_mode ? game.font_scale : s->font_scale;
        int sel = game_mode ? 0 : -1;
        for (int i = 0; i < SETTINGS_FONT_SCALE_CHOICE_COUNT; i++)
        {
            if (SETTINGS_FONT_SCALE_CHOICES[i] == cur)
                sel = (int)data.size();
            data.push_back(std::to_string(SETTINGS_FONT_SCALE_CHOICES[i]) + "%");
        }
        if (sel < 0)    // giá trị lạ trong file ini: về 100%
        {
            for (int i = 0; i < SETTINGS_FONT_SCALE_CHOICE_COUNT; i++)
                if (SETTINGS_FONT_SCALE_CHOICES[i] == 100)
                    sel = i;
        }
        auto* cell = new brls::SelectorCell();
        cell->init(T(S_FONT_SCALE), data, sel < 0 ? 0 : sel, [this](int i)
            {
                int base = game_mode ? 1 : 0;
                int v    = game_mode && i == 0 ? -1 : SETTINGS_FONT_SCALE_CHOICES[i - base];
                if (game_mode)
                    game.font_scale = v;
                else
                    settings()->font_scale = v;
                save(); });
        add_item(tab, cell, T(S_FONT_SCALE_HINT));
    }
    add_toggle(tab, S_SYSTEM_FONT, S_SYSTEM_FONT_HINT, &s->system_font, &game.system_font);
    smooth_item = add_toggle(tab, S_SMOOTH_TEXT, S_SMOOTH_TEXT_HINT, &s->smooth_text, &game.smooth_text);
    update_smooth_item();

    if (!game_mode)
    {
        // SoundFont: [Tắt] [Tự động] [Có sẵn] + các file .sf2
        static char soundfonts[SOUNDFONT_MAX][128];
        int count = settings_list_soundfonts(soundfonts, SOUNDFONT_MAX);
        std::vector<std::string> data = { T(S_SOUNDFONT_NONE),
            trf(S_SOUNDFONT_AUTO, count ? soundfonts[0] : "TimGM6mb"), T(S_SOUNDFONT_BUILTIN) };
        static const char* fixed[] = { "-", "", "builtin" };
        int sel                    = 1;
        for (int i = 0; i < 3; i++)
            if (strcmp(s->soundfont, fixed[i]) == 0)
                sel = i;
        for (int i = 0; i < count; i++)
        {
            if (strcmp(s->soundfont, soundfonts[i]) == 0)
                sel = 3 + i;
            data.push_back(soundfonts[i]);
        }
        auto* cell = new brls::SelectorCell();
        cell->init(T(S_SOUNDFONT), data, sel, [this](int i)
            {
                snprintf(settings()->soundfont, sizeof(settings()->soundfont), "%s",
                         i < 3 ? fixed[i] : soundfonts[i - 3]);
                save(); });
        add_item(tab, cell, T(S_SOUNDFONT_HINT));
    }
    return scroll;
}

brls::View* SettingsActivity::tab_system()
{
    brls::Box* tab;
    auto* scroll = new_tab(&tab);
    Settings* s  = settings();

    if (aot_available())
    {
        add_toggle(tab, S_AOT, game_mode ? S_AOT_HINT_APP : S_AOT_HINT, &s->aot, &game.aot);
    }
    else
    {
        auto* cell = new brls::DetailCell();
        cell->setText(T(S_AOT));
        cell->setDetailText(T(S_AOT_UNSUPPORTED));
        add_item(tab, cell, T(game_mode ? S_AOT_HINT_APP : S_AOT_HINT));
    }

    if (!game_mode)
    {
        add_toggle(tab, S_CHECK_UPDATE, S_CHECK_UPDATE_HINT, &s->check_update, nullptr);

        std::vector<std::string> data;
        for (int i = 0; i < LANG_COUNT; i++)
            data.push_back(lang_name((Lang)i));
        auto* cell = new brls::SelectorCell();
        cell->init(T(S_LANGUAGE), data, s->lang, [](int i) {}, [](int i)
            {
                if (i == settings()->lang)
                    return;
                settings()->lang = i;
                lang_set((Lang)i);
                settings_save();
                // Dựng lại màn hình bằng ngôn ngữ mới, giữ ở tab Hệ thống
                brls::Application::popActivity(brls::TransitionAnimation::NONE, []()
                    {
                        brls::Application::pushActivity(new SettingsActivity("", "", 3),
                                                         brls::TransitionAnimation::NONE);
                        if (MainActivity* m = MainActivity::get())
                            m->relabel(); });
            });
        add_item(tab, cell, T(S_LANGUAGE_HINT) + "\n" + T(S_LANG_RESTART));
    }
    return scroll;
}

brls::View* SettingsActivity::createContentView()
{
    auto* tabs = new brls::TabFrame();
    tabs->addTab(T(S_SEC_SCREEN), [this]()
        {
            orient_item = width_item = height_item = nullptr;
            orient_cell = nullptr;
            width_cell = height_cell = nullptr;
            preview = nullptr;
            smooth_item = nullptr;
            keybind_cell = vpad_layout_cell = nullptr;
            return tab_screen(); });
    tabs->addTab(T(S_SEC_CONTROLS), [this]()
        {
            orient_item = width_item = height_item = nullptr;
            orient_cell = nullptr;
            width_cell = height_cell = nullptr;
            preview = nullptr;
            smooth_item = nullptr;
            return tab_controls(); });
    tabs->addTab(T(S_SEC_TEXT), [this]()
        {
            orient_item = width_item = height_item = nullptr;
            orient_cell = nullptr;
            width_cell = height_cell = nullptr;
            preview = nullptr;
            keybind_cell = vpad_layout_cell = nullptr;
            return tab_text(); });
    tabs->addTab(T(S_SEC_SYSTEM), [this]()
        {
            orient_item = width_item = height_item = nullptr;
            orient_cell = nullptr;
            width_cell = height_cell = nullptr;
            preview = nullptr;
            smooth_item = nullptr;
            keybind_cell = vpad_layout_cell = nullptr;
            return tab_system(); });

    auto* frame = new brls::AppletFrame(tabs);
    frame->setTitle(game_mode ? T(S_GAME_OPTIONS) + "  -  " + game_title : T(S_SETTINGS));
    if (start_tab > 0)
        brls::sync([tabs, tab = start_tab]()
            { tabs->focusTab(tab); });
    return frame;
}

} // namespace ui
