package javax.microedition.lcdui;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;

public class Image {
    // VM đọc trực tiếp 4 field này (source/midp/graphics.c)
    int[] pixels;
    int width;
    int height;
    boolean mutable;

    Image(int w, int h, boolean mutable) {
        width = w;
        height = h;
        this.mutable = mutable;
        pixels = new int[w * h];
    }

    private Image() {
    }

    public static Image createImage(int width, int height) {
        if (width <= 0 || height <= 0) {
            throw new IllegalArgumentException();
        }
        Image img = new Image(width, height, true);
        int[] p = img.pixels;
        for (int i = 0; i < p.length; i++) {
            p[i] = 0xffffffff;
        }
        return img;
    }

    public static Image createImage(Image source) {
        if (!source.mutable) {
            return source;
        }
        Image img = new Image(source.width, source.height, false);
        System.arraycopy(source.pixels, 0, img.pixels, 0, img.pixels.length);
        return img;
    }

    public static Image createImage(String name) throws IOException {
        if (name == null) {
            throw new NullPointerException();
        }
        InputStream in = Image.class.getResourceAsStream(name.startsWith("/") ? name : "/" + name);
        if (in == null) {
            throw new IOException("Khong tim thay anh: " + name);
        }
        return createImage(in);
    }

    public static Image createImage(byte[] data, int offset, int length) {
        if (offset < 0 || length < 0 || offset + length > data.length) {
            throw new ArrayIndexOutOfBoundsException();
        }
        Image img = new Image();
        if (!img.decode0(data, offset, length)) {
            throw new IllegalArgumentException("Anh hong hoac khong ho tro");
        }
        return img;
    }

    public static Image createImage(InputStream in) throws IOException {
        if (in == null) {
            throw new NullPointerException();
        }
        ByteArrayOutputStream bo = new ByteArrayOutputStream(4096);
        byte[] buf = new byte[4096];
        int n;
        while ((n = in.read(buf, 0, buf.length)) > 0) {
            bo.write(buf, 0, n);
        }
        byte[] data = bo.toByteArray();
        Image img = new Image();
        if (!img.decode0(data, 0, data.length)) {
            throw new IOException("Anh hong hoac khong ho tro");
        }
        return img;
    }

    public static Image createImage(Image image, int x, int y, int width, int height, int transform) {
        if (x < 0 || y < 0 || width <= 0 || height <= 0 || x + width > image.width || y + height > image.height) {
            throw new IllegalArgumentException();
        }
        boolean swap = Graphics.swapsAxes(transform);
        int w = swap ? height : width;
        int h = swap ? width : height;
        Image img = new Image(w, h, false);
        Graphics g = new Graphics(img);
        g.drawRegionImpl(image, x, y, width, height, transform, 0, 0, true);
        return img;
    }

    public static Image createRGBImage(int[] rgb, int width, int height, boolean processAlpha) {
        if (width <= 0 || height <= 0) {
            throw new IllegalArgumentException();
        }
        if (rgb.length < width * height) {
            throw new ArrayIndexOutOfBoundsException();
        }
        Image img = new Image(width, height, false);
        int n = width * height;
        if (processAlpha) {
            System.arraycopy(rgb, 0, img.pixels, 0, n);
        } else {
            for (int i = 0; i < n; i++) {
                img.pixels[i] = rgb[i] | 0xff000000;
            }
        }
        return img;
    }

    public Graphics getGraphics() {
        if (!mutable) {
            throw new IllegalStateException("Anh khong sua duoc");
        }
        return new Graphics(this);
    }

    public int getWidth() {
        return width;
    }

    public int getHeight() {
        return height;
    }

    public boolean isMutable() {
        return mutable;
    }

    public void getRGB(int[] rgbData, int offset, int scanlength, int x, int y, int w, int h) {
        if (x < 0 || y < 0 || x + w > width || y + h > height) {
            throw new IllegalArgumentException();
        }
        for (int row = 0; row < h; row++) {
            System.arraycopy(pixels, (y + row) * width + x, rgbData, offset + row * scanlength, w);
        }
    }

    // Đọc PNG/JPEG...: đặt pixels/width/height
    private native boolean decode0(byte[] data, int offset, int length);
}
