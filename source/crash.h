// Báo cáo crash: mỗi lần crash ghi 1 file <data_dir>/crash/crash-YYYYMMDD-HHMMSS.txt
// (app crash: thanh ghi CPU + backtrace; game Java lỗi: exception + stack trace), kèm log gần nhất.
#pragma once

#include <stdbool.h>
#include <stddef.h>

// Cài bộ bắt crash (Switch: exception handler của libnx, desktop: signal). Gọi sớm trong main.
void crash_init(void);
// RAM dự phòng (giữ từ crash_init): nhả ra khi hết RAM / sập để còn chỗ thoát cho gọn, lấy lại khi đã có chỗ
void crash_release_reserve(void);
void crash_restore_reserve(void);
// Game đang chạy (NULL khi về danh sách game), để ghi vào báo cáo
void crash_set_game(const char *jar_path, const char *midlet_class);
// Thêm 1 dòng vào bộ đệm log gần nhất
void crash_log(const char *line);
// Log của app (khởi động, cập nhật, gửi game, quét danh sách, mở game...): vào bộ đệm log gần nhất và
// <data_dir>/app.log (ghi thẳng ra thẻ, sập giữa chừng vẫn còn; log lần mở trước ở app-prev.log).
// Gọi được từ mọi luồng, cả trước crash_init (khi đó chỉ vào bộ đệm).
void app_logf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
// Như app_logf nhưng không in ra stderr trên desktop (dòng đã được in ở chỗ khác, vd log của borealis)
void app_log_line(const char *line);
// Ghi báo cáo cho lỗi không làm app sập (game Java lỗi, không chạy được MIDlet).
// Trả về true và tên file (không kèm thư mục) ở name.
bool crash_write_report(const char *title, const char *detail, char *name, size_t name_size);
// File crash ghi lần trước (app sập), để báo khi mở lại app; false nếu không có. Gọi 1 lần.
bool crash_take_previous(char *name, size_t name_size);
// Xoá log.txt, log-prev.txt và báo cáo crash cũ hơn 1 tuần. Gọi sau crash_take_previous.
void crash_prune_logs(void);
