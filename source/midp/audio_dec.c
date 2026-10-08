// AMR-NB / AMR-WB qua opencore-amr, AAC qua faad2, bóc tách MP4 / M4A / 3GP bằng minimp4.
// Kết quả là PCM mono ở tần số gốc; audio.c đổi tần số khi phát.
#include "audio_dec.h"

#include <stdlib.h>
#include <string.h>

#include <neaacdec.h>
#include <interf_dec.h>
#include <dec_if.h>

#define MINIMP4_IMPLEMENTATION
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-compare"
#endif
#include "../third_party/minimp4.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#define MAX_SECONDS 600     // giới hạn độ dài khi giải mã cả file (tránh hết RAM với file hỏng)

// Bộ đệm PCM mono tăng dần
typedef struct {
    int16_t *data;
    size_t len, cap;
    size_t max;
} Pcm;

static bool pcm_push(Pcm *p, const int16_t *src, size_t n, int channels) {
    if (p->len + n > p->max)
        return false;
    if (p->len + n > p->cap) {
        size_t cap = p->cap ? p->cap * 2 : 16384;
        while (cap < p->len + n)
            cap *= 2;
        int16_t *d = realloc(p->data, cap * sizeof(int16_t));
        if (!d)
            return false;
        p->data = d;
        p->cap = cap;
    }
    for (size_t i = 0; i < n; i++) {
        int acc = 0;
        for (int c = 0; c < channels; c++)
            acc += src[i * channels + c];
        p->data[p->len + i] = (int16_t)(acc / channels);
    }
    p->len += n;
    return true;
}

// ---------------------------------------------------------------------------
// AMR: mỗi khung bắt đầu bằng 1 byte (loại khung << 3); kích thước phần dữ liệu theo loại khung

static const uint8_t amrnb_bytes[16] = { 12, 13, 15, 17, 19, 20, 26, 31, 5, 0, 0, 0, 0, 0, 0, 0 };
static const uint8_t amrwb_bytes[16] = { 17, 23, 32, 36, 40, 46, 50, 58, 60, 5, 0, 0, 0, 0, 0, 0 };

#define AMRNB_SAMPLES 160   // 20ms ở 8000Hz
#define AMRWB_SAMPLES 320   // 20ms ở 16000Hz

typedef struct {
    bool wb;
    void *state;
} Amr;

static bool amr_open(Amr *a, bool wb) {
    a->wb = wb;
    a->state = wb ? D_IF_init() : Decoder_Interface_init();
    return a->state != NULL;
}

static void amr_close(Amr *a) {
    if (!a->state)
        return;
    if (a->wb)
        D_IF_exit(a->state);
    else
        Decoder_Interface_exit(a->state);
    a->state = NULL;
}

// Giải mã các khung liền nhau (định dạng lưu trữ RFC 4867); trả về false khi bộ đệm đầy
static bool amr_frames(Amr *a, const uint8_t *d, size_t n, Pcm *out) {
    const uint8_t *sizes = a->wb ? amrwb_bytes : amrnb_bytes;
    int samples = a->wb ? AMRWB_SAMPLES : AMRNB_SAMPLES;
    int16_t buf[AMRWB_SAMPLES];
    uint8_t frame[64];
    size_t pos = 0;
    while (pos < n) {
        int ft = (d[pos] >> 3) & 0x0F;
        size_t len = 1 + sizes[ft];
        if (pos + len > n)
            break;
        // Bộ giải mã có thể đọc quá độ dài khung: chép ra bộ đệm đủ lớn
        memset(frame, 0, sizeof(frame));
        memcpy(frame, d + pos, len);
        if (a->wb)
            D_IF_decode(a->state, frame, buf, _good_frame);
        else
            Decoder_Interface_Decode(a->state, frame, buf, 0);
        if (!pcm_push(out, buf, (size_t)samples, 1))
            return false;
        pos += len;
    }
    return true;
}

