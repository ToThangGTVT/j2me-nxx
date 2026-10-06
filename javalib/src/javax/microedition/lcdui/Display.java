package javax.microedition.lcdui;

import java.util.Vector;
import javax.microedition.midlet.MIDlet;

public class Display {
    public static final int LIST_ELEMENT = 1;
    public static final int CHOICE_GROUP_ELEMENT = 2;
    public static final int ALERT = 3;
    public static final int COLOR_BACKGROUND = 0;
    public static final int COLOR_FOREGROUND = 1;
    public static final int COLOR_HIGHLIGHTED_BACKGROUND = 2;
    public static final int COLOR_HIGHLIGHTED_FOREGROUND = 3;
    public static final int COLOR_BORDER = 4;
    public static final int COLOR_HIGHLIGHTED_BORDER = 5;

    // Loại sự kiện (khớp với source/midp/midp.h)
    static final int EV_KEY_PRESSED = 1;
    static final int EV_KEY_RELEASED = 2;
    static final int EV_KEY_REPEATED = 3;
    static final int EV_PAINT = 4;
    static final int EV_POINTER_PRESSED = 5;
    static final int EV_POINTER_RELEASED = 6;
    static final int EV_POINTER_DRAGGED = 7;
    static final int EV_SERIAL = 8;
    static final int EV_PAUSE = 9;
    static final int EV_RESUME = 10;
    static final int EV_DESTROY = 11;
    static final int EV_MEDIA_END = 12;

    private static Display instance;
    static final Object paintLock = new Object();

    static int screenW;
    static int screenH;
    static Image screen;
    static Graphics screenGraphics;

    private Displayable current;
    private boolean repaintPending;
    private final Vector serialQueue = new Vector();

    // Trạng thái phím cho GameCanvas.getKeyStates()
    static int keyStates;
    static int keyLatched;

    // Menu lệnh (khi có nhiều hơn 2 Command)
    private Vector menuItems;
    private int menuIndex;

    private static String fatal;

    private Display() {
    }

    public static Display getDisplay(MIDlet m) {
        return get();
    }

    static synchronized Display get() {
        if (instance == null) {
            screenW = screenWidth0();
            screenH = screenHeight0();
            screen = Image.createImage(screenW, screenH);
            screenGraphics = screen.getGraphics();
            instance = new Display();
        }
        return instance;
    }

    public Displayable getCurrent() {
        return current;
    }

    public void setCurrent(Displayable next) {
        if (next == null) {
            return;
        }
        if (next instanceof Alert) {
            ((Alert) next).setNext(current);
        }
        setCurrent0(next);
    }

    public void setCurrent(Alert alert, Displayable next) {
        if (alert == null || next == null || next instanceof Alert) {
            throw new NullPointerException();
        }
        alert.setNext(next);
        setCurrent0(alert);
    }

    void setCurrent0(Displayable next) {
        Displayable old = current;
        if (old == next) {
            requestRepaint();
            return;
        }
        menuItems = null;
        current = next;
        if (old != null) {
            old.shown = false;
            try {
                old.hideNotify0();
            } catch (Throwable e) {
                e.printStackTrace();
            }
        }
        next.shown = true;
        try {
            next.showNotify0();
        } catch (Throwable e) {
            e.printStackTrace();
        }
        requestRepaint();
    }

    public void setCurrentItem(Item item) {
        if (item.owner != null) {
            setCurrent(item.owner);
        }
    }

    public void callSerially(Runnable r) {
        synchronized (serialQueue) {
            serialQueue.addElement(r);
        }
        postSerial0();
    }

    public boolean flashBacklight(int duration) {
        return true;
    }

    public boolean vibrate(int duration) {
        vibrate0(duration);
        return true;
    }

    public int numColors() {
        return 1 << 24;
    }

    public boolean isColor() {
        return true;
    }

    public int numAlphaLevels() {
        return 256;
    }

    public int getColor(int colorSpecifier) {
        switch (colorSpecifier) {
        case COLOR_BACKGROUND: return 0xffffff;
        case COLOR_FOREGROUND: return 0x000000;
        case COLOR_HIGHLIGHTED_BACKGROUND: return 0x2060c0;
        case COLOR_HIGHLIGHTED_FOREGROUND: return 0xffffff;
        case COLOR_BORDER: return 0x808080;
        case COLOR_HIGHLIGHTED_BORDER: return 0x2060c0;
        default: throw new IllegalArgumentException();
        }
    }

