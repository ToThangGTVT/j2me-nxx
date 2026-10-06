// Lớp MIDP: native cho javax.microedition.* và giao tiếp với host (màn hình, phím)
#pragma once

#include <stdbool.h>
#include <stdint.h>

// Loại sự kiện, khớp với hằng số trong javax/microedition/lcdui/Display.java
enum {
    MIDP_EV_KEY_PRESSED = 1,
    MIDP_EV_KEY_RELEASED = 2,
    MIDP_EV_KEY_REPEATED = 3,
    MIDP_EV_PAINT = 4,
    MIDP_EV_POINTER_PRESSED = 5,
    MIDP_EV_POINTER_RELEASED = 6,
    MIDP_EV_POINTER_DRAGGED = 7,
    MIDP_EV_SERIAL = 8,
    MIDP_EV_PAUSE = 9,
    MIDP_EV_RESUME = 10,
    MIDP_EV_MEDIA_END = 12,
};

// Mã phím J2ME (kiểu Nokia)
enum {
    MIDP_KEY_UP = -1,
    MIDP_KEY_DOWN = -2,
    MIDP_KEY_LEFT = -3,
    MIDP_KEY_RIGHT = -4,
    MIDP_KEY_FIRE = -5,
    MIDP_KEY_SOFT_LEFT = -6,
    MIDP_KEY_SOFT_RIGHT = -7,
    MIDP_KEY_CLEAR = -8,
    MIDP_KEY_STAR = '*',
    MIDP_KEY_POUND = '#',
};

typedef struct {
    int screen_w, screen_h;
    const char *rms_dir;                                    // thư mục lưu RecordStore của game
    const char *(*app_property)(const char *key);          // thuộc tính trong MANIFEST.MF / JAD
    // Bàn phím ảo: trả về chuỗi UTF-8 malloc, NULL nếu huỷ
    char *(*keyboard)(const char *title, const char *text, int max_len, int type);
    void (*vibrate)(int ms);
    int fps_limit;                                          // 0 = không giới hạn
    const char *lang;                                       // "vi" / "en": ngôn ngữ giao diện MIDP
    const char *platform;                                   // microedition.platform
    // Mã phím của hãng: lên, xuống, trái, phải, fire, mềm trái, mềm phải, xoá
    int keycodes[8];
} MidpConfig;

// Đăng ký native (gọi trước vm_init)
void midp_register_natives(void);

// Sau vm_init: tạo thread sự kiện + thread chạy MIDlet
bool midp_start(const MidpConfig *cfg, const char *midlet_class);
void midp_shutdown(void);

void midp_post_event(int type, int a, int b);
void midp_post_key(int code, bool pressed);

// Framebuffer ARGB của màn hình J2ME. *dirty = có khung hình mới từ lần gọi trước
const uint32_t *midp_framebuffer(int *w, int *h, bool *dirty);
bool midp_exit_requested(void);

// audio.c
void midp_audio_register(void);
void midp_audio_poll(void);         // gọi mỗi frame: báo END_OF_MEDIA
void midp_audio_shutdown(void);

// net.c, tls.c
void midp_net_register(void);
void midp_net_shutdown(void);
void midp_tls_register(void);
void midp_tls_shutdown(void);

// graphics.c
void midp_graphics_register(void);
void midp_graphics_shutdown(void);
