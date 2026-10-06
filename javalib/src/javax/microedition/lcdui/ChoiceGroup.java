package javax.microedition.lcdui;

public class ChoiceGroup extends Item implements Choice {
    private final ChoiceModel model;

    public ChoiceGroup(String label, int choiceType) {
        this(label, choiceType, new String[0], null);
    }

    public ChoiceGroup(String label, int choiceType, String[] stringElements, Image[] imageElements) {
        super(label);
        if (choiceType != EXCLUSIVE && choiceType != MULTIPLE && choiceType != POPUP) {
            throw new IllegalArgumentException();
        }
        model = new ChoiceModel(choiceType);
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
    public boolean isSelected(int i) { return model.isSelected(i); }
    public int getSelectedIndex() { return model.getSelectedIndex(); }
    public int getSelectedFlags(boolean[] arr) { return model.getSelectedFlags(arr); }
    public void setSelectedIndex(int i, boolean sel) { model.setSelectedIndex(i, sel); repaint0(); }
    public void setSelectedFlags(boolean[] arr) { model.setSelectedFlags(arr); repaint0(); }
    public void setFitPolicy(int p) { model.fitPolicy = p; }
    public int getFitPolicy() { return model.fitPolicy; }
    public void setFont(int i, Font f) { }
    public Font getFont(int i) { return Screen.plainFont(); }

    boolean focusable() {
        return model.size() > 0;
    }

    int paintBody(Graphics g, int y, int w, boolean focused) {
        if (model.type == POPUP) {
            int i = model.getSelectedIndex();
            String s = i >= 0 ? (String) model.strings.elementAt(i) : "";
            Font f = Screen.plainFont();
            int h = f.getHeight() + 6;
            if (focused) {
                g.setColor(Screen.ACCENT);
                g.fillRect(2, y, w - 4, h);
            }
            g.setColor(focused ? 0xffffff : Screen.FG);
            g.setFont(f);
            g.drawString("< " + s + " >", 6, y + 3, Graphics.TOP | Graphics.LEFT);
            return h;
        }
        return model.paint(g, 2, y, w - 4, focused);
    }

    int bodyHeight(int w) {
        return model.type == POPUP ? Screen.plainFont().getHeight() + 6 : model.size() * model.rowHeight();
    }

    // Vị trí dòng đang chọn (để cuộn)
    int focusOffset() {
        return model.type == POPUP ? 0 : model.focus * model.rowHeight();
    }

    boolean key(int code) {
        int a = Canvas.actionOf(code);
        int n = model.size();
        if (n == 0) {
            return false;
        }
        if (model.type == POPUP) {
            if (a == Canvas.LEFT || a == Canvas.RIGHT || a == Canvas.FIRE) {
                int i = model.getSelectedIndex();
                i = (i + (a == Canvas.LEFT ? n - 1 : 1)) % n;
                model.setSelectedIndex(i, true);
                notifyStateChanged();
                return true;
            }
            return super.key(code);
        }
        if (a == Canvas.UP && model.focus > 0) {
            model.focus--;
            return true;
        }
        if (a == Canvas.DOWN && model.focus < n - 1) {
            model.focus++;
            return true;
        }
        if (a == Canvas.FIRE) {
            if (model.type == EXCLUSIVE) {
                model.setSelectedIndex(model.focus, true);
            } else {
                model.setSelectedIndex(model.focus, !model.isSelected(model.focus));
            }
            notifyStateChanged();
            return true;
        }
        return super.key(code);
    }
}
