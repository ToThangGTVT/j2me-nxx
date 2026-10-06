package javax.microedition.m3g;

public class VertexBuffer extends Object3D {
    VertexArray positions, normals, colors;
    float posScale = 1;
    final float[] posBias = new float[3];
    final VertexArray[] texCoords = new VertexArray[2];
    final float[] tcScale = { 1, 1 };
    final float[][] tcBias = new float[2][3];
    int defaultColor = 0xffffffff;
    // Vị trí đã tính sẵn (skinning / morphing), không dùng scale/bias
    float[] floatPositions;

    public VertexBuffer() {
    }

    Object3D duplicateImpl() {
        VertexBuffer v = new VertexBuffer();
        v.positions = positions;
        v.normals = normals;
        v.colors = colors;
        v.posScale = posScale;
        System.arraycopy(posBias, 0, v.posBias, 0, 3);
        for (int i = 0; i < 2; i++) {
            v.texCoords[i] = texCoords[i];
            v.tcScale[i] = tcScale[i];
            System.arraycopy(tcBias[i], 0, v.tcBias[i], 0, 3);
        }
        v.defaultColor = defaultColor;
        return v;
    }

    int getReferencesImpl(Object3D[] out) {
        int n = super.getReferencesImpl(out);
        n = addRef(out, n, positions);
        n = addRef(out, n, normals);
        n = addRef(out, n, colors);
        n = addRef(out, n, texCoords[0]);
        n = addRef(out, n, texCoords[1]);
        return n;
    }

    public int getVertexCount() {
        return positions != null ? positions.count : normals != null ? normals.count
                : colors != null ? colors.count : texCoords[0] != null ? texCoords[0].count : 0;
    }

    public void setPositions(VertexArray p, float scale, float[] bias) {
        if (p != null && p.comps != 3) {
            throw new IllegalArgumentException();
        }
        positions = p;
        posScale = scale;
        posBias[0] = posBias[1] = posBias[2] = 0;
        if (bias != null) {
            System.arraycopy(bias, 0, posBias, 0, Math.min(3, bias.length));
        }
    }

    public VertexArray getPositions(float[] scaleBias) {
        if (scaleBias != null) {
            scaleBias[0] = posScale;
            for (int i = 0; i < 3 && i + 1 < scaleBias.length; i++) {
                scaleBias[i + 1] = posBias[i];
            }
        }
        return positions;
    }

    public void setTexCoords(int index, VertexArray tc, float scale, float[] bias) {
        if (index < 0 || index >= 2) {
            throw new IndexOutOfBoundsException();
        }
        if (tc != null && tc.comps > 3) {
            throw new IllegalArgumentException();
        }
        texCoords[index] = tc;
        tcScale[index] = scale;
        tcBias[index][0] = tcBias[index][1] = tcBias[index][2] = 0;
        if (bias != null) {
            System.arraycopy(bias, 0, tcBias[index], 0, Math.min(3, bias.length));
        }
    }

    public VertexArray getTexCoords(int index, float[] scaleBias) {
        if (index < 0 || index >= 2) {
            throw new IndexOutOfBoundsException();
        }
        if (scaleBias != null) {
            scaleBias[0] = tcScale[index];
            for (int i = 0; i < 3 && i + 1 < scaleBias.length; i++) {
                scaleBias[i + 1] = tcBias[index][i];
            }
        }
        return texCoords[index];
    }

    public void setNormals(VertexArray n) {
        if (n != null && n.comps != 3) {
            throw new IllegalArgumentException();
        }
        normals = n;
    }

    public VertexArray getNormals() { return normals; }

    public void setColors(VertexArray c) {
        if (c != null && (c.size != 1 || c.comps < 3)) {
            throw new IllegalArgumentException();
        }
        colors = c;
    }

    public VertexArray getColors() { return colors; }

    public void setDefaultColor(int argb) { defaultColor = argb; }
    public int getDefaultColor() { return defaultColor; }

    void applyAnimation(int property, float[] v) {
        if (property == AnimationTrack.COLOR) {
            defaultColor = toARGB(v, defaultColor >>> 24);
        } else if (property == AnimationTrack.ALPHA) {
            defaultColor = (defaultColor & 0xffffff) | (clampByte(v[0]) << 24);
        } else {
            super.applyAnimation(property, v);
        }
    }
}
