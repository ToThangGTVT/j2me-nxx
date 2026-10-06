package javax.microedition.m3g;

public class Material extends Object3D {
    public static final int AMBIENT = 1024;
    public static final int DIFFUSE = 2048;
    public static final int EMISSIVE = 4096;
    public static final int SPECULAR = 8192;

    int ambient = 0x333333;
    int diffuse = 0xffcccccc;
    int emissive = 0;
    int specular = 0;
    float shininess;
    boolean vertexColorTracking;

    public Material() {
    }

    Object3D duplicateImpl() {
        Material m = new Material();
        m.ambient = ambient;
        m.diffuse = diffuse;
        m.emissive = emissive;
        m.specular = specular;
        m.shininess = shininess;
        m.vertexColorTracking = vertexColorTracking;
        return m;
    }

    public void setColor(int target, int argb) {
        if ((target & ~(AMBIENT | DIFFUSE | EMISSIVE | SPECULAR)) != 0 || target == 0) {
            throw new IllegalArgumentException();
        }
        if ((target & AMBIENT) != 0) {
            ambient = argb & 0xffffff;
        }
        if ((target & DIFFUSE) != 0) {
            diffuse = argb;
        }
        if ((target & EMISSIVE) != 0) {
            emissive = argb & 0xffffff;
        }
        if ((target & SPECULAR) != 0) {
            specular = argb & 0xffffff;
        }
    }

    public int getColor(int target) {
        switch (target) {
        case AMBIENT: return ambient;
        case DIFFUSE: return diffuse;
        case EMISSIVE: return emissive;
        case SPECULAR: return specular;
        default: throw new IllegalArgumentException();
        }
    }

    public void setShininess(float s) {
        if (s < 0 || s > 128) {
            throw new IllegalArgumentException();
        }
        shininess = s;
    }

    public float getShininess() { return shininess; }
    public void setVertexColorTrackingEnable(boolean e) { vertexColorTracking = e; }
    public boolean isVertexColorTrackingEnabled() { return vertexColorTracking; }

    void applyAnimation(int property, float[] v) {
        switch (property) {
        case AnimationTrack.AMBIENT_COLOR: ambient = toRGB(v); break;
        case AnimationTrack.DIFFUSE_COLOR: diffuse = toARGB(v, diffuse >>> 24); break;
        case AnimationTrack.EMISSIVE_COLOR: emissive = toRGB(v); break;
        case AnimationTrack.SPECULAR_COLOR: specular = toRGB(v); break;
        case AnimationTrack.SHININESS: shininess = Math.max(0, Math.min(128, v[0])); break;
        case AnimationTrack.ALPHA: diffuse = (diffuse & 0xffffff) | (clampByte(v[0]) << 24); break;
        default: super.applyAnimation(property, v);
        }
    }
}
