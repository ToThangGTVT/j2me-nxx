package javax.microedition.lcdui;

public abstract class Canvas extends Displayable {
    public static final int UP = 1;
    public static final int DOWN = 6;
    public static final int LEFT = 2;
    public static final int RIGHT = 5;
    public static final int FIRE = 8;
    public static final int GAME_A = 9;
    public static final int GAME_B = 10;
    public static final int GAME_C = 11;
    public static final int GAME_D = 12;

    public static final int KEY_NUM0 = 48;
    public static final int KEY_NUM1 = 49;
    public static final int KEY_NUM2 = 50;
    public static final int KEY_NUM3 = 51;
    public static final int KEY_NUM4 = 52;
    public static final int KEY_NUM5 = 53;
    public static final int KEY_NUM6 = 54;
    public static final int KEY_NUM7 = 55;
    public static final int KEY_NUM8 = 56;
    public static final int KEY_NUM9 = 57;
    public static final int KEY_STAR = 42;
    public static final int KEY_POUND = 35;

    // Mã phím theo hãng (mặc định Nokia), app truyền qua thuộc tính j2menx.keys
    static int KEY_UP = -1;
    static int KEY_DOWN = -2;
    static int KEY_LEFT = -3;
    static int KEY_RIGHT = -4;
    static int KEY_FIRE = -5;
    static int KEY_SOFT_LEFT = -6;
    static int KEY_SOFT_RIGHT = -7;
    static int KEY_CLEAR = -8;

    static {
        String p = System.getProperty("j2menx.keys");
        if (p != null) {
            int[] v = new int[8];
            int n = 0, start = 0;
            try {
                while (n < 8) {
                    int comma = p.indexOf(',', start);
                    v[n++] = Integer.parseInt(comma < 0 ? p.substring(start) : p.substring(start, comma));
                    if (comma < 0) {
                        break;
                    }
                    start = comma + 1;
                }
            } catch (NumberFormatException e) {
                n = 0;
            }
            if (n == 8) {
                KEY_UP = v[0];
                KEY_DOWN = v[1];
                KEY_LEFT = v[2];
                KEY_RIGHT = v[3];
                KEY_FIRE = v[4];
                KEY_SOFT_LEFT = v[5];
                KEY_SOFT_RIGHT = v[6];
                KEY_CLEAR = v[7];
            }
        }
    }

    boolean fullScreen;

    protected Canvas() {
    }

    public boolean isDoubleBuffered() {
        return true;
    }

    public boolean hasPointerEvents() {
        return true;
    }

    public boolean hasPointerMotionEvents() {
        return true;
    }

    public boolean hasRepeatEvents() {
        return true;
    }

    static int actionOf(int keyCode) {
        if (keyCode == KEY_UP || keyCode == KEY_NUM2) return UP;
        if (keyCode == KEY_DOWN || keyCode == KEY_NUM8) return DOWN;
        if (keyCode == KEY_LEFT || keyCode == KEY_NUM4) return LEFT;
        if (keyCode == KEY_RIGHT || keyCode == KEY_NUM6) return RIGHT;
        if (keyCode == KEY_FIRE || keyCode == KEY_NUM5) return FIRE;
        switch (keyCode) {
        case KEY_NUM7: return GAME_A;
        case KEY_NUM9: return GAME_B;
        case KEY_STAR: return GAME_C;
        case KEY_POUND: return GAME_D;
        default: return 0;
        }
    }

    static int actionBit(int keyCode) {
        int a = actionOf(keyCode);
        return a == 0 ? 0 : 1 << a;
    }

    public int getKeyCode(int gameAction) {
        switch (gameAction) {
        case UP: return KEY_UP;
        case DOWN: return KEY_DOWN;
        case LEFT: return KEY_LEFT;
        case RIGHT: return KEY_RIGHT;
        case FIRE: return KEY_FIRE;
        case GAME_A: return KEY_NUM7;
        case GAME_B: return KEY_NUM9;
        case GAME_C: return KEY_STAR;
        case GAME_D: return KEY_POUND;
        default: throw new IllegalArgumentException();
        }
    }

    public String getKeyName(int keyCode) {
        if (keyCode >= KEY_NUM0 && keyCode <= KEY_NUM9) {
            return String.valueOf((char) keyCode);
        }
        if (keyCode == KEY_STAR) return "*";
        if (keyCode == KEY_POUND) return "#";
        if (keyCode == KEY_UP) return "Up";
        if (keyCode == KEY_DOWN) return "Down";
        if (keyCode == KEY_LEFT) return "Left";
        if (keyCode == KEY_RIGHT) return "Right";
        if (keyCode == KEY_FIRE) return "Select";
        if (keyCode == KEY_SOFT_LEFT) return "Soft1";
        if (keyCode == KEY_SOFT_RIGHT) return "Soft2";
        if (keyCode == KEY_CLEAR) return "Clear";
        return "Key" + keyCode;
    }

    public int getGameAction(int keyCode) {
        return actionOf(keyCode);
    }

    public void setFullScreenMode(boolean mode) {
        if (fullScreen != mode) {
            fullScreen = mode;
            repaint();
        }
    }

    // FullCanvas của Nokia nhận phím mềm như phím thường
    boolean noSoftCommands() {
        return false;
    }

    protected void keyPressed(int keyCode) {
    }

    protected void keyRepeated(int keyCode) {
    }

    protected void keyReleased(int keyCode) {
    }

    protected void pointerPressed(int x, int y) {
    }

    protected void pointerReleased(int x, int y) {
    }

    protected void pointerDragged(int x, int y) {
    }

    public final void repaint(int x, int y, int width, int height) {
        repaint();
    }

    public final void repaint() {
        if (shown) {
            Display.get().requestRepaint();
        }
    }

    public final void serviceRepaints() {
        if (shown) {
            Display.get().serviceRepaints(this);
        }
    }

    protected void showNotify() {
    }

    protected void hideNotify() {
    }

    protected abstract void paint(Graphics g);

    void paintScreen(Graphics g) {
        paint(g);
        if (!fullScreen && !commands.isEmpty() && !noSoftCommands()) {
            g.reset();
            Screen.paintSoftBar(g, leftLabel(), rightLabel());
        }
    }

    void keyEvent(int type, int code) {
        if (type == Display.EV_KEY_PRESSED) {
            keyPressed(code);
        } else if (type == Display.EV_KEY_RELEASED) {
            keyReleased(code);
        } else {
            keyRepeated(code);
        }
    }

    void pointerEvent(int type, int x, int y) {
        if (type == Display.EV_POINTER_PRESSED) {
            pointerPressed(x, y);
        } else if (type == Display.EV_POINTER_RELEASED) {
            pointerReleased(x, y);
        } else {
            pointerDragged(x, y);
        }
    }

    void showNotify0() {
        showNotify();
    }

    void hideNotify0() {
        hideNotify();
    }
}
