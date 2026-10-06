package javax.microedition.m3g;

public class MorphingMesh extends Mesh {
    private final VertexBuffer[] targets;
    private final float[] weights;

    public MorphingMesh(VertexBuffer base, VertexBuffer[] targets, IndexBuffer submesh, Appearance appearance) {
        this(base, targets, new IndexBuffer[] { submesh }, new Appearance[] { appearance });
    }

    public MorphingMesh(VertexBuffer base, VertexBuffer[] targets, IndexBuffer[] submeshes, Appearance[] appearances) {
        super(base, submeshes, appearances);
        if (targets == null) {
            throw new NullPointerException();
        }
        this.targets = new VertexBuffer[targets.length];
        System.arraycopy(targets, 0, this.targets, 0, targets.length);
        weights = new float[targets.length];
    }

    Object3D duplicateImpl() {
        MorphingMesh m = new MorphingMesh(vertices, targets, submeshes, appearances);
        m.copyNode(this);
        System.arraycopy(weights, 0, m.weights, 0, weights.length);
        return m;
    }

    int getReferencesImpl(Object3D[] out) {
        int n = super.getReferencesImpl(out);
        for (int i = 0; i < targets.length; i++) {
            n = addRef(out, n, targets[i]);
        }
        return n;
    }

    public VertexBuffer getMorphTarget(int index) { return targets[index]; }
    public int getMorphTargetCount() { return targets.length; }

    public void setWeights(float[] w) {
        if (w == null) {
            throw new NullPointerException();
        }
        if (w.length < targets.length) {
            throw new IllegalArgumentException();
        }
        System.arraycopy(w, 0, weights, 0, targets.length);
    }

    public void getWeights(float[] w) {
        if (w == null) {
            throw new NullPointerException();
        }
        if (w.length < targets.length) {
            throw new IllegalArgumentException();
        }
        System.arraycopy(weights, 0, w, 0, targets.length);
    }

    private static float pos(VertexBuffer vb, int i, int c) {
        return vb.positions.comp(i * 3 + c) * vb.posScale + vb.posBias[c];
    }

    VertexBuffer renderVertices() {
        VertexArray pa = vertices.positions;
        if (pa == null) {
            return vertices;
        }
        int n = pa.count;
        float[] out = new float[n * 3];
        for (int v = 0; v < n; v++) {
            for (int c = 0; c < 3; c++) {
                float base = pos(vertices, v, c);
                float r = base;
                for (int t = 0; t < targets.length; t++) {
                    if (weights[t] != 0 && targets[t].positions != null && v < targets[t].positions.count) {
                        r += weights[t] * (pos(targets[t], v, c) - base);
                    }
                }
                out[v * 3 + c] = r;
            }
        }
        VertexBuffer vb = (VertexBuffer) vertices.duplicateImpl();
        vb.floatPositions = out;
        return vb;
    }

    void applyAnimation(int property, float[] v) {
        if (property == AnimationTrack.MORPH_WEIGHTS) {
            System.arraycopy(v, 0, weights, 0, Math.min(v.length, weights.length));
        } else {
            super.applyAnimation(property, v);
        }
    }
}
