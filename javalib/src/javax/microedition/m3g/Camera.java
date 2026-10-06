package javax.microedition.m3g;

public class Camera extends Node {
    public static final int GENERIC = 48;
    public static final int PARALLEL = 49;
    public static final int PERSPECTIVE = 50;

    int type = GENERIC;
    float fovy, aspect, near, far;
    final Transform generic = new Transform();

    public Camera() {
    }

    Object3D duplicateImpl() {
        Camera c = new Camera();
        c.copyNode(this);
        c.type = type;
        c.fovy = fovy;
        c.aspect = aspect;
        c.near = near;
        c.far = far;
        c.generic.set(generic);
        return c;
    }

    public void setParallel(float fovy, float aspect, float near, float far) {
        if (fovy <= 0 || aspect <= 0) {
            throw new IllegalArgumentException();
        }
        type = PARALLEL;
        this.fovy = fovy;
        this.aspect = aspect;
        this.near = near;
        this.far = far;
    }

    public void setPerspective(float fovy, float aspect, float near, float far) {
        if (fovy <= 0 || fovy >= 180 || aspect <= 0 || near <= 0 || far <= 0) {
            throw new IllegalArgumentException();
        }
        type = PERSPECTIVE;
        this.fovy = fovy;
        this.aspect = aspect;
        this.near = near;
        this.far = far;
    }

    public void setGeneric(Transform transform) {
        if (transform == null) {
            throw new NullPointerException();
        }
        type = GENERIC;
        generic.set(transform);
    }

    public int getProjection(float[] params) {
        if (params != null) {
            if (params.length < 4) {
                throw new IllegalArgumentException();
            }
            params[0] = fovy;
            params[1] = aspect;
            params[2] = near;
            params[3] = far;
        }
        return type;
    }

    public int getProjection(Transform transform) {
        if (transform != null) {
            projection(transform);
        }
        return type;
    }

    void projection(Transform t) {
        float[] m = t.m;
        for (int i = 0; i < 16; i++) {
            m[i] = 0;
        }
        if (type == GENERIC) {
            t.set(generic);
        } else if (type == PERSPECTIVE) {
            float f = (float) (1.0 / Math.tan(Math.toRadians(fovy) / 2));
            m[0] = f / aspect;
            m[5] = f;
            m[10] = (far + near) / (near - far);
            m[11] = 2 * far * near / (near - far);
            m[14] = -1;
        } else {
            float h = fovy, w = aspect * h;
            m[0] = 2 / w;
            m[5] = 2 / h;
            m[10] = -2 / (far - near);
            m[11] = -(far + near) / (far - near);
            m[15] = 1;
        }
    }

    void applyAnimation(int property, float[] v) {
        switch (property) {
        case AnimationTrack.FIELD_OF_VIEW: fovy = v[0]; break;
        case AnimationTrack.NEAR_DISTANCE: near = v[0]; break;
        case AnimationTrack.FAR_DISTANCE: far = v[0]; break;
        default: super.applyAnimation(property, v);
        }
    }
}
