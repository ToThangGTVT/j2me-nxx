package javax.microedition.rms;

import java.util.Vector;

class RecordEnumerationImpl implements RecordEnumeration {
    private final RecordStore store;
    private final RecordFilter filter;
    private final RecordComparator comparator;
    private int[] list;
    private int index;
    private boolean destroyed;

    RecordEnumerationImpl(RecordStore store, RecordFilter filter, RecordComparator comparator) {
        this.store = store;
        this.filter = filter;
        this.comparator = comparator;
        rebuild();
    }

    public void rebuild() {
        int[] all = store.idsSnapshot();
        Vector ids = new Vector();
        Vector data = new Vector();
        for (int i = 0; i < all.length; i++) {
            byte[] b;
            try {
                b = store.getRecord(all[i]);
            } catch (RecordStoreException e) {
                continue;
            }
            if (filter != null && !filter.matches(b)) {
                continue;
            }
            int pos = ids.size();
            if (comparator != null) {
                while (pos > 0 && comparator.compare((byte[]) data.elementAt(pos - 1), b) == RecordComparator.FOLLOWS) {
                    pos--;
                }
            }
            ids.insertElementAt(new Integer(all[i]), pos);
            data.insertElementAt(b, pos);
        }
        list = new int[ids.size()];
        for (int i = 0; i < list.length; i++) {
            list[i] = ((Integer) ids.elementAt(i)).intValue();
        }
        index = 0;
    }

    private void check() {
        if (destroyed) {
            throw new IllegalStateException();
        }
    }

    public int numRecords() {
        check();
        return list.length;
    }

    public byte[] nextRecord() throws InvalidRecordIDException, RecordStoreNotOpenException, RecordStoreException {
        return store.getRecord(nextRecordId());
    }

    public int nextRecordId() throws InvalidRecordIDException {
        check();
        if (index >= list.length) {
            throw new InvalidRecordIDException();
        }
        return list[index++];
    }

    public byte[] previousRecord() throws InvalidRecordIDException, RecordStoreNotOpenException,
            RecordStoreException {
        return store.getRecord(previousRecordId());
    }

    public int previousRecordId() throws InvalidRecordIDException {
        check();
        if (index <= 0) {
            index = list.length;
        }
        if (index <= 0) {
            throw new InvalidRecordIDException();
        }
        return list[--index];
    }

    public boolean hasNextElement() {
        check();
        return index < list.length;
    }

    public boolean hasPreviousElement() {
        check();
        return index > 0;
    }

    public void reset() {
        check();
        index = 0;
    }

    public void keepUpdated(boolean keepUpdated) {
    }

    public boolean isKeptUpdated() {
        return false;
    }

    public void destroy() {
        destroyed = true;
    }
}
