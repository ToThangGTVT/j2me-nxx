package javax.microedition.lcdui;

public class List extends Screen implements Choice {
    public static final Command SELECT_COMMAND = new Command("", Command.SCREEN, 0);

    private final ChoiceModel model;
    private Command selectCommand = SELECT_COMMAND;

    public List(String title, int listType) {
        this(title, listType, new String[0], null);
    }

    public List(String title, int listType, String[] stringElements, Image[] imageElements) {
        if (listType != IMPLICIT && listType != EXCLUSIVE && listType != MULTIPLE) {
            throw new IllegalArgumentException();
        }
        this.title = title;
        model = new ChoiceModel(listType);
        for (int i = 0; i < stringElements.length; i++) {
            model.append(stringElements[i], imageElements != null ? imageElements[i] : null);
        }
    }

    public int size() { return model.size(); }
    public String getString(int i) { model.check(i); return (String) model.strings.elementAt(i); }
    public Image getImage(int i) { model.check(i); return (Image) model.images.elementAt(i); }
    public int append(String s, Image img) { int r = model.append(s, img); repaint0(); return r; }
    public void insert(int i, String s, Image img) { model.insert(i, s, img); repaint0(); }
    public void delete(int i) { model.delete(i); repaint0(); }
    public void deleteAll() { model.deleteAll(); repaint0(); }
    public void set(int i, String s, Image img) { model.set(i, s, img); repaint0(); }
    public boolean isSelected(int i) { return model.type == IMPLICIT ? i == model.focus : model.isSelected(i); }
    public int getSelectedIndex() { return model.getSelectedIndex(); }
    public int getSelectedFlags(boolean[] arr) { return model.getSelectedFlags(arr); }
    public void setSelectedFlags(boolean[] arr) { model.setSelectedFlags(arr); repaint0(); }
    public void setFitPolicy(int p) { model.fitPolicy = p; }
    public int getFitPolicy() { return model.fitPolicy; }
    public void setFont(int i, Font f) { }
    public Font getFont(int i) { return Screen.plainFont(); }

    public void setSelectedIndex(int i, boolean sel) {
        model.setSelectedIndex(i, sel);
        if (model.type == IMPLICIT && sel) {
            model.focus = i;
        }
        repaint0();
    }

    public void setSelectCommand(Command c) {
        selectCommand = c;
    }

    public void removeCommand(Command c) {
        if (c == selectCommand) {
            selectCommand = null;
        }
        super.removeCommand(c);
    }

    String softLeftLabel() {
        String l = leftLabel();
        if (l == null && model.type == IMPLICIT && model.size() > 0) {
            return j2menx.Lang.t("Chọn", "Select");
        }
        return l;
    }

    void keyEvent(int type, int code) {
        if (type == Display.EV_KEY_RELEASED || model.size() == 0) {
            return;
        }
        int a = Canvas.actionOf(code);
        int n = model.size();
        if (a == Canvas.UP) {
            model.focus = (model.focus + n - 1) % n;
        } else if (a == Canvas.DOWN) {
            model.focus = (model.focus + 1) % n;
        } else if (a == Canvas.FIRE || (code == Canvas.KEY_SOFT_LEFT && leftLabel() == null)) {
            if (type != Display.EV_KEY_PRESSED) {
                return;
            }
            if (model.type == IMPLICIT) {
                if (selectCommand != null) {
                    Display.fireCommand(this, selectCommand);
                }
            } else if (model.type == EXCLUSIVE) {
                model.setSelectedIndex(model.focus, true);
            } else {
                model.setSelectedIndex(model.focus, !model.isSelected(model.focus));
            }
        } else {
            return;
        }
        int rh = model.rowHeight();
        ensureVisible(model.focus * rh, rh);
        repaint0();
    }

    void paintContent(Graphics g, int w) {
        model.paint(g, 0, 0, w, true);
    }
}
