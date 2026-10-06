package java.io;

public class PrintStream extends OutputStream {
    private OutputStream out;
    private boolean error;

    public PrintStream(OutputStream out) {
        this.out = out;
    }

    public void flush() {
        try {
            out.flush();
        } catch (IOException e) {
            error = true;
        }
    }

    public void close() {
        try {
            out.close();
        } catch (IOException e) {
            error = true;
        }
    }

    public boolean checkError() {
        return error;
    }

    protected void setError() {
        error = true;
    }

    public void write(int b) {
        try {
            out.write(b);
        } catch (IOException e) {
            error = true;
        }
    }

    public void write(byte[] b, int off, int len) {
        try {
            out.write(b, off, len);
        } catch (IOException e) {
            error = true;
        }
    }

    private void writeString(String s) {
        byte[] b = s.getBytes();
        write(b, 0, b.length);
    }

    public void print(boolean b) { writeString(String.valueOf(b)); }
    public void print(char c) { writeString(String.valueOf(c)); }
    public void print(int i) { writeString(String.valueOf(i)); }
    public void print(long l) { writeString(String.valueOf(l)); }
    public void print(float f) { writeString(String.valueOf(f)); }
    public void print(double d) { writeString(String.valueOf(d)); }
    public void print(char[] s) { writeString(new String(s)); }
    public void print(String s) { writeString(s == null ? "null" : s); }
    public void print(Object o) { writeString(String.valueOf(o)); }

    public void println() {
        write('\n');
    }

    public synchronized void println(boolean x) { print(x); println(); }
    public synchronized void println(char x) { print(x); println(); }
    public synchronized void println(int x) { print(x); println(); }
    public synchronized void println(long x) { print(x); println(); }
    public synchronized void println(float x) { print(x); println(); }
    public synchronized void println(double x) { print(x); println(); }
    public synchronized void println(char[] x) { print(x); println(); }
    public synchronized void println(String x) { print(x); println(); }
    public synchronized void println(Object x) { print(x); println(); }
}
