package javax.microedition.lcdui.game;

import java.util.Vector;
import javax.microedition.lcdui.Graphics;

public class LayerManager {
    private final Vector layers = new Vector();
    private int viewX, viewY;
    private int viewW = 100000, viewH = 100000;

    public LayerManager() {
    }

    public void append(Layer l) {
        layers.removeElement(l);
        layers.addElement(l);
    }

    public void insert(Layer l, int index) {
        layers.removeElement(l);
        layers.insertElementAt(l, index);
    }

    public Layer getLayerAt(int index) {
        return (Layer) layers.elementAt(index);
    }

    public int getSize() {
        return layers.size();
    }

    public void remove(Layer l) {
        layers.removeElement(l);
    }

    public void setViewWindow(int x, int y, int width, int height) {
        if (width < 0 || height < 0) {
            throw new IllegalArgumentException();
        }
        viewX = x;
        viewY = y;
        viewW = width;
        viewH = height;
    }

    public void paint(Graphics g, int x, int y) {
        int cx = g.getClipX(), cy = g.getClipY(), cw = g.getClipWidth(), ch = g.getClipHeight();
        g.translate(x - viewX, y - viewY);
        g.clipRect(viewX, viewY, viewW, viewH);
        for (int i = layers.size() - 1; i >= 0; i--) {
            Layer l = (Layer) layers.elementAt(i);
            if (l.visible) {
                l.paint(g);
            }
        }
        g.translate(viewX - x, viewY - y);
        g.setClip(cx, cy, cw, ch);
    }
}
