package j2menx;

import java.io.IOException;
import java.io.InterruptedIOException;

// Socket TCP không chặn (native: source/midp/net.c). Chờ bằng Thread.sleep để không khoá VM.
public final class Net {
    static final int CONNECT_TIMEOUT = 20000;
    static final int POLL_MS = 5;

    private Net() {
    }

    public static int connect(String host, int port) throws IOException {
        int fd = socket0(host, port);
        long deadline = System.currentTimeMillis() + CONNECT_TIMEOUT;
        while (true) {
            int r = connectPoll0(fd);
            if (r > 0) {
                return fd;
            }
            if (r < 0) {
                close0(fd);
                throw new IOException("Khong ket noi duoc " + host + ":" + port);
            }
            if (System.currentTimeMillis() > deadline) {
                close0(fd);
                throw new InterruptedIOException("Het thoi gian ket noi " + host + ":" + port);
            }
            sleep();
        }
    }

    static void sleep() throws IOException {
        try {
            Thread.sleep(POLL_MS);
        } catch (InterruptedException e) {
            throw new InterruptedIOException();
        }
    }

    // Đọc ít nhất 1 byte (chờ nếu chưa có), -1 khi hết dữ liệu
    public static int read(int fd, byte[] b, int off, int len) throws IOException {
        if (len == 0) {
            return 0;
        }
        while (true) {
            int n = read0(fd, b, off, len);
            if (n != 0) {
                return n;
            }
            sleep();
        }
    }

    public static void write(int fd, byte[] b, int off, int len) throws IOException {
        while (len > 0) {
            int n = write0(fd, b, off, len);
            if (n == 0) {
                sleep();
                continue;
            }
            off += n;
            len -= n;
        }
    }

    // Bắt tay TLS trên socket đã kết nối, trả về handle TLS
    public static int tlsConnect(int fd, String host) throws IOException {
        int h = tlsNew0(fd, host);
        long deadline = System.currentTimeMillis() + CONNECT_TIMEOUT;
        try {
            while (tlsHandshake0(h) == 0) {
                if (System.currentTimeMillis() > deadline) {
                    throw new InterruptedIOException("Het thoi gian bat tay TLS voi " + host);
                }
                sleep();
            }
        } catch (IOException e) {
            tlsClose0(h);
            throw e;
        }
        return h;
    }

    public static int tlsRead(int h, byte[] b, int off, int len) throws IOException {
        if (len == 0) {
            return 0;
        }
        while (true) {
            int n = tlsRead0(h, b, off, len);
            if (n != 0) {
                return n;
            }
            sleep();
        }
    }

    public static void tlsWrite(int h, byte[] b, int off, int len) throws IOException {
        while (len > 0) {
            int n = tlsWrite0(h, b, off, len);
            if (n == 0) {
                sleep();
                continue;
            }
            off += n;
            len -= n;
        }
    }

    static native int tlsNew0(int fd, String host) throws IOException;

    // 1 = xong, 0 = đang bắt tay
    static native int tlsHandshake0(int h) throws IOException;

    static native int tlsRead0(int h, byte[] b, int off, int len) throws IOException;

    static native int tlsWrite0(int h, byte[] b, int off, int len) throws IOException;

    public static native int tlsAvailable0(int h);

    public static native void tlsClose0(int h);

    public static native String tlsCipher0(int h);

    public static native String tlsVersion0(int h);

    static native int socket0(String host, int port) throws IOException;

    // 1 = đã kết nối, 0 = đang kết nối, -1 = lỗi
    static native int connectPoll0(int fd);

    // n > 0 số byte đọc được, 0 = chưa có dữ liệu, -1 = đóng kết nối
    static native int read0(int fd, byte[] b, int off, int len) throws IOException;

    // số byte đã ghi, 0 = bộ đệm đầy
    static native int write0(int fd, byte[] b, int off, int len) throws IOException;

    public static native int available0(int fd);

    public static native void close0(int fd);

    public static native String localAddress0(int fd);

    public static native int localPort0(int fd);
}
