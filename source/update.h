// Cập nhật app từ GitHub Releases: kiểm tra bản mới, tải j2me-nxx.nro, thay file đang chạy
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define UPDATE_REPO  "ToThangGTVT/j2me-nxx"
#define UPDATE_ASSET "j2me-nxx.nro"

typedef enum {
    UPDATE_IDLE,
    UPDATE_CHECKING,
    UPDATE_LATEST,          // đang dùng bản mới nhất
    UPDATE_AVAILABLE,       // có bản mới (update_latest_version)
    UPDATE_CHECK_FAILED,
    UPDATE_DOWNLOADING,
    UPDATE_DONE,            // đã thay file .nro
    UPDATE_FAILED,
} UpdateState;

// self_path: argv[0] (đường dẫn file .nro đang chạy)
void update_init(const char *self_path);
bool update_supported(void);
// Kiểm tra trên luồng nền (bỏ qua nếu đang bận)
void update_check(void);
UpdateState update_state(void);
const char *update_latest_version(void);    // vd "0.6.0"
// Đã biết có bản mới hơn bản đang chạy (kể cả khi lần tải trước lỗi / bị huỷ)
bool update_available(void);
const char *update_notes(void);             // ghi chú phát hành đã bỏ định dạng markdown
// Tải bản mới trên luồng nền rồi thay file .nro
void update_download(void);
void update_cancel(void);
// Byte đã tải, tổng (-1 nếu chưa biết), tốc độ byte/giây
void update_progress(int64_t *done, int64_t *total, int *bytes_per_sec);
void update_error(char *out, size_t size);
// Khởi động lại vào bản mới khi app thoát (Switch, chạy từ hbmenu). false nếu không làm được
bool update_restart(void);
// Huỷ và chờ luồng nền (trước khi thoát app)
void update_shutdown(void);