static bool decode_amr_file(const uint8_t *d, size_t n, bool wb, Pcm *out, int *rate) {
    size_t magic = wb ? 9 : 6;
    Amr a;
    if (!amr_open(&a, wb))
        return false;
    amr_frames(&a, d + magic, n - magic, out);
    amr_close(&a);
    *rate = wb ? 16000 : 8000;
    return out->len > 0;
}

// ---------------------------------------------------------------------------
// AAC

static NeAACDecHandle aac_open(void) {
    NeAACDecHandle h = NeAACDecOpen();
    if (!h)
        return NULL;
    NeAACDecConfigurationPtr cfg = NeAACDecGetCurrentConfiguration(h);
    cfg->outputFormat = FAAD_FMT_16BIT;
    cfg->downMatrix = 1;
    NeAACDecSetConfiguration(h, cfg);
    return h;
}

// Giải mã 1 khung; rate lấy theo khung đầu tiên giải mã được (HE-AAC có thể gấp đôi tần số khai báo)
static bool aac_frame(NeAACDecHandle h, const uint8_t *d, size_t n, Pcm *out, int *rate, size_t *consumed) {
    NeAACDecFrameInfo info;
    memset(&info, 0, sizeof(info));
    int16_t *s = NeAACDecDecode(h, &info, (unsigned char *)d, (unsigned long)n);
    if (consumed)
        *consumed = info.bytesconsumed;
    if (info.error || !s || info.channels == 0)
        return info.error == 0;
    if (!*rate)
        *rate = (int)info.samplerate;
    return pcm_push(out, s, info.samples / info.channels, info.channels);
}

static bool decode_adts(const uint8_t *d, size_t n, Pcm *out, int *rate) {
    NeAACDecHandle h = aac_open();
    if (!h)
        return false;
    unsigned long sr;
    unsigned char ch;
    long skip = NeAACDecInit(h, (unsigned char *)d, (unsigned long)n, &sr, &ch);
    if (skip < 0) {
        NeAACDecClose(h);
        return false;
    }
    size_t pos = (size_t)skip;
    int errors = 0;
    while (pos + 7 < n) {
        size_t used = 0;
        if (!aac_frame(h, d + pos, n - pos, out, rate, &used)) {
            // Khung hỏng: tìm từ đồng bộ ADTS tiếp theo
            if (++errors > 32)
                break;
            pos++;
            while (pos + 1 < n && !(d[pos] == 0xFF && (d[pos + 1] & 0xF6) == 0xF0))
                pos++;
            continue;
        }
        if (used == 0)
            break;
        pos += used;
    }
    NeAACDecClose(h);
    return out->len > 0 && *rate > 0;
}

// ---------------------------------------------------------------------------
// MP4 / M4A / 3GP: track tiếng đầu tiên (AAC, hoặc AMR với 3GP)

typedef struct {
    const uint8_t *data;
    size_t size;
} MemFile;

static int mem_read(int64_t offset, void *buffer, size_t size, void *token) {
    MemFile *f = token;
    if (offset < 0 || (size_t)offset > f->size || size > f->size - (size_t)offset)
        return 1;
    memcpy(buffer, f->data + offset, size);
    return 0;
}

static bool has_fourcc(const uint8_t *d, size_t n, const char *cc) {
    for (size_t i = 0; i + 4 <= n; i++) {
        if (memcmp(d + i, cc, 4) == 0)
            return true;
    }
    return false;
}

