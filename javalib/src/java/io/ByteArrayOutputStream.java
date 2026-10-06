package java.io;

public class ByteArrayOutputStream extends OutputStream {
    protected byte[] buf;
    protected int count;

    public ByteArrayOutputStream() {
        this(32);
    }

    public ByteArrayOutputStream(int size) {
        if (size < 0) {
            throw new IllegalArgumentException();
        }
        buf = new byte[size];
    }

    private void ensure(int min) {
        if (min > buf.length) {
            byte[] n = new byte[Math.max(buf.length << 1, min)];
            System.arraycopy(buf, 0, n, 0, count);
            buf = n;
        }
    }

    public synchronized void write(int b) {
        ensure(count + 1);
        buf[count++] = (byte) b;
    }

    public synchronized void write(byte[] b, int off, int len) {
        if (off < 0 || len < 0 || off + len > b.length) {
            throw new IndexOutOfBoundsException();
        }
        ensure(count + len);
        System.arraycopy(b, off, buf, count, len);
        count += len;
    }

    public synchronized void reset() {
        count = 0;
    }

    public synchronized byte[] toByteArray() {
        byte[] r = new byte[count];
        System.arraycopy(buf, 0, r, 0, count);
        return r;
    }

    public int size() {
        return count;
    }

    public String toString() {
        return new String(buf, 0, count);
    }

    public synchronized void close() {
    }
}
