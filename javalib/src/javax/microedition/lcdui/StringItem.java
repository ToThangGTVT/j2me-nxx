package javax.microedition.lcdui;

public class StringItem extends Item {
    private String text;
    private int appearance;
    private Font font;

    public StringItem(String label, String text) {
        this(label, text, PLAIN);
    }

    public StringItem(String label, String text, int appearanceMode) {
        super(label);
        this.text = text;
        this.appearance = appearanceMode;
    }

    public String getText() {
        return text;
    }

    public void setText(String text) {
        this.text = text;
        repaint0();
    }

    public int getAppearanceMode() {
        return appearance;
    }

    public void setFont(Font f) {
        font = f;
        repaint0();
    }

    public Font getFont() {
        return font != null ? font : Screen.plainFont();
    }

    int paintBody(Graphics g, int y, int w, boolean focused) {
        if (text == null) {
            return 0;
        }
        int h = bodyHeight(w);
        if (focused) {
            g.setColor(Screen.ACCENT);
            g.fillRect(2, y, w - 4, h);
        }
        g.setFont(getFont());
        g.setColor(focused ? 0xffffff : appearance == HYPERLINK ? 0x0000c0 : Screen.FG);
        if (appearance == BUTTON && !focused) {
            g.drawRect(2, y, w - 5, h - 1);
        }
        Screen.drawWrapped(g, text, 6, y + 2, w - 12);
        return h;
    }

    int bodyHeight(int w) {
        return text == null ? 0 : Screen.wrappedHeight(text, getFont(), w - 12) + 4;
    }
}
