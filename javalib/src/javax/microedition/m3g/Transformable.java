package javax.microedition.m3g;

public abstract class Transformable extends Object3D {
    float tx, ty, tz;
    float qx, qy, qz, qw = 1;   // hướng dạng quaternion
    float sx = 1, sy = 1, sz = 1;
    Transform matrix;           // phần biến đổi tổng quát (null = đơn vị)

    Transformable() {
    }

    void copyTransformable(Transformable o) {
        tx = o.tx;
        ty = o.ty;
        tz = o.tz;
        qx = o.qx;
        qy = o.qy;
        qz = o.qz;
        qw = o.qw;
        sx = o.sx;
        sy = o.sy;
        sz = o.sz;
        matrix = o.matrix == null ? null : new Transform(o.matrix);
    }

    static void axisAngleToQuat(float angleDeg, float x, float y, float z, float[] q) {
        double half = Math.toRadians(angleDeg) / 2;
        float s = (float) Math.sin(half);
        q[0] = x * s;
        q[1] = y * s;
        q[2] = z * s;
        q[3] = (float) Math.cos(half);
    }

    public void setOrientation(float angle, float ax, float ay, float az) {
        float l = (float) Math.sqrt(ax * ax + ay * ay + az * az);
        if (l == 0 && angle != 0) {
            throw new IllegalArgumentException();
        }
        if (angle == 0 || l == 0) {
            qx = qy = qz = 0;
            qw = 1;
            return;
        }
        float[] q = new float[4];
        axisAngleToQuat(angle, ax / l, ay / l, az / l, q);
        qx = q[0];
        qy = q[1];
        qz = q[2];
        qw = q[3];
    }

    // a * b
    static void quatMul(float ax, float ay, float az, float aw, float bx, float by, float bz, float bw, float[] o) {
        o[0] = aw * bx + ax * bw + ay * bz - az * by;
        o[1] = aw * by - ax * bz + ay * bw + az * bx;
        o[2] = aw * bz + ax * by - ay * bx + az * bw;
        o[3] = aw * bw - ax * bx - ay * by - az * bz;
    }

    public void preRotate(float angle, float ax, float ay, float az) {
        rotate(angle, ax, ay, az, true);
    }

    public void postRotate(float angle, float ax, float ay, float az) {
        rotate(angle, ax, ay, az, false);
    }

    private void rotate(float angle, float ax, float ay, float az, boolean pre) {
        float l = (float) Math.sqrt(ax * ax + ay * ay + az * az);
        if (l == 0) {
            if (angle != 0) {
                throw new IllegalArgumentException();
            }
            return;
        }
        float[] r = new float[4];
        axisAngleToQuat(angle, ax / l, ay / l, az / l, r);
        float[] o = new float[4];
        if (pre) {
            quatMul(r[0], r[1], r[2], r[3], qx, qy, qz, qw, o);
        } else {
            quatMul(qx, qy, qz, qw, r[0], r[1], r[2], r[3], o);
        }
        qx = o[0];
        qy = o[1];
        qz = o[2];
        qw = o[3];
    }

    public void getOrientation(float[] angleAxis) {
        if (angleAxis == null) {
            throw new NullPointerException();
        }
        if (angleAxis.length < 4) {
            throw new IllegalArgumentException();
        }
        float w = Math.max(-1, Math.min(1, qw));
        float s = (float) Math.sqrt(1 - w * w);
        angleAxis[0] = (float) Math.toDegrees(2 * Math.acos(w));
        if (s < 1e-6f) {
            angleAxis[1] = 0;
            angleAxis[2] = 0;
            angleAxis[3] = 0;
        } else {
            angleAxis[1] = qx / s;
            angleAxis[2] = qy / s;
            angleAxis[3] = qz / s;
        }
    }

    public void setScale(float sx, float sy, float sz) {
        this.sx = sx;
        this.sy = sy;
        this.sz = sz;
    }

    public void scale(float sx, float sy, float sz) {
        this.sx *= sx;
        this.sy *= sy;
        this.sz *= sz;
    }

    public void getScale(float[] xyz) {
        if (xyz == null) {
            throw new NullPointerException();
        }
        xyz[0] = sx;
        xyz[1] = sy;
        xyz[2] = sz;
    }

    public void setTranslation(float tx, float ty, float tz) {
        this.tx = tx;
        this.ty = ty;
        this.tz = tz;
    }

    public void translate(float tx, float ty, float tz) {
        this.tx += tx;
        this.ty += ty;
        this.tz += tz;
    }

    public void getTranslation(float[] xyz) {
        if (xyz == null) {
            throw new NullPointerException();
        }
        xyz[0] = tx;
        xyz[1] = ty;
        xyz[2] = tz;
    }

    public void setTransform(Transform transform) {
        matrix = transform == null ? null : new Transform(transform);
    }

    public void getTransform(Transform transform) {
        if (transform == null) {
            throw new NullPointerException();
        }
        if (matrix == null) {
            transform.setIdentity();
        } else {
            transform.set(matrix);
        }
    }

    // T * R * S * M
    public void getCompositeTransform(Transform transform) {
        if (transform == null) {
            throw new NullPointerException();
        }
        transform.setIdentity();
        transform.postTranslate(tx, ty, tz);
        transform.postRotateQuat(qx, qy, qz, qw);
        transform.postScale(sx, sy, sz);
        if (matrix != null) {
            transform.postMultiply(matrix);
        }
    }

    void applyAnimation(int property, float[] v) {
        switch (property) {
        case AnimationTrack.TRANSLATION:
            tx = v[0];
            ty = v.length > 1 ? v[1] : ty;
            tz = v.length > 2 ? v[2] : tz;
            break;
        case AnimationTrack.ORIENTATION:
            if (v.length >= 4) {
                qx = v[0];
                qy = v[1];
                qz = v[2];
                qw = v[3];
            }
            break;
        case AnimationTrack.SCALE:
            if (v.length == 1) {
                sx = sy = sz = v[0];
            } else {
                sx = v[0];
                sy = v[1];
                sz = v.length > 2 ? v[2] : sz;
            }
            break;
        default:
            super.applyAnimation(property, v);
        }
    }

    boolean animatable(int property) {
        return property == AnimationTrack.TRANSLATION || property == AnimationTrack.ORIENTATION
                || property == AnimationTrack.SCALE || super.animatable(property);
    }
}
