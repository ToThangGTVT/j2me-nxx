// HTTP(S) GET chặn (blocking) cho luồng nền: kiểm tra / tải bản cập nhật.
// TLS qua mbedTLS, không kiểm tra chứng chỉ (giống https:// của game). Theo redirect, đọc được chunked.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    const char *accept;                 // header Accept (NULL = */*)
    volatile bool *cancel;              // true thì dừng sớm (kiểm tra mỗi giây)
    // Gọi 1 lần khi biết độ dài nội dung (-1 nếu không rõ)
    void (*on_length)(void *ctx, int64_t length);
    // Nhận dữ liệu; trả về false để dừng
    bool (*sink)(void *ctx, const uint8_t *data, size_t len);
    void *ctx;
} HttpOptions;

// Trả về mã HTTP cuối cùng (200...), hoặc -1 khi lỗi mạng / bị huỷ (err ghi lý do)
int http_get(const char *url, const HttpOptions *opt, char *err, size_t err_size);

// Build có TLS (mbedTLS) không
bool http_supported(void);