static bool decode_mp4(const uint8_t *d, size_t n, Pcm *out, int *rate) {
    MemFile f = { d, n };
    MP4D_demux_t mp4;
    if (!MP4D_open(&mp4, mem_read, &f, (int64_t)n))
        return false;
    bool ok = false;
    for (unsigned t = 0; t < mp4.track_count && !ok; t++) {
        MP4D_track_t *tr = &mp4.track[t];
        if (tr->handler_type != MP4D_HANDLER_TYPE_SOUN || tr->sample_count == 0)
            continue;
        bool aac = tr->object_type_indication == MP4_OBJECT_TYPE_AUDIO_ISO_IEC_14496_3 ||
                   tr->object_type_indication == MP4_OBJECT_TYPE_AUDIO_ISO_IEC_13818_7_MAIN_PROFILE ||
                   tr->object_type_indication == MP4_OBJECT_TYPE_AUDIO_ISO_IEC_13818_7_LC_PROFILE;
        if (aac && tr->dsi && tr->dsi_bytes) {
            NeAACDecHandle h = aac_open();
            unsigned long sr;
            unsigned char ch;
            if (h && NeAACDecInit2(h, tr->dsi, tr->dsi_bytes, &sr, &ch) == 0) {
                for (unsigned s = 0; s < tr->sample_count; s++) {
                    unsigned bytes = 0, ts, dur;
                    MP4D_file_offset_t off = MP4D_frame_offset(&mp4, t, s, &bytes, &ts, &dur);
                    if (!bytes || off + bytes > n)
                        continue;
                    if (!aac_frame(h, d + off, bytes, out, rate, NULL) && out->len >= out->max)
                        break;
                }
                ok = out->len > 0 && *rate > 0;
            }
            if (h)
                NeAACDecClose(h);
            continue;
        }
        // 3GP có tiếng AMR: mỗi mẫu là vài khung AMR liền nhau. Loại (thường / băng rộng) xem ở mô tả mẫu
        bool wb = has_fourcc(d, n, "sawb");
        if (!wb && !has_fourcc(d, n, "samr"))
            continue;
        Amr a;
        if (!amr_open(&a, wb))
            continue;
        for (unsigned s = 0; s < tr->sample_count; s++) {
            unsigned bytes = 0, ts, dur;
            MP4D_file_offset_t off = MP4D_frame_offset(&mp4, t, s, &bytes, &ts, &dur);
            if (!bytes || off + bytes > n)
                continue;
            if (!amr_frames(&a, d + off, bytes, out))
                break;
        }
        amr_close(&a);
        *rate = wb ? 16000 : 8000;
        ok = out->len > 0;
    }
    MP4D_close(&mp4);
    return ok;
}

// ---------------------------------------------------------------------------

typedef enum {
    FMT_NONE,
    FMT_AMRNB,
    FMT_AMRWB,
    FMT_ADTS,
    FMT_MP4,
} Format;

static Format detect(const uint8_t *d, size_t n) {
    if (n >= 9 && memcmp(d, "#!AMR-WB\n", 9) == 0)
        return FMT_AMRWB;
    if (n >= 6 && memcmp(d, "#!AMR\n", 6) == 0)
        return FMT_AMRNB;
    if (n >= 12 && memcmp(d + 4, "ftyp", 4) == 0)
        return FMT_MP4;
    // ADTS: từ đồng bộ 12 bit, layer = 0 (MP3 không bao giờ có layer 0)
    if (n >= 7 && d[0] == 0xFF && (d[1] & 0xF6) == 0xF0)
        return FMT_ADTS;
    return FMT_NONE;
}

bool audio_dec_probe(const uint8_t *data, size_t size) {
    return data && detect(data, size) != FMT_NONE;
}

bool audio_dec_decode(const uint8_t *data, size_t size, int16_t **pcm, size_t *frames, int *rate) {
    *pcm = NULL;
    *frames = 0;
    *rate = 0;
    if (!data)
        return false;
    Pcm out = { .max = (size_t)MAX_SECONDS * 48000 };
    bool ok = false;
    switch (detect(data, size)) {
    case FMT_AMRNB:
        ok = decode_amr_file(data, size, false, &out, rate);
        break;
    case FMT_AMRWB:
        ok = decode_amr_file(data, size, true, &out, rate);
        break;
    case FMT_ADTS:
        ok = decode_adts(data, size, &out, rate);
        break;
    case FMT_MP4:
        ok = decode_mp4(data, size, &out, rate);
        break;
    case FMT_NONE:
        break;
    }
    if (!ok) {
        free(out.data);
        return false;
    }
    *pcm = out.data;
    *frames = out.len;
    return true;
}
