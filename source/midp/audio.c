// Âm thanh cho javax.microedition.media: trộn WAV / MP3 / AMR / AAC, tổng hợp MIDI / tone bằng phần mềm qua SDL audio.
// Có SoundFont (.sf2) thì MIDI / tone phát bằng TinySoundFont, không thì dùng bộ tổng hợp sóng cơ bản.
#include "midp.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>

#include "../third_party/dr_mp3.h"
#include "audio_dec.h"
#define TSF_IMPLEMENTATION
#include "../third_party/tsf.h"
#include "../vm/vm.h"

#define RATE        48000       // tần số gốc của Switch; thấp hơn thì SoundFont bị rè (răng cưa)
#define MAX_PLAYERS 32
#define MAX_VOICES  24
#define BLOCK       64          // số mẫu giữa 2 lần xử lý sự kiện MIDI
#define SF_VOICES   64          // số voice tối đa mỗi player khi dùng SoundFont (cấp sẵn, không malloc khi trộn)
#define SF_GAIN     0.3f

typedef enum {
    KIND_NONE,
    KIND_WAV,
    KIND_MIDI,                  // MIDI và tone sequence đều chạy bằng sequencer
} Kind;

typedef struct {
    uint32_t tick;              // MIDI: tick; tone: ms
    uint8_t status;             // 0xFF51 tempo dùng status = 0xFF
    uint8_t d1, d2;
    uint32_t tempo;             // micro giây / nốt đen (với sự kiện tempo)
    uint32_t seq;               // thứ tự gốc, để sắp xếp ổn định
} MidiEvent;

typedef struct {
    bool active;
    uint8_t ch, note;
    float vel;
    double phase, inc;
    int wave;
    float env, peak;
    int stage;                  // 0 attack, 1 decay, 2 sustain, 3 release
    float attack, decay, sustain, release;  // tốc độ / mức (theo mẫu)
    bool held;
    uint32_t noise;
    float drum_freq, drum_decay;
    int age;
} Voice;

typedef struct {
    uint8_t program, volume, expression;
    int bend;                   // -8192..8191
} MidiChannel;

typedef struct {
    bool used;
    Kind kind;
    bool playing;
    bool ended;                 // hết bài (cần báo END_OF_MEDIA)
    int loops;                  // -1 = lặp mãi; số lần còn lại phải phát
    int volume;                 // 0..100

    // WAV
    int16_t *pcm;
    uint32_t pcm_len, pcm_pos;

    // MIDI
    MidiEvent *events;
    uint32_t event_count, event_pos;
    uint16_t division;          // tick / nốt đen; 0 = đơn vị ms (tone)
    double tick;                // vị trí hiện tại (tick hoặc ms)
    uint32_t tempo;
    uint64_t sample_pos;        // số mẫu đã phát (để tính media time)
    uint64_t total_samples;     // độ dài bài (mẫu)
    MidiChannel chans[16];
    Voice voices[MAX_VOICES];
    tsf *sf;                    // != NULL: phát bằng SoundFont (bản sao dùng chung mẫu với sf_base)
} Player;

static SDL_AudioDeviceID dev;
#ifndef __SWITCH__
static FILE *dump;  // J2ME_NX_AUDIO_DUMP=<file>: ghi PCM 16-bit mono 48000Hz để kiểm tra
#endif
static Player players[MAX_PLAYERS + 1];     // chỉ số 0 không dùng; MAX_PLAYERS dành cho playTone
static tsf *sf_base;                        // SoundFont đã nạp (giữ qua các lần chơi game)
static char sf_path[512];

// TimGM6mb nhúng bằng .incbin (third_party/soundfont/soundfont_data.c)
extern const unsigned char builtin_sf2[], builtin_sf2_end[];

// ---------------------------------------------------------------------------
// Synth

enum {
    WAVE_PLUCK,     // tam giác, tắt dần (piano, guitar)
    WAVE_SQUARE,
    WAVE_SAW,
    WAVE_TRIANGLE,
    WAVE_SINE,
    WAVE_NOISE,
    WAVE_DRUM,
};

static int program_wave(int program) {
    switch (program / 8) {
    case 0: case 1: case 3: case 13: case 14: return WAVE_PLUCK;   // piano, chromatic, guitar, ethnic, percussive
    case 2: return WAVE_SQUARE;                                     // organ
    case 4: return WAVE_TRIANGLE;                                   // bass
    case 5: case 6: case 11: return WAVE_SAW;                       // strings, ensemble, pad
    case 7: case 10: return WAVE_SQUARE;                            // brass, synth lead
    case 8: case 9: return WAVE_SINE;                               // reed, pipe
    default: return WAVE_NOISE;                                     // effects
    }
}

static float note_freq(int note, int bend) {
    return 440.0f * powf(2.0f, ((float)note - 69.0f + bend / 8192.0f * 2.0f) / 12.0f);
}

static void voice_setup_env(Voice *v) {
    float s = RATE;
    switch (v->wave) {
    case WAVE_PLUCK:
        v->attack = 1.0f / (0.004f * s);
        v->decay = 1.0f / (0.9f * s);
        v->sustain = 0.0f;
        v->release = 1.0f / (0.15f * s);
        break;
    case WAVE_SAW:
        v->attack = 1.0f / (0.04f * s);
        v->decay = 1.0f / (0.3f * s);
        v->sustain = 0.7f;
        v->release = 1.0f / (0.25f * s);
        break;
    case WAVE_NOISE:
        v->attack = 1.0f / (0.01f * s);
        v->decay = 1.0f / (0.3f * s);
        v->sustain = 0.3f;
        v->release = 1.0f / (0.2f * s);
        break;
    default:
        v->attack = 1.0f / (0.008f * s);
        v->decay = 1.0f / (0.2f * s);
        v->sustain = 0.75f;
        v->release = 1.0f / (0.08f * s);
        break;
    }
}

