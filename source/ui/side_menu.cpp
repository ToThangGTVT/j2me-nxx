#include "ui/side_menu.hpp"

using namespace brls::literals;

namespace ui
{

#define PANEL_W  460
#define SLIDE_PX 80

SideMenu::SideMenu(const std::string& title, std::vector<MenuSection> sections)
    : sections(std::move(sections))
{
    this->setAxis(brls::Axis::ROW);
    this->setJustifyContent(brls::JustifyContent::FLEX_END);
    this->setWidth(brls::Application::contentWidth);
    this->setHeight(brls::Application::contentHeight);
    this->setBackgroundColor(nvgRGBA(0, 0, 0, 150));

    // Chạm ra ngoài panel thì đóng
    auto* outside = new brls::Box();
    outside->setGrow(1.0f);
    outside->addGestureRecognizer(new brls::TapGestureRecognizer(outside, [this]()
        { close(nullptr); }));
    this->addView(outside);

    panel = new brls::Box(brls::Axis::COLUMN);
    panel->setWidth(PANEL_W);
    panel->setHeight(brls::Application::contentHeight);
    panel->setBackgroundColor(brls::Application::getTheme()["brls/background"]);
    panel->setShadowType(brls::ShadowType::GENERIC);
    panel->setShadowVisibility(true);
    this->addView(panel);

    auto* head = make_label(title, 28, brls::Application::getTheme()["brls/text"], false);
    head->setMargins(34, 36, 14, 36);
    panel->addView(head);
    auto* sep = new brls::Box();
    sep->setHeight(1);
    sep->setMargins(0, 24, 0, 24);
    sep->setBackgroundColor(color_dim());
    panel->addView(sep);

    auto* scroll = new brls::ScrollingFrame();
    scroll->setGrow(1.0f);
    auto* list = new brls::Box(brls::Axis::COLUMN);
    list->setPadding(6, 24, 30, 24);
    for (size_t s = 0; s < this->sections.size(); s++)
    {
        const MenuSection& sec = this->sections[s];
        if (!sec.title.empty())
        {
            auto* h = make_label(sec.title, 17, color_dim(), false);
            h->setMargins(s ? 22 : 12, 12, 6, 12);
            list->addView(h);
        }
        for (size_t i = 0; i < sec.items.size(); i++)
        {
            const MenuItem& it = sec.items[i];
            auto* cell         = new brls::DetailCell();
            cell->setHeight(60);
            cell->setText(it.title);
            cell->setDetailText(it.key);
            cell->setDetailTextColor(color_dim());
            // Nhãn rỗng của borealis chiếm hết bề ngang, làm cụt tên mục
            if (it.key.empty())
                cell->detail->setVisibility(brls::Visibility::GONE);
            if (it.warn)
                cell->setTextColor(color_warn());
            auto run = it.run;
            cell->registerClickAction([this, run](brls::View*)
                {
                    close(run);
                    return true; });
            list->addView(cell);
        }
    }
    scroll->setContentView(list);
    panel->addView(scroll);

    // Panel che thanh gợi ý dưới cùng: tự ghi nút
    auto* foot_sep = new brls::Box();
    foot_sep->setHeight(1);
    foot_sep->setMargins(0, 24, 0, 24);
    foot_sep->setBackgroundColor(color_dim());
    panel->addView(foot_sep);
    auto* foot = make_label(ICON_B " " + "hints/back"_i18n + "      " ICON_A " " + "hints/ok"_i18n, 20,
                            brls::Application::getTheme()["brls/text"], false);
    foot->setAlignSelf(brls::AlignSelf::FLEX_END);
    foot->setMargins(18, 36, 22, 36);
    panel->addView(foot);

    auto back = [this](brls::View*)
    {
        close(nullptr);
        return true;
    };
    this->registerAction("hints/back"_i18n, brls::BUTTON_B, back);
    // Bấm lại nút mở menu (X) cũng đóng
    bind_action(this, "", brls::BUTTON_X, back, true);
}

void SideMenu::open(const std::string& title, std::vector<MenuSection> sections)
{
    brls::Application::pushActivity(new brls::Activity(new SideMenu(title, std::move(sections))),
                                    brls::TransitionAnimation::FADE);
}

void SideMenu::close(std::function<void()> then)
{
    brls::Application::popActivity(brls::TransitionAnimation::FADE, [then]()
        {
            if (then)
                then(); });
}

void SideMenu::show(std::function<void(void)> cb, bool animate, float animationDuration)
{
    if (animate)
    {
        panel->setTranslationX(SLIDE_PX);
        slide.stop();
        slide.reset(SLIDE_PX);
        slide.addStep(0, animationDuration, brls::EasingFunction::quadraticOut);
        slide.setTickCallback([this]
            { panel->setTranslationX(slide); });
        slide.start();
    }
    brls::Box::show(cb, animate, animationDuration);
}

void SideMenu::hide(std::function<void(void)> cb, bool animated, float animationDuration)
{
    if (animated)
    {
        slide.stop();
        slide.reset(0);
        slide.addStep(SLIDE_PX, animationDuration, brls::EasingFunction::quadraticOut);
        slide.setTickCallback([this]
            { panel->setTranslationX(slide); });
        slide.start();
    }
    brls::Box::hide(cb, animated, animationDuration);
}

} // namespace ui
