package javax.microedition.m3g;

import java.util.Hashtable;
import java.util.Vector;
import javax.microedition.lcdui.DisplayAccess;
import javax.microedition.lcdui.Graphics;

public class Graphics3D {
    public static final int ANTIALIAS = 2;
    public static final int DITHER = 4;
    public static final int TRUE_COLOR = 8;
    public static final int OVERWRITE = 16;

    // Bố cục tham số cho native renderSubmesh (khớp source/midp/m3g.c)
    private static final int F_MODELVIEW = 0, F_PROJECTION = 16, F_POS_BIAS = 32, F_POS_SCALE = 35, F_TC_SCALE = 36,
            F_TC_BIAS = 37, F_TEX_MATRIX = 40, F_VIEWPORT = 56, F_DEPTH_RANGE = 60, F_MAT_AMBIENT = 62,
            F_MAT_DIFFUSE = 66, F_MAT_EMISSIVE = 70, F_MAT_SPECULAR = 74, F_MAT_SHININESS = 78, F_ALPHA_FACTOR = 79,
            F_FOG_COLOR = 80, F_FOG_DENSITY = 84, F_FOG_NEAR = 85, F_FOG_FAR = 86, F_TEX_BLEND_COLOR = 87,
            F_LIGHTS = 96, F_COUNT = 96 + 16 * 8;
    private static final int P_POS_COMPS = 0, P_POS_TYPE = 1, P_HAS_NORMALS = 2, P_NORMAL_TYPE = 3,
            P_COLOR_COMPS = 4, P_DEFAULT_COLOR = 5, P_TC_COMPS = 6, P_TC_TYPE = 7, P_TEX_W = 8, P_TEX_H = 9,
            P_TEX_FUNC = 10, P_TEX_WRAP_S = 11, P_TEX_WRAP_T = 12, P_TEX_HAS_ALPHA = 13, P_BLENDING = 14,
            P_ALPHA_THRESHOLD = 15, P_DEPTH_TEST = 16, P_DEPTH_WRITE = 17, P_COLOR_WRITE = 18, P_ALPHA_WRITE = 19,
            P_CULLING = 20, P_SHADING = 21, P_WINDING = 22, P_TWO_SIDED = 23, P_LIGHTING = 24,
            P_COLOR_TRACKING = 25, P_LIGHT_COUNT = 26, P_CLIP_X = 27, P_CLIP_Y = 28, P_CLIP_W = 29, P_CLIP_H = 30,
            P_FOG_MODE = 31, P_INDEX_COUNT = 32, P_VERTEX_COUNT = 33, P_TARGET_HAS_ALPHA = 34, P_COUNT = 40;
    private static final int MAX_LIGHTS = 8;

    private static Graphics3D instance;
    private static Hashtable properties;

    private Object target;
    private int[] pixels;
    private int tw, th;
    private int transX, transY;
    private int clipX, clipY, clipW, clipH;
    private boolean targetAlpha;
    private int vx, vy, vw, vh;
    private float depthNear = 0, depthFar = 1;
    private boolean depthEnabled = true;
    private int hints;

    private Camera camera;
    private final Transform cameraTransform = new Transform();
    private final Vector lights = new Vector();
    private final Vector lightTransforms = new Vector();

    private final float[] f = new float[F_COUNT];
    private final int[] p = new int[P_COUNT];
    private final int[] info = new int[8];

    private Graphics3D() {
    }

    public static synchronized Graphics3D getInstance() {
        if (instance == null) {
            instance = new Graphics3D();
        }
        return instance;
    }

    public static synchronized Hashtable getProperties() {
        if (properties == null) {
            properties = new Hashtable();
            properties.put("supportAntialiasing", Boolean.FALSE);
            properties.put("supportTrueColor", Boolean.TRUE);
            properties.put("supportDithering", Boolean.FALSE);
            properties.put("supportMipmapping", Boolean.FALSE);
            properties.put("supportPerspectiveCorrection", Boolean.TRUE);
            properties.put("supportLocalCameraLighting", Boolean.FALSE);
            properties.put("maxLights", new Integer(MAX_LIGHTS));
            properties.put("maxViewportWidth", new Integer(1280));
            properties.put("maxViewportHeight", new Integer(1280));
            properties.put("maxViewportDimension", new Integer(1280));
            properties.put("maxTextureDimension", new Integer(1024));
            properties.put("maxSpriteCropDimension", new Integer(1024));
            properties.put("maxTransformsPerVertex", new Integer(4));
            properties.put("numTextureUnits", new Integer(1));
        }
        return properties;
    }

