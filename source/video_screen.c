#include "video_screen.h"

#include <stdio.h>
#include <string.h>

#include "gfx.h"
#include "input.h"
#include "lang.h"
#include "video_dec.h"

#define RATE            48000
#define CHANNELS        2
#define AUDIO_AHEAD_MS  400     // giữ sẵn trong hàng đợi âm thanh
#define FRAMES_PER_TICK 4       // số khung hình giải mã tối đa mỗi lần vẽ (khi bị chậm)
#define OSD_MS          3000
#define SEEK_SHORT      10000
#define SEEK_LONG       60000

#define COL_BAR     ((SDL_Color){ 0, 0, 0, 170 })
#define COL_TEXT    RGB(0xee, 0xee, 0xee)
#define COL_DIM     RGB(0x9a, 0x9f, 0xa8)
#define COL_ACCENT  RGB(0x00, 0xb4, 0xe6)
#define COL_TRACK   RGB(0x50, 0x54, 0x5c)

static VideoDec *dec;
static SDL_Texture *tex;
static SDL_AudioDeviceID adev;
static char title[256];
static bool has_video, has_audio;
static int64_t duration;
static double aspect;

static bool paused, ended;
static int64_t pause_ms;
// Đồng hồ: theo số mẫu tiếng đã phát; không có tiếng (hoặc tiếng đã hết) thì theo giờ thực
static bool audio_clock, audio_done;
static int64_t base_ms;
static uint64_t queued;             // số mẫu đã đưa vào thiết bị kể từ base_ms
static int64_t wall_base_ms;
static Uint32 wall_start;

static bool pending, video_done, shown_any;
static int64_t pending_ms;
static int volume = 100;
static Uint32 osd_until;
static int16_t abuf[4096 * CHANNELS];

static int64_t clock_ms(void) {
    if (paused)
        return pause_ms;
    if (audio_clock) {
        Uint32 left = SDL_GetQueuedAudioSize(adev) / (2 * CHANNELS);
        int64_t t = base_ms + (int64_t)(queued - left) * 1000 / RATE;
        if (audio_done && left == 0) {
            audio_clock = false;
            wall_base_ms = t;
            wall_start = SDL_GetTicks();
        }
        return t;
    }
    return wall_base_ms + (int64_t)(SDL_GetTicks() - wall_start);
}

static void reset_clock(int64_t ms) {
    base_ms = wall_base_ms = ms;
    wall_start = SDL_GetTicks();
    queued = 0;
    if (adev)
        SDL_ClearQueuedAudio(adev);
    audio_clock = has_audio;
    audio_done = !has_audio;
}

static void feed_audio(void) {
    if (!adev || audio_done)
        return;
    while (SDL_GetQueuedAudioSize(adev) < (Uint32)(RATE * CHANNELS * 2 * AUDIO_AHEAD_MS / 1000)) {
        int n = vdec_read_audio(dec, abuf, 4096);
        if (n <= 0) {
            audio_done = true;
            break;
        }
        if (volume < 100) {
            for (int i = 0; i < n * CHANNELS; i++)
                abuf[i] = (int16_t)(abuf[i] * volume / 100);
        }
        SDL_QueueAudio(adev, abuf, (Uint32)(n * CHANNELS * 2));
        queued += (uint64_t)n;
    }
}

static void update_video(void) {
    if (!has_video || video_done)
        return;
    int64_t now = clock_ms();
    for (int i = 0; i < FRAMES_PER_TICK; i++) {
        if (!pending) {
            if (!vdec_next_frame(dec, &pending_ms)) {
                video_done = true;
                return;
            }
            pending = true;
        }
        if (pending_ms > now && shown_any)
            return;
        VDecYUV y;
        if (vdec_frame_yuv(dec, &y))
            SDL_UpdateYUVTexture(tex, NULL, y.y, y.y_pitch, y.u, y.u_pitch, y.v, y.v_pitch);
        shown_any = true;
        pending = false;
    }
}

