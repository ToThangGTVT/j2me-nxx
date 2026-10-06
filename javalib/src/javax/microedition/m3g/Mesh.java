package javax.microedition.m3g;

public class Mesh extends Node {
    VertexBuffer vertices;
    IndexBuffer[] submeshes;
    Appearance[] appearances;

    public Mesh(VertexBuffer vertices, IndexBuffer submesh, Appearance appearance) {
        this(vertices, new IndexBuffer[] { submesh }, new Appearance[] { appearance });
    }

    public Mesh(VertexBuffer vertices, IndexBuffer[] submeshes, Appearance[] appearances) {
        if (vertices == null || submeshes == null) {
            throw new NullPointerException();
        }
        if (submeshes.length == 0 || (appearances != null && appearances.length < submeshes.length)) {
            throw new IllegalArgumentException();
        }
        for (int i = 0; i < submeshes.length; i++) {
            if (submeshes[i] == null) {
                throw new NullPointerException();
            }
        }
        this.vertices = vertices;
        this.submeshes = new IndexBuffer[submeshes.length];
        System.arraycopy(submeshes, 0, this.submeshes, 0, submeshes.length);
        this.appearances = new Appearance[submeshes.length];
        if (appearances != null) {
            System.arraycopy(appearances, 0, this.appearances, 0, submeshes.length);
        }
    }

    Object3D duplicateImpl() {
        Mesh m = new Mesh(vertices, submeshes, appearances);
        m.copyNode(this);
        return m;
    }

    int getReferencesImpl(Object3D[] out) {
        int n = super.getReferencesImpl(out);
        n = addRef(out, n, vertices);
        for (int i = 0; i < submeshes.length; i++) {
            n = addRef(out, n, submeshes[i]);
            n = addRef(out, n, appearances[i]);
        }
        return n;
    }

    public void setAppearance(int index, Appearance a) {
        appearances[index] = a;
    }

    public Appearance getAppearance(int index) {
        return appearances[index];
    }

    public IndexBuffer getIndexBuffer(int index) {
        return submeshes[index];
    }

    public VertexBuffer getVertexBuffer() {
        return vertices;
    }

    public int getSubmeshCount() {
        return submeshes.length;
    }

    // Dữ liệu đỉnh thực tế dùng để vẽ (SkinnedMesh / MorphingMesh tính lại)
    VertexBuffer renderVertices() {
        return vertices;
    }
}
