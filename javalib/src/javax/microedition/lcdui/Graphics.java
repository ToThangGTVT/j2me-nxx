package javax.microedition.lcdui;

public class Graphics {
    public static final int HCENTER = 1;
    public static final int VCENTER = 2;
    public static final int LEFT = 4;
    public static final int RIGHT = 8;
    public static final int TOP = 16;
    public static final int BOTTOM = 32;
    public static final int BASELINE = 64;
    public static final int SOLID = 0;
    public static final int DOTTED = 1;

    // Field dùng bởi native (source/midp/graphics.c): không đổi tên
    Image img;
    int transX, transY;
    int clipX, clipY, clipW, clipH;     // toạ độ tuyệt đối
    int color;                          // ARGB
    Font font;
    int stroke;

    Graphics(Image img) {
        this.img = img;
        reset();
    }

    void reset() {
        transX = transY = 0;
        clipX = clipY = 0;
        clipW = img.width;
        clipH = img.height;
        color = 0xff000000;
        font = Font.getDefaultFont();
        stroke = SOLID;
    }

    public void translate(int x, int y) {
        transX += x;
        transY += y;
    }

    public int getTranslateX() {
        return transX;
    }

    public int getTranslateY() {
        return transY;
    }

    public int getColor() {
        return color & 0xffffff;
    }

    public int getRedComponent() {
        return (color >> 16) & 0xff;
    }

    public int getGreenComponent() {
        return (color >> 8) & 0xff;
    }

    public int getBlueComponent() {
        return color & 0xff;
    }

    public int getGrayScale() {
        return (getRedComponent() * 76 + getGreenComponent() * 150 + getBlueComponent() * 29) >> 8;
    }

    public void setColor(int red, int green, int blue) {
        if (((red | green | blue) & ~0xff) != 0) {
            throw new IllegalArgumentException();
        }
        color = 0xff000000 | (red << 16) | (green << 8) | blue;
    }

    public void setColor(int rgb) {
        color = 0xff000000 | rgb;
    }

    // DirectGraphics.setARGBColor
    void setARGB(int argb) {
        color = argb;
    }

    public void setGrayScale(int v) {
        setColor(v, v, v);
    }

    public Font getFont() {
        return font;
    }

    public void setFont(Font f) {
        font = f == null ? Font.getDefaultFont() : f;
    }

    public void setStrokeStyle(int style) {
        if (style != SOLID && style != DOTTED) {
            throw new IllegalArgumentException();
        }
        stroke = style;
    }

    public int getStrokeStyle() {
        return stroke;
    }

    public int getClipX() {
        return clipX - transX;
    }

    public int getClipY() {
        return clipY - transY;
    }

    public int getClipWidth() {
        return clipW;
    }

    public int getClipHeight() {
        return clipH;
    }

    public void clipRect(int x, int y, int w, int h) {
        int x1 = Math.max(clipX, x + transX);
        int y1 = Math.max(clipY, y + transY);
        int x2 = Math.min(clipX + clipW, x + transX + w);
        int y2 = Math.min(clipY + clipH, y + transY + h);
        clipX = x1;
        clipY = y1;
        clipW = Math.max(0, x2 - x1);
        clipH = Math.max(0, y2 - y1);
    }

    public void setClip(int x, int y, int w, int h) {
        int x1 = Math.max(0, x + transX);
        int y1 = Math.max(0, y + transY);
        int x2 = Math.min(img.width, x + transX + w);
        int y2 = Math.min(img.height, y + transY + h);
        clipX = x1;
        clipY = y1;
        clipW = Math.max(0, x2 - x1);
        clipH = Math.max(0, y2 - y1);
    }

    public native void drawLine(int x1, int y1, int x2, int y2);

    public native void fillRect(int x, int y, int width, int height);

    public void drawRect(int x, int y, int width, int height) {
        if (width < 0 || height < 0) {
            return;
        }
        if (width == 0 || height == 0) {
            drawLine(x, y, x + width, y + height);
            return;
        }
        drawLine(x, y, x + width - 1, y);
        drawLine(x + width, y, x + width, y + height - 1);
        drawLine(x + width, y + height, x + 1, y + height);
        drawLine(x, y + height, x, y + 1);
    }

    public native void drawRoundRect(int x, int y, int width, int height, int arcWidth, int arcHeight);

    public native void fillRoundRect(int x, int y, int width, int height, int arcWidth, int arcHeight);

    public native void fillArc(int x, int y, int width, int height, int startAngle, int arcAngle);

    public native void drawArc(int x, int y, int width, int height, int startAngle, int arcAngle);

