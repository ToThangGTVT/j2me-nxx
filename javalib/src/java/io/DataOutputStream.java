package java.io;

public class DataOutputStream extends OutputStream implements DataOutput {
    protected OutputStream out;
    protected int written;

    public DataOutputStream(OutputStream out) {
        this.out = out;
    }

    public void write(int b) throws IOException {
        out.write(b);
        written++;
    }

    public void write(byte[] b, int off, int len) throws IOException {
        out.write(b, off, len);
        written += len;
    }

    public void flush() throws IOException {
        out.flush();
    }

    public void close() throws IOException {
        out.close();
    }

    public final void writeBoolean(boolean v) throws IOException {
        write(v ? 1 : 0);
    }

    public final void writeByte(int v) throws IOException {
        write(v);
    }

    public final void writeShort(int v) throws IOException {
        write((v >>> 8) & 0xff);
        write(v & 0xff);
    }

    public final void writeChar(int v) throws IOException {
        writeShort(v);
    }

    public final void writeInt(int v) throws IOException {
        write((v >>> 24) & 0xff);
        write((v >>> 16) & 0xff);
        write((v >>> 8) & 0xff);
        write(v & 0xff);
    }

    public final void writeLong(long v) throws IOException {
        writeInt((int) (v >>> 32));
        writeInt((int) v);
    }

    public final void writeFloat(float v) throws IOException {
        writeInt(Float.floatToIntBits(v));
    }

    public final void writeDouble(double v) throws IOException {
        writeLong(Double.doubleToLongBits(v));
    }

    public final void writeChars(String s) throws IOException {
        for (int i = 0; i < s.length(); i++) {
            writeChar(s.charAt(i));
        }
    }

    public final void writeUTF(String s) throws IOException {
        int len = s.length();
        int size = 0;
        for (int i = 0; i < len; i++) {
            char c = s.charAt(i);
            size += (c >= 1 && c < 0x80) ? 1 : c < 0x800 ? 2 : 3;
        }
        if (size > 65535) {
            throw new UTFDataFormatException();
        }
        byte[] b = new byte[size + 2];
        b[0] = (byte) (size >>> 8);
        b[1] = (byte) size;
        int n = 2;
        for (int i = 0; i < len; i++) {
            char c = s.charAt(i);
            if (c >= 1 && c < 0x80) {
                b[n++] = (byte) c;
            } else if (c < 0x800) {
                b[n++] = (byte) (0xc0 | (c >> 6));
                b[n++] = (byte) (0x80 | (c & 0x3f));
            } else {
                b[n++] = (byte) (0xe0 | (c >> 12));
                b[n++] = (byte) (0x80 | ((c >> 6) & 0x3f));
                b[n++] = (byte) (0x80 | (c & 0x3f));
            }
        }
        write(b, 0, n);
    }

    public final int size() {
        return written;
    }
}
