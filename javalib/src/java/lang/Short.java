package java.lang;

public final class Short {
    public static final short MIN_VALUE = -32768;
    public static final short MAX_VALUE = 32767;

    private final short value;

    public Short(short value) {
        this.value = value;
    }

    public static Short valueOf(short s) {
        return new Short(s);
    }

    public static short parseShort(String s) throws NumberFormatException {
        return parseShort(s, 10);
    }

    public static short parseShort(String s, int radix) throws NumberFormatException {
        int v = Integer.parseInt(s, radix);
        if (v < MIN_VALUE || v > MAX_VALUE) {
            throw new NumberFormatException(s);
        }
        return (short) v;
    }

    public short shortValue() { return value; }
    public int intValue() { return value; }

    public String toString() {
        return Integer.toString(value);
    }

    public int hashCode() {
        return value;
    }

    public boolean equals(Object o) {
        return o instanceof Short && ((Short) o).value == value;
    }
}
