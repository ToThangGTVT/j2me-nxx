// Giải mã video / âm thanh bằng FFmpeg: 3GP, MP4, AVI, MKV... (H.263, MPEG-4, H.264...; AMR, AAC, MP3...)
// Dùng cho Player video của J2ME (midp/video.c), AMR/AAC của AudioPlayer và trình xem video (video_screen.c).
// Build không có FFmpeg (J2ME_NX_NO_VIDEO): mọi hàm mở đều trả về NULL / false.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct VideoDec VideoDec;

// Luồng cần giải mã
enum {
    VDEC_VIDEO = 1,
    VDEC_AUDIO = 2,
};

typedef struct {
    int streams;            // VDEC_VIDEO | VDEC_AUDIO; luồng không cần thì bỏ gói của nó
    int audio_rate;         // tần số mẫu ra (mặc định 22050)
    int audio_channels;     // 1 hoặc 2 (mặc định 1)
    int threads;            // số luồng giải mã video (mặc định 1)
    volatile int *abort;    // mở URL: khác 0 thì huỷ đọc mạng (NULL = không dùng)
} VDecOptions;

typedef struct {
    const uint8_t *y, *u, *v;
    int y_pitch, u_pitch, v_pitch;
} VDecYUV;

bool vdec_available(void);

// Mở từ bộ nhớ (chép dữ liệu) hoặc từ file
VideoDec *vdec_open_mem(const uint8_t *data, size_t size, const VDecOptions *opt);
VideoDec *vdec_open_file(const char *path, const VDecOptions *opt);
// Mở luồng mạng (http://...; https:// nếu FFmpeg có TLS). Chặn tới khi đọc xong phần đầu.
VideoDec *vdec_open_url(const char *url, const VDecOptions *opt);
void vdec_close(VideoDec *d);

bool vdec_has_video(const VideoDec *d);
bool vdec_has_audio(const VideoDec *d);
int vdec_width(const VideoDec *d);
int vdec_height(const VideoDec *d);
// Tỉ lệ hiển thị rộng / cao (tính cả điểm ảnh không vuông)
double vdec_aspect(const VideoDec *d);
// Độ dài (ms), -1 nếu không rõ
int64_t vdec_duration_ms(const VideoDec *d);

// Giải mã khung hình tiếp theo (thành khung hình hiện tại); false khi hết video
bool vdec_next_frame(VideoDec *d, int64_t *pts_ms);
// Khung hình hiện tại -> ARGB8888 kích thước w x h
bool vdec_frame_argb(VideoDec *d, uint32_t *dst, int w, int h);
// Khung hình hiện tại dạng YUV 4:2:0 (đổi định dạng nếu cần); con trỏ dùng tới lần giải mã sau
bool vdec_frame_yuv(VideoDec *d, VDecYUV *out);

// Đọc tối đa max mẫu âm thanh (mỗi mẫu gồm audio_channels giá trị int16 xen kẽ).
// Trả về số mẫu đọc được, 0 khi hết.
int vdec_read_audio(VideoDec *d, int16_t *out, int max);

// Tua tới mốc ms: khung hình / mẫu âm thanh trước mốc bị bỏ qua
bool vdec_seek(VideoDec *d, int64_t ms);

// Có luồng hình / tiếng nào FFmpeg đọc được (VDEC_VIDEO | VDEC_AUDIO), 0 nếu không phải file media
int vdec_probe(const uint8_t *data, size_t size);
// Giải mã toàn bộ âm thanh thành PCM int16 (malloc, người gọi free); frames = số mẫu
bool vdec_decode_audio(const uint8_t *data, size_t size, int rate, int channels, int16_t **pcm, size_t *frames);
