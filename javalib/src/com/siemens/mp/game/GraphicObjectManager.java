package com.siemens.mp.game;

import java.util.Vector;
import javax.microedition.lcdui.Graphics;
import javax.microedition.lcdui.Image;

public class GraphicObjectManager {
    private final Vector objects = new Vector();

    public GraphicObjectManager() {
    }

    public void addObject(GraphicObject g) {
        objects.addElement(g);
    }

    public void insertObject(GraphicObject g, int pos) {
        objects.insertElementAt(g, pos);
    }

    public void deleteObject(GraphicObject g) {
        objects.removeElement(g);
    }

    public void deleteObject(int pos) {
        objects.removeElementAt(pos);
    }

    public GraphicObject getObjectAt(int index) {
        return (GraphicObject) objects.elementAt(index);
    }

    public int getObjectPosition(GraphicObject g) {
        return objects.indexOf(g);
    }

    // Vẽ lần lượt (đối tượng thêm trước nằm dưới) vào ảnh tại (x, y)
    public void paint(Image image, int x, int y) {
        Graphics g = image.getGraphics();
        for (int i = 0; i < objects.size(); i++) {
            GraphicObject o = (GraphicObject) objects.elementAt(i);
            if (o.visible) {
                o.paint(g, x, y);
            }
        }
    }

    public void paint(ExtendedImage image, int x, int y) {
        paint(image.getImage(), x, y);
    }

    public static byte[] createTextureBits(int width, int height, byte[] texture) {
        return texture;
    }
}
