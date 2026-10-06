package javax.microedition.m3g;

public abstract class IndexBuffer extends Object3D {
    // Danh sách tam giác đã khai triển (3 chỉ số mỗi tam giác) cho bộ dựng hình
    int[] triangles = new int[0];

    IndexBuffer() {
    }

    public abstract int getIndexCount();

    public abstract void getIndices(int[] indices);
}
