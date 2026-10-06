package java.lang;

public final class Double {
    public static final double POSITIVE_INFINITY = 1.0 / 0.0;
    public static final double NEGATIVE_INFINITY = -1.0 / 0.0;
    public static final double NaN = 0.0d / 0.0;
    public static final double MAX_VALUE = 1.7976931348623157e+308;
    public static final double MIN_VALUE = 4.9e-324;

    private final double value;

    public Double(double value) {
        this.value = value;
    }

    public static Double valueOf(double d) {
        return new Double(d);
    }

    public static Double valueOf(String s) throws NumberFormatException {
        return new Double(parseDouble(s));
    }

    public static double parseDouble(String s) throws NumberFormatException {
        if (s == null) {
            throw new NullPointerException();
        }
        String t = s.trim();
        if (t.length() == 0) {
            throw new NumberFormatException(s);
        }
        double[] out = new double[1];
        if (!parse0(t, out)) {
            throw new NumberFormatException(s);
        }
        return out[0];
    }

    private static native boolean parse0(String s, double[] out);

    static native String toString0(double d, boolean isFloat);

    public static String toString(double d) {
        return toString0(d, false);
    }

    public static boolean isNaN(double v) {
        return v != v;
    }

    public static boolean isInfinite(double v) {
        return v == POSITIVE_INFINITY || v == NEGATIVE_INFINITY;
    }

    public boolean isNaN() { return isNaN(value); }
    public boolean isInfinite() { return isInfinite(value); }

    public static native long doubleToLongBits(double d);

    public static native double longBitsToDouble(long bits);

    public byte byteValue() { return (byte) value; }
    public short shortValue() { return (short) value; }
    public int intValue() { return (int) value; }
    public long longValue() { return (long) value; }
    public float floatValue() { return (float) value; }
    public double doubleValue() { return value; }

    public String toString() {
        return toString(value);
    }

    public int hashCode() {
        long b = doubleToLongBits(value);
        return (int) (b ^ (b >>> 32));
    }

    public boolean equals(Object o) {
        return o instanceof Double && doubleToLongBits(((Double) o).value) == doubleToLongBits(value);
    }
}
