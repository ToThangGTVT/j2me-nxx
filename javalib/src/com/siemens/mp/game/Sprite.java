package com.siemens.mp.game;

import javax.microedition.lcdui.DisplayAccess;
import javax.microedition.lcdui.Graphics;
import javax.microedition.lcdui.Image;

// Sprite của Siemens: các frame xếp dọc trong ảnh; mask trắng = trong suốt
public class Sprite extends GraphicObject {
    private final Image frames;
    private final int frameW, frameH, frameCount;
    private int x, y, frame;
    private int colX, colY, colW, colH;

    public Sprite(Image pixels, Image mask, int numFrames) {
        if (numFrames <= 0 || pixels.getHeight() % numFrames != 0) {
            throw new IllegalArgumentException();
        }
        frameW = pixels.getWidth();
        frameH = pixels.getHeight() / numFrames;
        frameCount = numFrames;
        frames = combine(pixels, mask);
        colW = frameW;
        colH = frameH;
    }

    public Sprite(byte[] pixels, int pixelOffset, int width, int height, byte[] mask, int maskOffset, int numFrames) {
        this(bitImage(pixels, pixelOffset, width, height * numFrames, false),
                mask == null ? null : bitImage(mask, maskOffset, width, height * numFrames, true), numFrames);
    }

    public Sprite(ExtendedImage pixels, ExtendedImage mask, int numFrames) {
        this(pixels.getImage(), mask == null ? null : mask.getImage(), numFrames);
    }

    // Ảnh 1 bit -> ARGB (mask: bit 1 = trong suốt)
    static Image bitImage(byte[] data, int off, int w, int h, boolean isMask) {
        int[] argb = new int[w * h];
        int bpr = (w + 7) / 8;
        for (int r = 0; r < h; r++) {
            for (int c = 0; c < w; c++) {
                int bit = (data[off + r * bpr + c / 8] >> (7 - (c & 7))) & 1;
                argb[r * w + c] = isMask ? (bit != 0 ? 0xffffffff : 0xff000000) : (bit != 0 ? 0xff000000 : 0xffffffff);
            }
        }
        return Image.createRGBImage(argb, w, h, false);
    }

    private static Image combine(Image pixels, Image mask) {
        int w = pixels.getWidth(), h = pixels.getHeight();
        int[] p = new int[w * h];
        pixels.getRGB(p, 0, w, 0, 0, w, h);
        if (mask != null) {
            int[] m = new int[w * h];
            mask.getRGB(m, 0, w, 0, 0, Math.min(w, mask.getWidth()), Math.min(h, mask.getHeight()));
            for (int i = 0; i < p.length; i++) {
                if ((m[i] & 0xffffff) == 0xffffff) {
                    p[i] = 0;
                }
            }
        }
        return Image.createRGBImage(p, w, h, true);
    }

    public int getFrame() {
        return frame;
    }

    public void setFrame(int f) {
        if (f < 0 || f >= frameCount) {
            throw new IllegalArgumentException();
        }
        frame = f;
    }

    public int getXPosition() {
        return x;
    }

    public int getYPosition() {
        return y;
    }

    public void setPosition(int x, int y) {
        this.x = x;
        this.y = y;
    }

    public void setCollisionRectangle(int x, int y, int w, int h) {
        colX = x;
        colY = y;
        colW = w;
        colH = h;
    }

    public boolean isCollidingWith(Sprite o) {
        return visible && o.visible && x + colX < o.x + o.colX + o.colW && o.x + o.colX < x + colX + colW
                && y + colY < o.y + o.colY + o.colH && o.y + o.colY < y + colY + colH;
    }

    public boolean isCollidingWithPos(int px, int py) {
        return visible && px >= x + colX && px < x + colX + colW && py >= y + colY && py < y + colY + colH;
    }

    void paint(Graphics g, int ox, int oy) {
        g.drawRegion(frames, 0, frame * frameH, frameW, frameH, 0, ox + x, oy + y, Graphics.TOP | Graphics.LEFT);
    }
}
