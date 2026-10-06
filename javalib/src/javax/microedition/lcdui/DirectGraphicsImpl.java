package javax.microedition.lcdui;

import com.nokia.mid.ui.DirectGraphics;

// Cài đặt DirectGraphics của Nokia trên Graphics MIDP (không phải API công khai)
public final class DirectGraphicsImpl implements DirectGraphics {
    private final Graphics g;

    public DirectGraphicsImpl(Graphics g) {
        this.g = g;
    }

    public static Image createFilledImage(int w, int h, int argb) {
        Image img = new Image(w, h, true);
        int[] p = img.pixels;
        for (int i = 0; i < p.length; i++) {
            p[i] = argb;
        }
        img.opaque = (argb >>> 24) == 255;
        return img;
    }

    public void setARGBColor(int argb) {
        g.setARGB(argb);
    }

    public int getAlphaComponent() {
        return g.color >>> 24;
    }

    public int getNativePixelFormat() {
        return TYPE_INT_8888_ARGB;
    }

    static int toTransform(int manipulation) {
        boolean fh = (manipulation & FLIP_HORIZONTAL) != 0;
        boolean fv = (manipulation & FLIP_VERTICAL) != 0;
        int rot = (manipulation & 0x0fff) % 360;
        // Góc quay của Nokia là ngược chiều kim đồng hồ
        int cw = (360 - rot) % 360;
        if (fh && fv) {
            cw = (cw + 180) % 360;
            fh = fv = false;
        }
        if (fv) {
            // Lật dọc = lật ngang + quay 180
            cw = (cw + 180) % 360;
            fh = true;
        }
        if (fh) {
            switch (cw) {
            case 90: return 7;   // MIRROR_ROT90
            case 180: return 1;  // MIRROR_ROT180
            case 270: return 4;  // MIRROR_ROT270
            default: return 2;   // MIRROR
            }
        }
        switch (cw) {
        case 90: return 5;
        case 180: return 3;
        case 270: return 6;
        default: return 0;
        }
    }

    public void drawImage(Image img, int x, int y, int anchor, int manipulation) {
        if (img == null) {
            throw new NullPointerException();
        }
        if (anchor == 0) {
            anchor = Graphics.TOP | Graphics.LEFT;
        }
        g.drawRegion(img, 0, 0, img.width, img.height, toTransform(manipulation), x, y, anchor);
    }

    public void drawTriangle(int x1, int y1, int x2, int y2, int x3, int y3, int argb) {
        int old = g.color;
        g.color = argb;
        g.drawLine(x1, y1, x2, y2);
        g.drawLine(x2, y2, x3, y3);
        g.drawLine(x3, y3, x1, y1);
        g.color = old;
    }

    public void fillTriangle(int x1, int y1, int x2, int y2, int x3, int y3, int argb) {
        int old = g.color;
        g.color = argb;
        g.fillTriangle(x1, y1, x2, y2, x3, y3);
        g.color = old;
    }

    public void drawPolygon(int[] xs, int xo, int[] ys, int yo, int n, int argb) {
        int old = g.color;
        g.color = argb;
        for (int i = 0; i < n; i++) {
            int j = (i + 1) % n;
            g.drawLine(xs[xo + i], ys[yo + i], xs[xo + j], ys[yo + j]);
        }
        g.color = old;
    }

    public void fillPolygon(int[] xs, int xo, int[] ys, int yo, int n, int argb) {
        if (n < 3) {
            return;
        }
        int old = g.color;
        g.color = argb;
        int minY = Integer.MAX_VALUE, maxY = Integer.MIN_VALUE;
        for (int i = 0; i < n; i++) {
            minY = Math.min(minY, ys[yo + i]);
            maxY = Math.max(maxY, ys[yo + i]);
        }
        int[] nodes = new int[n];
        for (int py = minY; py <= maxY; py++) {
            int cnt = 0;
            for (int i = 0, j = n - 1; i < n; j = i++) {
                int yi = ys[yo + i], yj = ys[yo + j];
                if ((yi <= py && yj > py) || (yj <= py && yi > py)) {
                    int xi = xs[xo + i], xj = xs[xo + j];
                    nodes[cnt++] = xi + (py - yi) * (xj - xi) / (yj - yi);
                }
            }
            for (int a = 1; a < cnt; a++) {
                for (int b = a; b > 0 && nodes[b - 1] > nodes[b]; b--) {
                    int t = nodes[b];
                    nodes[b] = nodes[b - 1];
                    nodes[b - 1] = t;
                }
            }
            for (int k = 0; k + 1 < cnt; k += 2) {
                g.fillRect(nodes[k], py, nodes[k + 1] - nodes[k] + 1, 1);
            }
        }
        g.color = old;
    }

    private void drawConverted(int[] argb, int w, int h, int x, int y, int manipulation, boolean alpha) {
        if (manipulation == 0) {
            g.drawRGB(argb, 0, w, x, y, w, h, alpha);
        } else {
            Image tmp = Image.createRGBImage(argb, w, h, alpha);
            drawImage(tmp, x, y, Graphics.TOP | Graphics.LEFT, manipulation);
        }
    }

