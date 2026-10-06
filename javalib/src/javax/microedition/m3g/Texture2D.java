package javax.microedition.m3g;

public class Texture2D extends Transformable {
    public static final int FILTER_BASE_LEVEL = 208;
    public static final int FILTER_LINEAR = 209;
    public static final int FILTER_NEAREST = 210;
    public static final int FUNC_ADD = 224;
    public static final int FUNC_BLEND = 225;
    public static final int FUNC_DECAL = 226;
    public static final int FUNC_MODULATE = 227;
    public static final int FUNC_REPLACE = 228;
    public static final int WRAP_CLAMP = 240;
    public static final int WRAP_REPEAT = 241;

    Image2D image;
    int blendColor;
    int blending = FUNC_MODULATE;
    int wrapS = WRAP_REPEAT, wrapT = WRAP_REPEAT;
    int levelFilter = FILTER_BASE_LEVEL, imageFilter = FILTER_NEAREST;

    public Texture2D(Image2D image) {
        setImage(image);
    }

    Object3D duplicateImpl() {
        Texture2D t = new Texture2D(image);
        t.copyTransformable(this);
        t.blendColor = blendColor;
        t.blending = blending;
        t.wrapS = wrapS;
        t.wrapT = wrapT;
        t.levelFilter = levelFilter;
        t.imageFilter = imageFilter;
        return t;
    }

    int getReferencesImpl(Object3D[] out) {
        return addRef(out, super.getReferencesImpl(out), image);
    }

    private static boolean pow2(int v) {
        return v > 0 && (v & (v - 1)) == 0;
    }

    public void setImage(Image2D image) {
        if (image == null) {
            throw new NullPointerException();
        }
        // M3G yêu cầu cạnh là luỹ thừa của 2; vẫn chấp nhận ảnh khác để game cũ không lỗi
        this.image = image;
    }

    public Image2D getImage() { return image; }

    public void setFiltering(int levelFilter, int imageFilter) {
        this.levelFilter = levelFilter;
        this.imageFilter = imageFilter;
    }

    public int getLevelFilter() { return levelFilter; }
    public int getImageFilter() { return imageFilter; }

    public void setWrapping(int wrapS, int wrapT) {
        if ((wrapS != WRAP_CLAMP && wrapS != WRAP_REPEAT) || (wrapT != WRAP_CLAMP && wrapT != WRAP_REPEAT)) {
            throw new IllegalArgumentException();
        }
        this.wrapS = wrapS;
        this.wrapT = wrapT;
    }

    public int getWrappingS() { return wrapS; }
    public int getWrappingT() { return wrapT; }

    public void setBlending(int func) {
        if (func < FUNC_ADD || func > FUNC_REPLACE) {
            throw new IllegalArgumentException();
        }
        blending = func;
    }

    public int getBlending() { return blending; }

    public void setBlendColor(int rgb) { blendColor = rgb & 0xffffff; }
    public int getBlendColor() { return blendColor; }

    void applyAnimation(int property, float[] v) {
        if (property == AnimationTrack.COLOR) {
            blendColor = toRGB(v);
        } else {
            super.applyAnimation(property, v);
        }
    }

    boolean unusedPow2() {
        return pow2(image.width) && pow2(image.height);
    }
}
