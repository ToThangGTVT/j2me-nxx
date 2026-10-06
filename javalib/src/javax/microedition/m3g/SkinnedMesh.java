package javax.microedition.m3g;

import java.util.Vector;

public class SkinnedMesh extends Mesh {
    private final Group skeleton;
    private final Vector bones = new Vector();

    private static class Bone {
        Node node;
        int weight, first, count;
        Transform restInverse;      // nghịch đảo biến đổi xương -> mesh ở tư thế gốc
    }

    public SkinnedMesh(VertexBuffer vertices, IndexBuffer submesh, Appearance appearance, Group skeleton) {
        this(vertices, new IndexBuffer[] { submesh }, new Appearance[] { appearance }, skeleton);
    }

    public SkinnedMesh(VertexBuffer vertices, IndexBuffer[] submeshes, Appearance[] appearances, Group skeleton) {
        super(vertices, submeshes, appearances);
        if (skeleton == null) {
            throw new NullPointerException();
        }
        if (skeleton.parent != null || skeleton instanceof World) {
            throw new IllegalArgumentException();
        }
        this.skeleton = skeleton;
        skeleton.parent = this;
    }

    Object3D duplicateImpl() {
        SkinnedMesh m = new SkinnedMesh(vertices, submeshes, appearances, (Group) skeleton.duplicate());
        m.copyNode(this);
        return m;
    }

    int getReferencesImpl(Object3D[] out) {
        return addRef(out, super.getReferencesImpl(out), skeleton);
    }

    public Group getSkeleton() {
        return skeleton;
    }

    public void addTransform(Node bone, int weight, int firstVertex, int numVertices) {
        if (bone == null) {
            throw new NullPointerException();
        }
        if (weight <= 0 || numVertices <= 0 || firstVertex < 0 || firstVertex + numVertices > 65535) {
            throw new IllegalArgumentException();
        }
        Bone b = new Bone();
        b.node = bone;
        b.weight = weight;
        b.first = firstVertex;
        b.count = numVertices;
        b.restInverse = new Transform();
        if (bone.getTransformTo(this, b.restInverse)) {
            b.restInverse.invert();
        }
        bones.addElement(b);
    }

    public void getBoneTransform(Node bone, Transform transform) {
        for (int i = 0; i < bones.size(); i++) {
            Bone b = (Bone) bones.elementAt(i);
            if (b.node == bone) {
                transform.set(b.restInverse);
                transform.invert();
                return;
            }
        }
        throw new IllegalArgumentException();
    }

    public int getBoneVertices(Node bone, int[] indices, float[] weights) {
        int n = 0;
        for (int i = 0; i < bones.size(); i++) {
            Bone b = (Bone) bones.elementAt(i);
            if (b.node != bone) {
                continue;
            }
            for (int v = 0; v < b.count; v++, n++) {
                if (indices != null && n < indices.length) {
                    indices[n] = b.first + v;
                }
                if (weights != null && n < weights.length) {
                    weights[n] = b.weight;
                }
            }
        }
        return n;
    }

    // Tính vị trí / pháp tuyến sau skinning thành mảng float
    VertexBuffer renderVertices() {
        VertexArray pa = vertices.positions;
        if (pa == null || bones.isEmpty()) {
            return vertices;
        }
        int n = pa.count;
        float[] pos = new float[n * 3];
        float[] wsum = new float[n];
        float[] src = new float[4];
        Transform cur = new Transform();
        for (int i = 0; i < bones.size(); i++) {
            Bone b = (Bone) bones.elementAt(i);
            if (!b.node.getTransformTo(this, cur)) {
                continue;
            }
            cur.postMultiply(b.restInverse);
            float[] m = cur.m;
            for (int v = b.first; v < b.first + b.count && v < n; v++) {
                float x = pa.comp(v * 3) * vertices.posScale + vertices.posBias[0];
                float y = pa.comp(v * 3 + 1) * vertices.posScale + vertices.posBias[1];
                float z = pa.comp(v * 3 + 2) * vertices.posScale + vertices.posBias[2];
                float w = b.weight;
                pos[v * 3] += w * (m[0] * x + m[1] * y + m[2] * z + m[3]);
                pos[v * 3 + 1] += w * (m[4] * x + m[5] * y + m[6] * z + m[7]);
                pos[v * 3 + 2] += w * (m[8] * x + m[9] * y + m[10] * z + m[11]);
                wsum[v] += w;
            }
        }
        for (int v = 0; v < n; v++) {
            if (wsum[v] > 0) {
                pos[v * 3] /= wsum[v];
                pos[v * 3 + 1] /= wsum[v];
                pos[v * 3 + 2] /= wsum[v];
            } else {
                pos[v * 3] = pa.comp(v * 3) * vertices.posScale + vertices.posBias[0];
                pos[v * 3 + 1] = pa.comp(v * 3 + 1) * vertices.posScale + vertices.posBias[1];
                pos[v * 3 + 2] = pa.comp(v * 3 + 2) * vertices.posScale + vertices.posBias[2];
            }
        }
        VertexBuffer out = (VertexBuffer) vertices.duplicateImpl();
        out.floatPositions = pos;
        return out;
    }
}