    // ------------------------------------------------------------------
    // Đích vẽ

    public void bindTarget(Object target) {
        bindTarget(target, true, 0);
    }

    public void bindTarget(Object target, boolean depthBuffer, int hints) {
        if (target == null) {
            throw new NullPointerException();
        }
        if (this.target != null) {
            throw new IllegalStateException("Graphics3D da gan dich ve");
        }
        if (target instanceof Graphics) {
            Graphics g = (Graphics) target;
            pixels = DisplayAccess.graphicsTarget(g, info);
            transX = info[0];
            transY = info[1];
            clipX = info[2];
            clipY = info[3];
            clipW = info[4];
            clipH = info[5];
            tw = info[6];
            th = info[7];
            targetAlpha = false;
        } else if (target instanceof Image2D) {
            Image2D img = (Image2D) target;
            if (!img.mutable || (img.format != Image2D.RGB && img.format != Image2D.RGBA)) {
                throw new IllegalArgumentException();
            }
            pixels = img.argb;
            transX = transY = 0;
            clipX = clipY = 0;
            clipW = tw = img.width;
            clipH = th = img.height;
            targetAlpha = img.format == Image2D.RGBA;
        } else {
            throw new IllegalArgumentException();
        }
        this.target = target;
        this.depthEnabled = depthBuffer;
        this.hints = hints;
        vx = clipX;
        vy = clipY;
        vw = clipW;
        vh = clipH;
    }

    public void releaseTarget() {
        target = null;
        pixels = null;
    }

    public Object getTarget() { return target; }
    public int getHints() { return hints; }
    public boolean isDepthBufferEnabled() { return depthEnabled; }

    public void setViewport(int x, int y, int width, int height) {
        if (width <= 0 || height <= 0) {
            throw new IllegalArgumentException();
        }
        vx = x + transX;
        vy = y + transY;
        vw = width;
        vh = height;
    }

    public int getViewportX() { return vx - transX; }
    public int getViewportY() { return vy - transY; }
    public int getViewportWidth() { return vw; }
    public int getViewportHeight() { return vh; }

    public void setDepthRange(float near, float far) {
        if (near < 0 || near > 1 || far < 0 || far > 1) {
            throw new IllegalArgumentException();
        }
        depthNear = near;
        depthFar = far;
    }

    public float getDepthRangeNear() { return depthNear; }
    public float getDepthRangeFar() { return depthFar; }

    private void checkTarget() {
        if (target == null) {
            throw new IllegalStateException("Chua bindTarget");
        }
    }

    // ------------------------------------------------------------------
    // Camera / đèn (chế độ immediate)

    public void setCamera(Camera camera, Transform transform) {
        this.camera = camera;
        if (transform == null) {
            cameraTransform.setIdentity();
        } else {
            cameraTransform.set(transform);
        }
    }

    public Camera getCamera(Transform transform) {
        if (transform != null) {
            transform.set(cameraTransform);
        }
        return camera;
    }

    public int addLight(Light light, Transform transform) {
        if (light == null) {
            throw new NullPointerException();
        }
        lights.addElement(light);
        lightTransforms.addElement(transform == null ? new Transform() : new Transform(transform));
        return lights.size() - 1;
    }

    public void setLight(int index, Light light, Transform transform) {
        lights.setElementAt(light, index);
        lightTransforms.setElementAt(transform == null ? new Transform() : new Transform(transform), index);
    }

    public void resetLights() {
        lights.removeAllElements();
        lightTransforms.removeAllElements();
    }

    public int getLightCount() { return lights.size(); }

    public Light getLight(int index, Transform transform) {
        if (transform != null) {
            transform.set((Transform) lightTransforms.elementAt(index));
        }
        return (Light) lights.elementAt(index);
    }

    // ------------------------------------------------------------------
    // Xoá nền

