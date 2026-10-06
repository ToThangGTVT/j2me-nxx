package java.lang;

public final class Long {
    public static final long MIN_VALUE = 0x8000000000000000L;
    public static final long MAX_VALUE = 0x7fffffffffffffffL;

    private final long value;

    public Long(long value) {
        this.value = value;
    }

    public static Long valueOf(long l) {
        return new Long(l);
    }

    public static long parseLong(String s) throws NumberFormatException {
        return parseLong(s, 10);
    }

    public static long parseLong(String s, int radix) throws NumberFormatException {
        if (s == null || radix < 2 || radix > 36) {
            throw new NumberFormatException(s);
        }
        int len = s.length();
        int i = 0;
        boolean neg = false;
        if (len > 0 && (s.charAt(0) == '-' || s.charAt(0) == '+')) {
            neg = s.charAt(0) == '-';
            i = 1;
        }
        if (i >= len) {
            throw new NumberFormatException(s);
        }
        long limit = neg ? MIN_VALUE : -MAX_VALUE;
        long multmin = limit / radix;
        long result = 0;
        for (; i < len; i++) {
            int d = Character.digit(s.charAt(i), radix);
            if (d < 0 || result < multmin) {
                throw new NumberFormatException(s);
            }
            result *= radix;
            if (result < limit + d) {
                throw new NumberFormatException(s);
            }
            result -= d;
        }
        return neg ? result : -result;
    }

    public static String toString(long l) {
        return toString(l, 10);
    }

    public static String toString(long l, int radix) {
        if (radix < 2 || radix > 36) {
            radix = 10;
        }
        if (l == 0) {
            return "0";
        }
        char[] buf = new char[65];
        int pos = 64;
        boolean neg = l < 0;
        if (!neg) {
            l = -l;
        }
        while (l <= -radix) {
            buf[pos--] = Character.forDigit((int) -(l % radix), radix);
            l = l / radix;
        }
        buf[pos] = Character.forDigit((int) -l, radix);
        if (neg) {
            buf[--pos] = '-';
        }
        return new String(buf, pos, 65 - pos);
    }

    public static String toHexString(long l) {
        char[] buf = new char[16];
        int pos = 16;
        do {
            buf[--pos] = Character.forDigit((int) (l & 15), 16);
            l >>>= 4;
        } while (l != 0);
        return new String(buf, pos, 16 - pos);
    }

    public long longValue() { return value; }
    public int intValue() { return (int) value; }
    public float floatValue() { return value; }
    public double doubleValue() { return value; }

    public String toString() {
        return toString(value);
    }

    public int hashCode() {
        return (int) (value ^ (value >>> 32));
    }

    public boolean equals(Object o) {
        return o instanceof Long && ((Long) o).value == value;
    }
}
