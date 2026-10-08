// Chế độ AOT (thử nghiệm): khi nạp lớp, dịch bytecode của từng method sang mã máy ARM64,
// chạy chung với trình thông dịch.
//
// - Locals và operand stack vẫn nằm trong Frame như trình thông dịch; mã máy đọc / ghi thẳng
//   bằng offset cố định (độ sâu stack tại mỗi lệnh biết lúc dịch), không còn giải mã từng lệnh.
// - Lệnh đơn giản (hằng, biến cục bộ, số học, so sánh, nhánh, mảng, field, return...) chạy
//   trong mã máy. Lệnh phức tạp (gọi method, new, monitor, throw...) và mọi trường hợp sẽ ném
//   exception (null, chỉ số mảng, chia 0, chưa resolve, lớp chưa khởi tạo): ghi f->pc / f->sp
//   rồi trả AOT_STEP, trình thông dịch chạy đúng lệnh đó rồi quay lại mã máy ở lệnh kế tiếp.
//   Nhờ vậy green thread, exception, <clinit>, GC giữ nguyên như khi thông dịch.
// - Nhánh lùi trừ ngân sách như trình thông dịch; hết thì trả AOT_YIELD.
// - Mỗi lệnh đều là điểm vào (aot_entry[pc]) nên trình thông dịch vào lại ở đâu cũng được.
#pragma once

#include "vm.h"

enum {
    AOT_STEP,       // trình thông dịch chạy lệnh ở f->pc rồi vào lại mã máy
    AOT_YIELD,      // hết ngân sách, f->pc / f->sp đã ghi
    AOT_RETURN,     // đã return: frame đã bỏ, giá trị trả về đã đẩy lên stack của caller
    AOT_EXCEPTION,  // native ném exception (t->exception), f->pc ở lệnh gọi
};

// Mã máy của 1 method: gọi aot_code với entry = aot_code + aot_entry[pc]
typedef int (*AotCall)(VMThread *t, Frame *f, int *budget, const void *entry);

// Nền tảng có hỗ trợ (ARM64: Switch, macOS, Linux)
bool aot_available(void);
// Bật cho phiên VM này (vm_init khi host->aot). false nếu không hỗ trợ / không cấp được vùng nhớ.
bool aot_start(void);
// vm_shutdown: bỏ toàn bộ mã đã sinh
void aot_stop(void);
bool aot_active(void);
// Dịch method (gọi khi link lớp). Không dịch được thì để nguyên (thông dịch).
void aot_compile(Class *c, Method *m);
void aot_free_method(Method *m);

// --- vùng nhớ thực thi (aot_mem.c)
bool aot_mem_init(void);
// Chép mã đã sinh vào vùng thực thi, trả về địa chỉ chạy (NULL nếu hết chỗ)
void *aot_mem_put(const void *code, size_t size);
void aot_mem_reset(void);
size_t aot_mem_used(void);
