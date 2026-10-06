package com.siemens.mp.game;

import java.io.ByteArrayOutputStream;

public class MelodyComposer {
    // Nốt: 0..59 = C0..H4 (12 nốt mỗi quãng tám)
    public static final int TONE_C0 = 0, TONE_CIS0 = 1, TONE_D0 = 2, TONE_DIS0 = 3, TONE_E0 = 4, TONE_F0 = 5,
            TONE_FIS0 = 6, TONE_G0 = 7, TONE_GIS0 = 8, TONE_A0 = 9, TONE_AIS0 = 10, TONE_H0 = 11;
    public static final int TONE_C1 = 12, TONE_C2 = 24, TONE_C3 = 36, TONE_C4 = 48, TONE_H4 = 59;
    public static final int TONE_PAUSE = 60, TONE_REPEAT = 61, TONE_STOP = 62, TONE_REPEV = 63, TONE_REPON = 64,
            TONE_MARK = 65;
    // Độ dài: 0 = nốt tròn ... 5 = 1/32; +6 = có chấm
    public static final int TONELENGTH_1_1 = 0, TONELENGTH_1_2 = 1, TONELENGTH_1_4 = 2, TONELENGTH_1_8 = 3,
            TONELENGTH_1_16 = 4, TONELENGTH_1_32 = 5, TONELENGTH_DOTTED_1_1 = 6, TONELENGTH_DOTTED_1_2 = 7,
            TONELENGTH_DOTTED_1_4 = 8, TONELENGTH_DOTTED_1_8 = 9, TONELENGTH_DOTTED_1_16 = 10,
            TONELENGTH_DOTTED_1_32 = 11;
    public static final int NO_REPEAT = -1;

    private static final int MAX = 1024;
    private int[] notes = new int[MAX];
    private int[] lengths = new int[MAX];
    private int count;
    private int bpm = 120;

    public MelodyComposer() {
    }

    public MelodyComposer(int[] tones, int bpm) {
        this.bpm = bpm;
        for (int i = 0; i + 1 < tones.length; i += 2) {
            appendNote(tones[i], tones[i + 1]);
        }
    }

    public void setBPM(int bpm) {
        this.bpm = bpm;
    }

    public void appendNote(int note, int length) {
        if (count >= MAX) {
            throw new IllegalArgumentException("Melody qua dai");
        }
        notes[count] = note;
        lengths[count] = length;
        count++;
    }

    public int length() {
        return count;
    }

    public static int maxLength() {
        return MAX;
    }

    public void resetMelody() {
        count = 0;
    }

    public Melody getMelody() {
        // Đổi sang ToneControl: độ phân giải 64 (nốt tròn = 64)
        ByteArrayOutputStream b = new ByteArrayOutputStream();
        b.write(-2);
        b.write(1);
        b.write(-3);
        b.write(Math.max(5, Math.min(127, bpm / 4)));
        b.write(-4);
        b.write(64);
        for (int i = 0; i < count; i++) {
            int n = notes[i];
            if (n == TONE_STOP) {
                break;
            }
            if (n > TONE_PAUSE) {
                continue;
            }
            int len = lengths[i];
            boolean dotted = len >= 6;
            int dur = 64 >> Math.min(5, dotted ? len - 6 : len);
            if (dotted) {
                dur = dur * 3 / 2;
            }
            b.write(n == TONE_PAUSE ? -1 : Math.min(127, 48 + n));
            b.write(Math.max(1, dur));
        }
        return new Melody(b.toByteArray());
    }
}
