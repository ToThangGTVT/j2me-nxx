package javax.microedition.m3g;

public class KeyframeSequence extends Object3D {
    public static final int CONSTANT = 192;
    public static final int LOOP = 193;
    public static final int LINEAR = 176;
    public static final int SLERP = 177;
    public static final int SPLINE = 178;
    public static final int SQUAD = 179;
    public static final int STEP = 180;

    private final int keyCount, comps, interp;
    private final int[] times;
    private final float[] values;
    private int repeat = CONSTANT;
    private int duration;
    private int first, last;

    public KeyframeSequence(int numKeyframes, int numComponents, int interpolation) {
        if (numKeyframes < 1 || numComponents < 1) {
            throw new IllegalArgumentException();
        }
        if (interpolation < LINEAR || interpolation > STEP) {
            throw new IllegalArgumentException();
        }
        keyCount = numKeyframes;
        comps = numComponents;
        interp = interpolation;
        times = new int[numKeyframes];
        values = new float[numKeyframes * numComponents];
        last = numKeyframes - 1;
    }

    Object3D duplicateImpl() {
        KeyframeSequence k = new KeyframeSequence(keyCount, comps, interp);
        System.arraycopy(times, 0, k.times, 0, keyCount);
        System.arraycopy(values, 0, k.values, 0, values.length);
        k.repeat = repeat;
        k.duration = duration;
        k.first = first;
        k.last = last;
        return k;
    }

    public int getComponentCount() { return comps; }
    public int getKeyframeCount() { return keyCount; }
    public int getInterpolationType() { return interp; }
    public int getRepeatMode() { return repeat; }
    public int getDuration() { return duration; }
    public int getValidRangeFirst() { return first; }
    public int getValidRangeLast() { return last; }

    public void setKeyframe(int index, int time, float[] value) {
        if (value == null) {
            throw new NullPointerException();
        }
        if (index < 0 || index >= keyCount) {
            throw new IndexOutOfBoundsException();
        }
        if (time < 0 || value.length < comps) {
            throw new IllegalArgumentException();
        }
        times[index] = time;
        System.arraycopy(value, 0, values, index * comps, comps);
    }

    public int getKeyframe(int index, float[] value) {
        if (index < 0 || index >= keyCount) {
            throw new IndexOutOfBoundsException();
        }
        if (value != null) {
            System.arraycopy(values, index * comps, value, 0, comps);
        }
        return times[index];
    }

    public void setValidRange(int first, int last) {
        if (first < 0 || first >= keyCount || last < 0 || last >= keyCount) {
            throw new IndexOutOfBoundsException();
        }
        this.first = first;
        this.last = last;
    }

    public void setDuration(int duration) {
        if (duration <= 0) {
            throw new IllegalArgumentException();
        }
        this.duration = duration;
    }

    public void setRepeatMode(int mode) {
        if (mode != CONSTANT && mode != LOOP) {
            throw new IllegalArgumentException();
        }
        repeat = mode;
    }

    // Lấy mẫu tại thời điểm t (theo thời gian của chuỗi)
    void sample(float t, float[] out) {
        int n = last >= first ? last - first + 1 : keyCount - first + last + 1;
        if (n <= 0) {
            return;
        }
        if (repeat == LOOP && duration > 0) {
            t = t % duration;
            if (t < 0) {
                t += duration;
            }
        }
        // Tìm khung bao quanh t trong khoảng hợp lệ
        int prevK = -1, nextK = -1;
        for (int i = 0; i < n; i++) {
            int k = (first + i) % keyCount;
            if (times[k] <= t) {
                prevK = k;
            } else {
                nextK = k;
                break;
            }
        }
        if (prevK < 0) {
            int k = repeat == LOOP && n > 1 ? (first + n - 1) % keyCount : first;
            if (repeat == LOOP && n > 1 && nextK >= 0) {
                interpolate(k, nextK, times[k] - duration, times[nextK], t, out);
            } else {
                copyKey(first, out);
            }
            return;
        }
        if (nextK < 0) {
            if (repeat == LOOP && n > 1 && duration > 0) {
                interpolate(prevK, first, times[prevK], times[first] + duration, t, out);
            } else {
                copyKey(prevK, out);
            }
            return;
        }
        interpolate(prevK, nextK, times[prevK], times[nextK], t, out);
    }

    private void copyKey(int k, float[] out) {
        System.arraycopy(values, k * comps, out, 0, Math.min(comps, out.length));
    }

    private void interpolate(int a, int b, float ta, float tb, float t, float[] out) {
        if (interp == STEP || tb <= ta) {
            copyKey(a, out);
            return;
        }
        float s = (t - ta) / (tb - ta);
        if ((interp == SLERP || interp == SQUAD) && comps == 4) {
            slerp(a, b, s, out);
            return;
        }
        for (int c = 0; c < comps && c < out.length; c++) {
            float va = values[a * comps + c], vb = values[b * comps + c];
            out[c] = va + (vb - va) * s;
        }
    }

    private void slerp(int a, int b, float s, float[] out) {
        float ax = values[a * 4], ay = values[a * 4 + 1], az = values[a * 4 + 2], aw = values[a * 4 + 3];
        float bx = values[b * 4], by = values[b * 4 + 1], bz = values[b * 4 + 2], bw = values[b * 4 + 3];
        float dot = ax * bx + ay * by + az * bz + aw * bw;
        if (dot < 0) {
            dot = -dot;
            bx = -bx;
            by = -by;
            bz = -bz;
            bw = -bw;
        }
        float ka, kb;
        if (dot > 0.9995f) {
            ka = 1 - s;
            kb = s;
        } else {
            double th = Math.acos(dot);
            double sn = Math.sin(th);
            ka = (float) (Math.sin((1 - s) * th) / sn);
            kb = (float) (Math.sin(s * th) / sn);
        }
        out[0] = ax * ka + bx * kb;
        out[1] = ay * ka + by * kb;
        out[2] = az * ka + bz * kb;
        out[3] = aw * ka + bw * kb;
    }
}
