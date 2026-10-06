package javax.microedition.lcdui;

import java.util.Vector;

public class Form extends Screen {
    private final Vector items = new Vector();
    private ItemStateListener stateListener;
    private int focus = -1;

    public Form(String title) {
        this.title = title;
    }

    public Form(String title, Item[] list) {
        this.title = title;
        if (list != null) {
            for (int i = 0; i < list.length; i++) {
                append(list[i]);
            }
        }
    }

    public int append(Item item) {
        if (item.owner != null) {
            throw new IllegalStateException();
        }
        item.owner = this;
        items.addElement(item);
        if (focus < 0 && item.focusable()) {
            focus = items.size() - 1;
        }
        repaint0();
        return items.size() - 1;
    }

    public int append(String str) {
        return append(new StringItem(null, str));
    }

    public int append(Image img) {
        return append(new ImageItem(null, img, ImageItem.LAYOUT_DEFAULT, null));
    }

    public void insert(int itemNum, Item item) {
        if (item.owner != null) {
            throw new IllegalStateException();
        }
        item.owner = this;
        items.insertElementAt(item, itemNum);
        if (focus >= itemNum) {
            focus++;
        }
        repaint0();
    }

    public void delete(int itemNum) {
        Item it = (Item) items.elementAt(itemNum);
        it.owner = null;
        items.removeElementAt(itemNum);
        if (focus >= items.size() || focus == itemNum) {
            focus = nextFocusable(-1, 1);
        } else if (focus > itemNum) {
            focus--;
        }
        repaint0();
    }

    public void deleteAll() {
        for (int i = 0; i < items.size(); i++) {
            ((Item) items.elementAt(i)).owner = null;
        }
        items.removeAllElements();
        focus = -1;
        scrollY = 0;
        repaint0();
    }

    public void set(int itemNum, Item item) {
        ((Item) items.elementAt(itemNum)).owner = null;
        item.owner = this;
        items.setElementAt(item, itemNum);
        repaint0();
    }

    public Item get(int itemNum) {
        return (Item) items.elementAt(itemNum);
    }

    public int size() {
        return items.size();
    }

    public void setItemStateListener(ItemStateListener l) {
        stateListener = l;
    }

    void itemChanged(Item item) {
        if (stateListener != null) {
            stateListener.itemStateChanged(item);
        }
    }

    private int nextFocusable(int from, int dir) {
        for (int i = from + dir; i >= 0 && i < items.size(); i += dir) {
            if (((Item) items.elementAt(i)).focusable()) {
                return i;
            }
        }
        return -1;
    }

    private int itemY(int index, int w) {
        int y = 4;
        for (int i = 0; i < index; i++) {
            y += ((Item) items.elementAt(i)).height(w) + 4;
        }
        return y;
    }

    String softLeftLabel() {
        String l = leftLabel();
        if (l == null && focus >= 0) {
            Item it = (Item) items.elementAt(focus);
            if (it.defaultCommand != null) {
                return it.defaultCommand.getLabel();
            }
            if (it instanceof TextField) {
                return "Sua";
            }
        }
        return l;
    }

    void keyEvent(int type, int code) {
        if (type == Display.EV_KEY_RELEASED) {
            return;
        }
        int w = Display.screenW;
        int a = Canvas.actionOf(code);
        Item cur = focus >= 0 ? (Item) items.elementAt(focus) : null;
        if (cur != null && type == Display.EV_KEY_PRESSED && code == Canvas.KEY_SOFT_LEFT && leftLabel() == null) {
            cur.key(Canvas.KEY_FIRE);
        } else if (cur != null && cur.key(code)) {
            // item đã xử lý
        } else if (a == Canvas.DOWN || a == Canvas.UP) {
            int dir = a == Canvas.DOWN ? 1 : -1;
            int next = nextFocusable(focus, dir);
            if (next >= 0) {
                focus = next;
            } else {
                scrollY = Math.max(0, scrollY + dir * 30);
                repaint0();
                return;
            }
        } else {
            return;
        }
        if (focus >= 0) {
            Item it = (Item) items.elementAt(focus);
            int y = itemY(focus, w);
            int h = it.height(w);
            if (it instanceof ChoiceGroup) {
                int off = it.labelHeight(w - 8) + ((ChoiceGroup) it).focusOffset();
                ensureVisible(y + off, Screen.plainFont().getHeight() + 6);
            } else {
                ensureVisible(y, Math.min(h, contentHeight()));
            }
        }
        repaint0();
    }

    void paintContent(Graphics g, int w) {
        int y = 4;
        for (int i = 0; i < items.size(); i++) {
            Item it = (Item) items.elementAt(i);
            y += it.paint(g, y, w, i == focus) + 4;
        }
    }
}
