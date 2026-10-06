package javax.microedition.m3g;

public class Background extends Object3D {
    public static final int BORDER = 32;
    public static final int REPEAT = 33;

    int color;              // ARGB, mặc định đen trong suốt
    Image2D image;
    int modeX = BORDER, modeY = BORDER;
    int cropX, cropY, cropW, cropH;
    boolean colorClear = true, depthClear = true;

    public Background() {
    }

    Object3D duplicateImpl() {
        Background b = new Background();
        b.color = color;
        b.image = image;
        b.modeX = modeX;
        b.modeY = modeY;
        b.cropX = cropX;
        b.cropY = cropY;
        b.cropW = cropW;
        b.cropH = cropH;
        b.colorClear = colorClear;
        b.depthClear = depthClear;
        return b;
    }

    int getReferencesImpl(Object3D[] out) {
        return addRef(out, super.getReferencesImpl(out), image);
    }

    public void setColorClearEnable(boolean e) { colorClear = e; }
    public void setDepthClearEnable(boolean e) { depthClear = e; }
    public boolean isColorClearEnabled() { return colorClear; }
    public boolean isDepthClearEnabled() { return depthClear; }
    public void setColor(int argb) { color = argb; }
    public int getColor() { return color; }

    public void setImage(Image2D image) {
        if (image != null && image.format != Image2D.RGB && image.format != Image2D.RGBA) {
            throw new IllegalArgumentException();
        }
        this.image = image;
        if (image != null) {
            cropX = 0;
            cropY = 0;
            cropW = image.width;
            cropH = image.height;
        }
    }

    public Image2D getImage() { return image; }

    public void setImageMode(int modeX, int modeY) {
        if ((modeX != BORDER && modeX != REPEAT) || (modeY != BORDER && modeY != REPEAT)) {
            throw new IllegalArgumentException();
        }
        this.modeX = modeX;
        this.modeY = modeY;
    }

    public int getImageModeX() { return modeX; }
    public int getImageModeY() { return modeY; }

    public void setCrop(int x, int y, int w, int h) {
        if (w < 0 || h < 0) {
            throw new IllegalArgumentException();
        }
        cropX = x;
        cropY = y;
        cropW = w;
        cropH = h;
    }

    public int getCropX() { return cropX; }
    public int getCropY() { return cropY; }
    public int getCropWidth() { return cropW; }
    public int getCropHeight() { return cropH; }

    void applyAnimation(int property, float[] v) {
        if (property == AnimationTrack.COLOR) {
            color = toARGB(v, color >>> 24);
        } else if (property == AnimationTrack.ALPHA) {
            color = (color & 0xffffff) | (clampByte(v[0]) << 24);
        } else if (property == AnimationTrack.CROP) {
            if (v.length >= 4) {
                cropX = (int) v[0];
                cropY = (int) v[1];
                cropW = (int) v[2];
                cropH = (int) v[3];
            } else if (v.length >= 2) {
                cropX = (int) v[0];
                cropY = (int) v[1];
            }
        } else {
            super.applyAnimation(property, v);
        }
    }
}