    public void clear(Background bg) {
        checkTarget();
        boolean colorClear = bg == null || bg.colorClear;
        boolean depthClear = depthEnabled && (bg == null || bg.depthClear);
        int color = bg == null ? 0 : bg.color;
        if (!targetAlpha) {
            color |= 0xff000000;
        }
        int x0 = Math.max(vx, clipX), y0 = Math.max(vy, clipY);
        int x1 = Math.min(vx + vw, clipX + clipW), y1 = Math.min(vy + vh, clipY + clipH);
        if (x1 <= x0 || y1 <= y0) {
            return;
        }
        clear0(pixels, tw, th, x0, y0, x1 - x0, y1 - y0, color, colorClear && (bg == null || bg.image == null),
                depthClear);
        if (bg != null && bg.colorClear && bg.image != null) {
            blitBackground(pixels, tw, th, vx, vy, vw, vh, bg.image.argb, bg.image.width, bg.image.height,
                    bg.cropX, bg.cropY, bg.cropW, bg.cropH, bg.modeX, bg.modeY, color);
        }
    }

    // ------------------------------------------------------------------
    // Render

    // Một mục cần vẽ: mesh + submesh (hoặc sprite) với biến đổi về không gian mắt
    private static class Item {
        Node node;
        int submesh;
        Appearance app;
        Transform modelView;
        float alpha;
        int layer;
        boolean blended;
    }

    public void render(World world) {
        checkTarget();
        if (world == null) {
            throw new NullPointerException();
        }
        Camera cam = world.activeCamera;
        if (cam == null || cam.root() != world) {
            throw new IllegalStateException("World khong co camera hop le");
        }
        clear(world.background);
        Transform camWorld = new Transform();
        cam.worldTransform(camWorld);
        Transform view = new Transform(camWorld);
        view.invert();

        Vector sceneLights = new Vector(), sceneLightTf = new Vector();
        collectLights(world, sceneLights, sceneLightTf, view);
        Vector items = new Vector();
        collect(world, view, 1, cam.scope, items, true);
        draw(items, cam, sceneLights, sceneLightTf);
    }

    public void render(Node node, Transform transform) {
        checkTarget();
        if (node == null) {
            throw new NullPointerException();
        }
        if (!(node instanceof Mesh || node instanceof Sprite3D || node instanceof Group)) {
            throw new IllegalArgumentException();
        }
        if (camera == null) {
            throw new IllegalStateException("Chua setCamera");
        }
        Transform view = new Transform(cameraTransform);
        view.invert();
        Transform base = new Transform(view);
        if (transform != null) {
            base.postMultiply(transform);
        }
        Vector items = new Vector();
        collectAt(node, base, 1, camera.scope, items);
        draw(items, camera, immediateLights(view), null);
    }

    public void render(VertexBuffer vertices, IndexBuffer triangles, Appearance appearance, Transform transform) {
        render(vertices, triangles, appearance, transform, -1);
    }

    public void render(VertexBuffer vertices, IndexBuffer triangles, Appearance appearance, Transform transform,
            int scope) {
        checkTarget();
        if (vertices == null || triangles == null || appearance == null) {
            throw new NullPointerException();
        }
        if (camera == null) {
            throw new IllegalStateException("Chua setCamera");
        }
        if ((camera.scope & scope) == 0) {
            return;
        }
        Transform mv = new Transform(cameraTransform);
        mv.invert();
        if (transform != null) {
            mv.postMultiply(transform);
        }
        Vector lightList = immediateLights(new Transform(cameraTransform) {
            {
                invert();
            }
        });
        Transform projection = new Transform();
        camera.projection(projection);
        renderSubmeshJava(vertices, triangles, appearance, mv, projection, 1, lightList, scope);
    }

    // Đèn chế độ immediate: Vector các cặp {Light, Transform mắt}
    private Vector immediateLights(Transform view) {
        Vector out = new Vector();
        for (int i = 0; i < lights.size(); i++) {
            Transform t = new Transform(view);
            t.postMultiply((Transform) lightTransforms.elementAt(i));
            out.addElement(new Object[] { lights.elementAt(i), t });
        }
        return out;
    }

