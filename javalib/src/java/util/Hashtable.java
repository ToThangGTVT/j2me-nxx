package java.util;

public class Hashtable {
    private static class Entry {
        final int hash;
        final Object key;
        Object value;
        Entry next;

        Entry(int hash, Object key, Object value, Entry next) {
            this.hash = hash;
            this.key = key;
            this.value = value;
            this.next = next;
        }
    }

    private Entry[] table;
    private int count;
    private int threshold;

    public Hashtable() {
        this(11);
    }

    public Hashtable(int initialCapacity) {
        if (initialCapacity < 0) {
            throw new IllegalArgumentException();
        }
        if (initialCapacity == 0) {
            initialCapacity = 1;
        }
        table = new Entry[initialCapacity];
        threshold = initialCapacity * 3 / 4;
    }

    public int size() {
        return count;
    }

    public boolean isEmpty() {
        return count == 0;
    }

    private static int index(int hash, int len) {
        return (hash & 0x7fffffff) % len;
    }

    public synchronized boolean contains(Object value) {
        if (value == null) {
            throw new NullPointerException();
        }
        for (int i = 0; i < table.length; i++) {
            for (Entry e = table[i]; e != null; e = e.next) {
                if (e.value.equals(value)) {
                    return true;
                }
            }
        }
        return false;
    }

    public synchronized boolean containsKey(Object key) {
        int h = key.hashCode();
        for (Entry e = table[index(h, table.length)]; e != null; e = e.next) {
            if (e.hash == h && e.key.equals(key)) {
                return true;
            }
        }
        return false;
    }

    public synchronized Object get(Object key) {
        int h = key.hashCode();
        for (Entry e = table[index(h, table.length)]; e != null; e = e.next) {
            if (e.hash == h && e.key.equals(key)) {
                return e.value;
            }
        }
        return null;
    }

    protected void rehash() {
        Entry[] old = table;
        Entry[] n = new Entry[old.length * 2 + 1];
        for (int i = 0; i < old.length; i++) {
            Entry e = old[i];
            while (e != null) {
                Entry next = e.next;
                int idx = index(e.hash, n.length);
                e.next = n[idx];
                n[idx] = e;
                e = next;
            }
        }
        table = n;
        threshold = n.length * 3 / 4;
    }

    public synchronized Object put(Object key, Object value) {
        if (key == null || value == null) {
            throw new NullPointerException();
        }
        int h = key.hashCode();
        int idx = index(h, table.length);
        for (Entry e = table[idx]; e != null; e = e.next) {
            if (e.hash == h && e.key.equals(key)) {
                Object old = e.value;
                e.value = value;
                return old;
            }
        }
        if (count >= threshold) {
            rehash();
            idx = index(h, table.length);
        }
        table[idx] = new Entry(h, key, value, table[idx]);
        count++;
        return null;
    }

    public synchronized Object remove(Object key) {
        int h = key.hashCode();
        int idx = index(h, table.length);
        Entry prev = null;
        for (Entry e = table[idx]; e != null; prev = e, e = e.next) {
            if (e.hash == h && e.key.equals(key)) {
                if (prev != null) {
                    prev.next = e.next;
                } else {
                    table[idx] = e.next;
                }
                count--;
                return e.value;
            }
        }
        return null;
    }

    public synchronized void clear() {
        for (int i = 0; i < table.length; i++) {
            table[i] = null;
        }
        count = 0;
    }

    private Enumeration enumerate(final boolean keys) {
        final Entry[] t = table;
        return new Enumeration() {
            int index = t.length;
            Entry entry;

            public boolean hasMoreElements() {
                while (entry == null && index > 0) {
                    entry = t[--index];
                }
                return entry != null;
            }

            public Object nextElement() {
                if (!hasMoreElements()) {
                    throw new NoSuchElementException();
                }
                Entry e = entry;
                entry = e.next;
                return keys ? e.key : e.value;
            }
        };
    }

    public synchronized Enumeration keys() {
        return enumerate(true);
    }

    public synchronized Enumeration elements() {
        return enumerate(false);
    }

    public synchronized String toString() {
        StringBuffer sb = new StringBuffer("{");
        boolean first = true;
        for (int i = 0; i < table.length; i++) {
            for (Entry e = table[i]; e != null; e = e.next) {
                if (!first) {
                    sb.append(", ");
                }
                first = false;
                sb.append(String.valueOf(e.key)).append('=').append(String.valueOf(e.value));
            }
        }
        return sb.append('}').toString();
    }
}
