#include "video_dec.h"

#ifdef J2ME_NX_NO_VIDEO

bool vdec_available(void) { return false; }
VideoDec *vdec_open_mem(const uint8_t *data, size_t size, const VDecOptions *opt) { (void)data; (void)size; (void)opt; return NULL; }
VideoDec *vdec_open_file(const char *path, const VDecOptions *opt) { (void)path; (void)opt; return NULL; }
VideoDec *vdec_open_url(const char *url, const VDecOptions *opt) { (void)url; (void)opt; return NULL; }
void vdec_close(VideoDec *d) { (void)d; }
bool vdec_has_video(const VideoDec *d) { (void)d; return false; }
bool vdec_has_audio(const VideoDec *d) { (void)d; return false; }
int vdec_width(const VideoDec *d) { (void)d; return 0; }
int vdec_height(const VideoDec *d) { (void)d; return 0; }
double vdec_aspect(const VideoDec *d) { (void)d; return 1.0; }
int64_t vdec_duration_ms(const VideoDec *d) { (void)d; return -1; }
bool vdec_next_frame(VideoDec *d, int64_t *pts_ms) { (void)d; (void)pts_ms; return false; }
bool vdec_frame_argb(VideoDec *d, uint32_t *dst, int w, int h) { (void)d; (void)dst; (void)w; (void)h; return false; }
bool vdec_frame_yuv(VideoDec *d, VDecYUV *out) { (void)d; (void)out; return false; }
int vdec_read_audio(VideoDec *d, int16_t *out, int max) { (void)d; (void)out; (void)max; return 0; }
bool vdec_seek(VideoDec *d, int64_t ms) { (void)d; (void)ms; return false; }
int vdec_probe(const uint8_t *data, size_t size) { (void)data; (void)size; return 0; }
bool vdec_decode_audio(const uint8_t *data, size_t size, int rate, int channels, int16_t **pcm, size_t *frames) {
    (void)data; (void)size; (void)rate; (void)channels; (void)pcm; (void)frames;
    return false;
}

#else

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/imgutils.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>

#define IO_BUF 32768

typedef struct {
    AVPacket **items;
    int head, count, cap;
} PacketQueue;

struct VideoDec {
    // Nguồn dữ liệu: bộ nhớ hoặc file
    uint8_t *mem;
    int64_t mem_size, mem_pos;
    FILE *file;
    int64_t file_size;
    const char *url;                // mở bằng giao thức của FFmpeg (http)
    volatile int *abort;

    AVIOContext *avio;
    AVFormatContext *fmt;
    int64_t start_ms;               // mốc 0 của cả file (start_time)
    int vs, as;                     // chỉ số luồng hình / tiếng, -1 nếu không dùng
    AVCodecContext *vctx, *actx;
    PacketQueue vq, aq;
    bool demux_eof;
    bool vflushed, aflushed;        // đã gửi gói rỗng (xả bộ giải mã)
    bool vdone, adone;
    AVFrame *vframe, *aframe;
    int64_t last_vpts;

    // Đổi khung hình
    struct SwsContext *sws_argb, *sws_yuv;
    int argb_src_w, argb_src_h, argb_src_fmt, argb_w, argb_h;
    int yuv_src_fmt;
    uint8_t *yuv_buf[4];            // av_image_alloc ghi đủ 4 mặt phẳng
    int yuv_pitch[4];

    // Âm thanh ra: int16 xen kẽ
    SwrContext *swr;
    int out_rate, out_ch;
    int16_t *fifo;
    int fifo_len, fifo_pos, fifo_cap;   // tính theo mẫu (mỗi mẫu out_ch giá trị)

    // Sau khi tua: bỏ khung hình / mẫu trước mốc này (-1 = không bỏ)
    int64_t skip_video, skip_audio;
};

bool vdec_available(void) {
    return true;
}

// ---------------------------------------------------------------------------
// Đọc dữ liệu

static int mem_read(void *opaque, uint8_t *buf, int size) {
    VideoDec *d = opaque;
    int64_t left = d->mem_size - d->mem_pos;
    if (left <= 0)
        return AVERROR_EOF;
    if (size > left)
        size = (int)left;
    memcpy(buf, d->mem + d->mem_pos, (size_t)size);
    d->mem_pos += size;
    return size;
}

