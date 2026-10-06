package javax.microedition.m3g;

// Ma trận 4x4, lưu theo hàng (m[row * 4 + col]) như M3G
public class Transform {
    final float[] m = new float[16];

    public Transform() {
        setIdentity();
    }

    public Transform(Transform t) {
        set(t);
    }

    public void setIdentity() {
        for (int i = 0; i < 16; i++) {
            m[i] = (i % 5 == 0) ? 1 : 0;
        }
    }

    public void set(Transform t) {
        if (t == null) {
            throw new NullPointerException();
        }
        System.arraycopy(t.m, 0, m, 0, 16);
    }

    public void set(float[] matrix) {
        if (matrix == null) {
            throw new NullPointerException();
        }
        if (matrix.length < 16) {
            throw new IllegalArgumentException();
        }
        System.arraycopy(matrix, 0, m, 0, 16);
    }

    public void get(float[] matrix) {
        if (matrix == null) {
            throw new NullPointerException();
        }
        if (matrix.length < 16) {
            throw new IllegalArgumentException();
        }
        System.arraycopy(m, 0, matrix, 0, 16);
    }

    public void invert() {
        float[] inv = new float[16];
        if (!invert(m, inv)) {
            throw new ArithmeticException("Ma tran khong kha nghich");
        }
        System.arraycopy(inv, 0, m, 0, 16);
    }

    static boolean invert(float[] a, float[] inv) {
        inv[0] = a[5] * a[10] * a[15] - a[5] * a[11] * a[14] - a[9] * a[6] * a[15] + a[9] * a[7] * a[14] + a[13] * a[6] * a[11] - a[13] * a[7] * a[10];
        inv[4] = -a[4] * a[10] * a[15] + a[4] * a[11] * a[14] + a[8] * a[6] * a[15] - a[8] * a[7] * a[14] - a[12] * a[6] * a[11] + a[12] * a[7] * a[10];
        inv[8] = a[4] * a[9] * a[15] - a[4] * a[11] * a[13] - a[8] * a[5] * a[15] + a[8] * a[7] * a[13] + a[12] * a[5] * a[11] - a[12] * a[7] * a[9];
        inv[12] = -a[4] * a[9] * a[14] + a[4] * a[10] * a[13] + a[8] * a[5] * a[14] - a[8] * a[6] * a[13] - a[12] * a[5] * a[10] + a[12] * a[6] * a[9];
        inv[1] = -a[1] * a[10] * a[15] + a[1] * a[11] * a[14] + a[9] * a[2] * a[15] - a[9] * a[3] * a[14] - a[13] * a[2] * a[11] + a[13] * a[3] * a[10];
        inv[5] = a[0] * a[10] * a[15] - a[0] * a[11] * a[14] - a[8] * a[2] * a[15] + a[8] * a[3] * a[14] + a[12] * a[2] * a[11] - a[12] * a[3] * a[10];
        inv[9] = -a[0] * a[9] * a[15] + a[0] * a[11] * a[13] + a[8] * a[1] * a[15] - a[8] * a[3] * a[13] - a[12] * a[1] * a[11] + a[12] * a[3] * a[9];
        inv[13] = a[0] * a[9] * a[14] - a[0] * a[10] * a[13] - a[8] * a[1] * a[14] + a[8] * a[2] * a[13] + a[12] * a[1] * a[10] - a[12] * a[2] * a[9];
        inv[2] = a[1] * a[6] * a[15] - a[1] * a[7] * a[14] - a[5] * a[2] * a[15] + a[5] * a[3] * a[14] + a[13] * a[2] * a[7] - a[13] * a[3] * a[6];
        inv[6] = -a[0] * a[6] * a[15] + a[0] * a[7] * a[14] + a[4] * a[2] * a[15] - a[4] * a[3] * a[14] - a[12] * a[2] * a[7] + a[12] * a[3] * a[6];
        inv[10] = a[0] * a[5] * a[15] - a[0] * a[7] * a[13] - a[4] * a[1] * a[15] + a[4] * a[3] * a[13] + a[12] * a[1] * a[7] - a[12] * a[3] * a[5];
        inv[14] = -a[0] * a[5] * a[14] + a[0] * a[6] * a[13] + a[4] * a[1] * a[14] - a[4] * a[2] * a[13] - a[12] * a[1] * a[6] + a[12] * a[2] * a[5];
        inv[3] = -a[1] * a[6] * a[11] + a[1] * a[7] * a[10] + a[5] * a[2] * a[11] - a[5] * a[3] * a[10] - a[9] * a[2] * a[7] + a[9] * a[3] * a[6];
        inv[7] = a[0] * a[6] * a[11] - a[0] * a[7] * a[10] - a[4] * a[2] * a[11] + a[4] * a[3] * a[10] + a[8] * a[2] * a[7] - a[8] * a[3] * a[6];
        inv[11] = -a[0] * a[5] * a[11] + a[0] * a[7] * a[9] + a[4] * a[1] * a[11] - a[4] * a[3] * a[9] - a[8] * a[1] * a[7] + a[8] * a[3] * a[5];
        inv[15] = a[0] * a[5] * a[10] - a[0] * a[6] * a[9] - a[4] * a[1] * a[10] + a[4] * a[2] * a[9] + a[8] * a[1] * a[6] - a[8] * a[2] * a[5];
        float det = a[0] * inv[0] + a[1] * inv[4] + a[2] * inv[8] + a[3] * inv[12];
        if (det == 0) {
            return false;
        }
        det = 1.0f / det;
        for (int i = 0; i < 16; i++) {
            inv[i] *= det;
        }
        return true;
    }

