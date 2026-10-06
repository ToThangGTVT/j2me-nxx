package java.lang;

public final class Integer {
    public static final int MIN_VALUE = 0x80000000;
    public static final int MAX_VALUE = 0x7fffffff;

    private static final char[] DIGITS = "0123456789abcdefghijklmnopqrstuvwxyz".toCharArray();

    private final int value;

    public Integer(int value) {
        this.value = value;
    }

    public static Integer valueOf(int i) {
        return new Integer(i);
    }

    public static Integer valueOf(String s) throws NumberFormatException {
        return new Integer(parseInt(s, 10));
    }

    public static Integer valueOf(String s, int radix) throws NumberFormatException {
        return new Integer(parseInt(s, radix));
    }

    public static int parseInt(String s) throws NumberFormatException {
        return parseInt(s, 10);
    }

    public static int parseInt(String s, int radix) throws NumberFormatException {
        long v = Long.parseLong(s, radix);
        if (v < MIN_VALUE || v > MAX_VALUE) {
            throw new NumberFormatException(s);
        }
        return (int) v;
    }

    public static String toString(int i) {
        return toString(i, 10);
    }

    public static String toString(int i, int radix) {
        if (radix < 2 || radix > 36) {
            radix = 10;
        }
        if (i == 0) {
            return "0";
        }
        char[] buf = new char[33];
        int pos = 32;
        boolean neg = i < 0;
        if (!neg) {
            i = -i;
        }
        while (i <= -radix) {
            buf[pos--] = DIGITS[-(i % radix)];
            i = i / radix;
        }
        buf[pos] = DIGITS[-i];
        if (neg) {
            buf[--pos] = '-';
        }
        return new String(buf, pos, 33 - pos);
    }

    private static String toUnsigned(int i, int shift) {
        char[] buf = new char[32];
        int pos = 32;
        int radix = 1 << shift;
        int mask = radix - 1;
        do {
            buf[--pos] = DIGITS[i & mask];
            i >>>= shift;
        } while (i != 0);
        return new String(buf, pos, 32 - pos);
    }

    public static String toHexString(int i) {
        return toUnsigned(i, 4);
    }

    public static String toOctalString(int i) {
        return toUnsigned(i, 3);
    }

    public static String toBinaryString(int i) {
        return toUnsigned(i, 1);
    }

    public byte byteValue() { return (byte) value; }
    public short shortValue() { return (short) value; }
    public int intValue() { return value; }
    public long longValue() { return value; }
    public float floatValue() { return value; }
    public double doubleValue() { return value; }

    public String toString() {
        return toString(value);
    }

    public int hashCode() {
        return value;
    }

    public boolean equals(Object o) {
        return o instanceof Integer && ((Integer) o).value == value;
    }
}