static void set_paused(bool p) {
    if (p == paused)
        return;
    if (p) {
        pause_ms = clock_ms();
        paused = true;
    } else {
        paused = false;
        wall_base_ms = pause_ms;
        wall_start = SDL_GetTicks();
    }
    if (adev)
        SDL_PauseAudioDevice(adev, p ? 1 : 0);
}

static void seek_to(int64_t ms) {
    if (duration > 0 && ms > duration - 1000)
        ms = duration - 1000;
    if (ms < 0)
        ms = 0;
    if (!vdec_seek(dec, ms))
        return;
    pending = false;
    video_done = !has_video;
    ended = false;
    reset_clock(ms);
    if (paused)
        pause_ms = ms;
}

void video_screen_options(VDecOptions *opt) {
    memset(opt, 0, sizeof(*opt));
    opt->streams = VDEC_VIDEO | VDEC_AUDIO;
    opt->audio_rate = RATE;
    opt->audio_channels = CHANNELS;
    opt->threads = 3;
}

bool video_screen_open(const char *path, const char *name, char *err, size_t err_size) {
    VDecOptions opt;
    video_screen_options(&opt);
    VideoDec *d = vdec_open_file(path, &opt);
    if (!d) {
        snprintf(err, err_size, "%s", tr(vdec_available() ? S_ERR_VIDEO : S_ERR_NO_VIDEO_BUILD));
        return false;
    }
    return video_screen_open_dec(d, name, err, err_size);
}

bool video_screen_open_dec(VideoDec *d, const char *name, char *err, size_t err_size) {
    video_screen_close();
    dec = d;
    snprintf(title, sizeof(title), "%s", name);
    has_video = vdec_has_video(dec);
    has_audio = vdec_has_audio(dec);
    duration = vdec_duration_ms(dec);
    aspect = vdec_aspect(dec);
    if (has_video) {
        tex = SDL_CreateTexture(gfx_renderer(), SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, vdec_width(dec),
                                vdec_height(dec));
        if (!tex) {
            snprintf(err, err_size, "%s", tr(S_ERR_VIDEO));
            video_screen_close();
            return false;
        }
        SDL_SetTextureScaleMode(tex, SDL_ScaleModeLinear);
    }
    if (has_audio) {
        if (!SDL_WasInit(SDL_INIT_AUDIO))
            SDL_InitSubSystem(SDL_INIT_AUDIO);
        SDL_AudioSpec want;
        memset(&want, 0, sizeof(want));
        want.freq = RATE;
        want.format = AUDIO_S16SYS;
        want.channels = CHANNELS;
        want.samples = 1024;
        adev = SDL_OpenAudioDevice(NULL, 0, &want, NULL, 0);
        if (!adev)
            has_audio = false;
    }
    paused = ended = false;
    pending = video_done = shown_any = false;
    video_done = !has_video;
    reset_clock(0);
    feed_audio();
    if (adev)
        SDL_PauseAudioDevice(adev, 0);
    osd_until = SDL_GetTicks() + OSD_MS;
    return true;
}

void video_screen_close(void) {
    if (adev) {
        SDL_CloseAudioDevice(adev);
        adev = 0;
    }
    if (tex) {
        SDL_DestroyTexture(tex);
        tex = NULL;
    }
    vdec_close(dec);
    dec = NULL;
}

bool video_screen_update(void) {
    if (!dec)
        return false;
    if (input_pressed(BTN_B) || input_pressed(BTN_MINUS))
        return false;
    bool any = true;
    if (input_pressed(BTN_A)) {
        if (ended) {
            seek_to(0);
            set_paused(false);
        } else {
            set_paused(!paused);
        }
    } else if (input_pressed(BTN_LEFT) || input_pressed(BTN_RIGHT) || input_pressed(BTN_L) || input_pressed(BTN_R)) {
        int64_t step = input_pressed(BTN_L) || input_pressed(BTN_R) ? SEEK_LONG : SEEK_SHORT;
        int dir = input_pressed(BTN_LEFT) || input_pressed(BTN_L) ? -1 : 1;
        seek_to(clock_ms() + dir * step);
    } else if (input_pressed(BTN_UP) || input_pressed(BTN_DOWN)) {
        volume += input_pressed(BTN_UP) ? 10 : -10;
        volume = volume < 0 ? 0 : volume > 100 ? 100 : volume;
    } else {
        any = false;
    }
    if (any)
        osd_until = SDL_GetTicks() + OSD_MS;

    feed_audio();
    update_video();
    if (!ended && video_done && (!has_audio || (audio_done && SDL_GetQueuedAudioSize(adev) == 0))) {
        int64_t t = clock_ms();
        set_paused(true);
        pause_ms = duration > 0 ? duration : t;
        ended = true;
    }
    return true;
}

