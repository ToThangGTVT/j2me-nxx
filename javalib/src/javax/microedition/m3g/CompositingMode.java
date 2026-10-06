package javax.microedition.m3g;

public class CompositingMode extends Object3D {
    public static final int ALPHA = 64;
    public static final int ALPHA_ADD = 65;
    public static final int MODULATE = 66;
    public static final int MODULATE_X2 = 67;
    public static final int REPLACE = 68;

    int blending = REPLACE;
    float alphaThreshold;
    boolean depthTest = true, depthWrite = true, colorWrite = true, alphaWrite = true;
    float depthOffsetFactor, depthOffsetUnits;

    public CompositingMode() {
    }

    Object3D duplicateImpl() {
        CompositingMode c = new CompositingMode();
        c.blending = blending;
        c.alphaThreshold = alphaThreshold;
        c.depthTest = depthTest;
        c.depthWrite = depthWrite;
        c.colorWrite = colorWrite;
        c.alphaWrite = alphaWrite;
        c.depthOffsetFactor = depthOffsetFactor;
        c.depthOffsetUnits = depthOffsetUnits;
        return c;
    }

    public void setBlending(int mode) {
        if (mode < ALPHA || mode > REPLACE) {
            throw new IllegalArgumentException();
        }
        blending = mode;
    }

    public int getBlending() { return blending; }

    public void setAlphaThreshold(float t) {
        if (t < 0 || t > 1) {
            throw new IllegalArgumentException();
        }
        alphaThreshold = t;
    }

    public float getAlphaThreshold() { return alphaThreshold; }
    public void setDepthTestEnable(boolean e) { depthTest = e; }
    public boolean isDepthTestEnabled() { return depthTest; }
    public void setDepthWriteEnable(boolean e) { depthWrite = e; }
    public boolean isDepthWriteEnabled() { return depthWrite; }
    public void setColorWriteEnable(boolean e) { colorWrite = e; }
    public boolean isColorWriteEnabled() { return colorWrite; }
    public void setAlphaWriteEnable(boolean e) { alphaWrite = e; }
    public boolean isAlphaWriteEnabled() { return alphaWrite; }

    public void setDepthOffset(float factor, float units) {
        depthOffsetFactor = factor;
        depthOffsetUnits = units;
    }

    public float getDepthOffsetFactor() { return depthOffsetFactor; }
    public float getDepthOffsetUnits() { return depthOffsetUnits; }
}
