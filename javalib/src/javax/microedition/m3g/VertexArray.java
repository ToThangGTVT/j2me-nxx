package javax.microedition.m3g;

public class VertexArray extends Object3D {
    final int count, comps, size;
    byte[] bytes;       // size == 1
    short[] shorts;     // size == 2

    public VertexArray(int numVertices, int numComponents, int componentSize) {
        if (numVertices < 1 || numVertices > 65535 || numComponents < 2 || numComponents > 4
                || componentSize < 1 || componentSize > 2) {
            throw new IllegalArgumentException();
        }
        count = numVertices;
        comps = numComponents;
        size = componentSize;
        if (size == 1) {
            bytes = new byte[numVertices * numComponents];
        } else {
            shorts = new short[numVertices * numComponents];
        }
    }

    Object3D duplicateImpl() {
        VertexArray v = new VertexArray(count, comps, size);
        if (bytes != null) {
            System.arraycopy(bytes, 0, v.bytes, 0, bytes.length);
        } else {
            System.arraycopy(shorts, 0, v.shorts, 0, shorts.length);
        }
        return v;
    }

    public int getVertexCount() { return count; }
    public int getComponentCount() { return comps; }
    public int getComponentType() { return size; }

    Object data() {
        return bytes != null ? (Object) bytes : shorts;
    }

    float comp(int i) {
        return bytes != null ? bytes[i] : shorts[i];
    }

    private void check(int first, int n, int len) {
        if (first < 0 || n < 0 || first + n > count) {
            throw new IndexOutOfBoundsException();
        }
        if (len < n * comps) {
            throw new IllegalArgumentException();
        }
    }

    public void set(int firstVertex, int numVertices, short[] values) {
        if (values == null) {
            throw new NullPointerException();
        }
        if (size != 2) {
            throw new IllegalStateException();
        }
        check(firstVertex, numVertices, values.length);
        System.arraycopy(values, 0, shorts, firstVertex * comps, numVertices * comps);
    }

    public void set(int firstVertex, int numVertices, byte[] values) {
        if (values == null) {
            throw new NullPointerException();
        }
        if (size != 1) {
            throw new IllegalStateException();
        }
        check(firstVertex, numVertices, values.length);
        System.arraycopy(values, 0, bytes, firstVertex * comps, numVertices * comps);
    }

    public void get(int firstVertex, int numVertices, short[] values) {
        if (values == null) {
            throw new NullPointerException();
        }
        if (size != 2) {
            throw new IllegalStateException();
        }
        check(firstVertex, numVertices, values.length);
        System.arraycopy(shorts, firstVertex * comps, values, 0, numVertices * comps);
    }

    public void get(int firstVertex, int numVertices, byte[] values) {
        if (values == null) {
            throw new NullPointerException();
        }
        if (size != 1) {
            throw new IllegalStateException();
        }
        check(firstVertex, numVertices, values.length);
        System.arraycopy(bytes, firstVertex * comps, values, 0, numVertices * comps);
    }
}