    public void drawPixels(int[] pixels, boolean transparency, int offset, int scanlength, int x, int y, int w,
            int h, int manipulation, int format) {
        int[] tmp = new int[w * h];
        for (int r = 0; r < h; r++) {
            for (int c = 0; c < w; c++) {
                int p = pixels[offset + r * scanlength + c];
                tmp[r * w + c] = (format == TYPE_INT_888_RGB || !transparency) ? (p | 0xff000000) : p;
            }
        }
        drawConverted(tmp, w, h, x, y, manipulation, transparency);
    }

    static int convert16(int p, int format) {
        switch (format) {
        case TYPE_USHORT_4444_ARGB:
            return ((p >> 12) & 0xf) * 0x11000000 | ((p >> 8) & 0xf) * 0x110000 | ((p >> 4) & 0xf) * 0x1100
                    | (p & 0xf) * 0x11;
        case TYPE_USHORT_444_RGB:
            return 0xff000000 | ((p >> 8) & 0xf) * 0x110000 | ((p >> 4) & 0xf) * 0x1100 | (p & 0xf) * 0x11;
        case TYPE_USHORT_555_RGB:
            return 0xff000000 | (((p >> 10) & 0x1f) << 19) | (((p >> 5) & 0x1f) << 11) | ((p & 0x1f) << 3);
        case TYPE_USHORT_1555_ARGB:
            return ((p & 0x8000) != 0 ? 0xff000000 : 0) | (((p >> 10) & 0x1f) << 19) | (((p >> 5) & 0x1f) << 11)
                    | ((p & 0x1f) << 3);
        default: // 565
            return 0xff000000 | (((p >> 11) & 0x1f) << 19) | (((p >> 5) & 0x3f) << 10) | ((p & 0x1f) << 3);
        }
    }

    public void drawPixels(short[] pixels, boolean transparency, int offset, int scanlength, int x, int y, int w,
            int h, int manipulation, int format) {
        int[] tmp = new int[w * h];
        for (int r = 0; r < h; r++) {
            for (int c = 0; c < w; c++) {
                tmp[r * w + c] = convert16(pixels[offset + r * scanlength + c] & 0xffff, format);
            }
        }
        drawConverted(tmp, w, h, x, y, manipulation, transparency);
    }

    public void drawPixels(byte[] pixels, byte[] mask, int offset, int scanlength, int x, int y, int w, int h,
            int manipulation, int format) {
        int[] tmp = new int[w * h];
        for (int r = 0; r < h; r++) {
            for (int c = 0; c < w; c++) {
                int bit = offset * 8 + r * scanlength + c;
                if (format == TYPE_BYTE_332_RGB) {
                    int p = pixels[offset + r * scanlength + c] & 0xff;
                    tmp[r * w + c] = 0xff000000 | ((p >> 5) * 0x24 << 16) | (((p >> 2) & 7) * 0x24 << 8)
                            | ((p & 3) * 0x55);
                    continue;
                }
                boolean on = (pixels[bit >> 3] & (0x80 >> (bit & 7))) != 0;
                boolean opaque = mask == null || (mask[bit >> 3] & (0x80 >> (bit & 7))) != 0;
                tmp[r * w + c] = opaque ? (on ? 0xff000000 : 0xffffffff) : 0;
            }
        }
        drawConverted(tmp, w, h, x, y, manipulation, mask != null);
    }

    public void getPixels(int[] pixels, int offset, int scanlength, int x, int y, int w, int h, int format) {
        g.img.getRGB(pixels, offset, scanlength, x + g.transX, y + g.transY, w, h);
    }

    public void getPixels(short[] pixels, int offset, int scanlength, int x, int y, int w, int h, int format) {
        int[] tmp = new int[w * h];
        g.img.getRGB(tmp, 0, w, x + g.transX, y + g.transY, w, h);
        for (int r = 0; r < h; r++) {
            for (int c = 0; c < w; c++) {
                int p = tmp[r * w + c];
                int v;
                if (format == TYPE_USHORT_4444_ARGB) {
                    v = ((p >>> 28) << 12) | (((p >> 20) & 0xf) << 8) | (((p >> 12) & 0xf) << 4) | ((p >> 4) & 0xf);
                } else if (format == TYPE_USHORT_444_RGB) {
                    v = (((p >> 20) & 0xf) << 8) | (((p >> 12) & 0xf) << 4) | ((p >> 4) & 0xf);
                } else {
                    v = (((p >> 19) & 0x1f) << 11) | (((p >> 10) & 0x3f) << 5) | ((p >> 3) & 0x1f);
                }
                pixels[offset + r * scanlength + c] = (short) v;
            }
        }
    }

    public void getPixels(byte[] pixels, byte[] mask, int offset, int scanlength, int x, int y, int w, int h,
            int format) {
        throw new IllegalArgumentException("Dinh dang khong ho tro");
    }
}
