#include "ui/screen_activity.hpp"

namespace ui
{

class ScreenView : public brls::View
{
  public:
    ScreenView()
    {
        this->setFocusable(true);
        this->setHideHighlight(true);
        this->setDimensions(brls::View::AUTO, brls::View::AUTO);
        this->setGrow(1.0f);
    }

    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
              brls::FrameContext* ctx) override
    {
        if (draw_fn)
        {
            gfx_begin(x, y, width, height);
            draw_fn();
            gfx_end();
        }
    }

    std::function<void()> draw_fn;
};

static ScreenActivity* current;     // màn hình trên cùng
static int unblock_pending;         // đã đóng, chờ nhả hết nút rồi mới trả phím cho borealis

ScreenActivity::ScreenActivity(ScreenHooks hooks)
    : hooks(std::move(hooks))
{
}

ScreenActivity::~ScreenActivity()
{
    if (current == this)
        current = nullptr;
}

brls::View* ScreenActivity::createContentView()
{
    auto* v = new ScreenView();
    v->draw_fn = hooks.draw;
    return v;
}

void ScreenActivity::onContentAvailable()
{
}

void ScreenActivity::open(ScreenHooks hooks)
{
    auto* a = new ScreenActivity(std::move(hooks));
    current = a;
    sdl_events_set_handler(a->hooks.event);
    brls::Application::blockInputs(true);
    brls::Application::pushActivity(a, brls::TransitionAnimation::NONE);
}

bool ScreenActivity::active()
{
    return current != nullptr;
}

void ScreenActivity::close()
{
    if (closing)
        return;
    closing = true;
    sdl_events_set_handler(nullptr);
    if (current == this)
        current = nullptr;
    if (hooks.closed)
        hooks.closed();
    unblock_pending++;
    brls::Application::popActivity(brls::TransitionAnimation::NONE);
}

void ScreenActivity::tick()
{
    if (current && !current->closing && current->hooks.update && !current->hooks.update())
        current->close();
    // Nút vừa dùng để thoát (vd B) còn đang giữ thì borealis sẽ coi là vừa bấm: chờ nhả hết
    while (unblock_pending > 0 && !input_any_held())
    {
        unblock_pending--;
        brls::Application::unblockInputs();
    }
}

} // namespace ui
