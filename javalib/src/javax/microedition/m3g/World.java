package javax.microedition.m3g;

public class World extends Group {
    Camera activeCamera;
    Background background;

    public World() {
    }

    Object3D duplicateImpl() {
        World w = new World();
        copyGroup(w);
        w.background = background;
        // Camera đang dùng: tìm bản sao tương ứng theo vị trí trong cây là phức tạp; giữ tham chiếu cũ
        w.activeCamera = activeCamera;
        return w;
    }

    int getReferencesImpl(Object3D[] out) {
        int n = super.getReferencesImpl(out);
        n = addRef(out, n, background);
        return n;
    }

    public void setActiveCamera(Camera camera) {
        if (camera == null) {
            throw new NullPointerException();
        }
        activeCamera = camera;
    }

    public Camera getActiveCamera() { return activeCamera; }
    public void setBackground(Background b) { background = b; }
    public Background getBackground() { return background; }
}
