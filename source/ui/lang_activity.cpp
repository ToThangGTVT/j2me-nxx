#include "ui/lang_activity.hpp"

#include "ui/main_activity.hpp"

namespace ui
{

void LangActivity::open()
{
    brls::Application::pushActivity(new LangActivity());
}

// Thứ tự nút từ trái sang phải
static const Lang choices[] = { LANG_VI, LANG_EN };

brls::View* LangActivity::createContentView()
{
    auto* box = new brls::Box(brls::Axis::COLUMN);
    box->setAlignItems(brls::AlignItems::CENTER);
    box->setJustifyContent(brls::JustifyContent::CENTER);
    box->setGrow(1.0f);

    auto* title = make_label("Welcome to J2ME-NXX", 40, brls::Application::getTheme()["brls/text"], false);
    box->addView(title);
    auto* sub = make_label("Choose your language", 26, brls::Application::getTheme()["brls/text"], false);
    sub->setMarginTop(20);
    box->addView(sub);
    auto* note = make_label("You can change it later in Settings.", 20, color_dim(), false);
    note->setMarginTop(10);
    box->addView(note);

    auto* row = new brls::Box(brls::Axis::ROW);
    row->setMarginTop(60);
    for (Lang l : choices)
    {
        auto* button = new brls::Button();
        button->setText(lang_name(l));
        button->setFontSize(30);
        button->setDimensions(300, 110);
        button->setMargins(0, 20, 0, 20);
        button->setStyle(&brls::BUTTONSTYLE_BORDERED);
        button->registerClickAction([l](brls::View*)
            {
                settings()->lang = l;
                lang_set(l);
                settings_save();
                // Mở lần đầu: màn hình này nằm dưới cùng (không bỏ được), mở danh sách ứng dụng đè lên.
                // Đã có danh sách (mở từ test desktop) thì chỉ quay lại
                if (MainActivity::get())
                {
                    MainActivity::get()->relabel();
                    brls::Application::popActivity();
                }
                else
                {
                    MainActivity::open();
                }
                return true; });
        row->addView(button);
    }
    box->addView(row);

    auto* frame = new brls::AppletFrame(box);
    frame->setTitle("J2ME-NXX");
    return frame;
}

} // namespace ui
