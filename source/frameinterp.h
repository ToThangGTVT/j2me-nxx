// Nội suy khung hình (thử nghiệm): game vẽ 15-30 khung/giây thì dựng thêm khung ở giữa để màn hình chạy
// tới 50-60 khung/giây. Ước lượng chuyển động theo khối 8x8 giữa 2 khung gần nhất (kiểu 3DRS: thử vài vector
// ứng viên lấy từ khối bên cạnh, khung trước và chuyển động chung của cả màn hình), rồi dựng khung ở thời điểm t
// bằng cách dời 2 khung theo vector và trộn. Khối không khớp (đổi hình hoạt hoạ, vật che nhau) lấy nguyên khung
// gần hơn; đổi cảnh (đa số khối không khớp) thì không nội suy.
// Cái giá: hình hiện chậm hơn 1 khung của game (phải có khung sau mới dựng được khung giữa).
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void frameinterp_reset(void);
// Khung hình mới của game, now tính bằng ms; hi: bản nét cao gấp k lần (chữ font hệ thống) nếu có. Chỉ chép
// (gọi lúc đang giữ khoá framebuffer), việc tính để frameinterp_frame làm. false = không nội suy được (màn hình
// quá lớn, hết RAM): tự đưa khung lên như thường.
bool frameinterp_push(const uint32_t *fb, int w, int h, const uint32_t *hi, int k, double now);
// Ảnh cần hiện lúc now, NULL = giữ ảnh đang hiện. hires: ảnh là bản nét cao (gấp k lần) hay khung gốc
const uint32_t *frameinterp_frame(double now, bool *hires);

#ifdef __cplusplus
}
#endif
