package com.siemens.mp.game;

import javax.microedition.lcdui.DisplayAccess;
import javax.microedition.lcdui.Graphics;
import javax.microedition.lcdui.Image;

// Ảnh 1 bit / 2 bit của Siemens. Pixel: 0 = trắng, 1 = đen (2 bit: 0-3 mức xám)
public class ExtendedImage extends GraphicObject {
    private final Image image;

    public ExtendedImage(Image image) {
        if (!image.isMutable()) {
            throw new IllegalArgumentException("Can anh mutable");
        }
        this.image = image;
    }

    public Image getImage() {
        return image;
    }

    private static int toArgb(int v) {
        switch (v & 3) {
        case 0: return 0xffffffff;
        case 1: return 0xff000000;
        case 2: return 0xff555555;
        default: return 0xffaaaaaa;
        }
    }

    public void setPixel(int x, int y, byte color) {
        if (x < 0 || y < 0 || x >= image.getWidth() || y >= image.getHeight()) {
            return;
        }
        DisplayAccess.pixels(image)[y * image.getWidth() + x] = toArgb(color);
    }

    public int getPixel(int x, int y) {
        int p = DisplayAccess.pixels(image)[y * image.getWidth() + x];
        return (p & 0xffffff) == 0xffffff ? 0 : 1;
    }

    public void setPixels(byte[] pixels, int x, int y, int width, int height) {
        // 1 bit mỗi pixel, theo hàng, bit cao trước
        int bpr = (width + 7) / 8;
        for (int r = 0; r < height; r++) {
            for (int c = 0; c < width; c++) {
                int bit = (pixels[r * bpr + c / 8] >> (7 - (c & 7))) & 1;
                setPixel(x + c, y + r, (byte) bit);
            }
        }
    }

    public void getPixelBytes(byte[] pixels, int x, int y, int width, int height) {
        int bpr = (width + 7) / 8;
        for (int i = 0; i < bpr * height; i++) {
            pixels[i] = 0;
        }
        for (int r = 0; r < height; r++) {
            for (int c = 0; c < width; c++) {
                if (getPixel(x + c, y + r) != 0) {
                    pixels[r * bpr + c / 8] |= (byte) (0x80 >> (c & 7));
                }
            }
        }
    }

    public void clear(byte color) {
        int[] p = DisplayAccess.pixels(image);
        int v = toArgb(color);
        for (int i = 0; i < p.length; i++) {
            p[i] = v;
        }
    }

    public void blitToScreen(int x, int y) {
        DisplayAccess.blitToScreen(image, x, y);
    }

    void paint(Graphics g, int x, int y) {
        g.drawImage(image, x, y, Graphics.TOP | Graphics.LEFT);
    }
}