static void format_time(int64_t ms, char *out, size_t size) {
    int s = (int)(ms / 1000);
    if (s >= 3600)
        snprintf(out, size, "%d:%02d:%02d", s / 3600, s / 60 % 60, s % 60);
    else
        snprintf(out, size, "%d:%02d", s / 60, s % 60);
}

void video_screen_draw(void) {
    gfx_clear(RGB(0, 0, 0));
    if (tex && shown_any) {
        int w = SCREEN_W, h = (int)(SCREEN_W / aspect);
        if (h > SCREEN_H) {
            h = SCREEN_H;
            w = (int)(SCREEN_H * aspect);
        }
        SDL_Rect dst = { (SCREEN_W - w) / 2, (SCREEN_H - h) / 2, w, h };
        SDL_RenderCopy(gfx_renderer(), tex, NULL, &dst);
    } else if (!has_video) {
        gfx_text(FONT_LARGE, SCREEN_W / 2, SCREEN_H / 2 - 60, SCREEN_W - 160, ALIGN_CENTER, COL_TEXT, title);
    }
    if (!paused && SDL_TICKS_PASSED(SDL_GetTicks(), osd_until))
        return;

    // Thanh trên: tên file. Thanh dưới: tiến độ, thời gian, hướng dẫn
    gfx_fill_rect(0, 0, SCREEN_W, 64, COL_BAR);
    gfx_text(FONT_NORMAL, 32, (64 - gfx_font_height(FONT_NORMAL)) / 2, SCREEN_W - 64, ALIGN_LEFT, COL_TEXT, title);

    int bar_h = 120, y0 = SCREEN_H - bar_h;
    gfx_fill_rect(0, y0, SCREEN_W, bar_h, COL_BAR);
    int64_t t = clock_ms();
    char now_s[16], dur_s[16], left[96];
    format_time(t, now_s, sizeof(now_s));
    format_time(duration > 0 ? duration : 0, dur_s, sizeof(dur_s));
    const char *status = ended ? tr(S_VIDEO_ENDED) : paused ? tr(S_VIDEO_PAUSED) : "";
    snprintf(left, sizeof(left), "%s / %s%s%s", now_s, dur_s, status[0] ? "   " : "", status);

    int track_x = 32, track_w = SCREEN_W - 64, track_y = y0 + 22;
    gfx_fill_rect(track_x, track_y, track_w, 6, COL_TRACK);
    if (duration > 0) {
        int64_t p = t < 0 ? 0 : t > duration ? duration : t;
        gfx_fill_rect(track_x, track_y, (int)(track_w * p / duration), 6, COL_ACCENT);
    }
    int ty = y0 + 46;
    gfx_text(FONT_NORMAL, 32, ty, 0, ALIGN_LEFT, COL_TEXT, left);
    char vol[48];
    snprintf(vol, sizeof(vol), tr(S_VOLUME_FMT), volume);
    gfx_text(FONT_NORMAL, SCREEN_W - 32, ty, 0, ALIGN_RIGHT, COL_TEXT, vol);
    gfx_text(FONT_SMALL, SCREEN_W / 2, y0 + bar_h - gfx_font_height(FONT_SMALL) - 10, 0, ALIGN_CENTER, COL_DIM,
             tr(S_VIDEO_HINTS));
}
