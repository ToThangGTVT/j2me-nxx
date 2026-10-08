// Nhận game từ điện thoại qua Wi-Fi: web server nhỏ chạy trên luồng nền, trang tải lên ở
// http://<ip>:<port>/ (hiện bằng mã QR). File .jar / .jad được lưu thẳng vào thư mục games.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    int received;           // số file đã nhận xong từ lúc mở
    char last[128];         // file vừa nhận xong
    char current[128];      // file đang nhận ("" nếu không có)
    int64_t done, total;    // số byte đã nhận / tổng của file đang nhận
    char error[160];        // lỗi gần nhất ("" nếu không có)
} UploadStatus;

// Mở server, ghi địa chỉ trang tải lên vào url. Lỗi thì trả false và ghi lý do vào err
bool upload_start(const char *dir, char *url, size_t url_size, char *err, size_t err_size);
void upload_stop(void);
void upload_get_status(UploadStatus *out);
