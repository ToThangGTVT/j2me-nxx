package j2menx;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import javax.microedition.io.SocketConnection;

public class SocketConn implements SocketConnection {
    private final String host;
    private final int port;
    private int fd;
    private boolean closed;
    private final int[] options = new int[5];

    public SocketConn(String host, int port) throws IOException {
        this.host = host;
        this.port = port;
        fd = Net.connect(host, port);
    }

    int fd() throws IOException {
        if (closed) {
            throw new IOException("Connection closed");
        }
        return fd;
    }

    public InputStream openInputStream() throws IOException {
        fd();
        return new InputStream() {
            private final byte[] one = new byte[1];

            public int read() throws IOException {
                int n = Net.read(fd(), one, 0, 1);
                return n < 0 ? -1 : one[0] & 0xff;
            }

            public int read(byte[] b, int off, int len) throws IOException {
                if (off < 0 || len < 0 || off + len > b.length) {
                    throw new IndexOutOfBoundsException();
                }
                return Net.read(fd(), b, off, len);
            }

            public int available() throws IOException {
                return closed ? 0 : Net.available0(fd);
            }

            public void close() {
            }
        };
    }

    public DataInputStream openDataInputStream() throws IOException {
        return new DataInputStream(openInputStream());
    }

    public OutputStream openOutputStream() throws IOException {
        fd();
        return new OutputStream() {
            public void write(int b) throws IOException {
                Net.write(fd(), new byte[] { (byte) b }, 0, 1);
            }

            public void write(byte[] b, int off, int len) throws IOException {
                if (off < 0 || len < 0 || off + len > b.length) {
                    throw new IndexOutOfBoundsException();
                }
                Net.write(fd(), b, off, len);
            }

            public void close() {
            }
        };
    }

    public DataOutputStream openDataOutputStream() throws IOException {
        return new DataOutputStream(openOutputStream());
    }

    public void setSocketOption(byte option, int value) throws IOException {
        if (option < 0 || option >= options.length) {
            throw new IllegalArgumentException();
        }
        options[option] = value;
    }

    public int getSocketOption(byte option) throws IOException {
        if (option < 0 || option >= options.length) {
            throw new IllegalArgumentException();
        }
        return options[option];
    }

    public String getLocalAddress() throws IOException {
        return Net.localAddress0(fd());
    }

    public int getLocalPort() throws IOException {
        return Net.localPort0(fd());
    }

    public String getAddress() throws IOException {
        fd();
        return host;
    }

    public int getPort() throws IOException {
        fd();
        return port;
    }

    public synchronized void close() throws IOException {
        if (!closed) {
            closed = true;
            Net.close0(fd);
        }
    }
}
