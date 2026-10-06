package javax.microedition.lcdui;

import java.util.Vector;

// Dữ liệu dùng chung cho List và ChoiceGroup
class ChoiceModel {
    final int type;
    final Vector strings = new Vector();
    final Vector images = new Vector();
    final Vector selected = new Vector();
    int focus;
    int fitPolicy;

    ChoiceModel(int type) {
        this.type = type;
    }

    int size() {
        return strings.size();
    }

    void check(int i) {
        if (i < 0 || i >= strings.size()) {
            throw new IndexOutOfBoundsException();
        }
    }

    int append(String s, Image img) {
        insert(strings.size(), s, img);
        return strings.size() - 1;
    }

    void insert(int i, String s, Image img) {
        if (s == null) {
            throw new NullPointerException();
        }
        if (i < 0 || i > strings.size()) {
            throw new IndexOutOfBoundsException();
        }
        strings.insertElementAt(s, i);
        images.insertElementAt(img, i);
        selected.insertElementAt(Boolean.FALSE, i);
        if (type != Choice.MULTIPLE && strings.size() == 1) {
            selected.setElementAt(Boolean.TRUE, 0);
        }
    }

    void delete(int i) {
        check(i);
        boolean wasSel = isSelected(i);
        strings.removeElementAt(i);
        images.removeElementAt(i);
        selected.removeElementAt(i);
        if (focus >= strings.size()) {
            focus = Math.max(0, strings.size() - 1);
        }
        if (wasSel && type != Choice.MULTIPLE && strings.size() > 0) {
            selected.setElementAt(Boolean.TRUE, Math.min(i, strings.size() - 1));
        }
    }

    void deleteAll() {
        strings.removeAllElements();
        images.removeAllElements();
        selected.removeAllElements();
        focus = 0;
    }

    void set(int i, String s, Image img) {
        check(i);
        if (s == null) {
            throw new NullPointerException();
        }
        strings.setElementAt(s, i);
        images.setElementAt(img, i);
    }

    boolean isSelected(int i) {
        check(i);
        return ((Boolean) selected.elementAt(i)).booleanValue();
    }

    int getSelectedIndex() {
        if (type == Choice.MULTIPLE) {
            return -1;
        }
        if (type == Choice.IMPLICIT) {
            return strings.isEmpty() ? -1 : focus;
        }
        for (int i = 0; i < selected.size(); i++) {
            if (isSelected(i)) {
                return i;
            }
        }
        return -1;
    }

    int getSelectedFlags(boolean[] arr) {
        int n = 0;
        for (int i = 0; i < selected.size(); i++) {
            arr[i] = isSelected(i) || (type == Choice.IMPLICIT && i == focus);
            if (arr[i]) {
                n++;
            }
        }
        for (int i = selected.size(); i < arr.length; i++) {
            arr[i] = false;
        }
        return n;
    }

    void setSelectedIndex(int i, boolean sel) {
        check(i);
        if (type == Choice.MULTIPLE) {
            selected.setElementAt(sel ? Boolean.TRUE : Boolean.FALSE, i);
        } else if (sel) {
            for (int k = 0; k < selected.size(); k++) {
                selected.setElementAt(k == i ? Boolean.TRUE : Boolean.FALSE, k);
            }
            focus = i;
        }
    }

    void setSelectedFlags(boolean[] arr) {
        for (int i = 0; i < selected.size(); i++) {
            if (type == Choice.MULTIPLE || arr[i]) {
                setSelectedIndex(i, arr[i]);
            }
        }
    }

    // Vẽ danh sách, trả về chiều cao
    int paint(Graphics g, int x, int y, int w, boolean focused) {
        Font f = Screen.plainFont();
        g.setFont(f);
        int rowH = f.getHeight() + 6;
        int n = strings.size();
        for (int i = 0; i < n; i++) {
            int ry = y + i * rowH;
            Image img = (Image) images.elementAt(i);
            if (focused && i == focus) {
                g.setColor(Screen.ACCENT);
                g.fillRect(x, ry, w, rowH);
            }
            int fg = focused && i == focus ? 0xffffff : Screen.FG;
            int tx = x + 4;
            if (type == Choice.EXCLUSIVE || type == Choice.POPUP || type == Choice.MULTIPLE) {
                g.setColor(fg);
                int bs = rowH - 10;
                if (type == Choice.MULTIPLE) {
                    g.drawRect(tx, ry + 5, bs, bs);
                    if (isSelected(i)) {
                        g.fillRect(tx + 3, ry + 8, bs - 5, bs - 5);
                    }
                } else {
                    g.drawArc(tx, ry + 5, bs, bs, 0, 360);
                    if (isSelected(i)) {
                        g.fillArc(tx + 3, ry + 8, bs - 5, bs - 5, 0, 360);
                    }
                }
                tx += bs + 6;
            }
            if (img != null) {
                g.drawImage(img, tx, ry + rowH / 2, Graphics.VCENTER | Graphics.LEFT);
                tx += img.getWidth() + 4;
            }
            g.setColor(fg);
            g.drawString((String) strings.elementAt(i), tx, ry + 3, Graphics.TOP | Graphics.LEFT);
        }
        return n * rowH;
    }

    int rowHeight() {
        return Screen.plainFont().getHeight() + 6;
    }
}