static Voice *alloc_voice(Player *p) {
    Voice *best = &p->voices[0];
    for (int i = 0; i < MAX_VOICES; i++) {
        Voice *v = &p->voices[i];
        if (!v->active)
            return v;
        // Cướp voice đang release hoặc lâu nhất
        if ((v->stage == 3 && best->stage != 3) || (v->stage == best->stage && v->age > best->age))
            best = v;
    }
    return best;
}

static void note_on(Player *p, int ch, int note, int vel) {
    if (p->sf) {
        tsf_channel_note_on(p->sf, ch, note, vel / 127.0f);
        return;
    }
    Voice *v = alloc_voice(p);
    memset(v, 0, sizeof(*v));
    v->active = true;
    v->ch = (uint8_t)ch;
    v->note = (uint8_t)note;
    v->vel = vel / 127.0f;
    v->held = true;
    v->noise = 0x12345u + (uint32_t)note * 977u;
    if (ch == 9) {
        v->wave = WAVE_DRUM;
        v->env = 1.0f;
        v->stage = 1;
        // Trống: kick / snare / hi-hat / cymbal / tom
        if (note == 35 || note == 36) {
            v->drum_freq = 150.0f;
            v->drum_decay = 1.0f / (0.18f * RATE);
        } else if (note == 38 || note == 40 || note == 37 || note == 39) {
            v->drum_freq = 0;
            v->drum_decay = 1.0f / (0.14f * RATE);
        } else if (note == 42 || note == 44) {
            v->drum_freq = -1;
            v->drum_decay = 1.0f / (0.04f * RATE);
        } else if (note == 46 || note == 49 || note == 51 || note == 52 || note == 55 || note == 57 || note == 59) {
            v->drum_freq = -1;
            v->drum_decay = 1.0f / (0.45f * RATE);
        } else {
            v->drum_freq = 80.0f + (note - 41) * 12.0f;
            v->drum_decay = 1.0f / (0.2f * RATE);
        }
        v->inc = v->drum_freq > 0 ? v->drum_freq / RATE : 0;
        return;
    }
    v->wave = program_wave(p->chans[ch].program);
    v->inc = note_freq(note, p->chans[ch].bend) / RATE;
    voice_setup_env(v);
}

static void note_off(Player *p, int ch, int note) {
    if (p->sf) {
        tsf_channel_note_off(p->sf, ch, note);
        return;
    }
    for (int i = 0; i < MAX_VOICES; i++) {
        Voice *v = &p->voices[i];
        if (v->active && v->held && v->ch == ch && v->note == note) {
            v->held = false;
            if (v->wave != WAVE_DRUM)
                v->stage = 3;
        }
    }
}

static void all_notes_off(Player *p) {
    if (p->sf) {
        for (int c = 0; c < 16; c++)
            tsf_channel_note_off_all(p->sf, c);
        return;
    }
    for (int i = 0; i < MAX_VOICES; i++) {
        if (p->voices[i].active) {
            p->voices[i].held = false;
            if (p->voices[i].wave != WAVE_DRUM)
                p->voices[i].stage = 3;
        }
    }
}

static void set_program(Player *p, int ch, int program) {
    p->chans[ch].program = (uint8_t)program;
    if (p->sf)
        tsf_channel_set_presetnumber(p->sf, ch, program, ch == 9);
}

static void reset_channels(Player *p) {
    for (int c = 0; c < 16; c++) {
        p->chans[c].volume = 100;
        p->chans[c].expression = 127;
        p->chans[c].bend = 0;
        if (p->sf) {
            // Kênh đã cấp sẵn lúc tạo player: các hàm dưới không malloc (gọi được trong callback)
            tsf_channel_midi_control(p->sf, c, 121, 0);
            tsf_channel_set_pitchwheel(p->sf, c, 8192);
        }
        set_program(p, c, 0);
    }
}

// Gắn SoundFont cho player MIDI mới; thiếu bộ nhớ thì dùng bộ tổng hợp sóng
static void sf_attach(Player *p) {
    if (!sf_base || p->sf)
        return;
    tsf *f = tsf_copy(sf_base);
    if (!f)
        return;
    if (!tsf_set_max_voices(f, SF_VOICES)) {
        tsf_close(f);
        return;
    }
    for (int c = 0; c < 16; c++)
        tsf_channel_set_presetnumber(f, c, 0, c == 9);
    if (!f->channels || f->channels->channelNum < 16) {
        tsf_close(f);
        return;
    }
    p->sf = f;
}

