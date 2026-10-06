package javax.microedition.m3g;

import java.util.Vector;

public abstract class Object3D {
    int userID;
    Object userObject;
    final Vector tracks = new Vector();
    // Đánh dấu khi duyệt đồ thị (tránh lặp vô hạn)
    int visitMark;
    static int markCounter;

    Object3D() {
    }

    public final int getUserID() {
        return userID;
    }

    public final void setUserID(int userID) {
        this.userID = userID;
    }

    public final Object getUserObject() {
        return userObject;
    }

    public final void setUserObject(Object userObject) {
        this.userObject = userObject;
    }

    public void addAnimationTrack(AnimationTrack track) {
        if (track == null) {
            throw new NullPointerException();
        }
        if (tracks.contains(track)) {
            throw new IllegalArgumentException();
        }
        tracks.addElement(track);
    }

    public void removeAnimationTrack(AnimationTrack track) {
        tracks.removeElement(track);
    }

    public AnimationTrack getAnimationTrack(int index) {
        return (AnimationTrack) tracks.elementAt(index);
    }

    public int getAnimationTrackCount() {
        return tracks.size();
    }

    // Các object con (để duyệt animate / find)
    int getReferencesImpl(Object3D[] out) {
        int n = 0;
        for (int i = 0; i < tracks.size(); i++) {
            n = addRef(out, n, (Object3D) tracks.elementAt(i));
        }
        return n;
    }

    static int addRef(Object3D[] out, int n, Object3D o) {
        if (o == null) {
            return n;
        }
        if (out != null && n < out.length) {
            out[n] = o;
        }
        return n + 1;
    }

    public int getReferences(Object3D[] references) {
        int n = getReferencesImpl(null);
        if (references != null) {
            if (references.length < n) {
                throw new IllegalArgumentException();
            }
            getReferencesImpl(references);
        }
        return n;
    }

    public Object3D find(int id) {
        Object3D r = findImpl(id, ++markCounter);
        return r;
    }

    Object3D findImpl(int id, int mark) {
        if (visitMark == mark) {
            return null;
        }
        visitMark = mark;
        if (userID == id) {
            return this;
        }
        Object3D[] refs = new Object3D[getReferencesImpl(null)];
        getReferencesImpl(refs);
        for (int i = 0; i < refs.length; i++) {
            Object3D r = refs[i].findImpl(id, mark);
            if (r != null) {
                return r;
            }
        }
        return null;
    }

    // Trả về số ms tới lần animate có ý nghĩa tiếp theo (luôn 0: cứ gọi mỗi frame)
    public final int animate(int time) {
        animateImpl(time, ++markCounter);
        return 0;
    }

    void animateImpl(int time, int mark) {
        if (visitMark == mark) {
            return;
        }
        visitMark = mark;
        applyTracks(time);
        Object3D[] refs = new Object3D[getReferencesImpl(null)];
        getReferencesImpl(refs);
        for (int i = 0; i < refs.length; i++) {
            refs[i].animateImpl(time, mark);
        }
    }

    // Gộp các track cùng thuộc tính theo trọng số rồi áp dụng
    private void applyTracks(int time) {
        int n = tracks.size();
        if (n == 0) {
            return;
        }
        boolean[] done = new boolean[n];
        for (int i = 0; i < n; i++) {
            if (done[i]) {
                continue;
            }
            AnimationTrack t = (AnimationTrack) tracks.elementAt(i);
            int prop = t.getTargetProperty();
            int comps = t.getKeyframeSequence().getComponentCount();
            float[] acc = new float[comps];
            float[] tmp = new float[comps];
            float totalWeight = 0;
            for (int k = i; k < n; k++) {
                AnimationTrack tk = (AnimationTrack) tracks.elementAt(k);
                if (tk.getTargetProperty() != prop || tk.getKeyframeSequence().getComponentCount() != comps) {
                    continue;
                }
                done[k] = true;
                float w = tk.activeWeight(time);
                if (w <= 0) {
                    continue;
                }
                tk.sample(time, tmp);
                // Quaternion: đảo dấu nếu ngược hướng để cộng có trọng số không bị triệt tiêu
                if (prop == AnimationTrack.ORIENTATION && comps == 4 && totalWeight > 0
                        && acc[0] * tmp[0] + acc[1] * tmp[1] + acc[2] * tmp[2] + acc[3] * tmp[3] < 0) {
                    w = -w;
                }
                for (int c = 0; c < comps; c++) {
                    acc[c] += tmp[c] * w;
                }
                totalWeight += Math.abs(w);
            }
            if (totalWeight > 0) {
                if (prop == AnimationTrack.ORIENTATION && comps == 4) {
                    float l = (float) Math.sqrt(acc[0] * acc[0] + acc[1] * acc[1] + acc[2] * acc[2] + acc[3] * acc[3]);
                    if (l > 1e-6f) {
                        for (int c = 0; c < 4; c++) {
                            acc[c] /= l;
                        }
                    }
                }
                applyAnimation(prop, acc);
            }
        }
    }

    // Lớp con ghi đè để nhận giá trị animation
    void applyAnimation(int property, float[] value) {
    }

    // Không hỗ trợ animate thuộc tính này
    boolean animatable(int property) {
        return false;
    }

    public final Object3D duplicate() {
        Object3D o = duplicateImpl();
        o.userID = userID;
        o.userObject = userObject;
        for (int i = 0; i < tracks.size(); i++) {
            o.tracks.addElement(tracks.elementAt(i));
        }
        return o;
    }

    abstract Object3D duplicateImpl();

    static int clampByte(float v) {
        int i = (int) (v * 255 + 0.5f);
        return i < 0 ? 0 : i > 255 ? 255 : i;
    }

    static int toRGB(float[] v) {
        return (clampByte(v[0]) << 16) | (clampByte(v[1]) << 8) | clampByte(v[2]);
    }

    static int toARGB(float[] v, int oldAlpha) {
        int a = v.length > 3 ? clampByte(v[3]) : oldAlpha;
        return (a << 24) | toRGB(v);
    }
}
