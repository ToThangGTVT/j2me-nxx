#include "input.h"

#define REPEAT_DELAY_MS 300
#define REPEAT_RATE_MS  60
#define MAX_JOYSTICKS   8

// Thứ tự nút joystick của SDL2 bản Switch (theo bit HidNpadButton)
static const int joy_map[] = {
    [0]  = BTN_A,
    [1]  = BTN_B,
    [2]  = BTN_X,
    [3]  = BTN_Y,
    [4]  = -1,          // L stick click
    [5]  = -1,          // R stick click
    [6]  = BTN_L,
    [7]  = BTN_R,
    [8]  = BTN_L,       // ZL
    [9]  = BTN_R,       // ZR
    [10] = BTN_PLUS,
    [11] = BTN_MINUS,
    [12] = BTN_LEFT,
    [13] = BTN_UP,
    [14] = BTN_RIGHT,
    [15] = BTN_DOWN,
    [16] = BTN_LEFT,    // stick trái
    [17] = BTN_UP,
    [18] = BTN_RIGHT,
    [19] = BTN_DOWN,
};
#define JOY_MAP_LEN ((int)(sizeof(joy_map) / sizeof(joy_map[0])))

static SDL_Joystick *joysticks[MAX_JOYSTICKS];

// Số nguồn (phím / nút joystick) đang giữ cho mỗi Button
static int sources_down[BTN_COUNT];
static bool held[BTN_COUNT];
static bool pressed[BTN_COUNT];
static Uint32 next_repeat[BTN_COUNT];

static int key_to_button(SDL_Keycode key) {
    switch (key) {
    case SDLK_UP:        return BTN_UP;
    case SDLK_DOWN:      return BTN_DOWN;
    case SDLK_LEFT:      return BTN_LEFT;
    case SDLK_RIGHT:     return BTN_RIGHT;
    case SDLK_RETURN:
    case SDLK_z:         return BTN_A;
    case SDLK_BACKSPACE:
    case SDLK_x:         return BTN_B;
    case SDLK_a:         return BTN_Y;
    case SDLK_s:         return BTN_X;
    case SDLK_q:         return BTN_L;
    case SDLK_w:         return BTN_R;
    case SDLK_ESCAPE:    return BTN_PLUS;
    case SDLK_TAB:       return BTN_MINUS;
    default:             return -1;
    }
}

static void open_joystick(int index) {
    if (index < 0 || index >= MAX_JOYSTICKS || joysticks[index])
        return;
    joysticks[index] = SDL_JoystickOpen(index);
}

void input_init(void) {
    SDL_JoystickEventState(SDL_ENABLE);
    for (int i = 0; i < SDL_NumJoysticks(); i++)
        open_joystick(i);
}

void input_exit(void) {
    for (int i = 0; i < MAX_JOYSTICKS; i++) {
        if (joysticks[i])
            SDL_JoystickClose(joysticks[i]);
        joysticks[i] = NULL;
    }
}

static void source_change(int btn, bool down) {
    if (btn < 0)
        return;
    sources_down[btn] += down ? 1 : -1;
    if (sources_down[btn] < 0)
        sources_down[btn] = 0;
}

void input_handle_event(const SDL_Event *e) {
    switch (e->type) {
    case SDL_KEYDOWN:
        if (!e->key.repeat)
            source_change(key_to_button(e->key.keysym.sym), true);
        break;
    case SDL_KEYUP:
        source_change(key_to_button(e->key.keysym.sym), false);
        break;
    case SDL_JOYBUTTONDOWN:
    case SDL_JOYBUTTONUP:
        if (e->jbutton.button < JOY_MAP_LEN)
            source_change(joy_map[e->jbutton.button], e->type == SDL_JOYBUTTONDOWN);
        break;
    case SDL_JOYDEVICEADDED:
        open_joystick(e->jdevice.which);
        break;
    default:
        break;
    }
}

static bool is_direction(int b) {
    return b == BTN_UP || b == BTN_DOWN || b == BTN_LEFT || b == BTN_RIGHT;
}

void input_update(void) {
    Uint32 now = SDL_GetTicks();
    for (int b = 0; b < BTN_COUNT; b++) {
        bool down = sources_down[b] > 0;
        pressed[b] = false;

        if (down && !held[b]) {
            pressed[b] = true;
            next_repeat[b] = now + REPEAT_DELAY_MS;
        } else if (down && is_direction(b) && SDL_TICKS_PASSED(now, next_repeat[b])) {
            pressed[b] = true;
            next_repeat[b] = now + REPEAT_RATE_MS;
        }
        held[b] = down;
    }
}

bool input_pressed(Button b) {
    return pressed[b];
}

bool input_held(Button b) {
    return held[b];
}
