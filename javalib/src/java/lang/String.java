package java.lang;

import j2menx.Codec;
import java.io.UnsupportedEncodingException;

public final class String implements CharSequence, Comparable {
    // VM truy cập trực tiếp 3 field đầu, không đổi tên
    char[] value;
    int offset;
    int count;
    private int hash;

    public String() {
        value = new char[0];
    }

    public String(String s) {
        value = s.value;
        offset = s.offset;
        count = s.count;
    }

    public String(char[] v) {
        this(v, 0, v.length);
    }

    public String(char[] v, int off, int len) {
        if (off < 0 || len < 0 || off > v.length - len) {
            throw new StringIndexOutOfBoundsException();
        }
        value = new char[len];
        System.arraycopy(v, off, value, 0, len);
        count = len;
    }

    // Dùng nội bộ: không chép mảng
    String(int off, int len, char[] v) {
        value = v;
        offset = off;
        count = len;
    }

    public String(byte[] b) {
        this(b, 0, b.length);
    }

    public String(byte[] b, int off, int len) {
        checkBounds(b, off, len);
        value = Codec.decode(b, off, len, Codec.getDefault());
        count = value.length;
    }

    public String(byte[] b, String enc) throws UnsupportedEncodingException {
        this(b, 0, b.length, enc);
    }

    public String(byte[] b, int off, int len, String enc) throws UnsupportedEncodingException {
        checkBounds(b, off, len);
        value = Codec.decode(b, off, len, Codec.lookup(enc));
        count = value.length;
    }

    public String(StringBuffer sb) {
        String s = sb.toString();
        value = s.value;
        offset = s.offset;
        count = s.count;
    }

    private static void checkBounds(byte[] b, int off, int len) {
        if (off < 0 || len < 0 || off > b.length - len) {
            throw new StringIndexOutOfBoundsException();
        }
    }

    public int length() {
        return count;
    }

    public char charAt(int index) {
        if (index < 0 || index >= count) {
            throw new StringIndexOutOfBoundsException(index);
        }
        return value[offset + index];
    }

    public void getChars(int srcBegin, int srcEnd, char[] dst, int dstBegin) {
        if (srcBegin < 0 || srcEnd > count || srcBegin > srcEnd) {
            throw new StringIndexOutOfBoundsException();
        }
        System.arraycopy(value, offset + srcBegin, dst, dstBegin, srcEnd - srcBegin);
    }

    public byte[] getBytes() {
        return Codec.encode(value, offset, count, Codec.getDefault());
    }

    public byte[] getBytes(String enc) throws UnsupportedEncodingException {
        return Codec.encode(value, offset, count, Codec.lookup(enc));
    }

    public boolean equals(Object o) {
        if (this == o) {
            return true;
        }
        if (!(o instanceof String)) {
            return false;
        }
        String s = (String) o;
        if (s.count != count) {
            return false;
        }
        char[] a = value, b = s.value;
        int i = offset, j = s.offset;
        for (int n = count; n > 0; n--) {
            if (a[i++] != b[j++]) {
                return false;
            }
        }
        return true;
    }

    public boolean equalsIgnoreCase(String s) {
        return s != null && s.count == count && regionMatches(true, 0, s, 0, count);
    }

    public int compareTo(String s) {
        int n = Math.min(count, s.count);
        for (int k = 0; k < n; k++) {
            char a = value[offset + k], b = s.value[s.offset + k];
            if (a != b) {
                return a - b;
            }
        }
        return count - s.count;
    }

    public int compareTo(Object o) {
        return compareTo((String) o);
    }

    public boolean regionMatches(boolean ignoreCase, int toffset, String other, int ooffset, int len) {
        if (ooffset < 0 || toffset < 0 || toffset > (long) count - len || ooffset > (long) other.count - len) {
            return false;
        }
        for (int i = 0; i < len; i++) {
            char a = value[offset + toffset + i];
            char b = other.value[other.offset + ooffset + i];
            if (a == b) {
                continue;
            }
            if (ignoreCase && (Character.toUpperCase(a) == Character.toUpperCase(b)
                    || Character.toLowerCase(a) == Character.toLowerCase(b))) {
                continue;
            }
            return false;
        }
        return true;
    }

    public boolean regionMatches(int toffset, String other, int ooffset, int len) {
        return regionMatches(false, toffset, other, ooffset, len);
    }

    public boolean startsWith(String prefix, int toffset) {
        return regionMatches(false, toffset, prefix, 0, prefix.count);
    }

    public boolean startsWith(String prefix) {
        return startsWith(prefix, 0);
    }

