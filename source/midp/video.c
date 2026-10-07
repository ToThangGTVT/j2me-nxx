// Native cho j2menx.VideoPlayer: giải mã video bằng FFmpeg (video_dec.c), vẽ khung hình đè lên
// màn hình game (lớp phủ trong midp.c) hoặc chép ra mảng int cho Item trong Form.
// Tiếng của video do AudioPlayer phát (audio.c giải mã qua FFmpeg).
#include "midp.h"

#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "../video_dec.h"
#include "../vm/vm.h"

#define MAX_VIDEOS 8

typedef struct {
    bool used;
    VideoDec *dec;
    int w, h;                   // kích thước gốc của video
    uint32_t *rgb;              // khung hình đang hiện (w x h)
    bool has_frame;
    bool pending;               // dec đang giữ khung hình kế tiếp (chưa tới giờ hiện)
    int64_t pending_ms;
    bool ended;
    int overlay;                // -1 = chưa gắn lớp phủ
} Video;

static Video videos[MAX_VIDEOS + 1];       // chỉ số 0 không dùng

static Video *get(int h) {
    return h > 0 && h <= MAX_VIDEOS && videos[h].used ? &videos[h] : NULL;
}

static void video_free(Video *v) {
    if (v->overlay >= 0)
        midp_overlay_remove(v->overlay);
    vdec_close(v->dec);
    free(v->rgb);
    memset(v, 0, sizeof(*v));
}

static NativeResult V_probe0(VMThread *t, Value *args, Value *ret) {
    (void)t;
    Object *data = args[0].l;
    ret->i = data ? vdec_probe(ARRAY_DATA(data, uint8_t), (size_t)ARRAY_LEN(data)) : 0;
    return NATIVE_OK;
}

static NativeResult V_open0(VMThread *t, Value *args, Value *ret) {
    (void)t;
    Object *data = args[0].l;
    ret->i = 0;
    if (!data)
        return NATIVE_OK;
    int h = 0;
    for (int i = 1; i <= MAX_VIDEOS; i++) {
        if (!videos[i].used) {
            h = i;
            break;
        }
    }
    if (!h)
        return NATIVE_OK;
    VDecOptions opt = { .streams = VDEC_VIDEO };
    VideoDec *dec = vdec_open_mem(ARRAY_DATA(data, uint8_t), (size_t)ARRAY_LEN(data), &opt);
    if (!dec)
        return NATIVE_OK;
    Video *v = &videos[h];
    memset(v, 0, sizeof(*v));
    v->used = true;
    v->dec = dec;
    v->w = vdec_width(dec);
    v->h = vdec_height(dec);
    v->overlay = -1;
    v->rgb = calloc((size_t)v->w * v->h, 4);
    if (!v->rgb) {
        video_free(v);
        return NATIVE_OK;
    }
    ret->i = h;
    return NATIVE_OK;
}

static NativeResult V_width0(VMThread *t, Value *args, Value *ret) {
    (void)t;
    Video *v = get(args[0].i);
    ret->i = v ? v->w : 0;
    return NATIVE_OK;
}

static NativeResult V_height0(VMThread *t, Value *args, Value *ret) {
    (void)t;
    Video *v = get(args[0].i);
    ret->i = v ? v->h : 0;
    return NATIVE_OK;
}

static NativeResult V_duration0(VMThread *t, Value *args, Value *ret) {
    (void)t;
    Video *v = get(args[0].i);
    int64_t ms = v ? vdec_duration_ms(v->dec) : -1;
    ret->j = ms < 0 ? -1 : (jlong)ms * 1000;
    return NATIVE_OK;
}

// Hiện khung hình mới nhất có mốc <= now (micro giây). Trả về mốc của khung kế tiếp, -1 khi hết video.
static NativeResult V_frame0(VMThread *t, Value *args, Value *ret) {
    (void)t;
    Video *v = get(args[0].i);
    ret->j = -1;
    if (!v)
        return NATIVE_OK;
    int64_t now = args[1].j / 1000;
    bool shown = false;
    while (!v->ended) {
        if (!v->pending) {
            if (!vdec_next_frame(v->dec, &v->pending_ms)) {
                v->ended = true;
                break;
            }
            v->pending = true;
        }
        if (v->pending_ms > now && v->has_frame)
            break;
        // Tới giờ (hoặc chưa có khung nào): lấy khung này
        vdec_frame_argb(v->dec, v->rgb, v->w, v->h);
        v->has_frame = true;
        v->pending = false;
        shown = true;
        if (v->pending_ms > now)
            break;
    }
    if (shown && v->overlay >= 0)
        midp_overlay_frame(v->overlay, v->rgb, v->w, v->h);
    if (!v->ended)
        ret->j = (jlong)v->pending_ms * 1000;
    return NATIVE_OK;
}

static NativeResult V_seek0(VMThread *t, Value *args, Value *ret) {
    (void)t;
    Video *v = get(args[0].i);
    ret->i = 0;
    if (v && vdec_seek(v->dec, args[1].j / 1000)) {
        v->pending = false;
        v->ended = false;
        ret->i = 1;
    }
    return NATIVE_OK;
}

