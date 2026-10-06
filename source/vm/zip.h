// Đọc file ZIP/JAR (nạp cả file vào RAM, JAR J2ME thường < 2MB)
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct ZipFile ZipFile;

ZipFile *zip_open_file(const char *path);
// data phải sống lâu hơn ZipFile nếu owned = false
ZipFile *zip_open_mem(const uint8_t *data, size_t size, bool owned);
void zip_close(ZipFile *z);

bool zip_exists(ZipFile *z, const char *name);
// Trả về buffer malloc (thêm 1 byte '\0' ở cuối, không tính vào size), NULL nếu không có
uint8_t *zip_read(ZipFile *z, const char *name, size_t *size);
