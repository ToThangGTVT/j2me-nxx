// Tạo mã QR (chế độ byte, mức sửa lỗi M, phiên bản 1-10) cho đường dẫn ngắn
#pragma once

#include <stdint.h>

#define QR_MAX_SIZE 57  // phiên bản 10

// Mã hoá text vào modules (QR_MAX_SIZE * QR_MAX_SIZE, hàng y cột x: modules[y * size + x], 1 = ô tối).
// Trả về số ô mỗi cạnh, 0 nếu text quá dài.
int qr_encode(const char *text, uint8_t *modules);
