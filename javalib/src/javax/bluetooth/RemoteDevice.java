package javax.bluetooth;

import java.io.IOException;
import javax.microedition.io.Connection;

public class RemoteDevice {
    private final String address;

    protected RemoteDevice(String address) {
        if (address == null) {
            throw new NullPointerException();
        }
        if (address.length() != 12) {
            throw new IllegalArgumentException();
        }
        this.address = address.toUpperCase();
    }

    public boolean isTrustedDevice() {
        return false;
    }

    public String getFriendlyName(boolean alwaysAsk) throws IOException {
        return address;
    }

    public final String getBluetoothAddress() {
        return address;
    }

    public boolean equals(Object obj) {
        return obj instanceof RemoteDevice && ((RemoteDevice) obj).address.equals(address);
    }

    public int hashCode() {
        return address.hashCode();
    }

    public static RemoteDevice getRemoteDevice(Connection conn) throws IOException {
        throw new IllegalArgumentException("Khong phai ket noi Bluetooth");
    }

    public boolean authenticate() throws IOException {
        return false;
    }

    public boolean authorize(Connection conn) throws IOException {
        return false;
    }

    public boolean encrypt(Connection conn, boolean on) throws IOException {
        return false;
    }

    public boolean isAuthenticated() {
        return false;
    }

    public boolean isAuthorized(Connection conn) throws IOException {
        return false;
    }

    public boolean isEncrypted() {
        return false;
    }
}