    public void transpose() {
        for (int r = 0; r < 4; r++) {
            for (int c = r + 1; c < 4; c++) {
                float t = m[r * 4 + c];
                m[r * 4 + c] = m[c * 4 + r];
                m[c * 4 + r] = t;
            }
        }
    }

    // this = this * b
    static void mul(float[] a, float[] b, float[] out) {
        float[] r = new float[16];
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                r[i * 4 + j] = a[i * 4] * b[j] + a[i * 4 + 1] * b[4 + j] + a[i * 4 + 2] * b[8 + j] + a[i * 4 + 3] * b[12 + j];
            }
        }
        System.arraycopy(r, 0, out, 0, 16);
    }

    public void postMultiply(Transform t) {
        if (t == null) {
            throw new NullPointerException();
        }
        mul(m, t.m, m);
    }

    public void postScale(float sx, float sy, float sz) {
        for (int r = 0; r < 4; r++) {
            m[r * 4] *= sx;
            m[r * 4 + 1] *= sy;
            m[r * 4 + 2] *= sz;
        }
    }

    public void postRotate(float angle, float ax, float ay, float az) {
        if (angle == 0) {
            return;
        }
        float l = (float) Math.sqrt(ax * ax + ay * ay + az * az);
        if (l == 0) {
            throw new IllegalArgumentException();
        }
        float[] q = new float[4];
        Transformable.axisAngleToQuat(angle, ax / l, ay / l, az / l, q);
        postRotateQuat(q[0], q[1], q[2], q[3]);
    }

    public void postRotateQuat(float qx, float qy, float qz, float qw) {
        float[] r = new float[16];
        quatToMatrix(qx, qy, qz, qw, r);
        mul(m, r, m);
    }

    static void quatToMatrix(float x, float y, float z, float w, float[] r) {
        float l = (float) Math.sqrt(x * x + y * y + z * z + w * w);
        if (l == 0) {
            for (int i = 0; i < 16; i++) {
                r[i] = (i % 5 == 0) ? 1 : 0;
            }
            return;
        }
        x /= l;
        y /= l;
        z /= l;
        w /= l;
        r[0] = 1 - 2 * (y * y + z * z);
        r[1] = 2 * (x * y - z * w);
        r[2] = 2 * (x * z + y * w);
        r[3] = 0;
        r[4] = 2 * (x * y + z * w);
        r[5] = 1 - 2 * (x * x + z * z);
        r[6] = 2 * (y * z - x * w);
        r[7] = 0;
        r[8] = 2 * (x * z - y * w);
        r[9] = 2 * (y * z + x * w);
        r[10] = 1 - 2 * (x * x + y * y);
        r[11] = 0;
        r[12] = 0;
        r[13] = 0;
        r[14] = 0;
        r[15] = 1;
    }

    public void postTranslate(float tx, float ty, float tz) {
        for (int r = 0; r < 4; r++) {
            m[r * 4 + 3] += m[r * 4] * tx + m[r * 4 + 1] * ty + m[r * 4 + 2] * tz;
        }
    }

    // Biến đổi mảng vector 4 thành phần tại chỗ
    public void transform(float[] vectors) {
        if (vectors == null) {
            throw new NullPointerException();
        }
        if (vectors.length % 4 != 0) {
            throw new IllegalArgumentException();
        }
        for (int i = 0; i < vectors.length; i += 4) {
            float x = vectors[i], y = vectors[i + 1], z = vectors[i + 2], w = vectors[i + 3];
            for (int r = 0; r < 4; r++) {
                vectors[i + r] = m[r * 4] * x + m[r * 4 + 1] * y + m[r * 4 + 2] * z + m[r * 4 + 3] * w;
            }
        }
    }

    public void transform(VertexArray in, float[] out, boolean W) {
        if (in == null || out == null) {
            throw new NullPointerException();
        }
        int n = in.getVertexCount(), cc = in.getComponentCount();
        if (out.length < n * 4) {
            throw new IllegalArgumentException();
        }
        for (int i = 0; i < n; i++) {
            float x = in.comp(i * cc), y = cc > 1 ? in.comp(i * cc + 1) : 0, z = cc > 2 ? in.comp(i * cc + 2) : 0;
            float w = W ? 1 : 0;
            for (int r = 0; r < 4; r++) {
                out[i * 4 + r] = m[r * 4] * x + m[r * 4 + 1] * y + m[r * 4 + 2] * z + m[r * 4 + 3] * w;
            }
        }
    }

    // Chuyển sang cột-chính cho native
    void toColumnMajor(float[] out, int off) {
        for (int r = 0; r < 4; r++) {
            for (int c = 0; c < 4; c++) {
                out[off + c * 4 + r] = m[r * 4 + c];
            }
        }
    }
}
