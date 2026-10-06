package javax.microedition.lcdui;

// Dùng nội bộ bởi j2menx.GameAccess (không phải API MIDP)
public final class DisplayAccess {
    private DisplayAccess() {
    }

    public static int keyStates() {
        int s = Display.keyStates | Display.keyLatched;
        Display.keyLatched = 0;
        return s;
    }

    public static void fatalError(String msg) {
        Display.fatalError(msg);
    }

    public static void flush(Image img) {
        Display.flushImage(img, 0, 0, img.width, img.height);
    }
}
