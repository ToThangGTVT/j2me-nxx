package javax.microedition.lcdui.game;

import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Graphics;
import javax.microedition.lcdui.Image;

public abstract class GameCanvas extends Canvas {
    public static final int UP_PRESSED = 1 << UP;
    public static final int DOWN_PRESSED = 1 << DOWN;
    public static final int LEFT_PRESSED = 1 << LEFT;
    public static final int RIGHT_PRESSED = 1 << RIGHT;
    public static final int FIRE_PRESSED = 1 << FIRE;
    public static final int GAME_A_PRESSED = 1 << GAME_A;
    public static final int GAME_B_PRESSED = 1 << GAME_B;
    public static final int GAME_C_PRESSED = 1 << GAME_C;
    public static final int GAME_D_PRESSED = 1 << GAME_D;

    private final Image buffer;
    private final boolean suppressKeys;

    protected GameCanvas(boolean suppressKeyEvents) {
        suppressKeys = suppressKeyEvents;
        buffer = Image.createImage(getWidth(), getHeight());
    }

    protected Graphics getGraphics() {
        return buffer.getGraphics();
    }

    public int getKeyStates() {
        return j2menx.GameAccess.keyStates();
    }

    public void paint(Graphics g) {
        g.drawImage(buffer, 0, 0, Graphics.TOP | Graphics.LEFT);
    }

    public void flushGraphics(int x, int y, int width, int height) {
        flushGraphics();
    }

    public void flushGraphics() {
        if (isShown()) {
            j2menx.GameAccess.flush(buffer);
        }
    }

    protected void keyPressed(int keyCode) {
    }
}
