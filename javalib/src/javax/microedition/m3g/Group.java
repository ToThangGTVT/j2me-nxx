package javax.microedition.m3g;

import java.util.Vector;

public class Group extends Node {
    final Vector children = new Vector();

    public Group() {
    }

    Object3D duplicateImpl() {
        Group g = new Group();
        copyGroup(g);
        return g;
    }

    void copyGroup(Group g) {
        g.copyNode(this);
        for (int i = 0; i < children.size(); i++) {
            g.addChild((Node) ((Node) children.elementAt(i)).duplicate());
        }
    }

    int getReferencesImpl(Object3D[] out) {
        int n = super.getReferencesImpl(out);
        for (int i = 0; i < children.size(); i++) {
            n = addRef(out, n, (Object3D) children.elementAt(i));
        }
        return n;
    }

    public void addChild(Node child) {
        if (child == null) {
            throw new NullPointerException();
        }
        if (child == this || child instanceof World || (child.parent != null && child.parent != this)) {
            throw new IllegalArgumentException();
        }
        for (Node p = this; p != null; p = p.parent) {
            if (p == child) {
                throw new IllegalArgumentException();
            }
        }
        if (child.parent == this) {
            return;
        }
        child.parent = this;
        children.addElement(child);
    }

    public void removeChild(Node child) {
        if (child != null && children.removeElement(child)) {
            child.parent = null;
        }
    }

    public int getChildCount() {
        return children.size();
    }

    public Node getChild(int index) {
        return (Node) children.elementAt(index);
    }

    public boolean pick(int scope, float ox, float oy, float oz, float dx, float dy, float dz, RayIntersection ri) {
        if (dx == 0 && dy == 0 && dz == 0) {
            throw new IllegalArgumentException();
        }
        return Picker.pick(this, scope, new float[] { ox, oy, oz }, new float[] { dx, dy, dz }, ri);
    }

    public boolean pick(int scope, float x, float y, Camera camera, RayIntersection ri) {
        if (camera == null) {
            throw new NullPointerException();
        }
        // Tia từ camera qua điểm (x, y) của viewport [0, 1]
        float[] near = { 2 * x - 1, 1 - 2 * y, -1, 1 };
        float[] far = { 2 * x - 1, 1 - 2 * y, 1, 1 };
        Transform inv = new Transform();
        camera.projection(inv);
        inv.invert();
        inv.transform(near);
        inv.transform(far);
        for (int i = 0; i < 3; i++) {
            near[i] /= near[3];
            far[i] /= far[3];
        }
        Transform camToGroup = new Transform();
        if (!camera.getTransformTo(this, camToGroup)) {
            return false;
        }
        float[] p = { near[0], near[1], near[2], 1, far[0], far[1], far[2], 1 };
        camToGroup.transform(p);
        return Picker.pick(this, scope, new float[] { p[0], p[1], p[2] },
                new float[] { p[4] - p[0], p[5] - p[1], p[6] - p[2] }, ri);
    }
}
