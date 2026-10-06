package javax.microedition.lcdui;

public class TextField extends Item {
    public static final int ANY = 0;
    public static final int EMAILADDR = 1;
    public static final int NUMERIC = 2;
    public static final int PHONENUMBER = 3;
    public static final int URL = 4;
    public static final int DECIMAL = 5;
    public static final int PASSWORD = 0x10000;
    public static final int UNEDITABLE = 0x20000;
    public static final int SENSITIVE = 0x40000;
    public static final int NON_PREDICTIVE = 0x80000;
    public static final int INITIAL_CAPS_WORD = 0x100000;
    public static final int INITIAL_CAPS_SENTENCE = 0x200000;
    public static final int CONSTRAINT_MASK = 0xffff;

    private String text;
    private int maxSize;
    private int constraints;

    public TextField(String label, String text, int maxSize, int constraints) {
        super(label);
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

    public void setInitialInputMode(String characterSubset) {
    }

    boolean focusable() {
        return true;
    }

    static String display(String text, int constraints) {
        if ((constraints & PASSWORD) != 0) {
            StringBuffer sb = new StringBuffer();
            for (int i = 0; i < text.length(); i++) {
                sb.append('*');
            }
            return sb.toString();
        }
        return text;
    }

    int paintBody(Graphics g, int y, int w, boolean focused) {
        Font f = Screen.plainFont();
        int h = f.getHeight() + 6;
        g.setColor(focused ? 0xe8f0ff : 0xf4f4f4);
        g.fillRect(4, y, w - 8, h);
        g.setColor(focused ? Screen.ACCENT : 0x808080);
        g.drawRect(4, y, w - 9, h - 1);
        g.setColor(Screen.FG);
        g.setFont(f);
        g.drawString(display(text, constraints), 8, y + 3, Graphics.TOP | Graphics.LEFT);
        return h + 2;
    }

    int bodyHeight(int w) {
        return Screen.plainFont().getHeight() + 8;
    }

    boolean key(int code) {
        if (Canvas.actionOf(code) == Canvas.FIRE && (constraints & UNEDITABLE) == 0) {
            String r = j2menx.Keyboard.show(label, text, maxSize, constraints);
            if (r != null) {
                setString(r);
                notifyStateChanged();
            }
            return true;
        }
        return super.key(code);
    }
}