    public int getBorderStyle(boolean highlighted) {
        return highlighted ? Graphics.SOLID : Graphics.DOTTED;
    }

    public int getBestImageWidth(int imageType) {
        return screenW;
    }

    public int getBestImageHeight(int imageType) {
        return screenH;
    }

    // ------------------------------------------------------------------
    // Vẽ

    void requestRepaint() {
        synchronized (paintLock) {
            repaintPending = true;
        }
        postRepaint0();
    }

    void serviceRepaints(Displayable d) {
        if (d == current && repaintPending) {
            paintNow();
        }
    }

    private void paintNow() {
        synchronized (paintLock) {
            repaintPending = false;
            Displayable d = current;
            Graphics g = screenGraphics;
            g.reset();
            if (fatal != null) {
                paintFatal(g);
            } else if (d != null) {
                try {
                    d.paintScreen(g);
                } catch (Throwable e) {
                    System.out.println("Loi khi ve: " + e);
                    e.printStackTrace();
                }
                if (menuItems != null) {
                    g.reset();
                    paintMenu(g);
                }
            } else {
                g.setColor(0);
                g.fillRect(0, 0, screenW, screenH);
            }
            flush();
        }
    }

    // Chép bộ đệm màn hình ra host
    static void flush() {
        flush0(screen.pixels, screenW, screenH);
    }

    // GameCanvas.flushGraphics()
    static void flushImage(Image img, int x, int y, int w, int h) {
        synchronized (paintLock) {
            Graphics g = screenGraphics;
            g.reset();
            g.drawRegionImpl(img, 0, 0, Math.min(img.width, screenW), Math.min(img.height, screenH), 0, 0, 0, false);
            Display d = instance;
            if (d != null && d.menuItems != null) {
                d.paintMenu(g);
            }
            flush();
        }
    }

    static void fatalError(String msg) {
        fatal = msg;
        get().requestRepaint();
    }

    private void paintFatal(Graphics g) {
        g.setColor(0x400000);
        g.fillRect(0, 0, screenW, screenH);
        g.setColor(0xffffff);
        g.setFont(Font.getFont(Font.FACE_SYSTEM, Font.STYLE_BOLD, Font.SIZE_MEDIUM));
        g.drawString("Loi", 4, 4, Graphics.TOP | Graphics.LEFT);
        g.setFont(Font.getFont(Font.FACE_SYSTEM, Font.STYLE_PLAIN, Font.SIZE_SMALL));
        Screen.drawWrapped(g, fatal, 4, 30, screenW - 8);
    }

    // ------------------------------------------------------------------
    // Menu lệnh

    private void openMenu(Vector cmds) {
        menuItems = cmds;
        menuIndex = 0;
        requestRepaint();
    }

    private void paintMenu(Graphics g) {
        Font f = Font.getFont(Font.FACE_SYSTEM, Font.STYLE_PLAIN, Font.SIZE_MEDIUM);
        g.setFont(f);
        int lh = f.getHeight() + 6;
        int n = menuItems.size();
        int h = n * lh + 4;
        int y0 = screenH - Screen.SOFTBAR_H - h;
        int x0 = 0;
        int w = screenW * 3 / 4;
        g.setColor(0x202020);
        g.fillRect(x0, y0, w, h);
        g.setColor(0x808080);
        g.drawRect(x0, y0, w - 1, h - 1);
        for (int i = 0; i < n; i++) {
            Command c = (Command) menuItems.elementAt(i);
            int y = y0 + 2 + i * lh;
            if (i == menuIndex) {
                g.setColor(0x2060c0);
                g.fillRect(x0 + 2, y, w - 4, lh);
            }
            g.setColor(0xffffff);
            g.drawString((i + 1) + ". " + c.getLabel(), x0 + 6, y + 3, Graphics.TOP | Graphics.LEFT);
        }
        Screen.paintSoftBar(g, "Chon", "Huy");
    }

