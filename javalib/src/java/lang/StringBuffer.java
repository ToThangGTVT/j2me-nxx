package java.lang;

public final class StringBuffer implements CharSequence {
    private char[] value;
    private int count;

    public StringBuffer() {
        this(16);
    }

    public StringBuffer(int capacity) {
        if (capacity < 0) {
            throw new NegativeArraySizeException();
        }
        value = new char[capacity];
    }

    public StringBuffer(String s) {
        this(s.length() + 16);
        append(s);
    }

    public int length() {
        return count;
    }

    public int capacity() {
        return value.length;
    }

    public void ensureCapacity(int min) {
        if (min > value.length) {
            int n = value.length * 2 + 2;
            if (n < min) {
                n = min;
            }
            char[] v = new char[n];
            System.arraycopy(value, 0, v, 0, count);
            value = v;
        }
    }

    public void setLength(int len) {
        if (len < 0) {
            throw new StringIndexOutOfBoundsException(len);
        }
        ensureCapacity(len);
        for (int i = count; i < len; i++) {
            value[i] = 0;
        }
        count = len;
    }

    public char charAt(int i) {
        if (i < 0 || i >= count) {
            throw new StringIndexOutOfBoundsException(i);
        }
        return value[i];
    }

    public void getChars(int srcBegin, int srcEnd, char[] dst, int dstBegin) {
        if (srcBegin < 0 || srcEnd > count || srcBegin > srcEnd) {
            throw new StringIndexOutOfBoundsException();
        }
        System.arraycopy(value, srcBegin, dst, dstBegin, srcEnd - srcBegin);
    }

    public void setCharAt(int i, char c) {
        if (i < 0 || i >= count) {
            throw new StringIndexOutOfBoundsException(i);
        }
        value[i] = c;
    }

    public StringBuffer append(Object o) {
        return append(String.valueOf(o));
    }

    public StringBuffer append(String s) {
        if (s == null) {
            s = "null";
        }
        int len = s.count;
        ensureCapacity(count + len);
        System.arraycopy(s.value, s.offset, value, count, len);
        count += len;
        return this;
    }

    public StringBuffer append(StringBuffer sb) {
        return append(sb == null ? "null" : sb.toString());
    }

    public StringBuffer append(char[] s) {
        return append(s, 0, s.length);
    }

    public StringBuffer append(char[] s, int off, int len) {
        ensureCapacity(count + len);
        System.arraycopy(s, off, value, count, len);
        count += len;
        return this;
    }

    public StringBuffer append(boolean b) {
        return append(b ? "true" : "false");
    }

    public StringBuffer append(char c) {
        ensureCapacity(count + 1);
        value[count++] = c;
        return this;
    }

    public StringBuffer append(int i) {
        return append(Integer.toString(i));
    }

    public StringBuffer append(long l) {
        return append(Long.toString(l));
    }

    public StringBuffer append(float f) {
        return append(Float.toString(f));
    }

    public StringBuffer append(double d) {
        return append(Double.toString(d));
    }

    public StringBuffer delete(int start, int end) {
        if (end > count) {
            end = count;
        }
        if (start < 0 || start > end) {
            throw new StringIndexOutOfBoundsException();
        }
        int len = end - start;
        if (len > 0) {
            System.arraycopy(value, end, value, start, count - end);
            count -= len;
        }
        return this;
    }

    public StringBuffer deleteCharAt(int i) {
        if (i < 0 || i >= count) {
            throw new StringIndexOutOfBoundsException(i);
        }
        System.arraycopy(value, i + 1, value, i, count - i - 1);
        count--;
        return this;
    }

    public StringBuffer replace(int start, int end, String s) {
        delete(start, end);
        return insert(start, s);
    }

    public StringBuffer insert(int off, String s) {
        if (off < 0 || off > count) {
            throw new StringIndexOutOfBoundsException();
        }
        if (s == null) {
            s = "null";
        }
        int len = s.length();
        ensureCapacity(count + len);
        System.arraycopy(value, off, value, off + len, count - off);
        s.getChars(0, len, value, off);
        count += len;
        return this;
    }

    public StringBuffer insert(int off, Object o) {
        return insert(off, String.valueOf(o));
    }

    public StringBuffer insert(int off, char[] s) {
        return insert(off, new String(s));
    }

    public StringBuffer insert(int off, boolean b) {
        return insert(off, String.valueOf(b));
    }

    public StringBuffer insert(int off, char c) {
        return insert(off, String.valueOf(c));
    }

    public StringBuffer insert(int off, int i) {
        return insert(off, String.valueOf(i));
    }

    public StringBuffer insert(int off, long l) {
        return insert(off, String.valueOf(l));
    }

    public StringBuffer insert(int off, float f) {
        return insert(off, String.valueOf(f));
    }

    public StringBuffer insert(int off, double d) {
        return insert(off, String.valueOf(d));
    }

    public StringBuffer reverse() {
        for (int i = 0, j = count - 1; i < j; i++, j--) {
            char c = value[i];
            value[i] = value[j];
            value[j] = c;
        }
        return this;
    }

    public int indexOf(String s) {
        return toString().indexOf(s);
    }

    public String substring(int start) {
        return substring(start, count);
    }

    public String substring(int start, int end) {
        if (start < 0 || end > count || start > end) {
            throw new StringIndexOutOfBoundsException();
        }
        return new String(value, start, end - start);
    }

    public String toString() {
        return new String(value, 0, count);
    }
}
