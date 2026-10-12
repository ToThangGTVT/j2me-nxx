#include "touch.h"

#include <string.h>

#ifdef __SWITCH__
#include <switch.h>
#endif

#define TOUCH_DEVICE 1

static const TouchPoint *find(const TouchFrame *f, SDL_FingerID id) {
    for (int i = 0; i < f->count; i++) {
        if (f->p[i].id == id)
            return &f->p[i];
    }
    return NULL;
}

static void emit(TouchSendFn send, Uint32 type, const TouchPoint *p, float dx, float dy) {
    SDL_Event e;
    memset(&e, 0, sizeof(e));
    e.type = type;
    e.tfinger.timestamp = SDL_GetTicks();
    e.tfinger.touchId = TOUCH_DEVICE;
    e.tfinger.fingerId = p->id;
    e.tfinger.x = p->x;
    e.tfinger.y = p->y;
    e.tfinger.dx = dx;
    e.tfinger.dy = dy;
    e.tfinger.pressure = type == SDL_FINGERUP ? 0 : 1;
    send(&e);
}

void touch_diff(const TouchFrame *old, const TouchFrame *cur, TouchSendFn send) {
    for (int i = 0; i < old->count; i++) {
        if (!find(cur, old->p[i].id))
            emit(send, SDL_FINGERUP, &old->p[i], 0, 0);
    }
    for (int i = 0; i < cur->count; i++) {
        const TouchPoint *o = find(old, cur->p[i].id);
        if (o && (o->x != cur->p[i].x || o->y != cur->p[i].y))
            emit(send, SDL_FINGERMOTION, &cur->p[i], cur->p[i].x - o->x, cur->p[i].y - o->y);
    }
    for (int i = 0; i < cur->count; i++) {
        if (!find(old, cur->p[i].id))
            emit(send, SDL_FINGERDOWN, &cur->p[i], 0, 0);
    }
}

#ifdef __SWITCH__
void touch_poll(TouchSendFn send) {
    static TouchFrame last;
    HidTouchScreenState st;
    if (!hidGetTouchScreenStates(&st, 1))
        return;
    TouchFrame cur;
    cur.count = 0;
    for (int i = 0; i < st.count && cur.count < TOUCH_MAX; i++) {
        TouchPoint *p = &cur.p[cur.count++];
        p->id = st.touches[i].finger_id;
        p->x = st.touches[i].x / 1280.0f;
        p->y = st.touches[i].y / 720.0f;
    }
    touch_diff(&last, &cur, send);
    last = cur;
}
#endif
