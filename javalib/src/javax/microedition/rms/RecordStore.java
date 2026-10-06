package javax.microedition.rms;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.util.Hashtable;
import java.util.Vector;

// Mỗi RecordStore lưu thành 1 file trong thư mục của game (native)
public class RecordStore {
    public static final int AUTHMODE_PRIVATE = 0;
    public static final int AUTHMODE_ANY = 1;

    private static final Hashtable open = new Hashtable();

    private final String name;
    private int openCount;
    private int nextId = 1;
    private int version;
    private long lastModified;
    private final Vector ids = new Vector();
    private final Vector records = new Vector();
    private final Vector listeners = new Vector();

    private RecordStore(String name) {
        this.name = name;
    }

    public static synchronized RecordStore openRecordStore(String name, boolean create)
            throws RecordStoreException, RecordStoreFullException, RecordStoreNotFoundException {
        if (name == null || name.length() == 0 || name.length() > 32) {
            throw new IllegalArgumentException();
        }
        RecordStore rs = (RecordStore) open.get(name);
        if (rs == null) {
            rs = new RecordStore(name);
            byte[] data = load0(name);
            if (data == null) {
                if (!create) {
                    throw new RecordStoreNotFoundException(name);
                }
                rs.lastModified = System.currentTimeMillis();
                rs.save();
            } else {
                rs.parse(data);
            }
            open.put(name, rs);
        }
        rs.openCount++;
        return rs;
    }

    public static RecordStore openRecordStore(String name, boolean create, int authmode, boolean writable)
            throws RecordStoreException {
        return openRecordStore(name, create);
    }

    public static RecordStore openRecordStore(String name, String vendor, String suite) throws RecordStoreException {
        return openRecordStore(name, false);
    }

    public static synchronized void deleteRecordStore(String name) throws RecordStoreException {
        RecordStore rs = (RecordStore) open.get(name);
        if (rs != null && rs.openCount > 0) {
            throw new RecordStoreException("Record store dang mo");
        }
        if (!delete0(name)) {
            throw new RecordStoreNotFoundException(name);
        }
    }

    public static String[] listRecordStores() {
        String[] r = list0();
        return r == null || r.length == 0 ? null : r;
    }

    private void parse(byte[] data) throws RecordStoreException {
        try {
            DataInputStream in = new DataInputStream(new ByteArrayInputStream(data));
            if (in.readInt() != 0x524d5331) {
                throw new RecordStoreException("File RMS hong");
            }
            nextId = in.readInt();
            version = in.readInt();
            lastModified = in.readLong();
            int n = in.readInt();
            for (int i = 0; i < n; i++) {
                int id = in.readInt();
                int len = in.readInt();
                byte[] b = null;
                if (len >= 0) {
                    b = new byte[len];
                    in.readFully(b);
                }
                ids.addElement(new Integer(id));
                records.addElement(b);
            }
        } catch (IOException e) {
            throw new RecordStoreException("File RMS hong: " + e);
        }
    }

    private void save() throws RecordStoreException {
        try {
            ByteArrayOutputStream bo = new ByteArrayOutputStream();
            DataOutputStream out = new DataOutputStream(bo);
            out.writeInt(0x524d5331);
            out.writeInt(nextId);
            out.writeInt(version);
            out.writeLong(lastModified);
            out.writeInt(ids.size());
            for (int i = 0; i < ids.size(); i++) {
                out.writeInt(((Integer) ids.elementAt(i)).intValue());
                byte[] b = (byte[]) records.elementAt(i);
                out.writeInt(b == null ? -1 : b.length);
                if (b != null) {
                    out.write(b);
                }
            }
            if (!save0(name, bo.toByteArray())) {
                throw new RecordStoreFullException("Khong ghi duoc file");
            }
        } catch (IOException e) {
            throw new RecordStoreException(e.toString());
        }
    }

    private void checkOpen() throws RecordStoreNotOpenException {
        if (openCount <= 0) {
            throw new RecordStoreNotOpenException(name);
        }
    }

    private int indexOf(int id) throws InvalidRecordIDException {
        for (int i = 0; i < ids.size(); i++) {
            if (((Integer) ids.elementAt(i)).intValue() == id) {
                return i;
            }
        }
        throw new InvalidRecordIDException(String.valueOf(id));
    }

    private void changed(int id, int kind) throws RecordStoreException {
        version++;
        lastModified = System.currentTimeMillis();
        save();
        for (int i = 0; i < listeners.size(); i++) {
            RecordListener l = (RecordListener) listeners.elementAt(i);
            if (kind == 0) {
                l.recordAdded(this, id);
            } else if (kind == 1) {
                l.recordChanged(this, id);
            } else {
                l.recordDeleted(this, id);
            }
        }
    }

