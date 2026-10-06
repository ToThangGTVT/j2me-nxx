package j2menx;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import javax.microedition.io.SecureConnection;
import javax.microedition.io.SecurityInfo;
import javax.microedition.pki.Certificate;

// Kết nối TCP, có thể bọc TLS (ssl://, https://)
public class SocketConn implements SecureConnection {
    private final String host;
    private final int port;
    private final int fd;
    private int tls;
    private boolean closed;
    private final int[] options = new int[5];

    public SocketConn(String host, int port) throws IOException {
        this(host, port, false);
    }

    public SocketConn(String host, int port, boolean secure) throws IOException {
        this.host = host;
        this.port = port;
        fd = Net.connect(host, port);
        if (secure) {
            try {
                tls = Net.tlsConnect(fd, host);
            } catch (IOException e) {
                Net.close0(fd);
                throw e;
            }
        }
    }

    private void check() throws IOException {
        if (closed) {
            throw new IOException("Connection closed");
        }
    }

    int readBytes(byte[] b, int off, int len) throws IOException {
        check();
        return tls != 0 ? Net.tlsRead(tls, b, off, len) : Net.read(fd, b, off, len);
    }

    void writeBytes(byte[] b, int off, int len) throws IOException {
        check();
        if (tls != 0) {
            Net.tlsWrite(tls, b, off, len);
        } else {
            Net.write(fd, b, off, len);
        }
    }

    public InputStream openInputStream() throws IOException {
        check();
        return new InputStream() {
            private final byte[] one = new byte[1];

            public int read() throws IOException {
                int n = readBytes(one, 0, 1);
                return n < 0 ? -1 : one[0] & 0xff;
            }

            public int read(byte[] b, int off, int len) throws IOException {
                if (off < 0 || len < 0 || off + len > b.length) {
                    throw new IndexOutOfBoundsException();
                }
                return readBytes(b, off, len);
            }

            public int available() throws IOException {
                if (closed) {
                    return 0;
                }
                return tls != 0 ? Net.tlsAvailable0(tls) : Net.available0(fd);
            }

            public void close() {
            }
        };
    }

    public DataInputStream openDataInputStream() throws IOException {
        return new DataInputStream(openInputStream());
    }

    public OutputStream openOutputStream() throws IOException {
        check();
        return new OutputStream() {
            public void write(int b) throws IOException {
                writeBytes(new byte[] { (byte) b }, 0, 1);
            }

            public void write(byte[] b, int off, int len) throws IOException {
                if (off < 0 || len < 0 || off + len > b.length) {
                    throw new IndexOutOfBoundsException();
                }
                writeBytes(b, off, len);
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
        check();
        return Net.localAddress0(fd);
    }

    public int getLocalPort() throws IOException {
        check();
        return Net.localPort0(fd);
    }

    public String getAddress() throws IOException {
        check();
        return host;
    }

    public int getPort() throws IOException {
        check();
        return port;
    }

    public SecurityInfo getSecurityInfo() throws IOException {
        check();
        if (tls == 0) {
            throw new IOException("Not a secure connection");
        }
        final String cipher = Net.tlsCipher0(tls);
        final String version = Net.tlsVersion0(tls);
        final String server = host;
        return new SecurityInfo() {
            public Certificate getServerCertificate() {
                return new Certificate() {
                    public String getSubject() { return "CN=" + server; }
                    public String getIssuer() { return "CN=unknown"; }
                    public String getType() { return "X.509"; }
                    public String getVersion() { return "3"; }
                    public String getSigAlgName() { return "unknown"; }
                    public long getNotBefore() { return 0; }
                    public long getNotAfter() { return Long.MAX_VALUE; }
                    public String getSerialNumber() { return "0"; }
                };
            }

            public String getProtocolVersion() {
                return version != null && version.startsWith("TLSv") ? version.substring(4) : version;
            }

            public String getProtocolName() {
                return "TLS";
            }

            public String getCipherSuite() {
                return cipher;
            }
        };
    }

    public synchronized void close() throws IOException {
        if (!closed) {
            closed = true;
            if (tls != 0) {
                Net.tlsClose0(tls);
                tls = 0;
            }
            Net.close0(fd);
        }
    }
}
