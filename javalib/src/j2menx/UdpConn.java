package j2menx;

import java.io.IOException;
import java.io.InterruptedIOException;
import javax.microedition.io.Datagram;
import javax.microedition.io.UDPDatagramConnection;

// datagram://host:port (gửi tới 1 đích) hoặc datagram://:port (nhận ở cổng cục bộ)
public class UdpConn implements UDPDatagramConnection {
    private static final int MAX = 1472;

    private final int fd;
    private final String defaultAddress;
    private boolean closed;

    public UdpConn(String url) throws IOException {
        String hp = url.substring("datagram://".length());
        int colon = hp.lastIndexOf(':');
        String host = colon >= 0 ? hp.substring(0, colon) : hp;
        int port = 0;
        if (colon >= 0 && colon + 1 < hp.length()) {
            try {
                port = Integer.parseInt(hp.substring(colon + 1));
            } catch (NumberFormatException e) {
                throw new IllegalArgumentException("Cong khong hop le: " + url);
            }
        }
        if (host.length() == 0) {
            fd = Net.udpOpen0(port);
            defaultAddress = null;
        } else {
            fd = Net.udpOpen0(0);
            defaultAddress = url;
        }
    }

    private void check() throws IOException {
        if (closed) {
            throw new IOException("Connection closed");
        }
    }

    public int getMaximumLength() { return MAX; }
    public int getNominalLength() { return MAX; }

    public void send(Datagram dgram) throws IOException {
        check();
        String addr = dgram.getAddress() != null ? dgram.getAddress() : defaultAddress;
        if (addr == null) {
            throw new IOException("Datagram khong co dia chi");
        }
        String hp = addr.substring("datagram://".length());
        int colon = hp.lastIndexOf(':');
        if (colon <= 0) {
            throw new IllegalArgumentException("Dia chi khong hop le: " + addr);
        }
        String host = hp.substring(0, colon);
        int port = Integer.parseInt(hp.substring(colon + 1));
        while (Net.udpSend0(fd, host, port, dgram.getData(), dgram.getOffset(), dgram.getLength()) == 0) {
            Net.sleep();
        }
    }

    public void receive(Datagram dgram) throws IOException {
        check();
        int[] from = new int[5];
        byte[] b = dgram.getData();
        int off = dgram.getOffset();
        int cap = b.length - off;
        int n;
        while ((n = Net.udpRecv0(fd, b, off, cap, from)) == 0) {
            if (closed) {
                throw new InterruptedIOException("Connection closed");
            }
            Net.sleep();
        }
        dgram.setLength(n);
        if (dgram instanceof DatagramImpl) {
            ((DatagramImpl) dgram).pos = 0;
        }
        dgram.setAddress("datagram://" + from[0] + "." + from[1] + "." + from[2] + "." + from[3] + ":" + from[4]);
    }

    public Datagram newDatagram(int size) throws IOException {
        return newDatagram(new byte[size], size, defaultAddress);
    }

    public Datagram newDatagram(int size, String addr) throws IOException {
        return newDatagram(new byte[size], size, addr);
    }

    public Datagram newDatagram(byte[] buf, int size) throws IOException {
        return newDatagram(buf, size, defaultAddress);
    }

    public Datagram newDatagram(byte[] buf, int size, String addr) throws IOException {
        check();
        DatagramImpl d = new DatagramImpl(buf, size, null);
        if (addr != null) {
            d.setAddress(addr);
        }
        return d;
    }

    public String getLocalAddress() throws IOException {
        check();
        return Net.localAddress0(fd);
    }

    public int getLocalPort() throws IOException {
        check();
        return Net.localPort0(fd);
    }

    public synchronized void close() {
        if (!closed) {
            closed = true;
            Net.close0(fd);
        }
    }
}
