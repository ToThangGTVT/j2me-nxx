package javax.bluetooth;

// Không có thiết bị Bluetooth nào: tìm kiếm luôn kết thúc mà không thấy gì
public class DiscoveryAgent {
    public static final int NOT_DISCOVERABLE = 0;
    public static final int GIAC = 0x9E8B33;
    public static final int LIAC = 0x9E8B00;
    public static final int CACHED = 0x00;
    public static final int PREKNOWN = 0x01;

    private static int nextTrans = 1;
    private DiscoveryListener inquiryListener;

    DiscoveryAgent() {
    }

    public RemoteDevice[] retrieveDevices(int option) {
        if (option != CACHED && option != PREKNOWN) {
            throw new IllegalArgumentException();
        }
        return null;
    }

    public boolean startInquiry(int accessCode, final DiscoveryListener listener) throws BluetoothStateException {
        if (listener == null) {
            throw new NullPointerException();
        }
        if (accessCode != GIAC && accessCode != LIAC && (accessCode < 0x9E8B00 || accessCode > 0x9E8B3F)) {
            throw new IllegalArgumentException();
        }
        if (inquiryListener != null) {
            throw new BluetoothStateException("Inquiry dang chay");
        }
        inquiryListener = listener;
        new Thread() {
            public void run() {
                try {
                    Thread.sleep(1500);
                } catch (InterruptedException e) {
                    // bỏ qua
                }
                DiscoveryListener l = inquiryListener;
                if (l != null) {
                    inquiryListener = null;
                    l.inquiryCompleted(DiscoveryListener.INQUIRY_COMPLETED);
                }
            }
        }.start();
        return true;
    }

    public boolean cancelInquiry(DiscoveryListener listener) {
        if (listener == null) {
            throw new NullPointerException();
        }
        if (inquiryListener != listener) {
            return false;
        }
        inquiryListener = null;
        listener.inquiryCompleted(DiscoveryListener.INQUIRY_TERMINATED);
        return true;
    }

    public int searchServices(int[] attrSet, UUID[] uuidSet, RemoteDevice btDev, final DiscoveryListener discListener)
            throws BluetoothStateException {
        if (uuidSet == null || btDev == null || discListener == null) {
            throw new NullPointerException();
        }
        final int trans;
        synchronized (DiscoveryAgent.class) {
            trans = nextTrans++;
        }
        new Thread() {
            public void run() {
                discListener.serviceSearchCompleted(trans, DiscoveryListener.SERVICE_SEARCH_DEVICE_NOT_REACHABLE);
            }
        }.start();
        return trans;
    }

    public boolean cancelServiceSearch(int transID) {
        return false;
    }

    public String selectService(UUID uuid, int security, boolean master) throws BluetoothStateException {
        if (uuid == null) {
            throw new NullPointerException();
        }
        return null;
    }
}
