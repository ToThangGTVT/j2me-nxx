package javax.microedition.lcdui;

public abstract class CustomItem extends Item {
    protected static final int TRAVERSE_HORIZONTAL = 1;
    protected static final int TRAVERSE_VERTICAL = 2;
    protected static final int KEY_PRESS = 4;
    protected static final int KEY_RELEASE = 8;
    protected static final int KEY_REPEAT = 0x10;
    protected static final int POINTER_PRESS = 0x20;
    protected static final int POINTER_RELEASE = 0x40;
    protected static final int POINTER_DRAG = 0x80;
    protected static final int NONE = 0;

    protected CustomItem(String label) {
        super(label);
    }

    public int getGameAction(int keyCode) {
        return Canvas.actionOf(keyCode);
    }

    protected final int getInteractionModes() {
        return KEY_PRESS | KEY_RELEASE | TRAVERSE_VERTICAL;
    }

    protected abstract int getMinContentWidth();

    protected abstract int getMinContentHeight();

    protected abstract int getPrefContentWidth(int height);

    protected abstract int getPrefContentHeight(int width);

    protected abstract void paint(Graphics g, int w, int h);

    protected final void invalidate() {
        repaint0();
    }

    protected final void repaint() {
        repaint0();
    }

    protected final void repaint(int x, int y, int w, int h) {
        repaint0();
    }

    protected void keyPressed(int keyCode) {
    }

    protected void keyReleased(int keyCode) {
    }

    protected void keyRepeated(int keyCode) {
    }

    protected void pointerPressed(int x, int y) {
    }

    protected void pointerReleased(int x, int y) {
    }

    protected void pointerDragged(int x, int y) {
    }

    protected void showNotify() {
    }

    protected void hideNotify() {
    }

    protected void sizeChanged(int w, int h) {
    }

    protected boolean traverse(int dir, int viewportWidth, int viewportHeight, int[] visRect_inout) {
        return false;
    }

    protected void traverseOut() {
    }

    protected final void notifyStateChanged0() {
        notifyStateChanged();
    }

    boolean focusable() {
        return true;
    }

    int paintBody(Graphics g, int y, int w, boolean focused) {
        int cw = Math.min(w - 8, getPrefContentWidth(-1));
        int ch = getPrefContentHeight(cw);
        g.translate(4, y);
        int cx = g.getClipX(), cy = g.getClipY(), cwid = g.getClipWidth(), chei = g.getClipHeight();
        g.clipRect(0, 0, cw, ch);
        try {
            paint(g, cw, ch);
        } finally {
            g.setClip(cx, cy, cwid, chei);
            g.translate(-4, -y);
        }
        return ch + 2;
    }

    int bodyHeight(int w) {
        int cw = Math.min(w - 8, getPrefContentWidth(-1));
        return getPrefContentHeight(cw) + 2;
    }

    boolean key(int code) {
        keyPressed(code);
        return Canvas.actionOf(code) != Canvas.UP && Canvas.actionOf(code) != Canvas.DOWN;
    }
}