static int64_t mem_seek(void *opaque, int64_t offset, int whence) {
    VideoDec *d = opaque;
    if (whence & AVSEEK_SIZE)
        return d->mem_size;
    whence &= ~AVSEEK_FORCE;
    int64_t pos = whence == SEEK_SET ? offset : whence == SEEK_CUR ? d->mem_pos + offset : d->mem_size + offset;
    if (pos < 0 || pos > d->mem_size)
        return -1;
    d->mem_pos = pos;
    return pos;
}

static int file_read(void *opaque, uint8_t *buf, int size) {
    VideoDec *d = opaque;
    size_t n = fread(buf, 1, (size_t)size, d->file);
    return n > 0 ? (int)n : AVERROR_EOF;
}

static int64_t file_seek(void *opaque, int64_t offset, int whence) {
    VideoDec *d = opaque;
    if (whence & AVSEEK_SIZE)
        return d->file_size;
    whence &= ~AVSEEK_FORCE;
    if (fseeko(d->file, (off_t)offset, whence) != 0)
        return -1;
    return (int64_t)ftello(d->file);
}

// ---------------------------------------------------------------------------
// Hàng đợi gói (demux 1 lần, gói hình và tiếng chia ra 2 hàng)

static void queue_push(PacketQueue *q, AVPacket *p) {
    if (q->head > 0 && q->head == q->count) {
        q->head = q->count = 0;
    }
    if (q->count == q->cap) {
        if (q->head > 0) {
            memmove(q->items, q->items + q->head, sizeof(AVPacket *) * (size_t)(q->count - q->head));
            q->count -= q->head;
            q->head = 0;
        } else {
            q->cap = q->cap ? q->cap * 2 : 64;
            q->items = realloc(q->items, sizeof(AVPacket *) * (size_t)q->cap);
        }
    }
    q->items[q->count++] = p;
}

static AVPacket *queue_pop(PacketQueue *q) {
    return q->head < q->count ? q->items[q->head++] : NULL;
}

static void queue_clear(PacketQueue *q) {
    for (int i = q->head; i < q->count; i++)
        av_packet_free(&q->items[i]);
    q->head = q->count = 0;
}

static bool read_packet(VideoDec *d) {
    if (d->demux_eof)
        return false;
    AVPacket *p = av_packet_alloc();
    if (!p || av_read_frame(d->fmt, p) < 0) {
        av_packet_free(&p);
        d->demux_eof = true;
        return false;
    }
    if (p->stream_index == d->vs)
        queue_push(&d->vq, p);
    else if (p->stream_index == d->as)
        queue_push(&d->aq, p);
    else
        av_packet_free(&p);
    return true;
}

// ---------------------------------------------------------------------------
// Mở / đóng

static AVCodecContext *open_codec(AVStream *st, int threads) {
    const AVCodec *codec = avcodec_find_decoder(st->codecpar->codec_id);
    if (!codec)
        return NULL;
    AVCodecContext *c = avcodec_alloc_context3(codec);
    if (!c)
        return NULL;
    if (avcodec_parameters_to_context(c, st->codecpar) < 0) {
        avcodec_free_context(&c);
        return NULL;
    }
    c->pkt_timebase = st->time_base;
    c->thread_count = threads > 0 ? threads : 1;
    if (avcodec_open2(c, codec, NULL) < 0) {
        avcodec_free_context(&c);
        return NULL;
    }
    return c;
}

static int interrupt_cb(void *opaque) {
    VideoDec *d = opaque;
    return d->abort && *d->abort;
}

