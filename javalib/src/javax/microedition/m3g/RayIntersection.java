package javax.microedition.m3g;

public class RayIntersection {
    Node node;
    float distance;
    int submesh;
    final float[] ray = new float[6];
    final float[] normal = { 0, 0, 1 };
    final float[] s = new float[2], t = new float[2];

    public RayIntersection() {
    }

    public Node getIntersected() { return node; }
    public float getDistance() { return distance; }
    public int getSubmeshIndex() { return submesh; }
    public float getTextureS(int index) { return s[index]; }
    public float getTextureT(int index) { return t[index]; }
    public float getNormalX() { return normal[0]; }
    public float getNormalY() { return normal[1]; }
    public float getNormalZ() { return normal[2]; }

    public void getRay(float[] out) {
        if (out == null) {
            throw new NullPointerException();
        }
        if (out.length < 6) {
            throw new IllegalArgumentException();
        }
        System.arraycopy(ray, 0, out, 0, 6);
    }
}
