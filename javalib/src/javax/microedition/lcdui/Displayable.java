package javax.microedition.lcdui;

import java.util.Vector;

public abstract class Displayable {
    String title;
    Ticker ticker;
    final Vector commands = new Vector();
    CommandListener listener;
    boolean shown;

    Displayable() {
    }

    public String getTitle() {
        return title;
    }

    public void setTitle(String s) {
        title = s;
        repaint0();
    }

    public Ticker getTicker() {
        return ticker;
    }

    public void setTicker(Ticker t) {
        ticker = t;
        repaint0();
    }

    public boolean isShown() {
        return shown;
    }

    public void addCommand(Command cmd) {
        if (cmd == null) {
            throw new NullPointerException();
        }
        if (!commands.contains(cmd)) {
            commands.addElement(cmd);
            repaint0();
        }
    }

    public void removeCommand(Command cmd) {
        commands.removeElement(cmd);
        repaint0();
    }

    public void setCommandListener(CommandListener l) {
        listener = l;
    }

    public int getWidth() {
        Display.get();
        return Display.screenW;
    }

    public int getHeight() {
        Display.get();
        return Display.screenH;
    }

    protected void sizeChanged(int w, int h) {
    }

    void repaint0() {
        if (shown) {
            Display.get().requestRepaint();
        }
    }

    static boolean isNegative(Command c) {
        int t = c.getCommandType();
        return t == Command.BACK || t == Command.CANCEL || t == Command.EXIT || t == Command.STOP;
    }

    // [0] = phím mềm trái, [1] = phím mềm phải
    Command[] softCommands() {
        Command left = null, right = null;
        for (int i = 0; i < commands.size(); i++) {
            Command c = (Command) commands.elementAt(i);
            if (isNegative(c)) {
                if (right == null || c.getPriority() < right.getPriority()) {
                    right = c;
                }
            }
        }
        for (int i = 0; i < commands.size(); i++) {
            Command c = (Command) commands.elementAt(i);
            if (c != right && (left == null || c.getPriority() < left.getPriority())) {
                left = c;
            }
        }
        if (right == null && commands.size() == 2) {
            right = (Command) commands.elementAt(commands.elementAt(0) == left ? 1 : 0);
        }
        return new Command[] { left, right };
    }

    // Các lệnh hiện trong menu của phím mềm trái
    Vector menuCommands() {
        Command[] pair = softCommands();
        Vector v = new Vector();
        for (int i = 0; i < commands.size(); i++) {
            Command c = (Command) commands.elementAt(i);
            if (c != pair[1]) {
                v.addElement(c);
            }
        }
        // Sắp theo priority
        for (int i = 1; i < v.size(); i++) {
            for (int j = i; j > 0; j--) {
                Command a = (Command) v.elementAt(j - 1), b = (Command) v.elementAt(j);
                if (b.getPriority() < a.getPriority()) {
                    v.setElementAt(b, j - 1);
                    v.setElementAt(a, j);
                }
            }
        }
        return v;
    }

    String leftLabel() {
        Command[] p = softCommands();
        if (p[0] == null) {
            return null;
        }
        return menuCommands().size() > 1 ? "Menu" : p[0].getLabel();
    }

    String rightLabel() {
        Command[] p = softCommands();
        return p[1] == null ? null : p[1].getLabel();
    }

    abstract void paintScreen(Graphics g);

    void keyEvent(int type, int code) {
    }

    void pointerEvent(int type, int x, int y) {
    }

    void showNotify0() {
    }

    void hideNotify0() {
    }
}
