package com.motorola.multimedia;

import javax.microedition.lcdui.DisplayAccess;

public class Vibrator {
    private Vibrator() {
    }

    public static void vibrateFor(int ms) {
        DisplayAccess.vibrate(ms);
    }

    public static void vibratePeriodicaly(int on, int off) {
        DisplayAccess.vibrate(on);
    }

    public static void vibratePeriodically(int on, int off) {
        DisplayAccess.vibrate(on);
    }

    public static void vibratePeriodicaly(int on) {
        DisplayAccess.vibrate(on);
    }

    public static void setVibrateTone(int tone) {
    }

    public static void stopVibrator() {
        DisplayAccess.vibrate(0);
    }
}
