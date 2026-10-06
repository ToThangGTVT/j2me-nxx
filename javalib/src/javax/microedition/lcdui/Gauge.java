package javax.microedition.lcdui;

public class Gauge extends Item {
    public static final int INDEFINITE = -1;
    public static final int CONTINUOUS_IDLE = 0;
    public static final int INCREMENTAL_IDLE = 1;
    public static final int CONTINUOUS_RUNNING = 2;
    public static final int INCREMENTAL_UPDATING = 3;

    private final boolean interactive;
    private int max;
    private int value;

    public Gauge(String label, boolean interactive, int maxValue, int initialValue) {
        super(label);
        this.interactive = interactive;
        this.max = maxValue;
        this.value = initialValue;
    }

    public void setValue(int v) {
        if (max > 0) {
            v = Math.max(0, Math.min(max, v));
        }
        value = v;
        repaint0();
    }

    public int getValue() {
        return value;
    }

    public void setMaxValue(int m) {
        max = m;
        setValue(value);
    }

    public int getMaxValue() {
        return max;
    }

    public boolean isInteractive() {
        return interactive;
    }

    boolean focusable() {
        return interactive || super.focusable();
    }

    void paintBar(Graphics g, int x, int y, int w) {
        g.setColor(0x808080);
        g.drawRect(x, y, w - 1, 11);
        if (max > 0) {
            g.setColor(Screen.ACCENT);
            g.fillRect(x + 2, y + 2, (w - 4) * value / max, 8);
        }
    }

    int paintBody(Graphics g, int y, int w, boolean focused) {
        paintBar(g, 6, y + 3, w - 12);
        if (focused) {
            g.setColor(Screen.ACCENT);
            g.drawRect(3, y, w - 7, 17);
        }
        return 20;
    }

    int bodyHeight(int w) {
        return 20;
    }

    boolean key(int code) {
        int a = Canvas.actionOf(code);
        if (interactive && (a == Canvas.LEFT || a == Canvas.RIGHT)) {
            setValue(value + (a == Canvas.LEFT ? -1 : 1));
            notifyStateChanged();
            return true;
        }
        return super.key(code);
    }
}