    private boolean menuKey(int code) {
        if (menuItems == null) {
            return false;
        }
        int n = menuItems.size();
        int action = Canvas.actionOf(code);
        if (action == Canvas.UP) {
            menuIndex = (menuIndex + n - 1) % n;
        } else if (action == Canvas.DOWN) {
            menuIndex = (menuIndex + 1) % n;
        } else if (code == Canvas.KEY_SOFT_RIGHT || code == Canvas.KEY_CLEAR) {
            menuItems = null;
        } else if (action == Canvas.FIRE || code == Canvas.KEY_SOFT_LEFT) {
            Command c = (Command) menuItems.elementAt(menuIndex);
            menuItems = null;
            fireCommand(current, c);
        } else if (code >= Canvas.KEY_NUM1 && code <= Canvas.KEY_NUM9 && code - Canvas.KEY_NUM1 < n) {
            Command c = (Command) menuItems.elementAt(code - Canvas.KEY_NUM1);
            menuItems = null;
            fireCommand(current, c);
        }
        requestRepaint();
        return true;
    }

    static void fireCommand(Displayable d, Command c) {
        if (d != null && d.listener != null && c != null) {
            try {
                d.listener.commandAction(c, d);
            } catch (Throwable e) {
                e.printStackTrace();
            }
        }
    }

    // Phím mềm: trả về true nếu đã dùng phím để chạy Command
    private boolean softKey(Displayable d, int code) {
        if (d.commands.isEmpty() || (d instanceof Canvas && ((Canvas) d).noSoftCommands())) {
            return false;
        }
        Command[] pair = d.softCommands();
        if (code == Canvas.KEY_SOFT_LEFT) {
            if (pair[0] == null) {
                return pair[1] != null;
            }
            Vector menu = d.menuCommands();
            if (menu.size() > 1) {
                openMenu(menu);
            } else {
                fireCommand(d, pair[0]);
            }
            return true;
        }
        if (code == Canvas.KEY_SOFT_RIGHT) {
            if (pair[1] != null) {
                fireCommand(d, pair[1]);
            }
            return true;
        }
        return false;
    }

    // ------------------------------------------------------------------
    // Vòng lặp sự kiện, chạy trên thread riêng do VM tạo

    static void eventLoop() {
        Display d = get();
        int[] ev = new int[4];
        while (true) {
            waitEvent0(ev);
            try {
                d.dispatch(ev);
            } catch (Throwable e) {
                System.out.println("Loi xu ly su kien: " + e);
                e.printStackTrace();
            }
        }
    }

    private void dispatch(int[] ev) {
        Displayable c = current;
        switch (ev[0]) {
        case EV_KEY_PRESSED:
        case EV_KEY_RELEASED:
        case EV_KEY_REPEATED: {
            int code = ev[1];
            int bit = Canvas.actionBit(code);
            if (ev[0] == EV_KEY_PRESSED) {
                keyStates |= bit;
                keyLatched |= bit;
            } else if (ev[0] == EV_KEY_RELEASED) {
                keyStates &= ~bit;
            }
            if (c == null || fatal != null) {
                return;
            }
            if (ev[0] == EV_KEY_PRESSED || ev[0] == EV_KEY_REPEATED) {
                if (menuKey(code)) {
                    return;
                }
                if (ev[0] == EV_KEY_PRESSED && softKey(c, code)) {
                    return;
                }
            }
            c.keyEvent(ev[0], code);
            break;
        }
        case EV_POINTER_PRESSED:
        case EV_POINTER_RELEASED:
        case EV_POINTER_DRAGGED:
            if (c != null && menuItems == null) {
                c.pointerEvent(ev[0], ev[1], ev[2]);
            }
            break;
        case EV_PAINT:
            if (repaintPending) {
                paintNow();
            }
            break;
        case EV_SERIAL:
            while (true) {
                Runnable r;
                synchronized (serialQueue) {
                    if (serialQueue.isEmpty()) {
                        break;
                    }
                    r = (Runnable) serialQueue.elementAt(0);
                    serialQueue.removeElementAt(0);
                }
                try {
                    r.run();
                } catch (Throwable e) {
                    e.printStackTrace();
                }
            }
            break;
        case EV_PAUSE:
            if (c != null) {
                c.hideNotify0();
            }
            break;
        case EV_MEDIA_END:
            j2menx.AudioPlayer.mediaEnded(ev[1]);
            break;
        case EV_RESUME:
            if (c != null) {
                c.showNotify0();
            }
            requestRepaint();
            break;
        default:
            break;
        }
    }

    private static native int screenWidth0();

    private static native int screenHeight0();

    private static native void waitEvent0(int[] ev);

    private static native void postRepaint0();

    private static native void postSerial0();

    private static native void flush0(int[] pixels, int w, int h);

    private static native void vibrate0(int ms);
}
