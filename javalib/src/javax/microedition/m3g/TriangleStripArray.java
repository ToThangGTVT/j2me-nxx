package javax.microedition.m3g;

public class TriangleStripArray extends IndexBuffer {
    private final int[] indices;    // null nếu dùng chỉ số liên tiếp
    private final int first;
    private final int[] strips;

    public TriangleStripArray(int[] indices, int[] stripLengths) {
        if (indices == null || stripLengths == null) {
            throw new NullPointerException();
        }
        int total = check(stripLengths);
        if (indices.length < total) {
            throw new IllegalArgumentException();
        }
        this.indices = new int[total];
        System.arraycopy(indices, 0, this.indices, 0, total);
        this.first = 0;
        this.strips = copy(stripLengths);
        build();
    }

    public TriangleStripArray(int firstIndex, int[] stripLengths) {
        if (stripLengths == null) {
            throw new NullPointerException();
        }
        int total = check(stripLengths);
        if (firstIndex < 0 || firstIndex + total > 65536) {
            throw new IndexOutOfBoundsException();
        }
        this.indices = null;
        this.first = firstIndex;
        this.strips = copy(stripLengths);
        build();
    }

    private static int check(int[] lens) {
        if (lens.length == 0) {
            throw new IllegalArgumentException();
        }
        int t = 0;
        for (int i = 0; i < lens.length; i++) {
            if (lens[i] < 3) {
                throw new IllegalArgumentException();
            }
            t += lens[i];
        }
        return t;
    }

    private static int[] copy(int[] a) {
        int[] r = new int[a.length];
        System.arraycopy(a, 0, r, 0, a.length);
        return r;
    }

    private int at(int i) {
        return indices != null ? indices[i] : first + i;
    }

    private void build() {
        int tris = 0;
        for (int i = 0; i < strips.length; i++) {
            tris += strips[i] - 2;
        }
        int[] t = new int[tris * 3];
        int n = 0, base = 0;
        for (int s = 0; s < strips.length; s++) {
            for (int i = 0; i + 2 < strips[s]; i++) {
                int a = at(base + i), b = at(base + i + 1), c = at(base + i + 2);
                // Đảo thứ tự mỗi tam giác lẻ để giữ cùng chiều quấn
                if ((i & 1) == 0) {
                    t[n++] = a;
                    t[n++] = b;
                } else {
                    t[n++] = b;
                    t[n++] = a;
                }
                t[n++] = c;
            }
            base += strips[s];
        }
        triangles = t;
    }

    Object3D duplicateImpl() {
        return indices != null ? new TriangleStripArray(indices, strips) : new TriangleStripArray(first, strips);
    }

    public int getIndexCount() {
        return triangles.length;
    }

    public void getIndices(int[] out) {
        if (out == null) {
            throw new NullPointerException();
        }
        if (out.length < triangles.length) {
            throw new IllegalArgumentException();
        }
        System.arraycopy(triangles, 0, out, 0, triangles.length);
    }
}
