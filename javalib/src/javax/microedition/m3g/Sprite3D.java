package javax.microedition.m3g;

public class Sprite3D extends Node {
    private final boolean scaled;
    Image2D image;
    Appearance appearance;
    int cropX, cropY, cropW, cropH;

    public Sprite3D(boolean scaled, Image2D image, Appearance appearance) {
        this.scaled = scaled;
        setImage(image);
        this.appearance = appearance;
    }

    Object3D duplicateImpl() {
        Sprite3D s = new Sprite3D(scaled, image, appearance);
        s.copyNode(this);
        s.cropX = cropX;
        s.cropY = cropY;
        s.cropW = cropW;
        s.cropH = cropH;
        return s;
    }

    int getReferencesImpl(Object3D[] out) {
        int n = super.getReferencesImpl(out);
        n = addRef(out, n, image);
        n = addRef(out, n, appearance);
        return n;
    }

    public boolean isScaled() { return scaled; }
    public void setAppearance(Appearance a) { appearance = a; }
    public Appearance getAppearance() { return appearance; }

    public void setImage(Image2D image) {
        if (image == null) {
            throw new NullPointerException();
        }
        this.image = image;
        cropX = 0;
        cropY = 0;
        cropW = image.width;
        cropH = image.height;
    }

    public Image2D getImage() { return image; }

    public void setCrop(int x, int y, int w, int h) {
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
        if (property == AnimationTrack.CROP) {
            if (v.length >= 4) {
                setCrop((int) v[0], (int) v[1], (int) v[2], (int) v[3]);
            } else if (v.length >= 2) {
                cropX = (int) v[0];
                cropY = (int) v[1];
            }
        } else {
            super.applyAnimation(property, v);
        }
    }
}
