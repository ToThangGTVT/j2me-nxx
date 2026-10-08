#include "ui/update_activity.hpp"

#include <cstring>

namespace ui
{

ProgressBar::ProgressBar()
{
    this->setHeight(24);
}

void ProgressBar::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
                       brls::FrameContext* ctx)
{
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, width, height, height / 2);
    nvgFillColor(vg, nvgRGB(0x3a, 0x3e, 0x47));
    nvgFill(vg);
    float x0 = x, w = width * (value < 0 ? 0 : value > 1 ? 1 : value);
    if (value < 0)
    {
        // Chưa biết độ dài: vạch chạy qua lại
        float seg = width / 5, span = width - seg;
        float pos = (float)(SDL_GetTicks() % 2000) / 1000.0f;
        x0        = x + span * (pos < 1 ? pos : 2 - pos);
        w         = seg;
    }
    if (w <= 0)
        return;
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x0, y, w, height, height / 2);
    nvgFillColor(vg, color_accent());
    nvgFill(vg);
}

static std::string mb(int64_t bytes)
{
    char out[32];
    snprintf(out, sizeof(out), "%.1f MB", bytes / (1024.0 * 1024.0));
    return out;
}

void UpdateActivity::open()
{
    brls::Application::pushActivity(new UpdateActivity());
}

UpdateActivity::~UpdateActivity()
{
    brls::Application::getRunLoopEvent()->unsubscribe(loop);
}

brls::View* UpdateActivity::createContentView()
{
    auto* box = new brls::Box(brls::Axis::COLUMN);
    box->setPadding(36, 80, 30, 80);
    box->setGrow(1.0f);
    // Nhận focus để các phím của màn hình chạy
    box->setFocusable(true);
    box->setHideHighlight(true);

    title = make_label("", 34, brls::Application::getTheme()["brls/text"]);
    box->addView(title);
    sub = make_label("", 20, color_dim());
    sub->setMarginTop(8);
    box->addView(sub);
    error = make_label("", 22, color_warn());
    error->setMarginTop(20);
    box->addView(error);

    notes_scroll = new brls::ScrollingFrame();
    notes_scroll->setGrow(1.0f);
    notes_scroll->setMarginTop(20);
    notes = make_label("", 18, color_dim());
    notes_scroll->setContentView(notes);
    box->addView(notes_scroll);

    progress_box = new brls::Box(brls::Axis::COLUMN);
    progress_box->setMarginTop(50);
    bar = new ProgressBar();
    progress_box->addView(bar);
    auto* row = new brls::Box(brls::Axis::ROW);
    row->setJustifyContent(brls::JustifyContent::SPACE_BETWEEN);
    row->setAlignItems(brls::AlignItems::CENTER);
    row->setMarginTop(18);
    bytes   = make_label("", 24, brls::Application::getTheme()["brls/text"], false);
    percent = make_label("", 34, color_accent(), false);
    row->addView(bytes);
    row->addView(percent);
    progress_box->addView(row);
    speed = make_label("", 18, color_dim(), false);
    speed->setMarginTop(6);
    progress_box->addView(speed);
    auto* keep = make_label(T(S_UPDATE_KEEP_OPEN), 18, color_dim());
    keep->setMarginTop(30);
    progress_box->addView(keep);
    box->addView(progress_box);

    info = make_label("", 24, brls::Application::getTheme()["brls/text"]);
    info->setMarginTop(24);
    box->addView(info);

    frame = new brls::AppletFrame(box);
    frame->setTitle(T(S_ACT_UPDATE));
    return frame;
}

void UpdateActivity::onContentAvailable()
{
    loop = brls::Application::getRunLoopEvent()->subscribe([this]()
        { refresh(); });
    refresh();
}

