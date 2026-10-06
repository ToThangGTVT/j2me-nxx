package com.siemens.mp.game;

import j2menx.AudioPlayer;

public class Sound {
    private Sound() {
    }

    public static void playTone(int freq, int time) {
        if (freq <= 0 || time <= 0) {
            return;
        }
        int note = (int) Math.floor(69 + 12 * Math.log(freq / 440.0) / Math.log(2) + 0.5);
        AudioPlayer.playTone(Math.max(0, Math.min(127, note)), time, 80);
    }
}
