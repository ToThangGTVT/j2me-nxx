package java.lang;

public final class Byte {
    public static final byte MIN_VALUE = -128;
    public static final byte MAX_VALUE = 127;

    private final byte value;

    public Byte(byte value) {
        this.value = value;
    }

    public static Byte valueOf(byte b) {
        return new Byte(b);
    }

    public static byte parseByte(String s) throws NumberFormatException {
        return parseByte(s, 10);
    }

    public static byte parseByte(String s, int radix) throws NumberFormatException {
        int v = Integer.parseInt(s, radix);
        if (v < MIN_VALUE || v > MAX_VALUE) {
            throw new NumberFormatException(s);
        }
        return (byte) v;
    }

    public byte byteValue() { return value; }
    public int intValue() { return value; }

    public String toString() {
        return Integer.toString(value);
    }

    public int hashCode() {
        return value;
    }

    public boolean equals(Object o) {
        return o instanceof Byte && ((Byte) o).value == value;
    }
}
