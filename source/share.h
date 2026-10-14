// Chia sẻ backup với điện thoại qua Wi-Fi: trang web (mở bằng mã QR) liệt kê backup của mọi ứng dụng
// để tải về, và nhận file backup (.zip) điện thoại gửi lên
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    int sent;               // số backup điện thoại đã tải về
    int received;           // số backup nhận từ điện thoại
    char current[160];      // file đang gửi / nhận ("" nếu không có)
    bool receiving;         // current là file đang nhận (false: đang gửi)
    int64_t done, total;    // tiến độ của current
    char last[160];         // file vừa gửi / nhận xong
    char error[160];        // lỗi gần nhất ("" nếu không có)
} ShareStatus;

bool share_start(char *url, size_t url_size, char *err, size_t err_size);
void share_stop(void);
void share_get_status(ShareStatus *out);