static inline float voice_sample(Player *p, Voice *v) {
    float s;
    if (v->wave == WAVE_DRUM) {
        v->noise = v->noise * 1664525u + 1013904223u;
        float n = (int32_t)v->noise / 2147483648.0f;
        if (v->drum_freq > 0) {
            // Kick / tom: sine hạ tần số dần
            s = sinf((float)(v->phase * 2 * M_PI));
            v->phase += v->inc;
            v->inc *= 1.0 - 0.00015 * 22050.0 / RATE;  // hạ tần số như nhau ở mọi tần số mẫu
            s = s * 0.9f + n * 0.1f;
        } else if (v->drum_freq == 0) {
            s = n * 0.8f + sinf((float)(v->phase * 2 * M_PI)) * 0.3f;
            v->phase += 190.0 / RATE;
        } else {
            s = n * 0.6f;
        }
        s *= v->env;
        v->env -= v->drum_decay * v->env * 6.0f;
        if (v->env < 0.002f)
            v->active = false;
        return s * v->vel;
    }

    double ph = v->phase - floor(v->phase);
    switch (v->wave) {
    case WAVE_SQUARE: s = ph < 0.5 ? 0.5f : -0.5f; break;
    case WAVE_SAW: s = (float)(ph * 2.0 - 1.0) * 0.55f; break;
    case WAVE_SINE: s = sinf((float)(ph * 2 * M_PI)); break;
    case WAVE_NOISE:
        v->noise = v->noise * 1664525u + 1013904223u;
        s = (int32_t)v->noise / 2147483648.0f * 0.4f;
        break;
    default: s = (float)(ph < 0.5 ? ph * 4.0 - 1.0 : 3.0 - ph * 4.0); break;    // tam giác
    }
    v->phase += v->inc;

    switch (v->stage) {
    case 0:
        v->env += v->attack;
        if (v->env >= 1.0f) {
            v->env = 1.0f;
            v->stage = 1;
        }
        break;
    case 1:
        v->env -= v->decay;
        if (v->env <= v->sustain) {
            v->env = v->sustain;
            v->stage = 2;
            if (v->sustain <= 0.001f)
                v->active = false;
        }
        break;
    case 2:
        break;
    default:
        v->env -= v->release;
        if (v->env <= 0.0f) {
            v->env = 0;
            v->active = false;
        }
        break;
    }
    MidiChannel *c = &p->chans[v->ch];
    return s * v->env * v->vel * (c->volume / 127.0f) * (c->expression / 127.0f);
}

// ---------------------------------------------------------------------------
// Sequencer

static void midi_event(Player *p, const MidiEvent *e) {
    if (e->status == 0xFF) {
        p->tempo = e->tempo;
        return;
    }
    int ch = e->status & 15;
    if (p->sf) {
        switch (e->status & 0xF0) {
        case 0xB0: tsf_channel_midi_control(p->sf, ch, e->d1, e->d2); return;
        case 0xC0: set_program(p, ch, e->d1); return;
        case 0xE0: tsf_channel_set_pitchwheel(p->sf, ch, (e->d2 << 7) | e->d1); return;
        default: break;     // note on / off: qua note_on / note_off
        }
    }
    switch (e->status & 0xF0) {
    case 0x80: note_off(p, ch, e->d1); break;
    case 0x90:
        if (e->d2)
            note_on(p, ch, e->d1, e->d2);
        else
            note_off(p, ch, e->d1);
        break;
    case 0xB0:
        if (e->d1 == 7)
            p->chans[ch].volume = e->d2;
        else if (e->d1 == 11)
            p->chans[ch].expression = e->d2;
        else if (e->d1 == 120 || e->d1 == 123)
            all_notes_off(p);
        else if (e->d1 == 121) {
            p->chans[ch].volume = 100;
            p->chans[ch].expression = 127;
            p->chans[ch].bend = 0;
        }
        break;
    case 0xC0: set_program(p, ch, e->d1); break;
    case 0xE0: {
        p->chans[ch].bend = ((e->d2 << 7) | e->d1) - 8192;
        for (int i = 0; i < MAX_VOICES; i++) {
            Voice *v = &p->voices[i];
            if (v->active && v->ch == ch && v->wave != WAVE_DRUM)
                v->inc = note_freq(v->note, p->chans[ch].bend) / RATE;
        }
        break;
    }
    default:
        break;
    }
}

static double ticks_per_sample(const Player *p) {
    if (p->division == 0)
        return 1000.0 / RATE;   // tone: đơn vị ms
    return (double)p->division * 1000000.0 / ((double)p->tempo * RATE);
}

static bool any_voice(const Player *p) {
    if (p->sf)
        return tsf_active_voice_count(p->sf) > 0;
    for (int i = 0; i < MAX_VOICES; i++) {
        if (p->voices[i].active)
            return true;
    }
    return false;
}

static void seq_rewind(Player *p) {
    p->event_pos = 0;
    p->tick = 0;
    p->sample_pos = 0;
    p->tempo = 500000;
    reset_channels(p);
}

// Trộn 1 player MIDI vào buf (float, mono)
static void render_midi(Player *p, float *buf, int n) {
    float gain = p->volume / 100.0f * 0.35f;
    for (int off = 0; off < n; off += BLOCK) {
        int len = n - off < BLOCK ? n - off : BLOCK;
        if (p->playing) {
            while (p->event_pos < p->event_count && p->events[p->event_pos].tick <= p->tick)
                midi_event(p, &p->events[p->event_pos++]);
            if (p->event_pos >= p->event_count) {
                if (p->loops < 0 || p->loops > 1) {
                    if (p->loops > 1)
                        p->loops--;
                    all_notes_off(p);
                    seq_rewind(p);
                } else if (p->sf || !any_voice(p)) {
                    // SoundFont: báo hết bài đúng lúc, phần ngân (release) vẫn phát tiếp
                    p->playing = false;
                    p->ended = true;
                }
            }
            p->tick += ticks_per_sample(p) * len;
            p->sample_pos += (uint64_t)len;
        }
        if (p->sf) {
            if (p->volume > 0) {
                tsf_set_volume(p->sf, p->volume / 100.0f * SF_GAIN);
                tsf_render_float(p->sf, buf + off, len, 1);
            } else {
                float tmp[BLOCK];   // im lặng nhưng vẫn chạy voice cho đúng thời gian
                tsf_render_float(p->sf, tmp, len, 0);
            }
            continue;
        }
        for (int i = 0; i < MAX_VOICES; i++) {
            Voice *v = &p->voices[i];
            if (!v->active)
                continue;
            v->age++;
            for (int k = 0; k < len && v->active; k++)
                buf[off + k] += voice_sample(p, v) * gain;
        }
    }
}

