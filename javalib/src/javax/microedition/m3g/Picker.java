package javax.microedition.m3g;

// Tìm giao điểm tia với các Mesh trong cây (Group.pick)
final class Picker {
    private Picker() {
    }

    static boolean pick(Group group, int scope, float[] org, float[] dir, RayIntersection ri) {
        float[] best = { Float.MAX_VALUE };
        Object[] hit = new Object[1];
        int[] sub = new int[1];
        float[] nrm = new float[3];
        visit(group, group, scope, org, dir, best, hit, sub, nrm);
        if (hit[0] == null) {
            return false;
        }
        if (ri != null) {
            ri.node = (Node) hit[0];
            ri.submesh = sub[0];
            float len = (float) Math.sqrt(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
            ri.distance = best[0];
            ri.ray[0] = org[0];
            ri.ray[1] = org[1];
            ri.ray[2] = org[2];
            ri.ray[3] = dir[0] / len;
            ri.ray[4] = dir[1] / len;
            ri.ray[5] = dir[2] / len;
            System.arraycopy(nrm, 0, ri.normal, 0, 3);
        }
        return true;
    }

    private static void visit(Group root, Node n, int scope, float[] org, float[] dir, float[] best, Object[] hit,
            int[] sub, float[] nrm) {
        if (!n.pickingEnabled || (n.scope & scope) == 0) {
            return;
        }
        if (n instanceof Group) {
            Group g = (Group) n;
            for (int i = 0; i < g.children.size(); i++) {
                visit(root, (Node) g.children.elementAt(i), scope, org, dir, best, hit, sub, nrm);
            }
        } else if (n instanceof Mesh) {
            Mesh m = (Mesh) n;
            Transform t = new Transform();
            if (!root.getTransformTo(m, t)) {
                return;
            }
            // Đưa tia vào không gian của mesh
            float[] o = { org[0], org[1], org[2], 1, dir[0], dir[1], dir[2], 0 };
            t.transform(o);
            VertexBuffer vb = m.renderVertices();
            for (int s = 0; s < m.submeshes.length; s++) {
                int[] tri = m.submeshes[s].triangles;
                for (int i = 0; i + 2 < tri.length; i += 3) {
                    float d = intersect(vb, tri[i], tri[i + 1], tri[i + 2], o, nrm);
                    if (d >= 0 && d < best[0]) {
                        best[0] = d;
                        hit[0] = m;
                        sub[0] = s;
                    }
                }
            }
        }
    }

    private static void vtx(VertexBuffer vb, int i, float[] out) {
        if (vb.floatPositions != null) {
            out[0] = vb.floatPositions[i * 3];
            out[1] = vb.floatPositions[i * 3 + 1];
            out[2] = vb.floatPositions[i * 3 + 2];
            return;
        }
        VertexArray p = vb.positions;
        for (int c = 0; c < 3; c++) {
            out[c] = p.comp(i * 3 + c) * vb.posScale + vb.posBias[c];
        }
    }

    // Möller–Trumbore; trả về khoảng cách theo tham số tia (đơn vị độ dài dir), -1 nếu không cắt
    private static float intersect(VertexBuffer vb, int i0, int i1, int i2, float[] ray, float[] nrm) {
        if (vb.positions == null && vb.floatPositions == null) {
            return -1;
        }
        float[] a = new float[3], b = new float[3], c = new float[3];
        vtx(vb, i0, a);
        vtx(vb, i1, b);
        vtx(vb, i2, c);
        float e1x = b[0] - a[0], e1y = b[1] - a[1], e1z = b[2] - a[2];
        float e2x = c[0] - a[0], e2y = c[1] - a[1], e2z = c[2] - a[2];
        float dx = ray[4], dy = ray[5], dz = ray[6];
        float px = dy * e2z - dz * e2y, py = dz * e2x - dx * e2z, pz = dx * e2y - dy * e2x;
        float det = e1x * px + e1y * py + e1z * pz;
        if (Math.abs(det) < 1e-9f) {
            return -1;
        }
        float inv = 1 / det;
        float tx = ray[0] - a[0], ty = ray[1] - a[1], tz = ray[2] - a[2];
        float u = (tx * px + ty * py + tz * pz) * inv;
        if (u < 0 || u > 1) {
            return -1;
        }
        float qx = ty * e1z - tz * e1y, qy = tz * e1x - tx * e1z, qz = tx * e1y - ty * e1x;
        float v = (dx * qx + dy * qy + dz * qz) * inv;
        if (v < 0 || u + v > 1) {
            return -1;
        }
        float t = (e2x * qx + e2y * qy + e2z * qz) * inv;
        if (t < 0) {
            return -1;
        }
        float nx = e1y * e2z - e1z * e2y, ny = e1z * e2x - e1x * e2z, nz = e1x * e2y - e1y * e2x;
        float l = (float) Math.sqrt(nx * nx + ny * ny + nz * nz);
        if (l > 0) {
            nrm[0] = nx / l;
            nrm[1] = ny / l;
            nrm[2] = nz / l;
        }
        return t;
    }
}
