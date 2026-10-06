package com.siemens.mp.game;

import javax.microedition.lcdui.Graphics;

public abstract class GraphicObject {
    boolean visible = true;

    GraphicObject() {
    }

    public void setVisible(boolean visible) {
        this.visible = visible;
    }

    public boolean getVisible() {
        return visible;
    }

    abstract void paint(Graphics g, int x, int y);
}
