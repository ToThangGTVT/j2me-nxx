// Cài đặt chung (<data_dir>/settings.ini) và cài đặt riêng từng game (<data_dir>/games/<tên>.ini)
#pragma once

#include <stdbool.h>

#include "keybind.h"
#include "vpad.h"
#include <stddef.h>

typedef struct {
    int fps_limit;          // 0 = không giới hạn
    int screen_w, screen_h; // kích thước màn hình mặc định cho game
    int lang;               // Lang (lang.h)
    int keymap;             // KeyMapId (keymap.h)
    int scale_mode;         // 0 sắc nét (sharp-bilinear), 1 điểm ảnh, 2 điểm ảnh số nguyên
    bool show_help;         // hiện bảng phím bên trái khi chơi
    bool show_fps;          // hiện FPS và thời gian VM ở góc màn hình
    bool smooth_text;       // chữ trong game khử răng cưa (tắt: chữ điểm ảnh như điện thoại thật)
    bool system_font;       // chữ trong game dùng font hệ thống (tắt: font nhúng, font hệ thống chỉ bù ký tự thiếu)
    int font_scale;         // cỡ chữ trong game, % so với cỡ gốc
    bool check_update;      // mở app thì kiểm tra bản mới trên GitHub
    bool vkb_bubble;        // bong bóng bàn phím ảo QWERTY khi chơi
    char soundfont[128];    // file .sf2 trong <data_dir>/soundfonts; "" = tự động, "builtin" = bản có sẵn, "-" = tắt
    int keybinds[BIND_COUNT];   // nút Switch -> phím điện thoại (keybind.h)
    bool vpad;              // phím ảo trên màn hình khi chạy (vpad.h)
    VpadLayout vpad_layout;
    bool aot;               // chế độ AOT thử nghiệm: dịch sang mã máy khi nạp lớp (vm/aot.h)
} Settings;

typedef struct {
    int fps_limit;          // -1 = theo cài đặt chung
    int screen_w, screen_h; // 0 = tự động (MANIFEST, rồi tới cài đặt chung)
    int keymap;             // -1 = theo cài đặt chung
    int smooth_text;        // -1 = theo cài đặt chung, 0 tắt, 1 bật
    int system_font;        // -1 = theo cài đặt chung, 0 tắt, 1 bật
    int font_scale;         // -1 = theo cài đặt chung
    int keybinds[BIND_COUNT];   // BIND_INHERIT = theo cài đặt chung
    int vpad;               // phím ảo: -1 = theo cài đặt chung, 0 tắt, 1 bật
    int aot;                // chế độ AOT: -1 = theo cài đặt chung, 0 tắt, 1 bật
} GameSettings;

typedef struct {
    int w, h;
} ScreenSize;

// Trả về false khi chưa có settings.ini (mở app lần đầu)
bool settings_load(void);
bool settings_save(void);
Settings *settings(void);

void game_settings_load(const char *game, GameSettings *out);
bool game_settings_save(const char *game, const GameSettings *gs);

// Các lựa chọn
extern const int SETTINGS_FPS_CHOICES[];
extern const int SETTINGS_FPS_CHOICE_COUNT;
extern const int SETTINGS_FONT_SCALE_CHOICES[];
extern const int SETTINGS_FONT_SCALE_CHOICE_COUNT;
// Cỡ màn hình có sẵn, ghi theo hướng dọc (w <= h); hướng ngang = đảo w/h
extern const ScreenSize SETTINGS_SCREEN_CHOICES[];
extern const int SETTINGS_SCREEN_CHOICE_COUNT;

#define SCREEN_MIN 64
#define SCREEN_MAX 1280
bool settings_valid_screen(int w, int h);

// SoundFont: thư mục <data_dir>/soundfonts, các file .sf2 trong đó (sắp xếp theo tên)
#define SOUNDFONT_MAX 32
int settings_list_soundfonts(char names[][128], int max);
typedef enum {
    SOUNDFONT_OFF,          // bộ tổng hợp sóng
    SOUNDFONT_BUILTIN,      // TimGM6mb nhúng trong app
    SOUNDFONT_FILE,         // file .sf2 trên thẻ SD (đường dẫn ở out)
} SoundFontChoice;
// SoundFont sẽ dùng theo cài đặt. Tự động: file .sf2 đầu tiên trên thẻ, không có thì bản có sẵn;
// file đã chọn mà không còn thì cũng về bản có sẵn.
SoundFontChoice settings_soundfont(char *out, size_t size);
