// Báo cáo crash: mỗi lần crash ghi 1 file <data_dir>/crash/crash-YYYYMMDD-HHMMSS.txt
// (app crash: thanh ghi CPU + backtrace; game Java lỗi: exception + stack trace), kèm log gần nhất.
#pragma once

#include <stdbool.h>
#include <stddef.h>

// Cài bộ bắt crash (Switch: exception handler của libnx, desktop: signal). Gọi sớm trong main.
void crash_init(void);
// Game đang chạy (NULL khi về danh sách game), để ghi vào báo cáo
void crash_set_game(const char *jar_path, const char *midlet_class);
// Thêm 1 dòng vào bộ đệm log gần nhất
void crash_log(const char *line);
// Log của app (khởi động, cập nhật, thoát): vào bộ đệm log gần nhất và <data_dir>/app.log
// (ghi thẳng ra thẻ, sập giữa chừng vẫn còn; log lần mở trước ở app-prev.log)
void crash_logf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
// Lỗi nghiêm trọng làm app không chạy tiếp được (vd lúc khởi động): ghi báo cáo crash,
// Switch hiện hộp thoại lỗi của hệ thống thay vì thoát im lặng
void crash_fatal(const char *what);
// Ghi báo cáo cho lỗi không làm app sập (game Java lỗi, không chạy được MIDlet).
// Trả về true và tên file (không kèm thư mục) ở name.
bool crash_write_report(const char *title, const char *detail, char *name, size_t name_size);
// File crash ghi lần trước (app sập), để báo khi mở lại app; false nếu không có. Gọi 1 lần.
bool crash_take_previous(char *name, size_t name_size);
