// Web server nhỏ cho điện thoại trong cùng mạng Wi-Fi (gửi game, chia sẻ backup): chạy trên luồng nền,
// mỗi lần một kết nối. Cùng lúc chỉ có một server (mở server mới thì server cũ tắt).
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef struct {
    int fd;
    char method[8];
    char path[1024];        // gồm cả ?query
    int64_t length;         // Content-Length, -1 nếu không có
    const char *body;       // phần thân đã đọc cùng header
    int body_len;
} WebRequest;

typedef void (*WebHandler)(WebRequest *req);

// Mở server, ghi địa chỉ trang vào url. Lỗi thì trả false và ghi lý do vào err
bool web_start(WebHandler handler, char *url, size_t url_size, char *err, size_t err_size);
void web_stop(void);
// Đang tắt server: các vòng lặp gửi / nhận dài nên dừng
bool web_stopping(void);

bool web_send_all(int fd, const void *buf, size_t len);
// > 0: số byte, 0: điện thoại đóng kết nối, < 0: lỗi / quá hạn / đang tắt server
int web_recv_some(int fd, char *buf, int len);
// Bỏ phần thân còn lại, để điện thoại đọc được câu trả lời lỗi
void web_drain(int fd, int64_t left);

// Dòng trạng thái + header (extra: header thêm, mỗi dòng kết thúc bằng \r\n, có thể NULL)
bool web_send_head(int fd, int code, const char *type, int64_t len, const char *extra);
void web_send_response(int fd, int code, const char *type, const char *body, size_t len);
void web_send_json(int fd, int code, bool ok, const char *error);

// Chuỗi JSON có ngoặc kép: thoát " \ và ký tự điều khiển
void web_json_str(char *out, size_t size, const char *s);
// Giá trị (đã giải mã %XX) của tham số key trong query của path; false nếu không có
bool web_query(const char *path, const char *key, char *out, size_t size);

typedef enum {
    WEB_RECV_OK,
    WEB_RECV_NET,       // mất kết nối / quá hạn
    WEB_RECV_WRITE,     // không ghi được file
} WebRecvResult;

// Ghi thân request (length byte) vào f; progress(done, arg) sau mỗi đoạn nhận được
WebRecvResult web_recv_to_file(WebRequest *req, FILE *f, void (*progress)(int64_t done, void *arg), void *arg);
