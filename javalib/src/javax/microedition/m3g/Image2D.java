package javax.microedition.m3g;

import javax.microedition.lcdui.Image;

public class Image2D extends Object3D {
    public static final int ALPHA = 96;
    public static final int LUMINANCE = 97;
    public static final int LUMINANCE_ALPHA = 98;
    public static final int RGB = 99;
    public static final int RGBA = 100;

    final int format;
    final int width, height;
    final boolean mutable;
    int[] argb;         // luôn lưu dạng ARGB cho bộ dựng hình

    public Image2D(int format, Object image) {
        checkFormat(format);
        if (image == null) {
            throw new NullPointerException();
        }
        if (!(image instanceof Image)) {
            throw new IllegalArgumentException();
        }
        Image img = (Image) image;
        this.format = format;
        width = img.getWidth();
        height = img.getHeight();
        mutable = false;
        argb = new int[width * height];
        img.getRGB(argb, 0, width, 0, 0, width, height);
        convert(argb);
    }

    public Image2D(int format, int width, int height, byte[] image) {
        this(format, width, height, image, null);
    }

    public Image2D(int format, int width, int height, byte[] image, byte[] palette) {
        checkFormat(format);
        if (image == null) {
            throw new NullPointerException();
        }
        if (width <= 0 || height <= 0) {
            throw new IllegalArgumentException();
        }
        this.format = format;
        this.width = width;
        this.height = height;
        this.mutable = false;
        argb = new int[width * height];
        int bpp = bytesPerPixel(format);
        for (int i = 0; i < width * height; i++) {
            if (palette != null) {
                int idx = image[i] & 0xff;
                argb[i] = decode(palette, idx * bpp);
            } else {
                argb[i] = decode(image, i * bpp);
            }
        }
    }

    public Image2D(int format, int width, int height) {
        checkFormat(format);
        if (width <= 0 || height <= 0) {
            throw new IllegalArgumentException();
        }
        this.format = format;
        this.width = width;
        this.height = height;
        this.mutable = true;
        argb = new int[width * height];
    }

    private Image2D(Image2D o) {
        format = o.format;
        width = o.width;
        height = o.height;
        mutable = o.mutable;
        argb = new int[o.argb.length];
        System.arraycopy(o.argb, 0, argb, 0, argb.length);
    }

    Object3D duplicateImpl() {
        return new Image2D(this);
    }

    private static void checkFormat(int f) {
        if (f < ALPHA || f > RGBA) {
            throw new IllegalArgumentException();
        }
    }

    static int bytesPerPixel(int format) {
        switch (format) {
        case ALPHA: case LUMINANCE: return 1;
        case LUMINANCE_ALPHA: return 2;
        case RGB: return 3;
        default: return 4;
        }
    }

    private int decode(byte[] d, int o) {
        switch (format) {
        case ALPHA:
            return ((d[o] & 0xff) << 24) | 0xffffff;
        case LUMINANCE: {
            int l = d[o] & 0xff;
            return 0xff000000 | (l << 16) | (l << 8) | l;
        }
        case LUMINANCE_ALPHA: {
            int l = d[o] & 0xff;
            return ((d[o + 1] & 0xff) << 24) | (l << 16) | (l << 8) | l;
        }
        case RGB:
            return 0xff000000 | ((d[o] & 0xff) << 16) | ((d[o + 1] & 0xff) << 8) | (d[o + 2] & 0xff);
        default:
            return ((d[o + 3] & 0xff) << 24) | ((d[o] & 0xff) << 16) | ((d[o + 1] & 0xff) << 8) | (d[o + 2] & 0xff);
        }
    }

    // Từ ARGB của Image sang đúng ngữ nghĩa của format
    private void convert(int[] p) {
        for (int i = 0; i < p.length; i++) {
            int c = p[i];
            int r = (c >> 16) & 0xff, g = (c >> 8) & 0xff, b = c & 0xff, a = c >>> 24;
            switch (format) {
            case ALPHA:
                p[i] = (a << 24) | 0xffffff;
                break;
            case LUMINANCE: {
                int l = (r * 77 + g * 150 + b * 29) >> 8;
                p[i] = 0xff000000 | (l << 16) | (l << 8) | l;
                break;
            }
            case LUMINANCE_ALPHA: {
                int l = (r * 77 + g * 150 + b * 29) >> 8;
                p[i] = (a << 24) | (l << 16) | (l << 8) | l;
                break;
            }
            case RGB:
                p[i] = c | 0xff000000;
                break;
            default:
                break;
            }
        }
    }

    public void set(int x, int y, int w, int h, byte[] image) {
        if (!mutable) {
            throw new IllegalStateException();
        }
        if (image == null) {
            throw new NullPointerException();
        }
        if (x < 0 || y < 0 || w <= 0 || h <= 0 || x + w > width || y + h > height) {
            throw new IllegalArgumentException();
        }
        int bpp = bytesPerPixel(format);
        for (int r = 0; r < h; r++) {
            for (int c = 0; c < w; c++) {
                argb[(y + r) * width + x + c] = decode(image, (r * w + c) * bpp);
            }
        }
    }

    public boolean isMutable() { return mutable; }
    public int getFormat() { return format; }
    public int getWidth() { return width; }
    public int getHeight() { return height; }

    boolean hasAlpha() {
        return format == ALPHA || format == LUMINANCE_ALPHA || format == RGBA;
    }
}
