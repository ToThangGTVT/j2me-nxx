package com.samsung.util;

import javax.microedition.lcdui.DisplayAccess;

public class Vibration {
    private Vibration() {
    }

    public static boolean isSupported() {
        return true;
    }

    public static void start(int duration, int strength) {
        DisplayAccess.vibrate(duration * 1000);
    }

    public static void stop() {
        DisplayAccess.vibrate(0);
    }
}
