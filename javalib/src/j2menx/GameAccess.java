package j2menx;

import javax.microedition.lcdui.Image;

// Cầu nối để package lcdui.game dùng phần nội bộ của lcdui
public final class GameAccess {
    private GameAccess() {
    }

    public static int keyStates() {
        return javax.microedition.lcdui.DisplayAccess.keyStates();
    }

    public static void flush(Image img) {
        javax.microedition.lcdui.DisplayAccess.flush(img);
    }
}
