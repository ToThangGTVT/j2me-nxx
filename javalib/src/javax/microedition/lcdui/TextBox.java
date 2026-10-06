package javax.microedition.lcdui;

public class TextBox extends Screen {
    private String text;
    private int maxSize;
    private int constraints;

    public TextBox(String title, String text, int maxSize, int constraints) {
        this.title = title;
        this.maxSize = maxSize;
        this.constraints = constraints;
        setString(text);
    }

    public String getString() {
        return text;
    }

    public void setString(String s) {
        text = s == null ? "" : s;
        if (text.length() > maxSize) {
            text = text.substring(0, maxSize);
        }
        repaint0();
    }

    public int getChars(char[] data) {
        text.getChars(0, text.length(), data, 0);
        return text.length();
    }

    public void setChars(char[] data, int offset, int length) {
        setString(data == null ? "" : new String(data, offset, length));
    }

    public void insert(String src, int position) {
        position = Math.max(0, Math.min(position, text.length()));
        setString(text.substring(0, position) + src + text.substring(position));
    }

    public void insert(char[] data, int offset, int length, int position) {
        insert(new String(data, offset, length), position);
    }

    public void delete(int offset, int length) {
        setString(text.substring(0, offset) + text.substring(offset + length));
    }

    public int getMaxSize() {
        return maxSize;
    }

    public int setMaxSize(int maxSize) {
        this.maxSize = maxSize;
        setString(text);
        return maxSize;
    }

    public int size() {
        return text.length();
    }

    public int getCaretPosition() {
        return text.length();
    }

    public void setConstraints(int c) {
        constraints = c;
    }

    public int getConstraints() {
        return constraints;
    }

    public void setInitialInputMode(String s) {
    }

    private void edit() {
        String r = j2menx.Keyboard.show(title, text, maxSize, constraints);
        if (r != null) {
            setString(r);
        }
    }

    String softLeftLabel() {
        String l = leftLabel();
        return l != null ? l : j2menx.Lang.t("Sửa", "Edit");
    }

    void showNotify0() {
        // Mở bàn phím ngay khi hiện TextBox trống
        if (text.length() == 0 && (constraints & TextField.UNEDITABLE) == 0) {
            Display.get().callSerially(new Runnable() {
                public void run() {
                    if (shown) {
                        edit();
                    }
                }
            });
        }
    }

    void keyEvent(int type, int code) {
        if (type != Display.EV_KEY_PRESSED) {
            return;
        }
        int a = Canvas.actionOf(code);
        if (a == Canvas.FIRE || (code == Canvas.KEY_SOFT_LEFT && leftLabel() == null)) {
            if ((constraints & TextField.UNEDITABLE) == 0) {
                edit();
            }
        }
    }

    void paintContent(Graphics g, int w) {
        g.setColor(Screen.FG);
        g.setFont(plainFont());
        drawWrapped(g, TextField.display(text, constraints), 6, 6, w - 12);
    }
}
