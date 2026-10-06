package javax.microedition.m3g;

public abstract class Node extends Transformable {
    public static final int NONE = 144;
    public static final int ORIGIN = 145;
    public static final int X_AXIS = 146;
    public static final int Y_AXIS = 147;
    public static final int Z_AXIS = 148;

    Node parent;
    boolean renderingEnabled = true, pickingEnabled = true;
    float alphaFactor = 1;
    int scope = -1;
    Node zRef, yRef;
    int zTarget = NONE, yTarget = NONE;

    Node() {
    }

    void copyNode(Node o) {
        copyTransformable(o);
        renderingEnabled = o.renderingEnabled;
        pickingEnabled = o.pickingEnabled;
        alphaFactor = o.alphaFactor;
        scope = o.scope;
        zRef = o.zRef;
        yRef = o.yRef;
        zTarget = o.zTarget;
        yTarget = o.yTarget;
    }

    public Node getParent() { return parent; }
    public void setRenderingEnable(boolean e) { renderingEnabled = e; }
    public boolean isRenderingEnabled() { return renderingEnabled; }
    public void setPickingEnable(boolean e) { pickingEnabled = e; }
    public boolean isPickingEnabled() { return pickingEnabled; }

    public void setAlphaFactor(float a) {
        if (a < 0 || a > 1) {
            throw new IllegalArgumentException();
        }
        alphaFactor = a;
    }

    public float getAlphaFactor() { return alphaFactor; }
    public void setScope(int scope) { this.scope = scope; }
    public int getScope() { return scope; }

    public void setAlignment(Node zRef, int zTarget, Node yRef, int yTarget) {
        if (zTarget < NONE || zTarget > Z_AXIS || yTarget < NONE || yTarget > Z_AXIS) {
            throw new IllegalArgumentException();
        }
        this.zRef = zRef;
        this.zTarget = zTarget;
        this.yRef = yRef;
        this.yTarget = yTarget;
    }

    public Node getAlignmentReference(int axis) {
        if (axis == Z_AXIS) return zRef;
        if (axis == Y_AXIS) return yRef;
        throw new IllegalArgumentException();
    }

    public int getAlignmentTarget(int axis) {
        if (axis == Z_AXIS) return zTarget;
        if (axis == Y_AXIS) return yTarget;
        throw new IllegalArgumentException();
    }

    // Căn hướng (billboard) chưa hỗ trợ đầy đủ: chỉ duyệt cây
    public final void align(Node reference) {
    }

    Node root() {
        Node n = this;
        while (n.parent != null) {
            n = n.parent;
        }
        return n;
    }

    // Biến đổi từ không gian của node này lên gốc cây
    void worldTransform(Transform out) {
        Transform t = new Transform();
        out.setIdentity();
        Node[] chain = new Node[64];
        int n = 0;
        for (Node p = this; p != null && n < chain.length; p = p.parent) {
            chain[n++] = p;
        }
        for (int i = n - 1; i >= 0; i--) {
            chain[i].getCompositeTransform(t);
            out.postMultiply(t);
        }
    }

    public boolean getTransformTo(Node target, Transform transform) {
        if (target == null || transform == null) {
            throw new NullPointerException();
        }
        if (root() != target.root()) {
            return false;
        }
        Transform a = new Transform(), b = new Transform();
        worldTransform(a);
        target.worldTransform(b);
        b.invert();
        b.postMultiply(a);
        transform.set(b);
        return true;
    }

    void applyAnimation(int property, float[] v) {
        switch (property) {
        case AnimationTrack.ALPHA: alphaFactor = Math.max(0, Math.min(1, v[0])); break;
        case AnimationTrack.PICKABILITY: pickingEnabled = v[0] >= 0.5f; break;
        case AnimationTrack.VISIBILITY: renderingEnabled = v[0] >= 0.5f; break;
        default: super.applyAnimation(property, v);
        }
    }
}
