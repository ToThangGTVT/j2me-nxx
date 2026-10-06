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