void UpdateActivity::set_actions(UpdateState s)
{
    auto close = [](brls::View*)
    {
        brls::Application::popActivity();
        return true;
    };
    switch (s)
    {
        case UPDATE_DOWNLOADING:
            frame->registerAction(T(S_ACT_CANCEL), brls::BUTTON_B, [this](brls::View*)
                {
                    update_cancel();
                    closing = true;
                    return true; });
            frame->registerAction("", brls::BUTTON_A, [](brls::View*)
                { return true; }, true);
            break;
        case UPDATE_DONE:
#ifdef __SWITCH__
            frame->registerAction(T(S_ACT_RESTART), brls::BUTTON_A, [](brls::View*)
                {
                    // Khởi động lại vào bản mới; loader không hỗ trợ thì thoát để người dùng mở lại
                    update_restart();
                    brls::Application::quit();
                    return true; });
            frame->registerAction(T(S_ACT_LATER), brls::BUTTON_B, close);
#else
            frame->registerAction(T(S_ACT_CLOSE), brls::BUTTON_A, close);
            frame->registerAction(T(S_ACT_CLOSE), brls::BUTTON_B, close, true);
#endif
            break;
        default:
            frame->registerAction(T(s == UPDATE_FAILED ? S_ACT_RETRY : S_ACT_DOWNLOAD), brls::BUTTON_A,
                [](brls::View*)
                {
                    update_download();
                    return true; });
            frame->registerAction(T(s == UPDATE_FAILED ? S_ACT_CLOSE : S_ACT_LATER), brls::BUTTON_B, close);
            break;
    }
    brls::Application::getGlobalHintsUpdateEvent()->fire();
}

void UpdateActivity::refresh()
{
    UpdateState s = update_state();
    // Vừa huỷ tải: đóng khi luồng tải đã dừng
    if (closing && s != UPDATE_DOWNLOADING)
    {
        closing = false;
        brls::Application::popActivity();
        return;
    }
    const char* ver = update_latest_version();
    if (s == UPDATE_DOWNLOADING)
    {
        int64_t done, total;
        int bps;
        update_progress(&done, &total, &bps);
        bar->value = total > 0 ? (float)((double)done / total) : -1;
        set_text(bytes, total > 0 ? mb(done) + " / " + mb(total) : mb(done));
        set_text(percent, total > 0 ? std::to_string((int)(done * 100 / total)) + "%" : "");
        std::string line;
        if (bps > 0)
        {
            char buf[96];
            if (bps >= 1024 * 1024)
                snprintf(buf, sizeof(buf), "%.1f MB/s", bps / (1024.0 * 1024.0));
            else
                snprintf(buf, sizeof(buf), "%d KB/s", bps / 1024);
            line = buf;
            if (total > 0 && done < total)
            {
                int sec = (int)((total - done) / bps);
                line += "  -  " + trf(S_UPDATE_ETA, sec / 60, sec % 60);
            }
        }
        set_text(speed, line);
        if (closing)
            set_text(title, T(S_UPDATE_CANCELLING));
    }
    if ((int)s == shown_state)
        return;
    shown_state = s;

    auto vis = [](brls::View* v, bool on)
    { v->setVisibility(on ? brls::Visibility::VISIBLE : brls::Visibility::GONE); };
    bool prompt = s == UPDATE_AVAILABLE || s == UPDATE_FAILED;
    vis(sub, prompt);
    vis(error, s == UPDATE_FAILED);
    vis(notes_scroll, prompt && update_notes()[0]);
    vis(progress_box, s == UPDATE_DOWNLOADING);
    vis(info, s == UPDATE_DONE);
    title->setTextColor(s == UPDATE_DONE ? color_ok() : brls::Application::getTheme()["brls/text"]);
    switch (s)
    {
        case UPDATE_DOWNLOADING:
            set_text(title, trf(S_UPDATE_DOWNLOADING, ver));
            break;
        case UPDATE_DONE:
            set_text(title, trf(S_UPDATE_DONE, ver));
#ifdef __SWITCH__
            set_text(info, T(S_UPDATE_DONE_INFO));
#else
            set_text(info, T(S_UPDATE_DONE_DESKTOP));
#endif
            break;
        default:
        {
            set_text(title, trf(S_UPDATE_TITLE, ver));
            set_text(sub, trf(S_UPDATE_CURRENT, APP_VERSION_STR));
            char err[192];
            update_error(err, sizeof(err));
            set_text(error, trf(S_UPDATE_FAILED, err));
            set_text(notes, update_notes());
            break;
        }
    }
    set_actions(s);
}

} // namespace ui
