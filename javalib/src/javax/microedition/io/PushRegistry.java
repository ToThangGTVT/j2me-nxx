package javax.microedition.io;

import java.io.IOException;
import java.util.Enumeration;
import java.util.Hashtable;

// Không có tiến trình nền để đánh thức MIDlet: chỉ ghi nhớ đăng ký trong phiên chạy
public class PushRegistry {
    private static final Hashtable midlets = new Hashtable();
    private static final Hashtable filters = new Hashtable();
    private static final Hashtable alarms = new Hashtable();

    private PushRegistry() {
    }

    public static void registerConnection(String connection, String midlet, String filter)
            throws ClassNotFoundException, IOException {
        if (connection == null || midlet == null || filter == null) {
            throw new IllegalArgumentException();
        }
        Class.forName(midlet);
        midlets.put(connection, midlet);
        filters.put(connection, filter);
    }

    public static boolean unregisterConnection(String connection) {
        if (connection == null) {
            return false;
        }
        filters.remove(connection);
        return midlets.remove(connection) != null;
    }

    public static String[] listConnections(boolean available) {
        // available = true: chỉ các kết nối đang có dữ liệu chờ (không bao giờ có)
        if (available) {
            return new String[0];
        }
        String[] r = new String[midlets.size()];
        int i = 0;
        for (Enumeration e = midlets.keys(); e.hasMoreElements();) {
            r[i++] = (String) e.nextElement();
        }
        return r;
    }

    public static String getMIDlet(String connection) {
        return connection == null ? null : (String) midlets.get(connection);
    }

    public static String getFilter(String connection) {
        return connection == null ? null : (String) filters.get(connection);
    }

    public static long registerAlarm(String midlet, long time) throws ClassNotFoundException,
            ConnectionNotFoundException {
        if (midlet == null) {
            throw new IllegalArgumentException();
        }
        Class.forName(midlet);
        Long old = (Long) alarms.put(midlet, new Long(time));
        return old == null ? 0 : old.longValue();
    }
}
