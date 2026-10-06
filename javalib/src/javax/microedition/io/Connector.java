package javax.microedition.io;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

public class Connector {
    public static final int READ = 1;
    public static final int WRITE = 2;
    public static final int READ_WRITE = 3;

    private Connector() {
    }

    public static Connection open(String name) throws IOException {
        return open(name, READ_WRITE, false);
    }

    public static Connection open(String name, int mode) throws IOException {
        return open(name, mode, false);
    }

    public static Connection open(String name, int mode, boolean timeouts) throws IOException {
        if (name == null) {
            throw new IllegalArgumentException();
        }
        String lower = name.toLowerCase();
        if (lower.startsWith("socket://") || lower.startsWith("ssl://")) {
            boolean secure = lower.startsWith("ssl://");
            String hp = name.substring(secure ? 6 : 9);
            int colon = hp.lastIndexOf(':');
            if (colon <= 0) {
                throw new IllegalArgumentException("Thieu cong: " + name);
            }
            int port;
            try {
                port = Integer.parseInt(hp.substring(colon + 1));
            } catch (NumberFormatException e) {
                throw new IllegalArgumentException("Cong khong hop le: " + name);
            }
            return new j2menx.SocketConn(hp.substring(0, colon), port, secure);
        }
        if (lower.startsWith("http://") || lower.startsWith("https://")) {
            return new j2menx.HttpConn(name);
        }
        if (lower.startsWith("datagram://")) {
            return new j2menx.UdpConn(name);
        }
        if (lower.startsWith("sms://") || lower.startsWith("mms://") || lower.startsWith("cbs://")) {
            return new j2menx.SmsConn(name);
        }
        if (lower.startsWith("btspp://") || lower.startsWith("btl2cap://") || lower.startsWith("btgoep://")) {
            throw new javax.bluetooth.BluetoothConnectionException(
                    javax.bluetooth.BluetoothConnectionException.FAILED_NOINFO, "Bluetooth khong ho tro tren Switch");
        }
        System.out.println("Connector.open chua ho tro: " + name);
        throw new ConnectionNotFoundException(name);
    }

    public static DataInputStream openDataInputStream(String name) throws IOException {
        return new DataInputStream(openInputStream(name));
    }

    public static DataOutputStream openDataOutputStream(String name) throws IOException {
        return new DataOutputStream(openOutputStream(name));
    }

    public static InputStream openInputStream(String name) throws IOException {
        return ((InputConnection) open(name, READ)).openInputStream();
    }

    public static OutputStream openOutputStream(String name) throws IOException {
        return ((OutputConnection) open(name, WRITE)).openOutputStream();
    }
}
