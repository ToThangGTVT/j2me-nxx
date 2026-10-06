package javax.microedition.m3g;

public class Light extends Node {
    public static final int AMBIENT = 128;
    public static final int DIRECTIONAL = 129;
    public static final int OMNI = 130;
    public static final int SPOT = 131;

    int mode = DIRECTIONAL;
    int color = 0xffffff;
    float intensity = 1;
    float attConst = 1, attLinear, attQuad;
    float spotAngle = 45, spotExponent;

    public Light() {
    }

    Object3D duplicateImpl() {
        Light l = new Light();
        l.copyNode(this);
        l.mode = mode;
        l.color = color;
        l.intensity = intensity;
        l.attConst = attConst;
        l.attLinear = attLinear;
        l.attQuad = attQuad;
        l.spotAngle = spotAngle;
        l.spotExponent = spotExponent;
        return l;
    }

    public void setMode(int mode) {
        if (mode < AMBIENT || mode > SPOT) {
            throw new IllegalArgumentException();
        }
        this.mode = mode;
    }

    public int getMode() { return mode; }
    public void setIntensity(float i) { intensity = i; }
    public float getIntensity() { return intensity; }
    public void setColor(int rgb) { color = rgb & 0xffffff; }
    public int getColor() { return color; }

    public void setSpotAngle(float a) {
        if (a < 0 || a > 90) {
            throw new IllegalArgumentException();
        }
        spotAngle = a;
    }

    public float getSpotAngle() { return spotAngle; }

    public void setSpotExponent(float e) {
        if (e < 0 || e > 128) {
            throw new IllegalArgumentException();
        }
        spotExponent = e;
    }

    public float getSpotExponent() { return spotExponent; }

    public void setAttenuation(float c, float l, float q) {
        if (c < 0 || l < 0 || q < 0 || (c == 0 && l == 0 && q == 0)) {
            throw new IllegalArgumentException();
        }
        attConst = c;
        attLinear = l;
        attQuad = q;
    }

    public float getConstantAttenuation() { return attConst; }
    public float getLinearAttenuation() { return attLinear; }
    public float getQuadraticAttenuation() { return attQuad; }

    void applyAnimation(int property, float[] v) {
        switch (property) {
        case AnimationTrack.COLOR: color = toRGB(v); break;
        case AnimationTrack.INTENSITY: intensity = v[0]; break;
        case AnimationTrack.SPOT_ANGLE: spotAngle = Math.max(0, Math.min(90, v[0])); break;
        case AnimationTrack.SPOT_EXPONENT: spotExponent = Math.max(0, Math.min(128, v[0])); break;
        default: super.applyAnimation(property, v);
        }
    }
}