    private void collectLights(Node n, Vector out, Vector unused, Transform view) {
        if (n instanceof Light) {
            if (n.renderingEnabled) {
                Transform w = new Transform();
                n.worldTransform(w);
                Transform eye = new Transform(view);
                eye.postMultiply(w);
                out.addElement(new Object[] { n, eye });
            }
        } else if (n instanceof Group) {
            if (!n.renderingEnabled) {
                return;
            }
            Group g = (Group) n;
            for (int i = 0; i < g.children.size(); i++) {
                collectLights((Node) g.children.elementAt(i), out, unused, view);
            }
        }
    }

    private void collect(Node n, Transform view, float alpha, int camScope, Vector items, boolean fromRoot) {
        Transform w = new Transform();
        n.worldTransform(w);
        Transform mv = new Transform(view);
        mv.postMultiply(w);
        collectAt(n, mv, alpha, camScope, items);
    }

    // mv: biến đổi từ không gian của n sang không gian mắt
    private void collectAt(Node n, Transform mv, float alpha, int camScope, Vector items) {
        if (!n.renderingEnabled) {
            return;
        }
        float a = alpha * n.alphaFactor;
        if (n instanceof Group) {
            Group g = (Group) n;
            Transform t = new Transform();
            for (int i = 0; i < g.children.size(); i++) {
                Node c = (Node) g.children.elementAt(i);
                Transform cmv = new Transform(mv);
                c.getCompositeTransform(t);
                cmv.postMultiply(t);
                collectAt(c, cmv, a, camScope, items);
            }
            return;
        }
        if ((n.scope & camScope) == 0) {
            return;
        }
        if (n instanceof Mesh) {
            Mesh m = (Mesh) n;
            if (m instanceof SkinnedMesh) {
                // Khung xương có thể chứa mesh con
                Group sk = ((SkinnedMesh) m).getSkeleton();
                Transform t = new Transform();
                sk.getCompositeTransform(t);
                Transform smv = new Transform(mv);
                smv.postMultiply(t);
                collectAt(sk, smv, a, camScope, items);
            }
            for (int i = 0; i < m.submeshes.length; i++) {
                Appearance app = m.appearances[i];
                if (app == null) {
                    continue;
                }
                addItem(items, n, i, app, mv, a);
            }
        } else if (n instanceof Sprite3D) {
            Sprite3D s = (Sprite3D) n;
            if (s.appearance != null) {
                addItem(items, n, 0, s.appearance, mv, a);
            }
        }
    }

    private static void addItem(Vector items, Node n, int sub, Appearance app, Transform mv, float alpha) {
        Item it = new Item();
        it.node = n;
        it.submesh = sub;
        it.app = app;
        it.modelView = mv;
        it.alpha = alpha;
        it.layer = app.layer;
        it.blended = app.compositing != null && app.compositing.blending != CompositingMode.REPLACE;
        // Chèn giữ ổn định: theo layer, trong cùng layer vật đục trước vật trong suốt
        int pos = items.size();
        while (pos > 0) {
            Item prev = (Item) items.elementAt(pos - 1);
            if (prev.layer < it.layer || (prev.layer == it.layer && (!prev.blended || it.blended))) {
                break;
            }
            pos--;
        }
        items.insertElementAt(it, pos);
    }

    private void draw(Vector items, Camera cam, Vector lightList, Vector unused) {
        Transform projection = new Transform();
        cam.projection(projection);
        for (int i = 0; i < items.size(); i++) {
            Item it = (Item) items.elementAt(i);
            if (it.node instanceof Mesh) {
                Mesh m = (Mesh) it.node;
                renderSubmeshJava(m.renderVertices(), m.submeshes[it.submesh], it.app, it.modelView, projection,
                        it.alpha, lightList, m.scope);
            } else {
                renderSprite((Sprite3D) it.node, it.app, it.modelView, projection, it.alpha);
            }
        }
    }

    // ------------------------------------------------------------------
    // Đổ tham số cho native

    private static float r(int c) { return ((c >> 16) & 255) / 255f; }
    private static float g(int c) { return ((c >> 8) & 255) / 255f; }
    private static float b(int c) { return (c & 255) / 255f; }
    private static float a(int c) { return ((c >>> 24) & 255) / 255f; }

    private void putColor(int off, int c, boolean alpha) {
        f[off] = r(c);
        f[off + 1] = g(c);
        f[off + 2] = b(c);
        if (alpha) {
            f[off + 3] = a(c);
        }
    }

