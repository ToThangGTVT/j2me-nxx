#include "ui/keybind_activity.hpp"

namespace ui
{

KeybindActivity::KeybindActivity(int* binds, const int* global, const std::string& subtitle, std::function<void()> done)
    : binds(binds)
    , global(global)
    , subtitle(subtitle)
    , done(std::move(done))
{
}

void KeybindActivity::open(int* binds, const int* global, const std::string& subtitle, std::function<void()> done)
{
    brls::Application::pushActivity(new KeybindActivity(binds, global, subtitle, std::move(done)));
}

// Các lựa chọn của nút b: [Mặc định (phím của cài đặt chung) khi chỉnh tuỳ chọn riêng] + các phím điện thoại
std::vector<std::string> KeybindActivity::choices(int b)
{
    std::vector<std::string> data;
    if (global)
        data.push_back(trf(S_DEFAULT_FMT, keybind_key_name(global[b])));
    for (int i = 0; i < keybind_key_count(); i++)
        data.push_back(keybind_key_name(keybind_key_at(i)));
    return data;
}

int KeybindActivity::selection(int b)
{
    if (global && binds[b] == BIND_INHERIT)
        return 0;
    int i = keybind_key_index(binds[b]);
    return (i < 0 ? 0 : i) + (global ? 1 : 0);
}

void KeybindActivity::refresh()
{
    for (int b = 0; b < BIND_COUNT && b < (int)cells.size(); b++)
        cells[b]->setSelection(selection(b), true);
    if (done)
        done();
}

brls::View* KeybindActivity::createContentView()
{
    auto* scroll = new brls::ScrollingFrame();
    scroll->setScrollingBehavior(brls::ScrollingBehavior::CENTERED);
    auto* box    = new brls::Box(brls::Axis::COLUMN);
    box->setPadding(24, 80, 40, 80);
    scroll->setContentView(box);

    auto* info = make_label(T(global ? S_KEYBIND_INFO_APP : S_KEYBIND_INFO), 18, color_dim());
    info->setMarginBottom(20);
    box->addView(info);

    for (int b = 0; b < BIND_COUNT; b++)
    {
        auto* cell = new brls::SelectorCell();
        cell->init(keybind_button_name((BindButton)b), choices(b), selection(b), [this, b](int i)
            {
                if (global && i == 0)
                    binds[b] = BIND_INHERIT;
                else
                    binds[b] = keybind_key_at(i - (global ? 1 : 0));
                if (done)
                    done(); });
        // Y: nút này về mặc định
        bind_action(cell, T(S_ACT_DEFAULT), brls::BUTTON_Y, [this, b](brls::View*)
            {
                binds[b] = global ? BIND_INHERIT : keybind_default((BindButton)b);
                refresh();
                return true; });
        cells.push_back(cell);
        box->addView(cell);
    }

    auto* frame = new brls::AppletFrame(scroll);
    frame->setTitle(T(S_KEYBIND) + "  -  " + subtitle);
    // X: tất cả về mặc định
    bind_action(frame, T(S_ACT_RESET_ALL), brls::BUTTON_X, [this](brls::View*)
        {
            for (int b = 0; b < BIND_COUNT; b++)
                binds[b] = global ? BIND_INHERIT : keybind_default((BindButton)b);
            refresh();
            return true; });
    return frame;
}

} // namespace ui
