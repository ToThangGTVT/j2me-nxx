#include "ui/main_activity.hpp"

#include "ui/screen_activity.hpp"
#include "ui/settings_activity.hpp"
#include "ui/update_activity.hpp"
#include "ui/upload_activity.hpp"

namespace ui
{

static MainActivity* instance;

// Toàn màn hình: menu_draw vẽ cả thanh trên, danh sách, thanh dưới và hộp thoại
class MenuView : public brls::View
{
  public:
    explicit MenuView(MainActivity* owner)
        : owner(owner)
    {
        this->setFocusable(true);
        this->setHideHighlight(true);
        this->setHideClickAnimation(true);
        this->setGrow(1.0f);

        // Nút (ẩn gợi ý: thanh dưới do menu.c tự vẽ). Nút hướng lặp khi giữ như giao diện cũ
        struct Key
        {
            brls::ControllerButton button;
            Button btn;
            bool repeat;
        };
        static const Key keys[] = {
            { brls::BUTTON_A, BTN_A, false },
            { brls::BUTTON_B, BTN_B, false },
            { brls::BUTTON_X, BTN_X, false },
            { brls::BUTTON_Y, BTN_Y, false },
            { brls::BUTTON_LB, BTN_L, false },
            { brls::BUTTON_RB, BTN_R, false },
            { brls::BUTTON_START, BTN_PLUS, false },
            { brls::BUTTON_BACK, BTN_MINUS, false },
            { brls::BUTTON_NAV_UP, BTN_UP, true },
            { brls::BUTTON_NAV_DOWN, BTN_DOWN, true },
            { brls::BUTTON_NAV_LEFT, BTN_LEFT, true },
            { brls::BUTTON_NAV_RIGHT, BTN_RIGHT, true },
        };
        for (const Key& k : keys)
        {
            Button btn    = k.btn;
            auto listener = [this, btn](brls::View*)
            {
                this->owner->handle(menu_press(&this->owner->menu, &this->owner->list, btn));
                return true;
            };
            if (k.repeat)
                this->registerAction("", k.button, listener, true, true);
            else
                bind_action(this, "", k.button, listener, true);
        }

        // Chạm: chọn dòng, chạm lại thì mở; kéo để cuộn
        this->addGestureRecognizer(new brls::TapGestureRecognizer([this](brls::TapGestureStatus status, brls::Sound* sound)
            {
                if (status.state != brls::GestureState::END)
                    return;
                int x, y;
                to_screen(status.position, &x, &y);
                this->owner->handle(menu_tap(&this->owner->menu, &this->owner->list, x, y)); }));
        this->addGestureRecognizer(new brls::PanGestureRecognizer([this](brls::PanGestureStatus status, brls::Sound* sound)
            {
                if (status.state == brls::GestureState::START)
                    scroll0 = this->owner->menu.scroll;
                if (status.state == brls::GestureState::START || status.state == brls::GestureState::STAY ||
                    status.state == brls::GestureState::END)
                {
                    int x0, y0, x1, y1;
                    to_screen(status.startPosition, &x0, &y0);
                    to_screen(status.position, &x1, &y1);
                    menu_drag(&this->owner->menu, &this->owner->list, scroll0, y1 - y0);
                } },
            brls::PanAxis::VERTICAL));
    }

    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
              brls::FrameContext* ctx) override
    {
        frame_x = x;
        frame_y = y;
        frame_w = width;
        frame_h = height;
        gfx_begin(x, y, width, height);
        menu_draw(&owner->menu, &owner->list, platform_games_dir());
        gfx_end();
    }

  private:
    MainActivity* owner;
    int scroll0   = 0;
    float frame_x = 0, frame_y = 0, frame_w = SCREEN_W, frame_h = SCREEN_H;

    // Toạ độ của borealis -> toạ độ 1280x720 của menu (giống gfx_begin)
    void to_screen(brls::Point p, int* x, int* y)
    {
        float s = frame_w / SCREEN_W < frame_h / SCREEN_H ? frame_w / SCREEN_W : frame_h / SCREEN_H;
        *x      = (int)((p.x - frame_x - (frame_w - SCREEN_W * s) / 2) / s);
        *y      = (int)((p.y - frame_y - (frame_h - SCREEN_H * s) / 2) / s);
    }
};

