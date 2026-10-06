package java.lang;

public final class Boolean {
    public static final Boolean TRUE = new Boolean(true);
    public static final Boolean FALSE = new Boolean(false);

    private final boolean value;

    public Boolean(boolean value) {
        this.value = value;
    }

    public static Boolean valueOf(boolean b) {
        return b ? TRUE : FALSE;
    }

    public boolean booleanValue() {
        return value;
    }

    public String toString() {
        return value ? "true" : "false";
    }

    public int hashCode() {
        return value ? 1231 : 1237;
    }

    public boolean equals(Object o) {
        return o instanceof Boolean && ((Boolean) o).value == value;
    }
}
