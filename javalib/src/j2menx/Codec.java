package j2menx;

import java.io.UnsupportedEncodingException;

// Chuyển đổi byte <-> char cho String / Reader / Writer
public final class Codec {
    public static final int UTF8 = 0;
    public static final int LATIN1 = 1;

    private static int defaultEnc = -1;

    private Codec() {
    }

    public static int lookup(String enc) throws UnsupportedEncodingException {
        if (enc == null) {
            return getDefault();
        }
        String e = enc.toUpperCase();
        if (e.equals("UTF-8") || e.equals("UTF8")) {
            return UTF8;
        }
        if (e.equals("ISO-8859-1") || e.equals("ISO8859_1") || e.equals("ISO8859-1") || e.equals("US-ASCII")
                || e.equals("ASCII") || e.equals("LATIN1") || e.equals("CP1252") || e.equals("WINDOWS-1252")) {
            return LATIN1;
        }
        throw new UnsupportedEncodingException(enc);
    }

    public static int getDefault() {
        if (defaultEnc < 0) {
            String p = System.getProperty("microedition.encoding");
            defaultEnc = (p != null && p.toUpperCase().startsWith("ISO")) ? LATIN1 : UTF8;
        }
        return defaultEnc;
    }

    public static char[] decode(byte[] b, int off, int len, int enc) {
        if (enc == LATIN1) {
            char[] c = new char[len];
            for (int i = 0; i < len; i++) {
                c[i] = (char) (b[off + i] & 0xff);
            }
            return c;
        }
        char[] c = new char[len];
        int n = 0;
        int end = off + len;
        int i = off;
        while (i < end) {
            int x = b[i] & 0xff;
            if (x < 0x80) {
                c[n++] = (char) x;
                i++;
            } else if ((x & 0xe0) == 0xc0 && i + 1 < end) {
                c[n++] = (char) (((x & 0x1f) << 6) | (b[i + 1] & 0x3f));
                i += 2;
            } else if ((x & 0xf0) == 0xe0 && i + 2 < end) {
                c[n++] = (char) (((x & 0x0f) << 12) | ((b[i + 1] & 0x3f) << 6) | (b[i + 2] & 0x3f));
                i += 3;
            } else if ((x & 0xf8) == 0xf0 && i + 3 < end) {
                int cp = ((x & 0x07) << 18) | ((b[i + 1] & 0x3f) << 12) | ((b[i + 2] & 0x3f) << 6) | (b[i + 3] & 0x3f);
                cp -= 0x10000;
                c[n++] = (char) (0xd800 + (cp >> 10));
                if (n < c.length) {
                    c[n++] = (char) (0xdc00 + (cp & 0x3ff));
                }
                i += 4;
            } else {
                // Byte không hợp lệ: coi như Latin-1
                c[n++] = (char) x;
                i++;
            }
        }
        if (n == len) {
            return c;
        }
        char[] r = new char[n];
        System.arraycopy(c, 0, r, 0, n);
        return r;
    }

    public static byte[] encode(char[] c, int off, int len, int enc) {
        if (enc == LATIN1) {
            byte[] b = new byte[len];
            for (int i = 0; i < len; i++) {
                char ch = c[off + i];
                b[i] = ch < 256 ? (byte) ch : (byte) '?';
            }
            return b;
        }
        int size = 0;
        for (int i = 0; i < len; i++) {
            char ch = c[off + i];
            size += ch < 0x80 ? 1 : ch < 0x800 ? 2 : 3;
        }
        byte[] b = new byte[size];
        int n = 0;
        for (int i = 0; i < len; i++) {
            char ch = c[off + i];
            if (ch < 0x80) {
                b[n++] = (byte) ch;
            } else if (ch < 0x800) {
                b[n++] = (byte) (0xc0 | (ch >> 6));
                b[n++] = (byte) (0x80 | (ch & 0x3f));
            } else {
                b[n++] = (byte) (0xe0 | (ch >> 12));
                b[n++] = (byte) (0x80 | ((ch >> 6) & 0x3f));
                b[n++] = (byte) (0x80 | (ch & 0x3f));
            }
        }
        return b;
    }
}