    public synchronized void closeRecordStore() throws RecordStoreNotOpenException, RecordStoreException {
        checkOpen();
        if (--openCount == 0) {
            synchronized (RecordStore.class) {
                open.remove(name);
            }
            listeners.removeAllElements();
        }
    }

    public String getName() throws RecordStoreNotOpenException {
        checkOpen();
        return name;
    }

    public int getVersion() throws RecordStoreNotOpenException {
        checkOpen();
        return version;
    }

    public int getNumRecords() throws RecordStoreNotOpenException {
        checkOpen();
        return ids.size();
    }

    public int getSize() throws RecordStoreNotOpenException {
        checkOpen();
        int s = 32;
        for (int i = 0; i < records.size(); i++) {
            byte[] b = (byte[]) records.elementAt(i);
            s += 8 + (b == null ? 0 : b.length);
        }
        return s;
    }

    public int getSizeAvailable() throws RecordStoreNotOpenException {
        checkOpen();
        return 4 * 1024 * 1024 - getSize();
    }

    public long getLastModified() throws RecordStoreNotOpenException {
        checkOpen();
        return lastModified;
    }

    public void addRecordListener(RecordListener l) {
        if (!listeners.contains(l)) {
            listeners.addElement(l);
        }
    }

    public void removeRecordListener(RecordListener l) {
        listeners.removeElement(l);
    }

    public int getNextRecordID() throws RecordStoreNotOpenException, RecordStoreException {
        checkOpen();
        return nextId;
    }

    public synchronized int addRecord(byte[] data, int offset, int numBytes)
            throws RecordStoreNotOpenException, RecordStoreException, RecordStoreFullException {
        checkOpen();
        byte[] b = null;
        if (data != null) {
            b = new byte[numBytes];
            System.arraycopy(data, offset, b, 0, numBytes);
        } else if (numBytes > 0) {
            throw new NullPointerException();
        }
        int id = nextId++;
        ids.addElement(new Integer(id));
        records.addElement(b);
        changed(id, 0);
        return id;
    }

    public synchronized void deleteRecord(int recordId)
            throws RecordStoreNotOpenException, InvalidRecordIDException, RecordStoreException {
        checkOpen();
        int i = indexOf(recordId);
        ids.removeElementAt(i);
        records.removeElementAt(i);
        changed(recordId, 2);
    }

    public int getRecordSize(int recordId) throws RecordStoreNotOpenException, InvalidRecordIDException,
            RecordStoreException {
        checkOpen();
        byte[] b = (byte[]) records.elementAt(indexOf(recordId));
        return b == null ? 0 : b.length;
    }

    public int getRecord(int recordId, byte[] buffer, int offset)
            throws RecordStoreNotOpenException, InvalidRecordIDException, RecordStoreException {
        checkOpen();
        byte[] b = (byte[]) records.elementAt(indexOf(recordId));
        if (b == null) {
            return 0;
        }
        System.arraycopy(b, 0, buffer, offset, b.length);
        return b.length;
    }

    public byte[] getRecord(int recordId) throws RecordStoreNotOpenException, InvalidRecordIDException,
            RecordStoreException {
        checkOpen();
        byte[] b = (byte[]) records.elementAt(indexOf(recordId));
        if (b == null) {
            return null;
        }
        byte[] r = new byte[b.length];
        System.arraycopy(b, 0, r, 0, b.length);
        return r;
    }

    public synchronized void setRecord(int recordId, byte[] newData, int offset, int numBytes)
            throws RecordStoreNotOpenException, InvalidRecordIDException, RecordStoreException,
            RecordStoreFullException {
        checkOpen();
        int i = indexOf(recordId);
        byte[] b = null;
        if (newData != null) {
            b = new byte[numBytes];
            System.arraycopy(newData, offset, b, 0, numBytes);
        }
        records.setElementAt(b, i);
        changed(recordId, 1);
    }

    public RecordEnumeration enumerateRecords(RecordFilter filter, RecordComparator comparator,
            boolean keepUpdated) throws RecordStoreNotOpenException {
        checkOpen();
        return new RecordEnumerationImpl(this, filter, comparator);
    }

    public void setMode(int authmode, boolean writable) throws RecordStoreException {
    }

    // Danh sách id hiện có (cho RecordEnumerationImpl)
    synchronized int[] idsSnapshot() {
        int[] r = new int[ids.size()];
        for (int i = 0; i < r.length; i++) {
            r[i] = ((Integer) ids.elementAt(i)).intValue();
        }
        return r;
    }

    private static native byte[] load0(String name);

    private static native boolean save0(String name, byte[] data);

    private static native boolean delete0(String name);

    private static native String[] list0();
}
