package javax.bluetooth;

import java.util.Enumeration;
import java.util.Vector;

public class DataElement {
    public static final int NULL = 0x00;
    public static final int U_INT_1 = 0x08;
    public static final int U_INT_2 = 0x09;
    public static final int U_INT_4 = 0x0A;
    public static final int U_INT_8 = 0x0B;
    public static final int U_INT_16 = 0x0C;
    public static final int INT_1 = 0x10;
    public static final int INT_2 = 0x11;
    public static final int INT_4 = 0x12;
    public static final int INT_8 = 0x13;
    public static final int INT_16 = 0x14;
    public static final int URL = 0x40;
    public static final int UUID = 0x18;
    public static final int BOOL = 0x28;
    public static final int STRING = 0x20;
    public static final int DATSEQ = 0x30;
    public static final int DATALT = 0x38;

    private final int type;
    private long longValue;
    private boolean boolValue;
    private Object value;

    public DataElement(int valueType) {
        if (valueType != NULL && valueType != DATSEQ && valueType != DATALT) {
            throw new IllegalArgumentException();
        }
        type = valueType;
        if (valueType != NULL) {
            value = new Vector();
        }
    }

    public DataElement(boolean bool) {
        type = BOOL;
        boolValue = bool;
    }

    public DataElement(int valueType, long value) {
        if (valueType != U_INT_1 && valueType != U_INT_2 && valueType != U_INT_4 && valueType != INT_1
                && valueType != INT_2 && valueType != INT_4 && valueType != INT_8) {
            throw new IllegalArgumentException();
        }
        type = valueType;
        longValue = value;
    }

    public DataElement(int valueType, Object value) {
        if (value == null) {
            throw new IllegalArgumentException();
        }
        type = valueType;
        this.value = value;
    }

    private Vector seq() {
        if (type != DATSEQ && type != DATALT) {
            throw new ClassCastException();
        }
        return (Vector) value;
    }

    public void addElement(DataElement elem) {
        seq().addElement(elem);
    }

    public void insertElementAt(DataElement elem, int index) {
        seq().insertElementAt(elem, index);
    }

    public int getSize() {
        return seq().size();
    }

    public boolean removeElement(DataElement elem) {
        return seq().removeElement(elem);
    }

    public int getDataType() {
        return type;
    }

    public long getLong() {
        if (type == BOOL || value != null) {
            throw new ClassCastException();
        }
        return longValue;
    }

    public boolean getBoolean() {
        if (type != BOOL) {
            throw new ClassCastException();
        }
        return boolValue;
    }

    public Object getValue() {
        if (type == DATSEQ || type == DATALT) {
            Enumeration e = ((Vector) value).elements();
            return e;
        }
        if (value == null) {
            throw new ClassCastException();
        }
        return value;
    }
}
