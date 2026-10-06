package javax.microedition.lcdui;

import java.util.Vector;

public abstract class Item {
    public static final int LAYOUT_DEFAULT = 0;
    public static final int LAYOUT_LEFT = 1;
    public static final int LAYOUT_RIGHT = 2;
    public static final int LAYOUT_CENTER = 3;
    public static final int LAYOUT_TOP = 0x10;
    public static final int LAYOUT_BOTTOM = 0x20;
    public static final int LAYOUT_VCENTER = 0x30;
    public static final int LAYOUT_NEWLINE_BEFORE = 0x100;
    public static final int LAYOUT_NEWLINE_AFTER = 0x200;
    public static final int LAYOUT_SHRINK = 0x400;
    public static final int LAYOUT_EXPAND = 0x800;
    public static final int LAYOUT_VSHRINK = 0x1000;
    public static final int LAYOUT_VEXPAND = 0x2000;
    public static final int LAYOUT_2 = 0x4000;
    public static final int PLAIN = 0;
    public static final int HYPERLINK = 1;
    public static final int BUTTON = 2;

    String label;
    Screen owner;
    int layout;
    final Vector commands = new Vector();
    Command defaultCommand;
    ItemCommandListener itemListener;
    int prefW = -1, prefH = -1;

    Item(String label) {
        this.label = label;
    }

    public void setLabel(String label) {
        this.label = label;
        repaint0();
    }

    public String getLabel() {
        return label;
    }

    public int getLayout() {
        return layout;
    }

    public void setLayout(int layout) {
        this.layout = layout;
    }

    public void addCommand(Command c) {
        if (!commands.contains(c)) {
            commands.addElement(c);
        }
    }

    public void removeCommand(Command c) {
        commands.removeElement(c);
        if (defaultCommand == c) {
            defaultCommand = null;
        }
    }

    public void setItemCommandListener(ItemCommandListener l) {
        itemListener = l;
    }

    public void setDefaultCommand(Command c) {
        defaultCommand = c;
        if (c != null) {
            addCommand(c);
        }
    }

    public int getPreferredWidth() {
        return prefW >= 0 ? prefW : Display.screenW;
    }

    public int getPreferredHeight() {
        return prefH >= 0 ? prefH : 20;
    }

    public void setPreferredSize(int w, int h) {
        prefW = w;
        prefH = h;
    }

    public int getMinimumWidth() {
        return 10;
    }

    public int getMinimumHeight() {
        return 10;
    }

    public void notifyStateChanged() {
        if (owner instanceof Form) {
            ((Form) owner).itemChanged(this);
        }
    }

    void repaint0() {
        if (owner != null) {
            owner.repaint0();
        }
    }

    boolean focusable() {
        return !commands.isEmpty();
    }

    int labelHeight(int w) {
        return label == null || label.length() == 0 ? 0 : Screen.wrappedHeight(label, Screen.boldFont(), w);
    }

    // Vẽ item tại (0, y), trả về chiều cao
    int paint(Graphics g, int y, int w, boolean focused) {
        int h = 0;
        if (label != null && label.length() > 0) {
            g.setFont(Screen.boldFont());
            g.setColor(Screen.FG);
            h += Screen.drawWrapped(g, label, 4, y, w - 8);
        }
        h += paintBody(g, y + h, w, focused);
        return h;
    }

    int height(int w) {
        return labelHeight(w - 8) + bodyHeight(w);
    }

    abstract int paintBody(Graphics g, int y, int w, boolean focused);

    abstract int bodyHeight(int w);

    // true nếu item đã dùng phím
    boolean key(int code) {
        int a = Canvas.actionOf(code);
        if (a == Canvas.FIRE && defaultCommand != null && itemListener != null) {
            itemListener.commandAction(defaultCommand, this);
            return true;
        }
        return false;
    }
}
