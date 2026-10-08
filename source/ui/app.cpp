// J2ME-NXX - J2ME emulator cho Nintendo Switch
//
// Giao diện dựng bằng borealis (danh sách ứng dụng, cài đặt...); màn hình chạy game,
// chỉnh phím ảo vẽ bằng gfx.h trên NanoVG (ScreenActivity).
// Chạy được cả trên Switch (devkitPro) và desktop (để test nhanh).
// Desktop: có thể truyền đường dẫn .jar làm tham số để chạy thẳng game.

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <glad/glad.h>

#include "ui/common.hpp"
#include "ui/lang_activity.hpp"
#include "ui/main_activity.hpp"
#include "ui/screen_activity.hpp"
#include "ui/settings_activity.hpp"

extern "C" {
// Font giao diện nhúng (Google Sans, có đủ chữ tiếng Việt)
extern const unsigned char ui_font_ttf[];
extern const size_t ui_font_ttf_size;
}

// Google Sans làm font mặc định của borealis, các font có sẵn (CJK, icon nút Switch, Material) làm font dự phòng.
// Phải chạy trước khi tạo Label đầu tiên (borealis nhớ font mặc định ở lần gọi đầu).
static int setup_fonts()
{
    NVGcontext* vg = brls::Application::getNVGContext();
#ifdef __SWITCH__
    const std::string def = brls::FONT_CHINESE_SIMPLIFIED;
#else
    const std::string def = brls::FONT_REGULAR;
#endif
    int old_default = brls::Application::getFont(def);
    brls::Application::loadFontFromMemory(def, (void*)ui_font_ttf, ui_font_ttf_size, false);
    int font = brls::Application::getFont(def);
    if (font == brls::FONT_INVALID)
        return old_default;
    // Fontstash chỉ tìm ở các font dự phòng của chính font đang dùng (không đi tiếp), nên thêm hết
    std::vector<int> fallbacks = { old_default };
    for (const std::string& name : { brls::FONT_REGULAR, brls::FONT_CHINESE_SIMPLIFIED_EXT, brls::FONT_CHINESE_TRADITIONAL,
             brls::FONT_KOREAN_REGULAR, brls::FONT_SWITCH_ICONS, brls::FONT_MATERIAL_ICONS, brls::FONT_EMOJI })
        fallbacks.push_back(brls::Application::getFont(name));
    for (int f : fallbacks)
    {
        if (f != brls::FONT_INVALID && f != font)
            nvgAddFallbackFontId(vg, font, f);
    }
    return font;
}

