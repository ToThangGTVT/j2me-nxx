package java.lang;

import java.io.PrintStream;

public final class System {
    public static final PrintStream out = new PrintStream(new LogOutputStream(false));
    public static final PrintStream err = new PrintStream(new LogOutputStream(true));

    private System() {
    }

    public static native long currentTimeMillis();

    public static native void arraycopy(Object src, int srcPos, Object dst, int dstPos, int length);

    public static native int identityHashCode(Object o);

    public static String getProperty(String key) {
        if (key == null) {
            throw new NullPointerException();
        }
        return getProperty0(key);
    }

    private static native String getProperty0(String key);

    public static void exit(int status) {
        exit0(status);
    }

    private static native void exit0(int status);

    public static native void gc();
}

class LogOutputStream extends java.io.OutputStream {
    private final boolean err;
    private byte[] buf = new byte[256];
    private int len;

    LogOutputStream(boolean err) {
        this.err = err;
    }

    public synchronized void write(int b) {
        if (b == '\n') {
            flush();
            return;
        }
        if (len == buf.length) {
            byte[] n = new byte[buf.length * 2];
            System.arraycopy(buf, 0, n, 0, len);
            buf = n;
        }
        buf[len++] = (byte) b;
    }

    public synchronized void flush() {
        log(buf, len, err);
        len = 0;
    }

    private static native void log(byte[] b, int len, boolean err);
}