static void render_wav(Player *p, float *buf, int n) {
    if (!p->playing)
        return;
    float gain = p->volume / 100.0f / 32768.0f;
    for (int i = 0; i < n; i++) {
        if (p->pcm_pos >= p->pcm_len) {
            if (p->loops < 0 || p->loops > 1) {
                if (p->loops > 1)
                    p->loops--;
                p->pcm_pos = 0;
            } else {
                p->playing = false;
                p->ended = true;
                return;
            }
        }
        buf[i] += p->pcm[p->pcm_pos++] * gain;
    }
}

static void audio_callback(void *userdata, Uint8 *stream, int bytes) {
    (void)userdata;
    int n = bytes / 2;
    float mix[2048];
    int16_t *out = (int16_t *)stream;
    while (n > 0) {
        int len = n > 2048 ? 2048 : n;
        memset(mix, 0, sizeof(float) * len);
        for (int i = 1; i <= MAX_PLAYERS; i++) {
            Player *p = &players[i];
            if (!p->used)
                continue;
            if (p->kind == KIND_WAV)
                render_wav(p, mix, len);
            else if (p->kind == KIND_MIDI && (p->playing || any_voice(p)))
                render_midi(p, mix, len);
        }
        for (int i = 0; i < len; i++) {
            float s = mix[i];
            // Nén mềm phần vượt 0.75 thay vì cắt cứng (nốt SoundFont đánh mạnh / nhiều player cùng lúc)
            if (s > 0.75f)
                s = 0.75f + 0.25f * tanhf((s - 0.75f) * 4.0f);
            else if (s < -0.75f)
                s = -0.75f - 0.25f * tanhf((-s - 0.75f) * 4.0f);
            out[i] = (int16_t)(s * 32767.0f);
        }
#ifndef __SWITCH__
        if (dump)
            fwrite(out, 2, (size_t)len, dump);
#endif
        out += len;
        n -= len;
    }
}

static bool audio_open(void) {
    if (dev)
        return true;
    if (!SDL_WasInit(SDL_INIT_AUDIO) && SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
        vm_log("SDL audio: %s", SDL_GetError());
        return false;
    }
    SDL_AudioSpec want, have;
    memset(&want, 0, sizeof(want));
    want.freq = RATE;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 1024;
    want.callback = audio_callback;
    dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (!dev) {
        vm_log("Khong mo duoc thiet bi am thanh: %s", SDL_GetError());
        return false;
    }
#ifndef __SWITCH__
    const char *dump_path = SDL_getenv("J2ME_NX_AUDIO_DUMP");
    if (dump_path)
        dump = fopen(dump_path, "wb");
#endif
    SDL_PauseAudioDevice(dev, 0);
    return true;
}

static void lock(void) {
    if (dev)
        SDL_LockAudioDevice(dev);
}

static void unlock(void) {
    if (dev)
        SDL_UnlockAudioDevice(dev);
}

static void player_free(Player *p) {
    tsf_close(p->sf);
    free(p->pcm);
    free(p->events);
    memset(p, 0, sizeof(*p));
}