#ifndef __SWITCH__
// Test desktop: J2ME_NX_APPSHOT=<file.bmp> chụp màn hình app sau 1.5 giây (J2ME_NX_APPSHOT_MS để đổi) rồi thoát.
// Vẽ lại một khung hình như borealis (không đổi bộ đệm) rồi đọc điểm ảnh.
static void debug_appshot()
{
    const char* appshot = SDL_getenv("J2ME_NX_APPSHOT");
    const char* at      = SDL_getenv("J2ME_NX_APPSHOT_MS");
    static bool done;
    if (!appshot || done || SDL_GetTicks() <= (Uint32)(at ? atoi(at) : 1500))
        return;
    done = true;

    ::VideoContext* video = brls::Application::getPlatform()->getVideoContext();
    NVGcontext* vg            = brls::Application::getNVGContext();
    brls::FrameContext ctx;
    ctx.pixelRatio = (float)brls::Application::windowWidth / (float)brls::Application::windowHeight;
    ctx.vg         = vg;
    ctx.theme      = brls::Application::getTheme();
    video->beginFrame();
    video->clear(ctx.theme.getColor("brls/clear"));
    nvgBeginFrame(vg, brls::Application::windowWidth, brls::Application::windowHeight, video->getScaleFactor());
    nvgScale(vg, brls::Application::windowScale, brls::Application::windowScale);
    auto stack = brls::Application::getActivitiesStack();
    size_t first = stack.empty() ? 0 : stack.size() - 1;
    while (first > 0 && stack[first]->isTranslucent())
        first--;
    for (size_t i = first; i < stack.size(); i++)
    {
        if (stack[i]->getContentView())
            stack[i]->getContentView()->frame(&ctx);
    }
    brls::View* focus = brls::Application::getCurrentFocus();
    if (focus && brls::Application::getInputType() != brls::InputType::TOUCH)
        focus->frameHighlight(&ctx);
    nvgResetTransform(vg);
    nvgEndFrame(vg);

    int w = brls::Application::windowWidth, h = brls::Application::windowHeight;
    std::vector<uint8_t> px((size_t)w * h * 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glReadPixels(0, 0, w, h, GL_BGRA, GL_UNSIGNED_BYTE, px.data());
    SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
    // OpenGL đọc từ dưới lên
    for (int y = 0; y < h; y++)
        memcpy((uint8_t*)surf->pixels + (size_t)y * surf->pitch, px.data() + (size_t)(h - 1 - y) * w * 4, (size_t)w * 4);
    SDL_SaveBMP(surf, appshot);
    SDL_FreeSurface(surf);
    brls::Application::quit();
}

// Test desktop: J2ME_NX_PRESS="1000:Return,1300:Down" bấm phím bàn phím (tên theo SDL) theo mốc ms;
// J2ME_NX_TAPS="1000:640:500,..." chạm chuột trái tại (x, y) (toạ độ cửa sổ) theo mốc ms
static void debug_press()
{
    static Uint32 last;
    Uint32 now       = SDL_GetTicks();
    const char* spec = SDL_getenv("J2ME_NX_PRESS");
    for (const char* p = spec; p && *p;)
    {
        unsigned at;
        char name[32];
        int n = 0;
        if (sscanf(p, "%u:%31[^,]%n", &at, name, &n) != 2)
            break;
        // Nhấn tại mốc at, nhả sau 80ms (để vòng lặp kịp thấy phím được giữ)
        for (int phase = 0; phase < 2; phase++)
        {
            Uint32 t = at + (phase ? 80 : 0);
            if (t > last && t <= now)
            {
                SDL_Event e;
                SDL_zero(e);
                e.type                = phase ? SDL_KEYUP : SDL_KEYDOWN;
                e.key.keysym.sym      = SDL_GetKeyFromName(name);
                e.key.keysym.scancode = SDL_GetScancodeFromKey(e.key.keysym.sym);
                e.key.state           = phase ? SDL_RELEASED : SDL_PRESSED;
                SDL_PushEvent(&e);
            }
        }
        p += n;
        if (*p == ',')
            p++;
    }
    spec = SDL_getenv("J2ME_NX_TAPS");
    for (const char* p = spec; p && *p;)
    {
        unsigned at;
        int x, y, n = 0;
        if (sscanf(p, "%u:%d:%d%n", &at, &x, &y, &n) != 3)
            break;
        for (int phase = 0; phase < 2; phase++)
        {
            Uint32 t = at + (phase ? 80 : 0);
            if (t > last && t <= now)
            {
                SDL_WarpMouseInWindow(SDL_GL_GetCurrentWindow(), x, y);
                SDL_Event e;
                SDL_zero(e);
                e.type          = phase ? SDL_MOUSEBUTTONUP : SDL_MOUSEBUTTONDOWN;
                e.button.button = SDL_BUTTON_LEFT;
                e.button.state  = phase ? SDL_RELEASED : SDL_PRESSED;
                e.button.x      = x;
                e.button.y      = y;
                SDL_PushEvent(&e);
            }
        }
        p += n;
        if (*p == ',')
            p++;
    }
    last = now;
}
#endif

int main(int argc, char* argv[])
{
    if (!platform_init())
        return 1;
    crash_init();
    bool first_run = !settings_load();

    // Chữ của borealis (gợi ý nút) theo ngôn ngữ đã chọn; lần đầu theo máy
    brls::Platform::APP_LOCALE_DEFAULT = first_run ? brls::LOCALE_AUTO : settings()->lang == LANG_EN ? "en-US"
                                                                                                    : "vi";
    brls::Logger::setLogLevel(SDL_getenv("J2ME_NX_BRLS_DEBUG") ? brls::LogLevel::LOG_DEBUG : brls::LogLevel::LOG_INFO);
    if (!brls::Application::init())
    {
        platform_exit();
        return 1;
    }
    brls::Application::createWindow("J2ME-NXX");
    brls::Application::getPlatform()->setThemeVariant(brls::ThemeVariant::DARK);
    brls::Application::setGlobalQuit(false);

    // Chữ trong game (MIDP) vẽ bằng SDL_ttf; gfx cũng lấy số đo dòng chữ từ đây
    TTF_Init();
    gfx_init(brls::Application::getNVGContext(), setup_fonts());
    SDL_InitSubSystem(SDL_INIT_JOYSTICK);
    input_init();
    ui::sdl_events_init();

    update_init(argc > 0 ? argv[0] : nullptr);
    if (settings()->check_update)
        update_check();

    char crashed[64];
    std::string startup_note;
    if (crash_take_previous(crashed, sizeof(crashed)))
        startup_note = ui::trf(S_APP_CRASHED_BEFORE, crashed);

    if (first_run)
        ui::LangActivity::open();
    else
        ui::MainActivity::open();

#ifndef __SWITCH__
    // Desktop: J2ME_NX_SCREEN=settings / lang mở thẳng màn hình cài đặt / chọn ngôn ngữ (để test giao diện)
    const char* start_screen = SDL_getenv("J2ME_NX_SCREEN");
    if (start_screen && strcmp(start_screen, "settings") == 0)
        ui::SettingsActivity::open_global();
    else if (start_screen && strcmp(start_screen, "lang") == 0)
        ui::LangActivity::open();
    else if (start_screen && strcmp(start_screen, "keybind") == 0)
        ui::SettingsActivity::open_keybinds_global();
#endif
    if (argc > 1)
    {
        // Desktop: tham số là file .jar thì chạy luôn
        ui::MainActivity::launch_game(argv[1], nullptr, argc > 2 ? atoi(argv[2]) : 1);
    }
    ui::notify(startup_note);

    brls::Application::getRunLoopEvent()->subscribe([]()
        {
#ifndef __SWITCH__
            debug_press();
#endif
            ui::sdl_events_pump();
            input_update();
            ui::ScreenActivity::tick();
            ui::MainActivity::tick();
#ifndef __SWITCH__
            debug_appshot();
#endif
        });
    // Dọn trước khi borealis huỷ cửa sổ và context NanoVG
    brls::Application::getExitEvent()->subscribe([]()
        {
            update_shutdown();
            upload_stop();
            emu_stop();
            ui::MainActivity::shutdown();
            input_exit();
        });

    while (brls::Application::mainLoop())
        ;

    if (TTF_WasInit())
        TTF_Quit();
    platform_exit();
    return 0;
}
