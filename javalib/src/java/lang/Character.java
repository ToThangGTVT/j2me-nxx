package java.lang;

public final class Character {
    public static final int MIN_RADIX = 2;
    public static final int MAX_RADIX = 36;
    public static final char MIN_VALUE = '\u0000';
    public static final char MAX_VALUE = '￿';

    private final char value;

    public Character(char value) {
        this.value = value;
    }

    public static Character valueOf(char c) {
        return new Character(c);
    }

    public char charValue() {
        return value;
    }

    public static boolean isLowerCase(char c) {
        return (c >= 'a' && c <= 'z') || (c >= 0xdf && c <= 0xff && c != 0xf7)
                || (c >= 0x100 && c < 0x250 && (c & 1) == 1) || (c >= 0x3b1 && c <= 0x3c9)
                || (c >= 0x430 && c <= 0x44f);
    }

    public static boolean isUpperCase(char c) {
        return (c >= 'A' && c <= 'Z') || (c >= 0xc0 && c <= 0xde && c != 0xd7)
                || (c >= 0x100 && c < 0x250 && (c & 1) == 0) || (c >= 0x391 && c <= 0x3a9)
                || (c >= 0x410 && c <= 0x42f);
    }

    public static boolean isDigit(char c) {
        return c >= '0' && c <= '9';
    }

    public static boolean isLetter(char c) {
        return isLowerCase(c) || isUpperCase(c) || (c > 0x7f && !isWhitespace(c) && !isDigit(c));
    }

    public static boolean isLetterOrDigit(char c) {
        return isLetter(c) || isDigit(c);
    }

    public static boolean isWhitespace(char c) {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == 0x0b || c == 0x1c
                || c == 0x1d || c == 0x1e || c == 0x1f;
    }

    public static boolean isSpace(char c) {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
    }

    public static char toLowerCase(char c) {
        if (c >= 'A' && c <= 'Z') {
            return (char) (c + 32);
        }
        if (c < 0x80) {
            return c;
        }
        if ((c >= 0xc0 && c <= 0xde && c != 0xd7) || (c >= 0x391 && c <= 0x3a9) || (c >= 0x410 && c <= 0x42f)) {
            return (char) (c + 32);
        }
        if (c >= 0x100 && c < 0x250 && (c & 1) == 0) {
            return (char) (c + 1);
        }
        return c;
    }

    public static char toUpperCase(char c) {
        if (c >= 'a' && c <= 'z') {
            return (char) (c - 32);
        }
        if (c < 0x80) {
            return c;
        }
        if ((c >= 0xe0 && c <= 0xfe && c != 0xf7) || (c >= 0x3b1 && c <= 0x3c9) || (c >= 0x430 && c <= 0x44f)) {
            return (char) (c - 32);
        }
        if (c >= 0x100 && c < 0x250 && (c & 1) == 1) {
            return (char) (c - 1);
        }
        return c;
    }

    public static int digit(char c, int radix) {
        int d;
        if (c >= '0' && c <= '9') {
            d = c - '0';
        } else if (c >= 'a' && c <= 'z') {
            d = c - 'a' + 10;
        } else if (c >= 'A' && c <= 'Z') {
            d = c - 'A' + 10;
        } else {
            return -1;
        }
        return d < radix ? d : -1;
    }

    public static char forDigit(int digit, int radix) {
        if (digit < 0 || digit >= radix || radix < MIN_RADIX || radix > MAX_RADIX) {
            return 0;
        }
        return (char) (digit < 10 ? '0' + digit : 'a' + digit - 10);
    }

    public String toString() {
        return String.valueOf(value);
    }

    public int hashCode() {
        return value;
    }

    public boolean equals(Object o) {
        return o instanceof Character && ((Character) o).value == value;
    }
}