static int player_alloc(void) {
    for (int i = 1; i < MAX_PLAYERS; i++) {
        if (!players[i].used) {
            memset(&players[i], 0, sizeof(Player));
            players[i].used = true;
            players[i].volume = 100;
            players[i].loops = 1;
            return i;
        }
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Đọc WAV (PCM 8/16 bit, IMA ADPCM)

static uint16_t rd16le(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t rd32le(const uint8_t *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }

static const int ima_steps[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97,
    107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
    876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871,
    5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623,
    27086, 29794, 32767,
};
static const int ima_index[16] = { -1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8 };

static int16_t ima_step(uint8_t nib, int *pred, int *idx) {
    int step = ima_steps[*idx];
    int diff = step >> 3;
    if (nib & 1) diff += step >> 2;
    if (nib & 2) diff += step >> 1;
    if (nib & 4) diff += step;
    if (nib & 8) diff = -diff;
    *pred += diff;
    if (*pred > 32767) *pred = 32767;
    if (*pred < -32768) *pred = -32768;
    *idx += ima_index[nib & 15];
    if (*idx < 0) *idx = 0;
    if (*idx > 88) *idx = 88;
    return (int16_t)*pred;
}

// Đổi tần số mẫu về RATE (nội suy tuyến tính) và gán làm dữ liệu phát của player
static void set_pcm(Player *p, const int16_t *src, uint32_t frames, int rate) {
    uint32_t out_len = frames ? (uint32_t)((uint64_t)frames * RATE / rate) : 0;
    p->pcm = malloc(sizeof(int16_t) * (out_len ? out_len : 1));
    for (uint32_t i = 0; i < out_len; i++) {
        double sp = (double)i * rate / RATE;
        uint32_t a = (uint32_t)sp;
        double f = sp - a;
        int16_t s0 = src[a < frames ? a : frames - 1];
        int16_t s1 = src[a + 1 < frames ? a + 1 : frames - 1];
        p->pcm[i] = (int16_t)(s0 + (s1 - s0) * f);
    }
    p->pcm_len = out_len;
    p->kind = KIND_WAV;
    p->total_samples = out_len;
}

static bool load_wav(Player *p, const uint8_t *d, size_t size) {
    if (size < 12 || memcmp(d, "RIFF", 4) || memcmp(d + 8, "WAVE", 4))
        return false;
    int fmt = 0, channels = 1, rate = 8000, bits = 16, block_align = 0;
    const uint8_t *data = NULL;
    uint32_t data_len = 0;
    size_t pos = 12;
    while (pos + 8 <= size) {
        uint32_t len = rd32le(d + pos + 4);
        const uint8_t *c = d + pos + 8;
        if (len > size - pos - 8)
            len = (uint32_t)(size - pos - 8);
        if (!memcmp(d + pos, "fmt ", 4) && len >= 16) {
            fmt = rd16le(c);
            channels = rd16le(c + 2);
            rate = (int)rd32le(c + 4);
            block_align = rd16le(c + 12);
            bits = rd16le(c + 14);
        } else if (!memcmp(d + pos, "data", 4)) {
            data = c;
            data_len = len;
        }
        pos += 8 + len + (len & 1);
    }
    if (!data || channels < 1 || channels > 2 || rate <= 0)
        return false;

    // Giải mã về mono int16 ở tần số gốc
    int16_t *src = NULL;
    uint32_t frames = 0;
    if (fmt == 1 && (bits == 8 || bits == 16)) {
        int bps = bits / 8 * channels;
        frames = data_len / bps;
        src = malloc(sizeof(int16_t) * (frames ? frames : 1));
        for (uint32_t i = 0; i < frames; i++) {
            int acc = 0;
            for (int c = 0; c < channels; c++) {
                const uint8_t *s = data + i * bps + c * (bits / 8);
                acc += bits == 8 ? (s[0] - 128) << 8 : (int16_t)rd16le(s);
            }
            src[i] = (int16_t)(acc / channels);
        }
    } else if (fmt == 0x11 && bits == 4 && block_align > 4 * channels) {
        uint32_t blocks = data_len / block_align;
        uint32_t per_block = (uint32_t)((block_align - 4 * channels) * 2 / channels + 1);
        src = malloc(sizeof(int16_t) * (blocks * per_block + 1));
        for (uint32_t b = 0; b < blocks; b++) {
            const uint8_t *blk = data + b * block_align;
            int pred[2], idx[2];
            for (int c = 0; c < channels; c++) {
                pred[c] = (int16_t)rd16le(blk + c * 4);
                idx[c] = blk[c * 4 + 2] > 88 ? 88 : blk[c * 4 + 2];
            }
            src[frames++] = (int16_t)pred[0];
            const uint8_t *q = blk + 4 * channels;
            int remain = block_align - 4 * channels;
            if (channels == 1) {
                for (int i = 0; i < remain; i++) {
                    src[frames++] = ima_step(q[i] & 15, &pred[0], &idx[0]);
                    src[frames++] = ima_step(q[i] >> 4, &pred[0], &idx[0]);
                }
            } else {
                // Stereo: mỗi kênh 4 byte (8 mẫu) xen kẽ; chỉ lấy kênh trái
                for (int i = 0; i + 8 <= remain; i += 8) {
                    for (int k = 0; k < 4; k++) {
                        src[frames++] = ima_step(q[i + k] & 15, &pred[0], &idx[0]);
                        src[frames++] = ima_step(q[i + k] >> 4, &pred[0], &idx[0]);
                    }
                }
            }
        }
    } else {
        vm_log("WAV dinh dang %d / %d bit chua ho tro", fmt, bits);
        return false;
    }

    set_pcm(p, src, frames, rate);
    free(src);
    return true;
}

// MP3 -> mono int16
static bool load_mp3(Player *p, const uint8_t *d, size_t size) {
    // Nhận dạng: thẻ ID3 hoặc frame sync 0xFFE
    bool id3 = size > 10 && memcmp(d, "ID3", 3) == 0;
    bool sync = size > 4 && d[0] == 0xFF && (d[1] & 0xE0) == 0xE0;
    if (!id3 && !sync)
        return false;
    drmp3_config cfg;
    drmp3_uint64 frames = 0;
    drmp3_int16 *pcm = drmp3_open_memory_and_read_pcm_frames_s16(d, size, &cfg, &frames, NULL);
    if (!pcm || frames == 0 || cfg.channels == 0) {
        drmp3_free(pcm, NULL);
        return false;
    }
    int16_t *mono = malloc(sizeof(int16_t) * (size_t)frames);
    for (drmp3_uint64 i = 0; i < frames; i++) {
        int acc = 0;
        for (drmp3_uint32 c = 0; c < cfg.channels; c++)
            acc += pcm[i * cfg.channels + c];
        mono[i] = (int16_t)(acc / (int)cfg.channels);
    }
    drmp3_free(pcm, NULL);
    set_pcm(p, mono, (uint32_t)frames, (int)cfg.sampleRate);
    free(mono);
    return true;
}

// AMR, AAC, M4A, tiếng trong 3GP (audio_dec.c)
static bool load_compressed(Player *p, const uint8_t *d, size_t size) {
    if (!audio_dec_probe(d, size))
        return false;
    int16_t *pcm = NULL;
    size_t frames = 0;
    int rate = 0;
    if (!audio_dec_decode(d, size, &pcm, &frames, &rate))
        return false;
    set_pcm(p, pcm, (uint32_t)frames, rate);
    free(pcm);
    return true;
}

// ---------------------------------------------------------------------------
// Đọc MIDI (SMF 0/1)

static uint32_t read_var(const uint8_t **pp, const uint8_t *end) {
    uint32_t v = 0;
    while (*pp < end) {
        uint8_t b = *(*pp)++;
        v = (v << 7) | (b & 0x7F);
        if (!(b & 0x80))
            break;
    }
    return v;
}

static int cmp_event(const void *a, const void *b) {
    const MidiEvent *x = a, *y = b;
    if (x->tick != y->tick)
        return x->tick < y->tick ? -1 : 1;
    return x->seq < y->seq ? -1 : x->seq > y->seq;
}

static void push_event(Player *p, uint32_t *cap, MidiEvent e) {
    if (p->event_count == *cap) {
        *cap = *cap ? *cap * 2 : 1024;
        p->events = realloc(p->events, sizeof(MidiEvent) * *cap);
    }
    e.seq = p->event_count;
    p->events[p->event_count++] = e;
}

static uint64_t midi_length_samples(Player *p) {
    // Tính độ dài theo bản đồ tempo
    double samples = 0;
    uint32_t tempo = 500000, last = 0;
    for (uint32_t i = 0; i < p->event_count; i++) {
        MidiEvent *e = &p->events[i];
        samples += (double)(e->tick - last) * tempo * RATE / (p->division * 1000000.0);
        last = e->tick;
        if (e->status == 0xFF)
            tempo = e->tempo;
    }
    return (uint64_t)samples;
}

static bool load_midi(Player *p, const uint8_t *d, size_t size) {
    if (size < 14 || memcmp(d, "MThd", 4))
        return false;
    uint32_t hlen = (uint32_t)((d[4] << 24) | (d[5] << 16) | (d[6] << 8) | d[7]);
    int ntracks = (d[10] << 8) | d[11];
    int division = (d[12] << 8) | d[13];
    if (division & 0x8000) {
        // SMPTE: quy đổi gần đúng sang tick / nốt đen ở tempo 120
        int fps = -(int8_t)(division >> 8);
        division = fps * (division & 0xFF) / 2;
    }
    if (division <= 0)
        division = 96;
    p->division = (uint16_t)division;

    uint32_t cap = 0;
    size_t pos = 8 + hlen;
    for (int t = 0; t < ntracks && pos + 8 <= size; t++) {
        uint32_t len = (uint32_t)((d[pos + 4] << 24) | (d[pos + 5] << 16) | (d[pos + 6] << 8) | d[pos + 7]);
        if (memcmp(d + pos, "MTrk", 4)) {
            pos += 8 + len;
            t--;
            continue;
        }
        const uint8_t *q = d + pos + 8;
        const uint8_t *end = q + len > d + size ? d + size : q + len;
        uint32_t tick = 0;
        uint8_t running = 0;
        while (q < end) {
            tick += read_var(&q, end);
            if (q >= end)
                break;
            uint8_t st = *q;
            if (st & 0x80) {
                q++;
            } else {
                st = running;
            }
            if (st == 0xFF) {
                if (q >= end)
                    break;
                uint8_t type = *q++;
                uint32_t ml = read_var(&q, end);
                if (type == 0x51 && ml == 3 && q + 3 <= end)
                    push_event(p, &cap, (MidiEvent){ tick, 0xFF, 0, 0, (uint32_t)((q[0] << 16) | (q[1] << 8) | q[2]) });
                if (type == 0x2F)
                    break;
                q += ml;
            } else if (st == 0xF0 || st == 0xF7) {
                q += read_var(&q, end);
            } else if (st >= 0x80) {
                running = st;
                int nd = ((st & 0xF0) == 0xC0 || (st & 0xF0) == 0xD0) ? 1 : 2;
                if (q + nd > end)
                    break;
                MidiEvent e = { tick, st, q[0] & 0x7F, nd == 2 ? q[1] & 0x7F : 0, 0 };
                q += nd;
                uint8_t type = st & 0xF0;
                if (type == 0x80 || type == 0x90 || type == 0xB0 || type == 0xC0 || type == 0xE0)
                    push_event(p, &cap, e);
            } else {
                break;
            }
        }
        pos += 8 + len;
    }
    if (!p->events)
        return false;
    qsort(p->events, p->event_count, sizeof(MidiEvent), cmp_event);
    sf_attach(p);
    p->kind = KIND_MIDI;
    seq_rewind(p);
    p->total_samples = midi_length_samples(p);
    return true;
}

// ---------------------------------------------------------------------------
// Tone sequence (ToneControl): chuyển thành sự kiện MIDI theo đơn vị ms

typedef struct {
    Player *p;
    uint32_t cap;
    double time_ms;
    int tempo;          // bpm
    int resolution;
    int volume;         // 0..127
    const int8_t *seq;
    int len;
    int blocks[128];    // vị trí bắt đầu của từng block
} ToneParser;

static void tone_note(ToneParser *tp, int note, int dur) {
    double ms = dur * 60000.0 * 4 / ((double)tp->tempo * tp->resolution);
    if (note >= 0) {
        push_event(tp->p, &tp->cap, (MidiEvent){ (uint32_t)tp->time_ms, 0x90, (uint8_t)note, (uint8_t)tp->volume, 0 });
        push_event(tp->p, &tp->cap, (MidiEvent){ (uint32_t)(tp->time_ms + ms * 0.95), 0x80, (uint8_t)note, 0, 0 });
    }
    tp->time_ms += ms;
}

static void tone_run(ToneParser *tp, int pos, int depth) {
    while (pos + 1 < tp->len && depth < 8) {
        int cmd = tp->seq[pos], arg = tp->seq[pos + 1];
        pos += 2;
        switch (cmd) {
        case -2:    // VERSION
            break;
        case -3:    // TEMPO
            tp->tempo = (arg & 0xFF) * 4;
            if (tp->tempo <= 0)
                tp->tempo = 120;
            break;
        case -4:    // RESOLUTION
            tp->resolution = arg > 0 ? arg : 64;
            break;
        case -5:    // BLOCK_START: bỏ qua thân block khi định nghĩa
            if (arg >= 0 && arg < 128)
                tp->blocks[arg] = pos;
            while (pos + 1 < tp->len && !(tp->seq[pos] == -6 && tp->seq[pos + 1] == arg))
                pos += 2;
            pos += 2;
            break;
        case -6:    // BLOCK_END: kết thúc lần phát block
            if (depth > 0)
                return;
            break;
        case -7:    // PLAY_BLOCK
            if (arg >= 0 && arg < 128 && tp->blocks[arg] >= 0)
                tone_run(tp, tp->blocks[arg], depth + 1);
            break;
        case -8:    // SET_VOLUME
            tp->volume = arg * 127 / 100;
            break;
        case -9: {  // REPEAT n note dur
            if (pos + 1 >= tp->len)
                return;
            int note = tp->seq[pos], dur = tp->seq[pos + 1] & 0xFF;
            pos += 2;
            for (int i = 0; i < arg; i++)
                tone_note(tp, note, dur);
            break;
        }
        default:
            tone_note(tp, cmd, arg & 0xFF);
            break;
        }
    }
}

static bool load_tone(Player *p, const int8_t *seq, int len) {
    ToneParser tp = { .p = p, .tempo = 120, .resolution = 64, .volume = 100, .seq = seq, .len = len };
    for (int i = 0; i < 128; i++)
        tp.blocks[i] = -1;
    tone_run(&tp, 0, 0);
    if (!p->events)
        return false;
    qsort(p->events, p->event_count, sizeof(MidiEvent), cmp_event);
    sf_attach(p);
    p->kind = KIND_MIDI;
    p->division = 0;
    seq_rewind(p);
    for (int c = 0; c < 16; c++)
        set_program(p, c, 80);      // square lead, giống tiếng chuông điện thoại
    p->total_samples = (uint64_t)(tp.time_ms * RATE / 1000.0);
    return true;
}

// ---------------------------------------------------------------------------
// Native j2menx.AudioPlayer

static Player *get(int h) {
    return h > 0 && h <= MAX_PLAYERS && players[h].used ? &players[h] : NULL;
}

static NativeResult A_create0(VMThread *t, Value *args, Value *ret) {
    (void)t;
    Object *data = args[0].l;
    ret->i = 0;
    if (!data || !audio_open())
        return NATIVE_OK;
    lock();
    int h = player_alloc();
    unlock();
    if (h) {
        // Giải mã ngoài khoá (MP3 / AAC dài có thể mất vài trăm ms): player chưa có kind nên bộ trộn bỏ qua,
        // set_pcm / load_midi gán kind sau cùng
        Player *p = &players[h];
        const uint8_t *d = ARRAY_DATA(data, uint8_t);
        size_t n = (size_t)ARRAY_LEN(data);
        if (!load_wav(p, d, n) && !load_midi(p, d, n) && !load_compressed(p, d, n) && !load_mp3(p, d, n)) {
            lock();
            player_free(p);
            unlock();
            h = 0;
        }
    }
    ret->i = h;
    return NATIVE_OK;
}

static NativeResult A_createTone0(VMThread *t, Value *args, Value *ret) {
    (void)t;
    Object *seq = args[0].l;
    ret->i = 0;
    if (!seq || !audio_open())
        return NATIVE_OK;
    lock();
    int h = player_alloc();
    if (h && !load_tone(&players[h], ARRAY_DATA(seq, int8_t), ARRAY_LEN(seq))) {
        player_free(&players[h]);
        h = 0;
    }
    unlock();
    ret->i = h;
    return NATIVE_OK;
}

static NativeResult A_start0(VMThread *t, Value *args, Value *ret) {
    (void)t;
    (void)ret;
    lock();
    Player *p = get(args[0].i);
    if (p) {
        if (p->kind == KIND_WAV && p->pcm_pos >= p->pcm_len)
            p->pcm_pos = 0;
        if (p->kind == KIND_MIDI && p->event_pos >= p->event_count)
            seq_rewind(p);
        p->playing = true;
        p->ended = false;
    }
    unlock();
    return NATIVE_OK;
}

static NativeResult A_stop0(VMThread *t, Value *args, Value *ret) {
    (void)t;
    (void)ret;
    lock();
    Player *p = get(args[0].i);
    if (p) {
        p->playing = false;
        if (p->kind == KIND_MIDI)
            all_notes_off(p);
    }
    unlock();
    return NATIVE_OK;
}

static NativeResult A_setLoop0(VMThread *t, Value *args, Value *ret) {
    (void)t;
    (void)ret;
    lock();
    Player *p = get(args[0].i);
    if (p)
        p->loops = args[1].i;
    unlock();
    return NATIVE_OK;
}

static NativeResult A_setVolume0(VMThread *t, Value *args, Value *ret) {
    (void)t;
    (void)ret;
    lock();
    Player *p = get(args[0].i);
    if (p)
        p->volume = args[1].i < 0 ? 0 : args[1].i > 100 ? 100 : args[1].i;
    unlock();
    return NATIVE_OK;
}

static NativeResult A_getTime0(VMThread *t, Value *args, Value *ret) {
    (void)t;
    lock();
    Player *p = get(args[0].i);
    uint64_t s = 0;
    if (p)
        s = p->kind == KIND_WAV ? p->pcm_pos : p->sample_pos;
    unlock();
    ret->j = (jlong)(s * 1000000 / RATE);
    return NATIVE_OK;
}

static NativeResult A_setTime0(VMThread *t, Value *args, Value *ret) {
    (void)t;
    lock();
    Player *p = get(args[0].i);
    jlong us = args[1].j < 0 ? 0 : args[1].j;
    uint64_t target = (uint64_t)us * RATE / 1000000;
    if (p && p->kind == KIND_WAV) {
        p->pcm_pos = target < p->pcm_len ? (uint32_t)target : p->pcm_len;
    } else if (p && p->kind == KIND_MIDI) {
        // Tua: chạy lại sự kiện điều khiển (không phát nốt) tới vị trí đích
        all_notes_off(p);
        seq_rewind(p);
        double tps;
        while (p->event_pos < p->event_count) {
            tps = ticks_per_sample(p);
            uint64_t at = p->sample_pos + (uint64_t)((p->events[p->event_pos].tick - p->tick) / tps);
            if (at > target)
                break;
            MidiEvent *e = &p->events[p->event_pos++];
            p->sample_pos = at;
            p->tick = e->tick;
            if ((e->status & 0xF0) != 0x90)
                midi_event(p, e);
        }
        tps = ticks_per_sample(p);
        p->tick += (double)(target - p->sample_pos) * tps;
        p->sample_pos = target;
    }
    unlock();
    ret->j = us;
    return NATIVE_OK;
}

static NativeResult A_duration0(VMThread *t, Value *args, Value *ret) {
    (void)t;
    lock();
    Player *p = get(args[0].i);
    ret->j = p ? (jlong)(p->total_samples * 1000000 / RATE) : -1;
    unlock();
    return NATIVE_OK;
}

static NativeResult A_close0(VMThread *t, Value *args, Value *ret) {
    (void)t;
    (void)ret;
    lock();
    Player *p = get(args[0].i);
    if (p)
        player_free(p);
    unlock();
    return NATIVE_OK;
}

static NativeResult A_playTone0(VMThread *t, Value *args, Value *ret) {
    (void)t;
    (void)ret;
    int note = args[0].i, dur = args[1].i, vol = args[2].i;
    if (note < 0 || note > 127 || dur <= 0 || !audio_open())
        return NATIVE_OK;
    lock();
    Player *p = &players[MAX_PLAYERS];
    if (!p->used) {
        memset(p, 0, sizeof(*p));
        p->used = true;
        p->kind = KIND_MIDI;
        p->volume = 100;
        p->loops = 1;
        sf_attach(p);
        reset_channels(p);
        for (int c = 0; c < 16; c++)
            set_program(p, c, 80);
    }
    // Phát ngay 1 nốt; tự tắt nhờ sequence 1 sự kiện note off
    free(p->events);
    p->events = malloc(sizeof(MidiEvent));
    p->events[0] = (MidiEvent){ (uint32_t)dur, 0x80, (uint8_t)note, 0, 0 };
    p->event_count = 1;
    p->event_pos = 0;
    p->division = 0;
    p->tick = 0;
    p->sample_pos = 0;
    p->volume = vol < 0 ? 0 : vol > 100 ? 100 : vol;
    note_on(p, 0, note, 110);
    p->playing = true;
    p->ended = false;
    unlock();
    return NATIVE_OK;
}

// Gọi mỗi frame từ host: báo các player đã phát xong
void midp_audio_poll(void) {
    if (!dev)
        return;
    int ended[MAX_PLAYERS];
    int n = 0;
    lock();
    for (int i = 1; i < MAX_PLAYERS; i++) {
        if (players[i].used && players[i].ended) {
            players[i].ended = false;
            ended[n++] = i;
        }
    }
    unlock();
    for (int i = 0; i < n; i++)
        midp_post_event(MIDP_EV_MEDIA_END, ended[i], 0);
}

void midp_audio_register(void) {
    const char *A = "j2menx/AudioPlayer";
    native_register(A, "create0", "([B)I", A_create0);
    native_register(A, "createTone0", "([B)I", A_createTone0);
    native_register(A, "start0", "(I)V", A_start0);
    native_register(A, "stop0", "(I)V", A_stop0);
    native_register(A, "setLoop0", "(II)V", A_setLoop0);
    native_register(A, "setVolume0", "(II)V", A_setVolume0);
    native_register(A, "getTime0", "(I)J", A_getTime0);
    native_register(A, "setTime0", "(IJ)J", A_setTime0);
    native_register(A, "duration0", "(I)J", A_duration0);
    native_register(A, "close0", "(I)V", A_close0);
    native_register(A, "playTone0", "(III)V", A_playTone0);
}

void midp_audio_set_soundfont(const char *path) {
    if (!path)
        path = "";
    if (sf_base && strcmp(path, sf_path) == 0)
        return;
    // Gọi trước khi game chạy: không còn player nào dùng bản cũ
    tsf_close(sf_base);
    sf_base = NULL;
    snprintf(sf_path, sizeof(sf_path), "%s", path);
    if (!*path)
        return;
    uint32_t t0 = SDL_GetTicks();
    if (strcmp(path, MIDP_SOUNDFONT_BUILTIN) == 0)
        sf_base = tsf_load_memory(builtin_sf2, (int)(builtin_sf2_end - builtin_sf2));
    else
        sf_base = tsf_load_filename(path);
    if (!sf_base) {
        vm_log("Khong nap duoc SoundFont %s", path);
        sf_path[0] = 0;
        return;
    }
    tsf_set_output(sf_base, TSF_MONO, RATE, 0.0f);
    vm_log("SoundFont %s: %d preset, %u ms", strcmp(path, MIDP_SOUNDFONT_BUILTIN) ? path : "TimGM6mb (co san)",
           tsf_get_presetcount(sf_base), SDL_GetTicks() - t0);
}

void midp_audio_shutdown(void) {
    if (dev) {
        SDL_CloseAudioDevice(dev);
        dev = 0;
    }
#ifndef __SWITCH__
    if (dump)
        fclose(dump);
    dump = NULL;
#endif
    for (int i = 0; i <= MAX_PLAYERS; i++) {
        if (players[i].used)
            player_free(&players[i]);
    }
}