    public native void fillTriangle(int x1, int y1, int x2, int y2, int x3, int y3);

    public void drawString(String str, int x, int y, int anchor) {
        if (str == null) {
            throw new NullPointerException();
        }
        Font f = font;
        if ((anchor & (HCENTER | RIGHT)) != 0) {
            int w = f.stringWidth(str);
            x -= (anchor & HCENTER) != 0 ? w / 2 : w;
        }
        if ((anchor & BOTTOM) != 0) {
            y -= f.getHeight();
        } else if ((anchor & BASELINE) != 0) {
            y -= f.getBaselinePosition();
        } else if ((anchor & VCENTER) != 0) {
            y -= f.getHeight() / 2;
        }
        drawStringImpl(str, x, y);
        if (f.isUnderlined()) {
            int by = y + f.getBaselinePosition() + 1;
            drawLine(x, by, x + f.stringWidth(str) - 1, by);
        }
    }

    public void drawSubstring(String str, int offset, int len, int x, int y, int anchor) {
        drawString(str.substring(offset, offset + len), x, y, anchor);
    }

    public void drawChar(char character, int x, int y, int anchor) {
        drawString(String.valueOf(character), x, y, anchor);
    }

    public void drawChars(char[] data, int offset, int length, int x, int y, int anchor) {
        drawString(new String(data, offset, length), x, y, anchor);
    }

    private native void drawStringImpl(String s, int x, int y);

    public void drawImage(Image image, int x, int y, int anchor) {
        if (image == null) {
            throw new NullPointerException();
        }
        int w = image.width, h = image.height;
        if ((anchor & HCENTER) != 0) {
            x -= w / 2;
        } else if ((anchor & RIGHT) != 0) {
            x -= w;
        }
        if ((anchor & VCENTER) != 0) {
            y -= h / 2;
        } else if ((anchor & (BOTTOM | BASELINE)) != 0) {
            y -= h;
        }
        drawRegionImpl(image, 0, 0, w, h, 0, x, y, false);
    }

    static boolean swapsAxes(int transform) {
        return (transform & 4) != 0;
    }

    public void drawRegion(Image src, int xSrc, int ySrc, int width, int height, int transform, int xDest,
            int yDest, int anchor) {
        if (src == null) {
            throw new NullPointerException();
        }
        if (src == img) {
            throw new IllegalArgumentException("src == dest");
        }
        if (transform < 0 || transform > 7 || xSrc < 0 || ySrc < 0 || width < 0 || height < 0
                || xSrc + width > src.width || ySrc + height > src.height) {
            throw new IllegalArgumentException();
        }
        int w = swapsAxes(transform) ? height : width;
        int h = swapsAxes(transform) ? width : height;
        if ((anchor & HCENTER) != 0) {
            xDest -= w / 2;
        } else if ((anchor & RIGHT) != 0) {
            xDest -= w;
        }
        if ((anchor & VCENTER) != 0) {
            yDest -= h / 2;
        } else if ((anchor & (BOTTOM | BASELINE)) != 0) {
            yDest -= h;
        }
        drawRegionImpl(src, xSrc, ySrc, width, height, transform, xDest, yDest, false);
    }

    // copy = true: chép đè (kể cả alpha) thay vì trộn màu
    native void drawRegionImpl(Image src, int xSrc, int ySrc, int width, int height, int transform, int xDest,
            int yDest, boolean copy);

    public native void drawRGB(int[] rgbData, int offset, int scanlength, int x, int y, int width, int height,
            boolean processAlpha);

    public void copyArea(int xSrc, int ySrc, int width, int height, int xDest, int yDest, int anchor) {
        if (img == Display.screen) {
            throw new IllegalStateException();
        }
        int ax = xSrc + transX, ay = ySrc + transY;
        if (ax < 0 || ay < 0 || ax + width > img.width || ay + height > img.height) {
            throw new IllegalArgumentException();
        }
        int[] tmp = new int[width * height];
        img.getRGB(tmp, 0, width, ax, ay, width, height);
        if ((anchor & HCENTER) != 0) {
            xDest -= width / 2;
        } else if ((anchor & RIGHT) != 0) {
            xDest -= width;
        }
        if ((anchor & VCENTER) != 0) {
            yDest -= height / 2;
        } else if ((anchor & BOTTOM) != 0) {
            yDest -= height;
        }
        drawRGB(tmp, 0, width, xDest, yDest, width, height, false);
    }

    public int getDisplayColor(int color) {
        return color & 0xffffff;
    }
}
