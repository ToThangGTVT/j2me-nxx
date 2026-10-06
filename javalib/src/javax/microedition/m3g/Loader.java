package javax.microedition.m3g;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.Hashtable;
import java.util.Vector;
import javax.microedition.lcdui.Image;

// Đọc file .m3g (định dạng JSR-184) hoặc ảnh PNG
public class Loader {
    private static final byte[] ID = { (byte) 0xAB, 0x4A, 0x53, 0x52, 0x31, 0x38, 0x34, (byte) 0xBB, 0x0D, 0x0A,
            0x1A, 0x0A };

    private byte[] d;
    private int pos;
    private final Vector objects = new Vector();     // chỉ số 1 = object đầu tiên (header)
    private final Vector referenced = new Vector();
    private String baseName;

    private Loader() {
    }

    public static Object3D[] load(String name) throws IOException {
        if (name == null) {
            throw new NullPointerException();
        }
        byte[] data = readResource(name);
        Loader l = new Loader();
        l.baseName = name;
        return l.loadData(data, 0);
    }

    public static Object3D[] load(byte[] data, int offset) throws IOException {
        if (data == null) {
            throw new NullPointerException();
        }
        if (offset < 0 || offset >= data.length) {
            throw new IndexOutOfBoundsException();
        }
        return new Loader().loadData(data, offset);
    }

    private static byte[] readResource(String name) throws IOException {
        String res = name;
        if (res.startsWith("resource:")) {
            res = res.substring(9);
        }
        if (res.indexOf("://") >= 0 && !res.startsWith("file:")) {
            javax.microedition.io.InputConnection c =
                    (javax.microedition.io.InputConnection) javax.microedition.io.Connector.open(res);
            try {
                return readAll(c.openInputStream());
            } finally {
                c.close();
            }
        }
        InputStream in = Loader.class.getResourceAsStream(res.startsWith("/") ? res : "/" + res);
        if (in == null) {
            throw new IOException("Khong tim thay " + name);
        }
        return readAll(in);
    }

    private static byte[] readAll(InputStream in) throws IOException {
        ByteArrayOutputStream bo = new ByteArrayOutputStream();
        byte[] buf = new byte[4096];
        int n;
        while ((n = in.read(buf, 0, buf.length)) > 0) {
            bo.write(buf, 0, n);
        }
        in.close();
        return bo.toByteArray();
    }

    private Object3D[] loadData(byte[] data, int offset) throws IOException {
        boolean m3g = data.length - offset >= ID.length;
        for (int i = 0; m3g && i < ID.length; i++) {
            m3g = data[offset + i] == ID[i];
        }
        if (!m3g) {
            // Thử như ảnh (PNG / JPEG)
            try {
                Image img = Image.createImage(data, offset, data.length - offset);
                int[] px = new int[img.getWidth() * img.getHeight()];
                img.getRGB(px, 0, img.getWidth(), 0, 0, img.getWidth(), img.getHeight());
                boolean alpha = false;
                for (int i = 0; i < px.length && !alpha; i++) {
                    alpha = (px[i] >>> 24) != 255;
                }
                return new Object3D[] { new Image2D(alpha ? Image2D.RGBA : Image2D.RGB, img) };
            } catch (IllegalArgumentException e) {
                throw new IOException("Khong phai file M3G hoac anh");
            }
        }
        int p = offset + ID.length;
        while (p < data.length) {
            if (p + 9 > data.length) {
                break;
            }
            int scheme = data[p] & 0xff;
            int total = le32(data, p + 1);
            int uncompressed = le32(data, p + 5);
            if (total < 13 || p + total > data.length) {
                throw new IOException("M3G: section hong");
            }
            byte[] body;
            int bodyLen = total - 13;
            if (scheme == 0) {
                body = new byte[bodyLen];
                System.arraycopy(data, p + 9, body, 0, bodyLen);
            } else if (scheme == 1) {
                body = inflate0(data, p + 9, bodyLen, uncompressed);
            } else {
                throw new IOException("M3G: kieu nen khong ho tro " + scheme);
            }
            parseObjects(body);
            p += total;
        }

        // Object gốc: không bị object nào khác tham chiếu (bỏ header)
        Vector roots = new Vector();
        for (int i = 1; i < objects.size(); i++) {
            Object o = objects.elementAt(i);
            if (o instanceof Object3D && !referenced.contains(o)) {
                roots.addElement(o);
            }
        }
        Object3D[] r = new Object3D[roots.size()];
        roots.copyInto(r);
        return r;
    }

