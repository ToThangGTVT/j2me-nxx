// Nhóm ứng dụng ở màn hình chính (<data_dir>/groups.ini): tab "Tất cả" cố định của giao diện,
// còn các nhóm ở đây (mặc định có "Yêu thích") người dùng đổi tên, thêm, xoá được.
// Một ứng dụng có thể nằm trong nhiều nhóm; khoá là id của game_list_id.
#pragma once

#include <stdbool.h>

#define GROUP_MAX      12
#define GROUP_NAME_MAX 48

int group_count(void);
const char *group_name(int g);
bool group_has(int g, const char *game);
// Thêm / bỏ ứng dụng khỏi nhóm rồi ghi file ngay
bool group_set(int g, const char *game, bool on);
// Trả về vị trí nhóm mới, -1 nếu tên rỗng hoặc đã đủ GROUP_MAX nhóm
int group_add(const char *name);
bool group_rename(int g, const char *name);
bool group_delete(int g);