// Vị trí lớp phủ trên màn hình game; visible = false thì ẩn
static NativeResult V_display0(VMThread *t, Value *args, Value *ret) {
    (void)t;
    (void)ret;
    Video *v = get(args[0].i);
    if (!v)
        return NATIVE_OK;
    if (v->overlay < 0) {
        v->overlay = midp_overlay_add();
        if (v->overlay < 0)
            return NATIVE_OK;
        if (v->has_frame)
            midp_overlay_frame(v->overlay, v->rgb, v->w, v->h);
    }
    midp_overlay_set(v->overlay, args[5].i != 0, args[1].i, args[2].i, args[3].i, args[4].i);
    return NATIVE_OK;
}

// Chép khung hình hiện tại (co giãn) vào int[w*h]; false nếu chưa có khung nào
static NativeResult V_copyFrame0(VMThread *t, Value *args, Value *ret) {
    (void)t;
    Video *v = get(args[0].i);
    Object *arr = args[1].l;
    int w = args[2].i, h = args[3].i;
    ret->i = 0;
    if (!v || !v->has_frame || !arr || w <= 0 || h <= 0 || ARRAY_LEN(arr) < w * h)
        return NATIVE_OK;
    uint32_t *dst = ARRAY_DATA(arr, uint32_t);
    for (int y = 0; y < h; y++) {
        const uint32_t *src = v->rgb + (size_t)(y * v->h / h) * v->w;
        for (int x = 0; x < w; x++)
            dst[y * w + x] = src[x * v->w / w];
    }
    ret->i = 1;
    return NATIVE_OK;
}

// ---------------------------------------------------------------------------
// getSnapshot: khung hình hiện tại -> PNG

static void put32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static size_t png_chunk(uint8_t *out, const char *type, const uint8_t *data, uint32_t len) {
    put32(out, len);
    memcpy(out + 4, type, 4);
    if (len)
        memcpy(out + 8, data, len);
    uint32_t crc = (uint32_t)crc32(0, out + 4, len + 4);
    put32(out + 8 + len, crc);
    return 12 + len;
}

static uint8_t *encode_png(const uint32_t *argb, int w, int h, size_t *out_len) {
    size_t raw_len = (size_t)(w * 3 + 1) * h;
    uint8_t *raw = malloc(raw_len);
    uLongf z_len = compressBound((uLong)raw_len);
    uint8_t *z = malloc(z_len);
    uint8_t *png = raw && z ? malloc(z_len + 64) : NULL;
    if (!png) {
        free(raw);
        free(z);
        return NULL;
    }
    for (int y = 0; y < h; y++) {
        uint8_t *row = raw + (size_t)(w * 3 + 1) * y;
        row[0] = 0;     // không lọc
        for (int x = 0; x < w; x++) {
            uint32_t c = argb[y * w + x];
            row[1 + x * 3] = (uint8_t)(c >> 16);
            row[2 + x * 3] = (uint8_t)(c >> 8);
            row[3 + x * 3] = (uint8_t)c;
        }
    }
    compress2(z, &z_len, raw, (uLong)raw_len, 6);
    free(raw);
    static const uint8_t sig[8] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n' };
    memcpy(png, sig, 8);
    size_t n = 8;
    uint8_t ihdr[13];
    put32(ihdr, (uint32_t)w);
    put32(ihdr + 4, (uint32_t)h);
    ihdr[8] = 8;    // 8 bit
    ihdr[9] = 2;    // RGB
    ihdr[10] = ihdr[11] = ihdr[12] = 0;
    n += png_chunk(png + n, "IHDR", ihdr, 13);
    n += png_chunk(png + n, "IDAT", z, (uint32_t)z_len);
    n += png_chunk(png + n, "IEND", NULL, 0);
    free(z);
    *out_len = n;
    return png;
}

static NativeResult V_snapshot0(VMThread *t, Value *args, Value *ret) {
    Video *v = get(args[0].i);
    ret->l = NULL;
    if (!v || !v->has_frame)
        return NATIVE_OK;
    size_t len = 0;
    uint8_t *png = encode_png(v->rgb, v->w, v->h, &len);
    if (!png)
        return NATIVE_OK;
    Object *arr = heap_new_prim_array(t, 'B', (jint)len);
    if (arr)
        memcpy(ARRAY_DATA(arr, uint8_t), png, len);
    free(png);
    ret->l = arr;
    return NATIVE_OK;
}

static NativeResult V_close0(VMThread *t, Value *args, Value *ret) {
    (void)t;
    (void)ret;
    Video *v = get(args[0].i);
    if (v)
        video_free(v);
    return NATIVE_OK;
}

void midp_video_register(void) {
    const char *V = "j2menx/VideoPlayer";
    native_register(V, "probe0", "([B)I", V_probe0);
    native_register(V, "open0", "([B)I", V_open0);
    native_register(V, "width0", "(I)I", V_width0);
    native_register(V, "height0", "(I)I", V_height0);
    native_register(V, "duration0", "(I)J", V_duration0);
    native_register(V, "frame0", "(IJ)J", V_frame0);
    native_register(V, "seek0", "(IJ)Z", V_seek0);
    native_register(V, "display0", "(IIIIIZ)V", V_display0);
    native_register(V, "copyFrame0", "(I[III)Z", V_copyFrame0);
    native_register(V, "snapshot0", "(I)[B", V_snapshot0);
    native_register(V, "close0", "(I)V", V_close0);
}

void midp_video_shutdown(void) {
    for (int i = 1; i <= MAX_VIDEOS; i++) {
        if (videos[i].used) {
            videos[i].overlay = -1;     // lớp phủ đã bị xoá cùng framebuffer
            video_free(&videos[i]);
        }
    }
}