    private static int le32(byte[] b, int o) {
        return (b[o] & 0xff) | ((b[o + 1] & 0xff) << 8) | ((b[o + 2] & 0xff) << 16) | ((b[o + 3] & 0xff) << 24);
    }

    // --- Đọc kiểu dữ liệu (little-endian)

    private int u8() {
        return d[pos++] & 0xff;
    }

    private boolean bool() {
        return u8() != 0;
    }

    private int u16() {
        int v = (d[pos] & 0xff) | ((d[pos + 1] & 0xff) << 8);
        pos += 2;
        return v;
    }

    private short s16() {
        return (short) u16();
    }

    private int i32() {
        int v = le32(d, pos);
        pos += 4;
        return v;
    }

    private float f32() {
        return Float.intBitsToFloat(i32());
    }

    private String str() throws IOException {
        int start = pos;
        while (d[pos] != 0) {
            pos++;
        }
        String s = new String(d, start, pos - start, "UTF-8");
        pos++;
        return s;
    }

    private int rgb() {
        int r = u8(), g = u8(), b = u8();
        return (r << 16) | (g << 8) | b;
    }

    private int rgba() {
        int r = u8(), g = u8(), b = u8(), a = u8();
        return (a << 24) | (r << 16) | (g << 8) | b;
    }

    private byte[] bytes() {
        int n = i32();
        byte[] b = new byte[n];
        System.arraycopy(d, pos, b, 0, n);
        pos += n;
        return b;
    }

    private Object ref() throws IOException {
        int idx = i32();
        if (idx == 0) {
            return null;
        }
        if (idx < 1 || idx > objects.size()) {
            throw new IOException("M3G: tham chieu sai " + idx);
        }
        Object o = objects.elementAt(idx - 1);
        if (o instanceof Object3D && !referenced.contains(o)) {
            referenced.addElement(o);
        }
        return o;
    }

    // --- Object

    private void parseObjects(byte[] body) throws IOException {
        d = body;
        pos = 0;
        while (pos < d.length) {
            int type = u8();
            int len = i32();
            int end = pos + len;
            Object o;
            try {
                o = parseObject(type);
            } catch (ArrayIndexOutOfBoundsException e) {
                throw new IOException("M3G: object " + type + " hong");
            }
            objects.addElement(o == null ? new Object() : o);
            pos = end;
        }
    }

