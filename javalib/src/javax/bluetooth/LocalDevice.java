package javax.bluetooth;

import javax.microedition.io.Connection;

// Switch không dùng Bluetooth của điện thoại: thiết bị giả, không tìm thấy ai
public class LocalDevice {
    private static LocalDevice instance;

    private final DiscoveryAgent agent = new DiscoveryAgent();
    private int discoverable = DiscoveryAgent.NOT_DISCOVERABLE;

    private LocalDevice() {
    }

    public static synchronized LocalDevice getLocalDevice() throws BluetoothStateException {
        if (instance == null) {
            instance = new LocalDevice();
        }
        return instance;
    }

    public static boolean isPowerOn() {
        return true;
    }

    public DiscoveryAgent getDiscoveryAgent() {
        return agent;
    }

    public String getFriendlyName() {
        return "J2ME-NX";
    }

    public DeviceClass getDeviceClass() {
        return new DeviceClass(0x5a020c);
    }

    public boolean setDiscoverable(int mode) throws BluetoothStateException {
        if (mode != DiscoveryAgent.GIAC && mode != DiscoveryAgent.LIAC && mode != DiscoveryAgent.NOT_DISCOVERABLE
                && (mode < 0x9E8B00 || mode > 0x9E8B3F)) {
            throw new IllegalArgumentException();
        }
        discoverable = mode;
        return true;
    }

    public int getDiscoverable() {
        return discoverable;
    }

    public static String getProperty(String property) {
        if ("bluetooth.api.version".equals(property)) {
            return "1.1";
        }
        if ("bluetooth.master.switch".equals(property) || "bluetooth.sd.trans.max".equals(property)
                || "bluetooth.connected.inquiry".equals(property) || "bluetooth.connected.page".equals(property)) {
            return "false";
        }
        if ("bluetooth.l2cap.receiveMTU.max".equals(property)) {
            return "672";
        }
        if ("bluetooth.connected.devices.max".equals(property) || "bluetooth.sd.attr.retrievable.max".equals(property)) {
            return "7";
        }
        return null;
    }

    public String getBluetoothAddress() {
        return "001122334455";
    }

    public ServiceRecord getRecord(Connection notifier) {
        throw new IllegalArgumentException("Khong phai ket noi Bluetooth");
    }

    public void updateRecord(ServiceRecord srvRecord) throws ServiceRegistrationException {
        throw new ServiceRegistrationException("Bluetooth khong ho tro");
    }
}