    private void commonState(Appearance app, Transform projection, float alpha) {
        projection.toColumnMajor(f, F_PROJECTION);
        f[F_VIEWPORT] = vx;
        f[F_VIEWPORT + 1] = vy;
        f[F_VIEWPORT + 2] = vw;
        f[F_VIEWPORT + 3] = vh;
        f[F_DEPTH_RANGE] = depthNear;
        f[F_DEPTH_RANGE + 1] = depthFar;
        f[F_ALPHA_FACTOR] = alpha;
        p[P_CLIP_X] = clipX;
        p[P_CLIP_Y] = clipY;
        p[P_CLIP_W] = clipW;
        p[P_CLIP_H] = clipH;
        p[P_TARGET_HAS_ALPHA] = targetAlpha ? 1 : 0;

        CompositingMode cm = app.compositing;
        p[P_BLENDING] = cm == null ? CompositingMode.REPLACE : cm.blending;
        p[P_ALPHA_THRESHOLD] = cm == null ? 0 : (int) (cm.alphaThreshold * 256);
        p[P_DEPTH_TEST] = depthEnabled && (cm == null || cm.depthTest) ? 1 : 0;
        p[P_DEPTH_WRITE] = depthEnabled && (cm == null || cm.depthWrite) ? 1 : 0;
        p[P_COLOR_WRITE] = cm == null || cm.colorWrite ? 1 : 0;
        p[P_ALPHA_WRITE] = cm == null || cm.alphaWrite ? 1 : 0;

        PolygonMode pm = app.polygon;
        p[P_CULLING] = pm == null ? PolygonMode.CULL_BACK : pm.culling;
        p[P_SHADING] = pm == null ? PolygonMode.SHADE_SMOOTH : pm.shading;
        p[P_WINDING] = pm == null ? PolygonMode.WINDING_CCW : pm.winding;
        p[P_TWO_SIDED] = pm != null && pm.twoSided ? 1 : 0;

        Fog fog = app.fog;
        p[P_FOG_MODE] = fog == null ? 0 : fog.mode;
        if (fog != null) {
            putColor(F_FOG_COLOR, fog.color, false);
            f[F_FOG_DENSITY] = fog.density;
            f[F_FOG_NEAR] = fog.near;
            f[F_FOG_FAR] = fog.far;
        }
    }

    private static int arrayType(VertexArray va) {
        return va.size == 1 ? 1 : 2;
    }