    private Object parseObject(int type) throws IOException {
        switch (type) {
        case 0: {   // header
            u8();
            u8();
            bool();
            i32();
            i32();
            str();
            return new Object();
        }
        case 1: {
            AnimationController c = new AnimationController();
            object3D(c);
            float speed = f32(), weight = f32();
            int start = i32(), end = i32();
            float refSeq = f32();
            int refWorld = i32();
            c.setWeight(weight);
            c.setActiveInterval(Math.min(start, end), Math.max(start, end));
            c.setPosition(refSeq, refWorld);
            c.setSpeed(speed, refWorld);
            return c;
        }
        case 2: {
            int base = pos;
            // Object3D đứng trước nhưng track cần sequence ngay khi tạo: đọc tạm rồi quay lại
            skipObject3D();
            KeyframeSequence ks = (KeyframeSequence) ref();
            AnimationController ac = (AnimationController) ref();
            int prop = i32();
            AnimationTrack t = new AnimationTrack(ks, prop);
            t.setController(ac);
            int after = pos;
            pos = base;
            object3D(t);
            pos = after;
            return t;
        }
        case 3: {
            Appearance a = new Appearance();
            object3D(a);
            a.layer = (byte) u8();
            a.compositing = (CompositingMode) ref();
            a.fog = (Fog) ref();
            a.polygon = (PolygonMode) ref();
            a.material = (Material) ref();
            int n = i32();
            for (int i = 0; i < n; i++) {
                Texture2D t = (Texture2D) ref();
                if (i < a.textures.length) {
                    a.textures[i] = t;
                }
            }
            return a;
        }
        case 4: {
            Background b = new Background();
            object3D(b);
            b.color = rgba();
            Image2D img = (Image2D) ref();
            if (img != null) {
                b.image = img;
            }
            b.modeX = u8();
            b.modeY = u8();
            b.cropX = i32();
            b.cropY = i32();
            b.cropW = i32();
            b.cropH = i32();
            b.depthClear = bool();
            b.colorClear = bool();
            return b;
        }
        case 5: {
            Camera c = new Camera();
            node(c);
            int pt = u8();
            if (pt == Camera.GENERIC) {
                Transform t = new Transform();
                for (int i = 0; i < 16; i++) {
                    t.m[i] = f32();
                }
                c.setGeneric(t);
            } else {
                c.type = pt;
                c.fovy = f32();
                c.aspect = f32();
                c.near = f32();
                c.far = f32();
            }
            return c;
        }
        case 6: {
            CompositingMode c = new CompositingMode();
            object3D(c);
            c.depthTest = bool();
            c.depthWrite = bool();
            c.colorWrite = bool();
            c.alphaWrite = bool();
            c.blending = u8();
            c.alphaThreshold = u8() / 255f;
            c.depthOffsetFactor = f32();
            c.depthOffsetUnits = f32();
            return c;
        }
        case 7: {
            Fog f = new Fog();
            object3D(f);
            f.color = rgb();
            f.mode = u8();
            if (f.mode == Fog.EXPONENTIAL) {
                f.density = f32();
            } else {
                f.near = f32();
                f.far = f32();
            }
            return f;
        }
        case 8: {
            PolygonMode p = new PolygonMode();
            object3D(p);
            p.culling = u8();
            p.shading = u8();
            p.winding = u8();
            p.twoSided = bool();
            p.localCamera = bool();
            p.perspective = bool();
            return p;
        }
        case 9: {
            Group g = new Group();
            group(g);
            return g;
        }
        case 10: {
            int base = pos;
            skipObject3D();
            int format = u8();
            boolean mutable = bool();
            int w = i32(), h = i32();
            Image2D img;
            if (mutable) {
                img = new Image2D(format, w, h);
            } else {
                byte[] palette = bytes();
                byte[] pixels = bytes();
                img = new Image2D(format, w, h, pixels, palette.length > 0 ? palette : null);
            }
            int after = pos;
            pos = base;
            object3D(img);
            pos = after;
            return img;
        }
        case 11: {
            int base = pos;
            skipObject3D();
            int enc = u8();
            int[] indices = null;
            int start = 0;
            switch (enc) {
            case 0: start = i32(); break;
            case 1: start = u8(); break;
            case 2: start = u16(); break;
            case 128: case 129: case 130: {
                int n = i32();
                indices = new int[n];
                for (int i = 0; i < n; i++) {
                    indices[i] = enc == 128 ? i32() : enc == 129 ? u8() : u16();
                }
                break;
            }
            default:
                throw new IOException("M3G: kieu index " + enc);
            }
            int sc = i32();
            int[] strips = new int[sc];
            for (int i = 0; i < sc; i++) {
                strips[i] = i32();
            }
            TriangleStripArray t = indices != null ? new TriangleStripArray(indices, strips)
                    : new TriangleStripArray(start, strips);
            int after = pos;
            pos = base;
            object3D(t);
            pos = after;
            return t;
        }
        case 12: {
            Light l = new Light();
            node(l);
            l.attConst = f32();
            l.attLinear = f32();
            l.attQuad = f32();
            l.color = rgb();
            l.mode = u8();
            l.intensity = f32();
            l.spotAngle = f32();
            l.spotExponent = f32();
            return l;
        }
        case 13: {
            Material m = new Material();
            object3D(m);
            m.ambient = rgb();
            m.diffuse = rgba();
            m.emissive = rgb();
            m.specular = rgb();
            m.shininess = f32();
            m.vertexColorTracking = bool();
            return m;
        }
        case 14: case 15: case 16:
            return mesh(type);
        case 17: {
            int base = pos;
            skipTransformable();
            Image2D img = (Image2D) ref();
            Texture2D t = new Texture2D(img);
            t.blendColor = rgb();
            t.blending = u8();
            t.wrapS = u8();
            t.wrapT = u8();
            t.levelFilter = u8();
            t.imageFilter = u8();
            int after = pos;
            pos = base;
            transformable(t);
            pos = after;
            return t;
        }
        case 18: {
            int base = pos;
            skipNode();
            Image2D img = (Image2D) ref();
            Appearance a = (Appearance) ref();
            boolean scaled = bool();
            Sprite3D s = new Sprite3D(scaled, img, a);
            s.setCrop(i32(), i32(), i32(), i32());
            int after = pos;
            pos = base;
            node(s);
            pos = after;
            return s;
        }
        case 19:
            return keyframes();
        case 20: {
            int base = pos;
            skipObject3D();
            int size = u8(), comps = u8(), enc = u8(), count = u16();
            VertexArray va = new VertexArray(count, comps, size);
            int n = count * comps;
            if (size == 1) {
                byte[] b = new byte[n];
                for (int i = 0; i < n; i++) {
                    b[i] = (byte) u8();
                    if (enc == 1 && i >= comps) {
                        b[i] = (byte) (b[i] + b[i - comps]);
                    }
                }
                va.set(0, count, b);
            } else {
                short[] s = new short[n];
                for (int i = 0; i < n; i++) {
                    s[i] = s16();
                    if (enc == 1 && i >= comps) {
                        s[i] = (short) (s[i] + s[i - comps]);
                    }
                }
                va.set(0, count, s);
            }
            int after = pos;
            pos = base;
            object3D(va);
            pos = after;
            return va;
        }
        case 21: {
            VertexBuffer vb = new VertexBuffer();
            object3D(vb);
            vb.defaultColor = rgba();
            VertexArray p = (VertexArray) ref();
            float[] bias = { f32(), f32(), f32() };
            float scale = f32();
            vb.positions = p;
            vb.posScale = scale;
            System.arraycopy(bias, 0, vb.posBias, 0, 3);
            vb.normals = (VertexArray) ref();
            vb.colors = (VertexArray) ref();
            int n = i32();
            for (int i = 0; i < n; i++) {
                VertexArray tc = (VertexArray) ref();
                float[] tb = { f32(), f32(), f32() };
                float ts = f32();
                if (i < vb.texCoords.length) {
                    vb.texCoords[i] = tc;
                    vb.tcScale[i] = ts;
                    System.arraycopy(tb, 0, vb.tcBias[i], 0, 3);
                }
            }
            return vb;
        }
        case 22: {
            World w = new World();
            group(w);
            Camera c = (Camera) ref();
            if (c != null) {
                w.activeCamera = c;
            }
            w.background = (Background) ref();
            return w;
        }
        case 0xff: {
            String uri = str();
            String target = uri;
            if (!uri.startsWith("/") && uri.indexOf(':') < 0 && baseName != null && baseName.lastIndexOf('/') >= 0) {
                target = baseName.substring(0, baseName.lastIndexOf('/') + 1) + uri;
            }
            Object3D[] ext = load(target);
            return ext.length > 0 ? ext[0] : null;
        }
        default:
            // Kiểu chưa biết: bỏ qua (giữ chỗ trong bảng chỉ số)
            return null;
        }
    }

