package com.siemens.mp.game;

import javax.microedition.lcdui.DisplayAccess;

public class Vibrator {
    private Vibrator() {
    }

    public static void startVibrator() {
        DisplayAccess.vibrate(1000);
    }

    public static void stopVibrator() {
        DisplayAccess.vibrate(0);
    }

    public static void triggerVibrator(int duration) {
        DisplayAccess.vibrate(duration);
    }
}