    private void renderSubmeshJava(VertexBuffer vb, IndexBuffer ib, Appearance app, Transform mv,
            Transform projection, float alpha, Vector lightList, int meshScope) {
        Object posData;
        int nverts;
        if (vb.floatPositions != null) {
            posData = vb.floatPositions;
            nverts = vb.floatPositions.length / 3;
            p[P_POS_COMPS] = 3;
            p[P_POS_TYPE] = 4;
            f[F_POS_SCALE] = 1;
            f[F_POS_BIAS] = f[F_POS_BIAS + 1] = f[F_POS_BIAS + 2] = 0;
        } else {
            if (vb.positions == null) {
                return;
            }
            posData = vb.positions.data();
            nverts = vb.positions.count;
            p[P_POS_COMPS] = 3;
            p[P_POS_TYPE] = arrayType(vb.positions);
            f[F_POS_SCALE] = vb.posScale;
            f[F_POS_BIAS] = vb.posBias[0];
            f[F_POS_BIAS + 1] = vb.posBias[1];
            f[F_POS_BIAS + 2] = vb.posBias[2];
        }
        mv.toColumnMajor(f, F_MODELVIEW);
        commonState(app, projection, alpha);

        Object nrm = null;
        p[P_HAS_NORMALS] = 0;
        if (vb.normals != null) {
            nrm = vb.normals.data();
            p[P_HAS_NORMALS] = 1;
            p[P_NORMAL_TYPE] = arrayType(vb.normals);
        }
        Object col = null;
        p[P_COLOR_COMPS] = 0;
        if (vb.colors != null) {
            col = vb.colors.data();
            p[P_COLOR_COMPS] = vb.colors.comps;
        }
        p[P_DEFAULT_COLOR] = vb.defaultColor;

        // Texture đơn vị 0
        Object tc = null;
        int[] texPixels = null;
        p[P_TC_COMPS] = 0;
        Texture2D tex = app.textures[0];
        if (tex != null && vb.texCoords[0] != null) {
            VertexArray t = vb.texCoords[0];
            tc = t.data();
            p[P_TC_COMPS] = t.comps;
            p[P_TC_TYPE] = arrayType(t);
            f[F_TC_SCALE] = vb.tcScale[0];
            f[F_TC_BIAS] = vb.tcBias[0][0];
            f[F_TC_BIAS + 1] = vb.tcBias[0][1];
            f[F_TC_BIAS + 2] = vb.tcBias[0][2];
            Transform tm = new Transform();
            tex.getCompositeTransform(tm);
            tm.toColumnMajor(f, F_TEX_MATRIX);
            texPixels = tex.image.argb;
            p[P_TEX_W] = tex.image.width;
            p[P_TEX_H] = tex.image.height;
            p[P_TEX_FUNC] = tex.blending;
            p[P_TEX_WRAP_S] = tex.wrapS;
            p[P_TEX_WRAP_T] = tex.wrapT;
            p[P_TEX_HAS_ALPHA] = tex.image.hasAlpha() ? 1 : 0;
            putColor(F_TEX_BLEND_COLOR, tex.blendColor, false);
        }

        // Vật liệu + đèn
        Material mat = app.material;
        p[P_LIGHTING] = mat != null ? 1 : 0;
        p[P_LIGHT_COUNT] = 0;
        if (mat != null) {
            putColor(F_MAT_AMBIENT, mat.ambient, false);
            f[F_MAT_AMBIENT + 3] = 1;
            putColor(F_MAT_DIFFUSE, mat.diffuse, true);
            putColor(F_MAT_EMISSIVE, mat.emissive, false);
            putColor(F_MAT_SPECULAR, mat.specular, false);
            f[F_MAT_SHININESS] = mat.shininess;
            p[P_COLOR_TRACKING] = mat.vertexColorTracking ? 1 : 0;
            int n = 0;
            for (int i = 0; i < lightList.size() && n < MAX_LIGHTS; i++) {
                Object[] pair = (Object[]) lightList.elementAt(i);
                Light l = (Light) pair[0];
                if ((l.scope & meshScope) == 0) {
                    continue;
                }
                putLight(n++, l, (Transform) pair[1]);
            }
            p[P_LIGHT_COUNT] = n;
        }

        p[P_INDEX_COUNT] = ib.triangles.length;
        p[P_VERTEX_COUNT] = nverts;
        renderSubmesh(pixels, tw, th, f, p, posData, nrm, col, tc, texPixels, ib.triangles);
    }

    private void putLight(int i, Light l, Transform eye) {
        int o = F_LIGHTS + i * 16;
        f[o] = l.mode;
        f[o + 1] = r(l.color) * l.intensity;
        f[o + 2] = g(l.color) * l.intensity;
        f[o + 3] = b(l.color) * l.intensity;
        float[] m = eye.m;
        if (l.mode == Light.DIRECTIONAL) {
            // Hướng chiếu: trục -Z của đèn trong không gian mắt
            float dx = -m[2], dy = -m[6], dz = -m[10];
            float len = (float) Math.sqrt(dx * dx + dy * dy + dz * dz);
            f[o + 4] = dx / len;
            f[o + 5] = dy / len;
            f[o + 6] = dz / len;
        } else {
            f[o + 4] = m[3];
            f[o + 5] = m[7];
            f[o + 6] = m[11];
        }
        f[o + 7] = l.attConst;
        f[o + 8] = l.attLinear;
        f[o + 9] = l.attQuad;
        f[o + 10] = (float) Math.cos(Math.toRadians(l.spotAngle));
        f[o + 11] = l.spotExponent;
        float sx = -m[2], sy = -m[6], sz = -m[10];
        float sl = (float) Math.sqrt(sx * sx + sy * sy + sz * sz);
        f[o + 12] = sx / sl;
        f[o + 13] = sy / sl;
        f[o + 14] = sz / sl;
    }