    private void object3D(Object3D o) throws IOException {
        o.userID = i32();
        int tracks = i32();
        for (int i = 0; i < tracks; i++) {
            AnimationTrack t = (AnimationTrack) ref();
            if (t != null) {
                o.tracks.addElement(t);
            }
        }
        int params = i32();
        if (params > 0) {
            Hashtable h = new Hashtable();
            for (int i = 0; i < params; i++) {
                int id = i32();
                h.put(new Integer(id), bytes());
            }
            o.userObject = h;
        }
    }

    private void skipObject3D() {
        i32();
        int tracks = i32();
        pos += tracks * 4;
        int params = i32();
        for (int i = 0; i < params; i++) {
            i32();
            int n = i32();
            pos += n;
        }
    }

    private void transformable(Transformable t) throws IOException {
        object3D(t);
        if (bool()) {
            t.tx = f32();
            t.ty = f32();
            t.tz = f32();
            t.sx = f32();
            t.sy = f32();
            t.sz = f32();
            float angle = f32();
            float ax = f32(), ay = f32(), az = f32();
            t.setOrientation(angle, ax, ay, az);
        }
        if (bool()) {
            Transform m = new Transform();
            for (int i = 0; i < 16; i++) {
                m.m[i] = f32();
            }
            t.matrix = m;
        }
    }

