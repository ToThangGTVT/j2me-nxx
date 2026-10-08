#include "ui/upload_activity.hpp"

#include "ui/update_activity.hpp"

namespace ui
{

#define QR_PX    340    // cạnh tối đa của mã QR (kể cả viền trắng)
#define QR_QUIET 3      // viền trắng quanh mã, tính theo ô

class QrView : public brls::View
{
  public:
    explicit QrView(const char* text)
    {
        size = qr_encode(text, modules);
        int cells = size + 2 * QR_QUIET;
        cell      = size > 0 ? QR_PX / cells : 0;
        this->setDimensions(cells * cell, cells * cell);
    }

    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
              brls::FrameContext* ctx) override
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

  private:
    uint8_t modules[QR_MAX_SIZE * QR_MAX_SIZE];
    int size = 0;
    int cell = 0;
};

static std::string mb(int64_t bytes)
{
    char out[32];
    snprintf(out, sizeof(out), "%.1f MB", bytes / (1024.0 * 1024.0));
    return out;
}

UploadActivity::UploadActivity(const std::string& dir, std::function<void(int)> closed)
    : dir(dir)
    , closed(std::move(closed))
{
    server_on = upload_start(dir.c_str(), url, sizeof(url), error, sizeof(error));
}

UploadActivity::~UploadActivity()
{
    brls::Application::getRunLoopEvent()->unsubscribe(loop);
    UploadStatus st;
    upload_get_status(&st);
    upload_stop();
    // Báo kết quả sau khi màn hình này đã đóng hẳn
    auto cb = closed;
    int received = st.received;
    if (cb)
        brls::sync([cb, received]()
            { cb(received); });
}

void UploadActivity::open(const std::string& dir, std::function<void(int)> closed)
{
    brls::Application::pushActivity(new UploadActivity(dir, std::move(closed)));
}

brls::View* UploadActivity::createContentView()
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
        for (StrId step : { S_UPLOAD_STEP1, S_UPLOAD_STEP2, S_UPLOAD_STEP3 })
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
    frame->setTitle(T(S_UPLOAD_TITLE));
    return frame;
}

void UploadActivity::onContentAvailable()
{
    if (!server_on)
        return;
    loop = brls::Application::getRunLoopEvent()->subscribe([this]()
        { refresh(); });
}

void UploadActivity::refresh()
{
    UploadStatus st;
    upload_get_status(&st);
    bool receiving = st.current[0] != '\0';
    if (receiving)
    {
        set_text(status, trf(S_UPLOAD_RECEIVING, st.current));
        status->setTextColor(brls::Application::getTheme()["brls/text"]);
        bar->value = st.total > 0 ? (float)((double)st.done / (double)st.total) : -1;
        set_text(bytes, mb(st.done) + " / " + mb(st.total));
    }
    else if (st.received > 0)
    {
        set_text(status, trf(S_UPLOAD_RECEIVED, st.received, st.last));
        status->setTextColor(color_ok());
    }
    else
    {
        set_text(status, T(S_UPLOAD_WAITING));
        status->setTextColor(color_dim());
    }
    brls::Visibility vis = receiving ? brls::Visibility::VISIBLE : brls::Visibility::GONE;
    if (bar->getVisibility() != vis)
    {
        bar->setVisibility(vis);
        bytes->setVisibility(vis);
    }
    set_text(net_error, receiving ? "" : st.error);
}

} // namespace ui