static VideoDec *open_common(VideoDec *d, const VDecOptions *opt) {
    static bool log_set;
    if (!log_set) {
        av_log_set_level(AV_LOG_QUIET);
        avformat_network_init();
        log_set = true;
    }
    int streams = opt ? opt->streams : (VDEC_VIDEO | VDEC_AUDIO);
    d->out_rate = opt && opt->audio_rate > 0 ? opt->audio_rate : 22050;
    d->out_ch = opt && opt->audio_channels == 2 ? 2 : 1;
    d->vs = d->as = -1;
    d->skip_video = d->skip_audio = -1;

    d->abort = opt ? opt->abort : NULL;
    d->fmt = avformat_alloc_context();
    if (!d->fmt) {
        vdec_close(d);
        return NULL;
    }
    d->fmt->interrupt_callback.callback = interrupt_cb;
    d->fmt->interrupt_callback.opaque = d;
    int r;
    if (d->url) {
        // Mạng: hết 15 giây không nhận được gì thì bỏ; tự nối lại khi rớt giữa chừng
        AVDictionary *o = NULL;
        av_dict_set(&o, "rw_timeout", "15000000", 0);
        av_dict_set(&o, "reconnect", "1", 0);
        av_dict_set(&o, "user_agent", "Mozilla/5.0 (Nintendo Switch) J2ME-NXX", 0);
        r = avformat_open_input(&d->fmt, d->url, NULL, &o);
        av_dict_free(&o);
    } else {
        uint8_t *buf = av_malloc(IO_BUF);
        d->avio = buf ? avio_alloc_context(buf, IO_BUF, 0, d, d->file ? file_read : mem_read, NULL,
                                           d->file ? file_seek : mem_seek)
                      : NULL;
        if (!d->avio) {
            av_free(buf);
            vdec_close(d);
            return NULL;
        }
        d->fmt->pb = d->avio;
        d->fmt->flags |= AVFMT_FLAG_CUSTOM_IO;
        r = avformat_open_input(&d->fmt, NULL, NULL, NULL);
    }
    if (r < 0) {
        d->fmt = NULL;      // avformat_open_input đã giải phóng khi lỗi
        vdec_close(d);
        return NULL;
    }
    if (avformat_find_stream_info(d->fmt, NULL) < 0) {
        vdec_close(d);
        return NULL;
    }
    d->start_ms = d->fmt->start_time != AV_NOPTS_VALUE ? d->fmt->start_time / 1000 : 0;

    if (streams & VDEC_VIDEO) {
        int s = av_find_best_stream(d->fmt, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
        // Ảnh bìa (MP3/M4A) không phải video
        if (s >= 0 && !(d->fmt->streams[s]->disposition & AV_DISPOSITION_ATTACHED_PIC)) {
            d->vctx = open_codec(d->fmt->streams[s], opt ? opt->threads : 1);
            if (d->vctx && d->vctx->width > 0 && d->vctx->height > 0)
                d->vs = s;
            else
                avcodec_free_context(&d->vctx);
        }
    }
    if (streams & VDEC_AUDIO) {
        int s = av_find_best_stream(d->fmt, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
        if (s >= 0) {
            d->actx = open_codec(d->fmt->streams[s], 1);
            if (d->actx)
                d->as = s;
        }
    }
    for (unsigned i = 0; i < d->fmt->nb_streams; i++) {
        if ((int)i != d->vs && (int)i != d->as)
            d->fmt->streams[i]->discard = AVDISCARD_ALL;
    }
    if (d->vs < 0 && d->as < 0) {
        vdec_close(d);
        return NULL;
    }
    d->vframe = av_frame_alloc();
    d->aframe = av_frame_alloc();
    d->vdone = d->vs < 0;
    d->adone = d->as < 0;
    return d;
}

VideoDec *vdec_open_mem(const uint8_t *data, size_t size, const VDecOptions *opt) {
    if (!data || size == 0)
        return NULL;
    VideoDec *d = calloc(1, sizeof(VideoDec));
    if (!d)
        return NULL;
    d->mem = malloc(size);
    if (!d->mem) {
        free(d);
        return NULL;
    }
    memcpy(d->mem, data, size);
    d->mem_size = (int64_t)size;
    return open_common(d, opt);
}

VideoDec *vdec_open_file(const char *path, const VDecOptions *opt) {
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    VideoDec *d = calloc(1, sizeof(VideoDec));
    if (!d) {
        fclose(f);
        return NULL;
    }
    d->file = f;
    struct stat st;
    d->file_size = fstat(fileno(f), &st) == 0 ? (int64_t)st.st_size : -1;
    return open_common(d, opt);
}

VideoDec *vdec_open_url(const char *url, const VDecOptions *opt) {
    if (!url || !*url)
        return NULL;
    VideoDec *d = calloc(1, sizeof(VideoDec));
    if (!d)
        return NULL;
    d->url = url;   // chỉ dùng trong lúc mở
    VideoDec *r = open_common(d, opt);
    if (r)
        r->url = NULL;
    return r;
}

void vdec_close(VideoDec *d) {
    if (!d)
        return;
    queue_clear(&d->vq);
    queue_clear(&d->aq);
    free(d->vq.items);
    free(d->aq.items);
    avcodec_free_context(&d->vctx);
    avcodec_free_context(&d->actx);
    av_frame_free(&d->vframe);
    av_frame_free(&d->aframe);
    if (d->fmt)
        avformat_close_input(&d->fmt);
    if (d->avio) {
        av_freep(&d->avio->buffer);
        avio_context_free(&d->avio);
    }
    sws_freeContext(d->sws_argb);
    sws_freeContext(d->sws_yuv);
    av_free(d->yuv_buf[0]);
    swr_free(&d->swr);
    free(d->fifo);
    if (d->file)
        fclose(d->file);
    free(d->mem);
    free(d);
}

// ---------------------------------------------------------------------------
// Thông tin

bool vdec_has_video(const VideoDec *d) {
    return d && d->vs >= 0;
}

bool vdec_has_audio(const VideoDec *d) {
    return d && d->as >= 0;
}

int vdec_width(const VideoDec *d) {
    return d && d->vctx ? d->vctx->width : 0;
}

int vdec_height(const VideoDec *d) {
    return d && d->vctx ? d->vctx->height : 0;
}

double vdec_aspect(const VideoDec *d) {
    if (!d || !d->vctx || d->vctx->height <= 0)
        return 1.0;
    AVRational sar = av_guess_sample_aspect_ratio(d->fmt, d->fmt->streams[d->vs], NULL);
    double a = (double)d->vctx->width / d->vctx->height;
    if (sar.num > 0 && sar.den > 0)
        a *= (double)sar.num / sar.den;
    return a;
}

int64_t vdec_duration_ms(const VideoDec *d) {
    if (!d || d->fmt->duration == AV_NOPTS_VALUE || d->fmt->duration <= 0)
        return -1;
    return d->fmt->duration / 1000;
}

static int64_t to_ms(const VideoDec *d, int stream, int64_t ts) {
    if (ts == AV_NOPTS_VALUE)
        return AV_NOPTS_VALUE;
    return av_rescale_q(ts, d->fmt->streams[stream]->time_base, (AVRational){ 1, 1000 }) - d->start_ms;
}

// ---------------------------------------------------------------------------
// Hình

bool vdec_next_frame(VideoDec *d, int64_t *pts_ms) {
    if (!d || d->vs < 0)
        return false;
    while (!d->vdone) {
        int r = avcodec_receive_frame(d->vctx, d->vframe);
        if (r == 0) {
            int64_t pts = to_ms(d, d->vs, d->vframe->best_effort_timestamp);
            if (pts == AV_NOPTS_VALUE)
                pts = d->last_vpts + 40;
            d->last_vpts = pts;
            if (d->skip_video >= 0 && pts < d->skip_video)
                continue;
            d->skip_video = -1;
            if (pts_ms)
                *pts_ms = pts < 0 ? 0 : pts;
            return true;
        }
        if (r != AVERROR(EAGAIN)) {
            d->vdone = true;
            break;
        }
        while (d->vq.head == d->vq.count && read_packet(d)) {
        }
        AVPacket *p = queue_pop(&d->vq);
        if (p) {
            avcodec_send_packet(d->vctx, p);    // gói hỏng thì bỏ qua
            av_packet_free(&p);
        } else if (!d->vflushed) {
            avcodec_send_packet(d->vctx, NULL);
            d->vflushed = true;
        } else {
            d->vdone = true;
        }
    }
    return false;
}

bool vdec_frame_argb(VideoDec *d, uint32_t *dst, int w, int h) {
    if (!d || d->vs < 0 || !d->vframe->data[0] || w <= 0 || h <= 0)
        return false;
    AVFrame *f = d->vframe;
    if (!d->sws_argb || d->argb_src_w != f->width || d->argb_src_h != f->height || d->argb_src_fmt != f->format ||
        d->argb_w != w || d->argb_h != h) {
        sws_freeContext(d->sws_argb);
        // ARGB8888 trong bộ nhớ little-endian là B, G, R, A
        d->sws_argb = sws_getContext(f->width, f->height, f->format, w, h, AV_PIX_FMT_BGRA, SWS_BILINEAR, NULL,
                                     NULL, NULL);
        d->argb_src_w = f->width;
        d->argb_src_h = f->height;
        d->argb_src_fmt = f->format;
        d->argb_w = w;
        d->argb_h = h;
    }
    if (!d->sws_argb)
        return false;
    uint8_t *planes[1] = { (uint8_t *)dst };
    int pitches[1] = { w * 4 };
    sws_scale(d->sws_argb, (const uint8_t *const *)f->data, f->linesize, 0, f->height, planes, pitches);
    for (int i = 0; i < w * h; i++)
        dst[i] |= 0xff000000u;
    return true;
}

bool vdec_frame_yuv(VideoDec *d, VDecYUV *out) {
    if (!d || d->vs < 0 || !d->vframe->data[0])
        return false;
    AVFrame *f = d->vframe;
    if (f->format == AV_PIX_FMT_YUV420P || f->format == AV_PIX_FMT_YUVJ420P) {
        out->y = f->data[0];
        out->u = f->data[1];
        out->v = f->data[2];
        out->y_pitch = f->linesize[0];
        out->u_pitch = f->linesize[1];
        out->v_pitch = f->linesize[2];
        return true;
    }
    // Định dạng khác (10-bit, 4:2:2...): đổi sang YUV420P cùng kích thước
    int w = d->vctx->width, h = d->vctx->height;
    if (!d->yuv_buf[0] || d->yuv_src_fmt != f->format) {
        sws_freeContext(d->sws_yuv);
        d->sws_yuv = sws_getContext(f->width, f->height, f->format, w, h, AV_PIX_FMT_YUV420P, SWS_BILINEAR, NULL,
                                    NULL, NULL);
        d->yuv_src_fmt = f->format;
        if (!d->yuv_buf[0]) {
            if (av_image_alloc(d->yuv_buf, d->yuv_pitch, w, h, AV_PIX_FMT_YUV420P, 32) < 0)
                return false;
        }
    }
    if (!d->sws_yuv)
        return false;
    sws_scale(d->sws_yuv, (const uint8_t *const *)f->data, f->linesize, 0, f->height, d->yuv_buf, d->yuv_pitch);
    out->y = d->yuv_buf[0];
    out->u = d->yuv_buf[1];
    out->v = d->yuv_buf[2];
    out->y_pitch = d->yuv_pitch[0];
    out->u_pitch = d->yuv_pitch[1];
    out->v_pitch = d->yuv_pitch[2];
    return true;
}

// ---------------------------------------------------------------------------
// Tiếng

static bool fifo_reserve(VideoDec *d, int samples) {
    if (samples <= d->fifo_cap)
        return true;
    int16_t *p = realloc(d->fifo, sizeof(int16_t) * (size_t)samples * d->out_ch);
    if (!p)
        return false;
    d->fifo = p;
    d->fifo_cap = samples;
    return true;
}

static bool setup_swr(VideoDec *d, const AVFrame *f) {
    if (d->swr)
        return true;
    AVChannelLayout in, out;
    if (f->ch_layout.nb_channels > 0 && f->ch_layout.order != AV_CHANNEL_ORDER_UNSPEC)
        av_channel_layout_copy(&in, &f->ch_layout);
    else
        av_channel_layout_default(&in, f->ch_layout.nb_channels > 0 ? f->ch_layout.nb_channels : 1);
    av_channel_layout_default(&out, d->out_ch);
    int r = swr_alloc_set_opts2(&d->swr, &out, AV_SAMPLE_FMT_S16, d->out_rate, &in, f->format, f->sample_rate, 0,
                                NULL);
    av_channel_layout_uninit(&in);
    av_channel_layout_uninit(&out);
    if (r < 0 || swr_init(d->swr) < 0) {
        swr_free(&d->swr);
        return false;
    }
    return true;
}

// Giải mã 1 khung âm thanh vào fifo; false khi hết
static bool decode_audio_frame(VideoDec *d) {
    while (!d->adone) {
        int r = avcodec_receive_frame(d->actx, d->aframe);
        if (r == 0) {
            AVFrame *f = d->aframe;
            if (!setup_swr(d, f)) {
                d->adone = true;
                break;
            }
            int cap = swr_get_out_samples(d->swr, f->nb_samples);
            if (cap <= 0 || !fifo_reserve(d, cap))
                continue;
            uint8_t *outp[1] = { (uint8_t *)d->fifo };
            int n = swr_convert(d->swr, outp, cap, (const uint8_t **)f->extended_data, f->nb_samples);
            if (n <= 0)
                continue;
            d->fifo_len = n;
            d->fifo_pos = 0;
            // Sau khi tua: bỏ phần trước mốc
            if (d->skip_audio >= 0) {
                int64_t pts = to_ms(d, d->as, f->best_effort_timestamp);
                if (pts != AV_NOPTS_VALUE) {
                    int64_t drop = (d->skip_audio - pts) * d->out_rate / 1000;
                    if (drop >= n) {
                        d->fifo_len = 0;
                        continue;
                    }
                    if (drop > 0)
                        d->fifo_pos = (int)drop;
                }
                d->skip_audio = -1;
            }
            return true;
        }
        if (r != AVERROR(EAGAIN)) {
            d->adone = true;
            // Lấy nốt phần còn đọng trong bộ đổi tần số
            if (d->swr && fifo_reserve(d, 4096)) {
                uint8_t *outp[1] = { (uint8_t *)d->fifo };
                int n = swr_convert(d->swr, outp, 4096, NULL, 0);
                if (n > 0) {
                    d->fifo_len = n;
                    d->fifo_pos = 0;
                    return true;
                }
            }
            break;
        }
        while (d->aq.head == d->aq.count && read_packet(d)) {
        }
        AVPacket *p = queue_pop(&d->aq);
        if (p) {
            avcodec_send_packet(d->actx, p);
            av_packet_free(&p);
        } else if (!d->aflushed) {
            avcodec_send_packet(d->actx, NULL);
            d->aflushed = true;
        } else {
            d->adone = true;
        }
    }
    return false;
}

int vdec_read_audio(VideoDec *d, int16_t *out, int max) {
    if (!d || d->as < 0)
        return 0;
    int got = 0;
    while (got < max) {
        if (d->fifo_pos < d->fifo_len) {
            int n = d->fifo_len - d->fifo_pos;
            if (n > max - got)
                n = max - got;
            memcpy(out + (size_t)got * d->out_ch, d->fifo + (size_t)d->fifo_pos * d->out_ch,
                   sizeof(int16_t) * (size_t)n * d->out_ch);
            d->fifo_pos += n;
            got += n;
            continue;
        }
        d->fifo_pos = d->fifo_len = 0;
        if (!decode_audio_frame(d))
            break;
    }
    return got;
}

// ---------------------------------------------------------------------------

bool vdec_seek(VideoDec *d, int64_t ms) {
    if (!d)
        return false;
    if (ms < 0)
        ms = 0;
    int64_t ts = (ms + d->start_ms) * 1000;
    if (avformat_seek_file(d->fmt, -1, INT64_MIN, ts, ts, 0) < 0 &&
        avformat_seek_file(d->fmt, -1, INT64_MIN, ts, INT64_MAX, 0) < 0)
        return false;
    queue_clear(&d->vq);
    queue_clear(&d->aq);
    if (d->vctx)
        avcodec_flush_buffers(d->vctx);
    if (d->actx)
        avcodec_flush_buffers(d->actx);
    swr_free(&d->swr);
    d->fifo_len = d->fifo_pos = 0;
    d->demux_eof = false;
    d->vflushed = d->aflushed = false;
    d->vdone = d->vs < 0;
    d->adone = d->as < 0;
    d->skip_video = d->skip_audio = ms;
    d->last_vpts = ms;
    return true;
}

int vdec_probe(const uint8_t *data, size_t size) {
    VDecOptions opt = { .streams = VDEC_VIDEO | VDEC_AUDIO };
    VideoDec *d = vdec_open_mem(data, size, &opt);
    if (!d)
        return 0;
    int r = (d->vs >= 0 ? VDEC_VIDEO : 0) | (d->as >= 0 ? VDEC_AUDIO : 0);
    vdec_close(d);
    return r;
}

bool vdec_decode_audio(const uint8_t *data, size_t size, int rate, int channels, int16_t **pcm, size_t *frames) {
    VDecOptions opt = { .streams = VDEC_AUDIO, .audio_rate = rate, .audio_channels = channels };
    VideoDec *d = vdec_open_mem(data, size, &opt);
    if (!d)
        return false;
    size_t cap = (size_t)rate * 4, len = 0;
    int ch = d->out_ch;
    int16_t *buf = malloc(sizeof(int16_t) * cap * ch);
    while (buf) {
        if (len + 4096 > cap) {
            cap *= 2;
            int16_t *nb = realloc(buf, sizeof(int16_t) * cap * ch);
            if (!nb) {
                free(buf);
                buf = NULL;
                break;
            }
            buf = nb;
        }
        int n = vdec_read_audio(d, buf + len * ch, 4096);
        if (n <= 0)
            break;
        len += (size_t)n;
    }
    vdec_close(d);
    if (!buf || len == 0) {
        free(buf);
        return false;
    }
    *pcm = buf;
    *frames = len;
    return true;
}

#endif
