package java.io;

import j2menx.Codec;

public class OutputStreamWriter extends Writer {
    private final OutputStream out;
    private final int enc;

    public OutputStreamWriter(OutputStream out) {
        this.out = out;
        this.enc = Codec.getDefault();
    }

    public OutputStreamWriter(OutputStream out, String enc) throws UnsupportedEncodingException {
        this.out = out;
        this.enc = Codec.lookup(enc);
    }

    public void write(char[] c, int off, int len) throws IOException {
        out.write(Codec.encode(c, off, len, enc));
    }

    public void flush() throws IOException {
        out.flush();
    }

    public void close() throws IOException {
        out.close();
    }
}
