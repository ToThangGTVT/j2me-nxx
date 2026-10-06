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

    // Rung máy (ms), dùng cho API của các hãng
    public static void vibrate(int ms) {
        Display.get().vibrate(ms);
    }

    // Vẽ ảnh thẳng lên màn hình rồi đẩy ra (ExtendedImage.blitToScreen của Siemens)
    public static void blitToScreen(Image img, int x, int y) {
        synchronized (Display.paintLock) {
            Graphics g = Display.screenGraphics;
            g.reset();
            g.drawImage(img, x, y, Graphics.TOP | Graphics.LEFT);
            Display.flush();
        }
    }

    // Cho M3G: mảng pixel đích và info = {transX, transY, clipX, clipY, clipW, clipH, imgW, imgH}
    public static int[] graphicsTarget(Graphics g, int[] info) {
        info[0] = g.transX;
        info[1] = g.transY;
        info[2] = g.clipX;
        info[3] = g.clipY;
        info[4] = g.clipW;
        info[5] = g.clipH;
        info[6] = g.img.width;
        info[7] = g.img.height;
        return g.img.pixels;
    }

    public static int[] pixels(Image img) {
        return img.pixels;
    }

    public static void flush(Image img) {
        Display.flushImage(img, 0, 0, img.width, img.height);
    }
}