MainActivity::MainActivity()
{
    instance = this;
}

MainActivity::~MainActivity()
{
    menu_free_textures(&list);
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
    return new MenuView(this);
}

void MainActivity::onContentAvailable()
{
    rescan(false);
}

void MainActivity::set_status(const std::string& text)
{
    if (!instance)
    {
        notify(text);
        return;
    }
    snprintf(instance->menu.status, sizeof(instance->menu.status), "%s", text.c_str());
}

void MainActivity::rescan(bool keep)
{
    menu_free_textures(&list);
    game_list_free(&list);
    game_list_scan(&list, platform_games_dir());
    if (keep)
        menu_clamp_cursor(&menu, &list);
    else
        menu.cursor = menu.scroll = 0;
}

void MainActivity::handle(MenuAction action)
{
    switch (action)
    {
        case MENU_QUIT:
            brls::Application::quit();
            break;
        case MENU_RESCAN:
            rescan(false);
            set_status(trf(S_RESCANNED, list.count));
            break;
        case MENU_SETTINGS:
            SettingsActivity::open_global();
            break;
        case MENU_UPDATE:
            UpdateActivity::open();
            break;
        case MENU_DELETE:
        {
            GameEntry* g      = &list.items[menu.cursor];
            std::string title = g->title;
            bool ok           = game_list_delete(g);
            set_status(trf(ok ? S_DELETED : S_DELETE_FAILED, title.c_str()));
            if (ok)
                rescan(true);
            break;
        }
        case MENU_UPLOAD:
            UploadActivity::open(platform_games_dir(), [](int received)
                {
                    if (received > 0 && instance)
                    {
                        instance->rescan(false);
                        set_status(trf(S_UPLOAD_DONE_STATUS, received));
                    } });
            break;
        case MENU_GAME_OPTIONS:
        {
            GameEntry* g = &list.items[menu.cursor];
            if (g->video)
            {
                set_status(T(S_VIDEO_NO_OPTIONS));
                break;
            }
            char id[256];
            game_list_id(g, id, sizeof(id));
            game_list_load_info(g);
            SettingsActivity::open_game(id, g->title);
            break;
        }
        case MENU_LAUNCH:
        {
            GameEntry* g = &list.items[menu.cursor];
            if (g->video)
            {
                play_video(g->path, g->title);
            }
            else
            {
                char id[256];
                game_list_id(g, id, sizeof(id));
                launch_game(g->path, id, menu.midlet);
            }
            break;
        }
        case MENU_NONE:
            break;
    }
}

bool MainActivity::launch_game(const char* path, const char* game_id, int midlet)
{
    char err[256] = "";
    if (!emu_start(path, game_id, midlet, err, sizeof(err)))
    {
        set_status(trf(S_ERROR_FMT, err));
        return false;
    }
    if (instance)
        instance->menu.status[0] = '\0';
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
        emu_stop();
        set_status(msg.empty() ? T(S_GAME_EXITED) : msg);
#ifndef __SWITCH__
        // Kịch bản test (J2ME_NX_QUIT): thoát app luôn khi game kết thúc
        if (SDL_getenv("J2ME_NX_QUIT"))
            brls::Application::quit();
#endif
    };
    ScreenActivity::open(hooks);
    return true;
}

bool MainActivity::play_video(const char* path, const char* title)
{
    char err[160];
    if (!video_screen_open(path, title, err, sizeof(err)))
    {
        set_status(err);
        return false;
    }
    ScreenHooks hooks;
    hooks.update = []()
    { return video_screen_update(); };
    hooks.draw   = []()
    { video_screen_draw(); };
    hooks.closed = []()
    { video_screen_close(); };
    ScreenActivity::open(hooks);
    return true;
}

void MainActivity::tick()
{
    if (!instance)
        return;
    // Kiểm tra xong, có bản mới: hỏi 1 lần khi đang ở danh sách
    static bool prompted;
    auto stack  = brls::Application::getActivitiesStack();
    bool on_top = !stack.empty() && stack.back() == instance;
    if (!prompted && on_top && update_state() == UPDATE_AVAILABLE)
    {
        prompted = true;
        UpdateActivity::open();
    }
}

void MainActivity::shutdown()
{
    if (instance)
        menu_free_textures(&instance->list);
}

} // namespace ui