    // Sprite3D: tứ giác hướng về camera, đưa thẳng toạ độ NDC cho native
    private void renderSprite(Sprite3D s, Appearance app, Transform mv, Transform projection, float alpha) {
        if (s.cropW == 0 || s.cropH == 0) {
            return;
        }
        float[] m = mv.m;
        float[] c = { m[3], m[7], m[11], 1 };
        float[] clip = { c[0], c[1], c[2], c[3] };
        projection.transform(clip);
        if (clip[3] <= 0) {
            return;
        }
        float hx, hy;
        if (s.isScaled()) {
            // Kích thước 1x1 theo đơn vị cục bộ, lấy độ dài cột của ma trận
            float lx = (float) Math.sqrt(m[0] * m[0] + m[4] * m[4] + m[8] * m[8]);
            float ly = (float) Math.sqrt(m[1] * m[1] + m[5] * m[5] + m[9] * m[9]);
            float[] edge = { c[0] + lx / 2, c[1] + ly / 2, c[2], 1 };
            projection.transform(edge);
            hx = Math.abs(edge[0] / edge[3] - clip[0] / clip[3]);
            hy = Math.abs(edge[1] / edge[3] - clip[1] / clip[3]);
        } else {
            hx = Math.abs(s.cropW) / (float) vw;
            hy = Math.abs(s.cropH) / (float) vh;
        }
        float nx = clip[0] / clip[3], ny = clip[1] / clip[3], nz = clip[2] / clip[3];
        float[] pos = { nx - hx, ny + hy, nz, nx + hx, ny + hy, nz, nx + hx, ny - hy, nz, nx - hx, ny - hy, nz };
        Image2D img = s.image;
        float u0 = s.cropX / (float) img.width, v0 = s.cropY / (float) img.height;
        float u1 = (s.cropX + s.cropW) / (float) img.width, v1 = (s.cropY + s.cropH) / (float) img.height;
        float[] tcs = { u0, v0, u1, v0, u1, v1, u0, v1 };

        for (int i = 0; i < 16; i++) {
            f[F_MODELVIEW + i] = (i % 5 == 0) ? 1 : 0;
            f[F_TEX_MATRIX + i] = (i % 5 == 0) ? 1 : 0;
        }
        Transform ident = new Transform();
        commonState(app, ident, alpha);
        p[P_CULLING] = PolygonMode.CULL_NONE;
        p[P_POS_COMPS] = 3;
        p[P_POS_TYPE] = 4;
        f[F_POS_SCALE] = 1;
        f[F_POS_BIAS] = f[F_POS_BIAS + 1] = f[F_POS_BIAS + 2] = 0;
        p[P_HAS_NORMALS] = 0;
        p[P_COLOR_COMPS] = 0;
        p[P_DEFAULT_COLOR] = 0xffffffff;
        p[P_LIGHTING] = 0;
        p[P_LIGHT_COUNT] = 0;
        p[P_TC_COMPS] = 2;
        p[P_TC_TYPE] = 4;
        f[F_TC_SCALE] = 1;
        f[F_TC_BIAS] = f[F_TC_BIAS + 1] = f[F_TC_BIAS + 2] = 0;
        p[P_TEX_W] = img.width;
        p[P_TEX_H] = img.height;
        p[P_TEX_FUNC] = Texture2D.FUNC_REPLACE;
        p[P_TEX_WRAP_S] = Texture2D.WRAP_CLAMP;
        p[P_TEX_WRAP_T] = Texture2D.WRAP_CLAMP;
        p[P_TEX_HAS_ALPHA] = img.hasAlpha() ? 1 : 0;
        p[P_INDEX_COUNT] = 6;
        p[P_VERTEX_COUNT] = 4;
        renderSubmesh(pixels, tw, th, f, p, pos, null, null, tcs, img.argb, new int[] { 0, 1, 2, 0, 2, 3 });
    }

    private static native void renderSubmesh(int[] target, int w, int h, float[] f, int[] p, Object pos,
            Object nrm, Object col, Object tc, int[] tex, int[] indices);

    private static native void clear0(int[] target, int w, int h, int x, int y, int cw, int ch, int argb,
            boolean color, boolean depth);

    private static native void blitBackground(int[] target, int w, int h, int vx, int vy, int vw, int vh,
            int[] img, int iw, int ih, int cropX, int cropY, int cropW, int cropH, int modeX, int modeY, int bg);
}
