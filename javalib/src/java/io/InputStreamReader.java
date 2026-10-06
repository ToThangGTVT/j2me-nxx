package java.io;

import j2menx.Codec;

public class InputStreamReader extends Reader {
    private final InputStream in;
    private final int enc;
    private int pending = -1;

    public InputStreamReader(InputStream in) {
        this.in = in;
        this.enc = Codec.getDefault();
    }

    public InputStreamReader(InputStream in, String enc) throws UnsupportedEncodingException {
        this.in = in;
        this.enc = Codec.lookup(enc);
    }

    private int readChar() throws IOException {
        if (pending >= 0) {
            int c = pending;
            pending = -1;
            return c;
        }
        int b = in.read();
        if (b < 0) {
            return -1;
        }
        if (enc == Codec.LATIN1 || b < 0x80) {
            return b;
        }
        if ((b & 0xe0) == 0xc0) {
            int b2 = in.read();
            return ((b & 0x1f) << 6) | (b2 & 0x3f);
        }
        if ((b & 0xf0) == 0xe0) {
            int b2 = in.read(), b3 = in.read();
            return ((b & 0x0f) << 12) | ((b2 & 0x3f) << 6) | (b3 & 0x3f);
        }
        if ((b & 0xf8) == 0xf0) {
            int b2 = in.read(), b3 = in.read(), b4 = in.read();
            int cp = ((b & 0x07) << 18) | ((b2 & 0x3f) << 12) | ((b3 & 0x3f) << 6) | (b4 & 0x3f);
            cp -= 0x10000;
            pending = 0xdc00 + (cp & 0x3ff);
            return 0xd800 + (cp >> 10);
        }
        return b;
    }

    public int read() throws IOException {
        return readChar();
    }

    public int read(char[] c, int off, int len) throws IOException {
        if (len == 0) {
            return 0;
        }
        int n = 0;
        while (n < len) {
            int ch = readChar();
            if (ch < 0) {
                break;
            }
            c[off + n++] = (char) ch;
            if (in.available() <= 0 && pending < 0) {
                break;
            }
        }
        return n == 0 ? -1 : n;
    }

    public boolean ready() throws IOException {
        return pending >= 0 || in.available() > 0;
    }

    public void close() throws IOException {
        in.close();
    }
}
