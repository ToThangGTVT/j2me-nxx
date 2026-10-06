package javax.microedition.m3g;

public class Appearance extends Object3D {
    int layer;
    CompositingMode compositing;
    Fog fog;
    PolygonMode polygon;
    Material material;
    final Texture2D[] textures = new Texture2D[2];

    public Appearance() {
    }

    Object3D duplicateImpl() {
        Appearance a = new Appearance();
        a.layer = layer;
        a.compositing = compositing;
        a.fog = fog;
        a.polygon = polygon;
        a.material = material;
        a.textures[0] = textures[0];
        a.textures[1] = textures[1];
        return a;
    }

    int getReferencesImpl(Object3D[] out) {
        int n = super.getReferencesImpl(out);
        n = addRef(out, n, compositing);
        n = addRef(out, n, fog);
        n = addRef(out, n, polygon);
        n = addRef(out, n, material);
        n = addRef(out, n, textures[0]);
        n = addRef(out, n, textures[1]);
        return n;
    }

    public void setLayer(int layer) {
        if (layer < -63 || layer > 63) {
            throw new IndexOutOfBoundsException();
        }
        this.layer = layer;
    }

    public int getLayer() { return layer; }
    public void setFog(Fog fog) { this.fog = fog; }
    public Fog getFog() { return fog; }
    public void setPolygonMode(PolygonMode p) { polygon = p; }
    public PolygonMode getPolygonMode() { return polygon; }
    public void setCompositingMode(CompositingMode c) { compositing = c; }
    public CompositingMode getCompositingMode() { return compositing; }
    public void setMaterial(Material m) { material = m; }
    public Material getMaterial() { return material; }

    public void setTexture(int index, Texture2D t) {
        if (index < 0 || index >= textures.length) {
            throw new IndexOutOfBoundsException();
        }
        textures[index] = t;
    }

    public Texture2D getTexture(int index) {
        if (index < 0 || index >= textures.length) {
            throw new IndexOutOfBoundsException();
        }
        return textures[index];
    }
}
