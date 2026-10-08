// Màn hình chọn game
#pragma once

#include "game_list.h"
#include "input.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MENU_NONE,
    MENU_LAUNCH,    // chọn game: menu.cursor là index trong list
    MENU_RESCAN,
    MENU_SETTINGS,
    MENU_GAME_OPTIONS,  // tuỳ chọn riêng của game menu.cursor
    MENU_UPDATE,        // mở màn hình cập nhật (có bản mới)
    MENU_UPLOAD,        // gửi game từ điện thoại (mã QR)
    MENU_DELETE,        // đã xác nhận xoá file menu.cursor
    MENU_QUIT,
} MenuAction;

typedef struct {
    int cursor;
    int scroll;
    char status[160];
    bool picking;       // đang hiện hộp chọn MIDlet
    int pick;           // MIDlet đang chọn (0-based)
    int midlet;         // MIDlet sẽ chạy khi MENU_LAUNCH (từ 1)
    bool confirm_delete;    // đang hỏi có xoá file đang chọn không
    bool delete_has_jad;    // file đang hỏi xoá có kèm .jad
} Menu;

// Xử lý 1 lần bấm nút (có lặp khi giữ nút hướng)
MenuAction menu_press(Menu *m, GameList *list, Button b);
// Chạm vào toạ độ (x, y) của màn hình 1280x720: chọn dòng, chạm lại dòng đang chọn thì mở
MenuAction menu_tap(Menu *m, GameList *list, int x, int y);
// Kéo để cuộn: dy điểm ảnh tính từ lúc bắt đầu kéo (scroll0: vị trí cuộn lúc đó)
void menu_drag(Menu *m, const GameList *list, int scroll0, int dy);
// Vẽ danh sách; đọc dần thông tin (MANIFEST, icon) của các game đang hiện
void menu_draw(const Menu *m, GameList *list, const char *games_dir);
// Sau khi danh sách đổi (vd xoá file): giữ con trỏ ở gần chỗ cũ
void menu_clamp_cursor(Menu *m, const GameList *list);
// Giải phóng texture icon trước khi quét lại / thoát
void menu_free_textures(GameList *list);

#ifdef __cplusplus
}
#endif