    public boolean endsWith(String suffix) {
        return startsWith(suffix, count - suffix.count);
    }

    public int hashCode() {
        int h = hash;
        if (h == 0 && count > 0) {
            for (int i = 0; i < count; i++) {
                h = 31 * h + value[offset + i];
            }
            hash = h;
        }
        return h;
    }

    public int indexOf(int ch) {
        return indexOf(ch, 0);
    }

    public int indexOf(int ch, int from) {
        if (from < 0) {
            from = 0;
        }
        for (int i = from; i < count; i++) {
            if (value[offset + i] == ch) {
                return i;
            }
        }
        return -1;
    }

    public int lastIndexOf(int ch) {
        return lastIndexOf(ch, count - 1);
    }

    public int lastIndexOf(int ch, int from) {
        if (from >= count) {
            from = count - 1;
        }
        for (int i = from; i >= 0; i--) {
            if (value[offset + i] == ch) {
                return i;
            }
        }
        return -1;
    }

    public int indexOf(String s) {
        return indexOf(s, 0);
    }

    public int indexOf(String s, int from) {
        if (from < 0) {
            from = 0;
        }
        int max = count - s.count;
        for (int i = from; i <= max; i++) {
            if (regionMatches(false, i, s, 0, s.count)) {
                return i;
            }
        }
        return -1;
    }

    public int lastIndexOf(String s) {
        return lastIndexOf(s, count);
    }

    public int lastIndexOf(String s, int from) {
        int i = Math.min(from, count - s.count);
        for (; i >= 0; i--) {
            if (regionMatches(false, i, s, 0, s.count)) {
                return i;
            }
        }
        return -1;
    }

    public boolean contains(CharSequence s) {
        return indexOf(s.toString()) >= 0;
    }

    public String substring(int begin) {
        return substring(begin, count);
    }

    public String substring(int begin, int end) {
        if (begin < 0 || end > count || begin > end) {
            throw new StringIndexOutOfBoundsException("begin " + begin + ", end " + end + ", length " + count);
        }
        if (begin == 0 && end == count) {
            return this;
        }
        return new String(offset + begin, end - begin, value);
    }

    public String concat(String s) {
        if (s.count == 0) {
            return this;
        }
        char[] v = new char[count + s.count];
        System.arraycopy(value, offset, v, 0, count);
        System.arraycopy(s.value, s.offset, v, count, s.count);
        return new String(0, v.length, v);
    }

    public String replace(char oldChar, char newChar) {
        if (oldChar == newChar || indexOf(oldChar) < 0) {
            return this;
        }
        char[] v = new char[count];
        for (int i = 0; i < count; i++) {
            char c = value[offset + i];
            v[i] = c == oldChar ? newChar : c;
        }
        return new String(0, count, v);
    }

    public String toLowerCase() {
        char[] v = new char[count];
        boolean changed = false;
        for (int i = 0; i < count; i++) {
            char c = value[offset + i];
            v[i] = Character.toLowerCase(c);
            changed |= v[i] != c;
        }
        return changed ? new String(0, count, v) : this;
    }

    public String toUpperCase() {
        char[] v = new char[count];
        boolean changed = false;
        for (int i = 0; i < count; i++) {
            char c = value[offset + i];
            v[i] = Character.toUpperCase(c);
            changed |= v[i] != c;
        }
        return changed ? new String(0, count, v) : this;
    }

    public String trim() {
        int st = 0, len = count;
        while (st < len && value[offset + st] <= ' ') {
            st++;
        }
        while (st < len && value[offset + len - 1] <= ' ') {
            len--;
        }
        return (st > 0 || len < count) ? substring(st, len) : this;
    }

    public String toString() {
        return this;
    }

    public char[] toCharArray() {
        char[] r = new char[count];
        System.arraycopy(value, offset, r, 0, count);
        return r;
    }

    public native String intern();

    public static String valueOf(Object o) {
        return o == null ? "null" : o.toString();
    }

    public static String valueOf(char[] data) {
        return new String(data);
    }

    public static String valueOf(char[] data, int off, int count) {
        return new String(data, off, count);
    }

    public static String copyValueOf(char[] data) {
        return new String(data);
    }

    public static String valueOf(boolean b) {
        return b ? "true" : "false";
    }

    public static String valueOf(char c) {
        char[] v = { c };
        return new String(0, 1, v);
    }

    public static String valueOf(int i) {
        return Integer.toString(i);
    }

    public static String valueOf(long l) {
        return Long.toString(l);
    }

    public static String valueOf(float f) {
        return Float.toString(f);
    }

    public static String valueOf(double d) {
        return Double.toString(d);
    }
}
