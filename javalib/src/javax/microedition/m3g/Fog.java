package javax.microedition.m3g;

public class Fog extends Object3D {
    public static final int EXPONENTIAL = 80;
    public static final int LINEAR = 81;

    int mode = LINEAR;
    int color;
    float density = 1;
    float near = 0, far = 1;

    public Fog() {
    }

    Object3D duplicateImpl() {
        Fog f = new Fog();
        f.mode = mode;
        f.color = color;
        f.density = density;
        f.near = near;
        f.far = far;
        return f;
    }

    public void setMode(int mode) {
        if (mode != EXPONENTIAL && mode != LINEAR) {
            throw new IllegalArgumentException();
        }
        this.mode = mode;
    }

    public int getMode() { return mode; }

    public void setLinear(float near, float far) {
        this.near = near;
        this.far = far;
    }

    public float getNearDistance() { return near; }
    public float getFarDistance() { return far; }

    public void setDensity(float density) {
        if (density < 0) {
            throw new IllegalArgumentException();
        }
        this.density = density;
    }

    public float getDensity() { return density; }
    public void setColor(int rgb) { color = rgb & 0xffffff; }
    public int getColor() { return color; }

    void applyAnimation(int property, float[] v) {
        switch (property) {
        case AnimationTrack.COLOR: color = toRGB(v); break;
        case AnimationTrack.DENSITY: density = Math.max(0, v[0]); break;
        case AnimationTrack.NEAR_DISTANCE: near = v[0]; break;
        case AnimationTrack.FAR_DISTANCE: far = v[0]; break;
        default: super.applyAnimation(property, v);
        }
    }
}
