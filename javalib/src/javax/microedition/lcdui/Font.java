package javax.microedition.lcdui;

public final class Font {
    public static final int STYLE_PLAIN = 0;
    public static final int STYLE_BOLD = 1;
    public static final int STYLE_ITALIC = 2;
    public static final int STYLE_UNDERLINED = 4;
    public static final int SIZE_SMALL = 8;
    public static final int SIZE_MEDIUM = 0;
    public static final int SIZE_LARGE = 16;
    public static final int FACE_SYSTEM = 0;
    public static final int FACE_MONOSPACE = 32;
    public static final int FACE_PROPORTIONAL = 64;
    public static final int FONT_STATIC_TEXT = 0;
    public static final int FONT_INPUT_TEXT = 1;

    private static final Font[] cache = new Font[3 * 8 * 3];
    private static Font defaultFont;

    final int face;
    final int style;
    final int size;
    // Khoá font cho native: chỉ số size * 4 + (bold|italic)
    final int key;
    private final int height;
    private final int baseline;

    private Font(int face, int style, int size) {
        this.face = face;
        this.style = style;
        this.size = size;
        int si = size == SIZE_SMALL ? 0 : size == SIZE_LARGE ? 2 : 1;
        key = si * 4 + (style & 3);
        height = height0(key);
        baseline = baseline0(key);
    }

    public static Font getDefaultFont() {
        if (defaultFont == null) {
            defaultFont = getFont(FACE_SYSTEM, STYLE_PLAIN, SIZE_MEDIUM);
        }
        return defaultFont;
    }

    public static Font getFont(int specifier) {
        return getDefaultFont();
    }

    public static Font getFont(int face, int style, int size) {
        if ((face != FACE_SYSTEM && face != FACE_MONOSPACE && face != FACE_PROPORTIONAL)
                || (size != SIZE_SMALL && size != SIZE_MEDIUM && size != SIZE_LARGE) || (style & ~7) != 0) {
            throw new IllegalArgumentException();
        }
        int fi = face == FACE_SYSTEM ? 0 : face == FACE_MONOSPACE ? 1 : 2;
        int si = size == SIZE_SMALL ? 0 : size == SIZE_LARGE ? 2 : 1;
        int idx = (fi * 8 + style) * 3 + si;
        synchronized (cache) {
            if (cache[idx] == null) {
                cache[idx] = new Font(face, style, size);
            }
            return cache[idx];
        }
    }

    public int getStyle() { return style; }
    public int getSize() { return size; }
    public int getFace() { return face; }
    public boolean isPlain() { return style == STYLE_PLAIN; }
    public boolean isBold() { return (style & STYLE_BOLD) != 0; }
    public boolean isItalic() { return (style & STYLE_ITALIC) != 0; }
    public boolean isUnderlined() { return (style & STYLE_UNDERLINED) != 0; }

    public int getHeight() {
        return height;
    }

    public int getBaselinePosition() {
        return baseline;
    }

    public int charWidth(char ch) {
        return stringWidth0(key, String.valueOf(ch));
    }

    public int charsWidth(char[] ch, int offset, int length) {
        return stringWidth0(key, new String(ch, offset, length));
    }

    public int stringWidth(String str) {
        return stringWidth0(key, str);
    }

    public int substringWidth(String str, int offset, int len) {
        return stringWidth0(key, str.substring(offset, offset + len));
    }

    private static native int height0(int key);

    private static native int baseline0(int key);

    private static native int stringWidth0(int key, String s);
}
