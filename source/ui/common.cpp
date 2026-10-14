#include "ui/common.hpp"

#include <cstdarg>
#include <cstdio>

#include "touch.h"

namespace ui
{

std::string T(StrId id)
{
    return tr(id);
}

std::string trf(int id, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, id);
    vsnprintf(buf, sizeof(buf), tr((StrId)id), ap);
    va_end(ap);
    return buf;
}

NVGcolor color_accent()
{
    return nvgRGB(0x00, 0xb4, 0xe6);
}

NVGcolor color_dim()
{
    return nvgRGB(0x9a, 0x9f, 0xa8);
}

NVGcolor color_warn()
{
    return nvgRGB(0xff, 0xc1, 0x4d);
}

NVGcolor color_ok()
{
    return nvgRGB(0x5c, 0xd6, 0x7a);
}

void notify(const std::string& text)
{
    if (!text.empty())
        brls::Application::notify(text);
}

static SdlEventHandler handler;

static void deliver(SDL_Event* e)
{
    input_handle_event(e);
    if (handler)
        handler(e);
}

static int event_watch(void*, SDL_Event* e)
{
#ifdef __SWITCH__
    // Chạm của SDL bản Switch sai mã ngón (phím ảo kẹt): dùng touch_poll trong sdl_events_pump
    if (e->type == SDL_FINGERDOWN || e->type == SDL_FINGERUP || e->type == SDL_FINGERMOTION)
        return 0;
#endif
    deliver(e);
    return 0;
}

void sdl_events_init()
{
    SDL_AddEventWatch(event_watch, nullptr);
}

void sdl_events_pump()
{
#ifdef __SWITCH__
    // Sự kiện đi qua event_watch khi SDL đưa vào hàng đợi; hàng đợi thì bỏ đi
    SDL_PumpEvents();
    SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
    touch_poll(deliver);
#endif
}

void sdl_events_set_handler(SdlEventHandler h)
{
    handler = std::move(h);
}

void set_text(brls::Label* label, const std::string& text)
{
    if (label->getFullText() != text)
        label->setText(text);
}

#define QR_PX    340    // cạnh tối đa của mã QR (kể cả viền trắng)
#define QR_QUIET 3      // viền trắng quanh mã, tính theo ô

QrView::QrView(const char* text)
{
    size = qr_encode(text, modules);
    int cells = size + 2 * QR_QUIET;
    cell      = size > 0 ? QR_PX / cells : 0;
    this->setDimensions(cells * cell, cells * cell);
}

void QrView::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
                  brls::FrameContext* ctx)
{
    if (size <= 0)
        return;
    nvgShapeAntiAlias(vg, 0);
    nvgBeginPath(vg);
    nvgRect(vg, x, y, width, height);
    nvgFillColor(vg, nvgRGB(255, 255, 255));
    nvgFill(vg);
    // Gộp các ô tối liền nhau trên một hàng thành một hình chữ nhật
    float ox = x + QR_QUIET * cell, oy = y + QR_QUIET * cell;
    nvgBeginPath(vg);
    for (int r = 0; r < size; r++)
    {
        for (int c = 0; c < size;)
        {
            if (!modules[r * size + c])
            {
                c++;
                continue;
            }
            int c0 = c;
            while (c < size && modules[r * size + c])
                c++;
            nvgRect(vg, ox + c0 * cell, oy + r * cell, (c - c0) * cell, cell);
        }
    }
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFill(vg);
    nvgShapeAntiAlias(vg, 1);
}

std::string format_mb(int64_t bytes)
{
    char out[32];
    snprintf(out, sizeof(out), "%.1f MB", bytes / (1024.0 * 1024.0));
    return out;
}

brls::Label* make_label(const std::string& text, float size, NVGcolor color, bool wrap)
{
    auto* l = new brls::Label();
    l->setText(text);
    l->setFontSize(size);
    l->setTextColor(color);
    if (wrap)
        l->setIsWrapping(true);
    else
        l->setSingleLine(true);
    return l;
}

brls::ActionIdentifier bind_action(brls::View* view, const std::string& hint, brls::ControllerButton button,
                            const brls::ActionListener& listener, bool hidden)
{
#ifndef __SWITCH__
    brls::BrlsKeyboardScancode key = brls::BRLS_KBD_KEY_UNKNOWN;
    switch (button)
    {
        case brls::BUTTON_X: key = brls::BRLS_KBD_KEY_S; break;
        case brls::BUTTON_Y: key = brls::BRLS_KBD_KEY_A; break;
        case brls::BUTTON_LB: key = brls::BRLS_KBD_KEY_Q; break;
        case brls::BUTTON_RB: key = brls::BRLS_KBD_KEY_W; break;
        case brls::BUTTON_RT: key = brls::BRLS_KBD_KEY_E; break;
        case brls::BUTTON_BACK: key = brls::BRLS_KBD_KEY_TAB; break;
        case brls::BUTTON_START: key = brls::BRLS_KBD_KEY_EQUAL; break;
        default: break;
    }
    if (key != brls::BRLS_KBD_KEY_UNKNOWN)
        view->registerAction(brls::BrlsKeyCombination(key), listener);
#endif
    return view->registerAction(hint, button, listener, hidden);
}

} // namespace ui
