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

    // Mã phím kiểu Nokia (khớp với source/midp)
    static final int KEY_UP = -1;
    static final int KEY_DOWN = -2;
    static final int KEY_LEFT = -3;
    static final int KEY_RIGHT = -4;
    static final int KEY_FIRE = -5;
    static final int KEY_SOFT_LEFT = -6;
    static final int KEY_SOFT_RIGHT = -7;
    static final int KEY_CLEAR = -8;

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
        switch (keyCode) {
        case KEY_UP: case KEY_NUM2: return UP;
        case KEY_DOWN: case KEY_NUM8: return DOWN;
        case KEY_LEFT: case KEY_NUM4: return LEFT;
        case KEY_RIGHT: case KEY_NUM6: return RIGHT;
        case KEY_FIRE: case KEY_NUM5: return FIRE;
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
        switch (keyCode) {
        case KEY_STAR: return "*";
        case KEY_POUND: return "#";
        case KEY_UP: return "Up";
        case KEY_DOWN: return "Down";
        case KEY_LEFT: return "Left";
        case KEY_RIGHT: return "Right";
        case KEY_FIRE: return "Select";
        case KEY_SOFT_LEFT: return "Soft1";
        case KEY_SOFT_RIGHT: return "Soft2";
        case KEY_CLEAR: return "Clear";
        default: return "Key" + keyCode;
        }
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