    private void skipTransformable() {
        skipObject3D();
        if (bool()) {
            pos += 10 * 4;
        }
        if (bool()) {
            pos += 16 * 4;
        }
    }

    private void node(Node n) throws IOException {
        transformable(n);
        n.renderingEnabled = bool();
        n.pickingEnabled = bool();
        n.alphaFactor = u8() / 255f;
        n.scope = i32();
        if (bool()) {
            n.zTarget = u8();
            n.yTarget = u8();
            n.zRef = (Node) ref();
            n.yRef = (Node) ref();
        }
    }

    private void skipNode() {
        skipTransformable();
        pos += 1 + 1 + 1 + 4;
        if (bool()) {
            pos += 1 + 1 + 4 + 4;
        }
    }

    private void group(Group g) throws IOException {
        node(g);
        int n = i32();
        for (int i = 0; i < n; i++) {
            Node c = (Node) ref();
            if (c != null && c.parent == null) {
                g.addChild(c);
            }
        }
    }

    private Mesh mesh(int type) throws IOException {
        int base = pos;
        skipNode();
        VertexBuffer vb = (VertexBuffer) ref();
        int n = i32();
        IndexBuffer[] ib = new IndexBuffer[n];
        Appearance[] ap = new Appearance[n];
        for (int i = 0; i < n; i++) {
            ib[i] = (IndexBuffer) ref();
            ap[i] = (Appearance) ref();
        }
        Mesh m;
        if (type == 15) {
            int tc = i32();
            VertexBuffer[] targets = new VertexBuffer[tc];
            float[] weights = new float[tc];
            for (int i = 0; i < tc; i++) {
                targets[i] = (VertexBuffer) ref();
                weights[i] = f32();
            }
            MorphingMesh mm = new MorphingMesh(vb, targets, ib, ap);
            mm.setWeights(weights);
            m = mm;
        } else if (type == 16) {
            Group skeleton = (Group) ref();
            SkinnedMesh sm = new SkinnedMesh(vb, ib, ap, skeleton);
            int tc = i32();
            for (int i = 0; i < tc; i++) {
                Node bone = (Node) ref();
                int first = i32(), count = i32(), weight = i32();
                if (bone != null && weight > 0 && count > 0) {
                    sm.addTransform(bone, weight, first, count);
                }
            }
            m = sm;
        } else {
            m = new Mesh(vb, ib, ap);
        }
        int after = pos;
        pos = base;
        node(m);
        pos = after;
        return m;
    }

    private KeyframeSequence keyframes() throws IOException {
        int base = pos;
        skipObject3D();
        int interp = u8(), repeat = u8(), enc = u8();
        int duration = i32(), first = i32(), last = i32();
        int comps = i32(), count = i32();
        KeyframeSequence ks = new KeyframeSequence(count, comps, interp);
        float[] bias = new float[comps], scale = new float[comps];
        if (enc != 0) {
            for (int i = 0; i < comps; i++) {
                bias[i] = f32();
            }
            for (int i = 0; i < comps; i++) {
                scale[i] = f32();
            }
        }
        float[] v = new float[comps];
        for (int k = 0; k < count; k++) {
            int time = i32();
            for (int c = 0; c < comps; c++) {
                if (enc == 0) {
                    v[c] = f32();
                } else if (enc == 1) {
                    v[c] = bias[c] + scale[c] * (u8() / 255f);
                } else {
                    v[c] = bias[c] + scale[c] * (u16() / 65535f);
                }
            }
            ks.setKeyframe(k, time, v);
        }
        if (duration > 0) {
            ks.setDuration(duration);
        }
        ks.setRepeatMode(repeat == KeyframeSequence.LOOP ? KeyframeSequence.LOOP : KeyframeSequence.CONSTANT);
        if (first < count && last < count) {
            ks.setValidRange(first, last);
        }
        int after = pos;
        pos = base;
        object3D(ks);
        pos = after;
        return ks;
    }

    private static native byte[] inflate0(byte[] data, int off, int len, int outLen) throws IOException;
}
