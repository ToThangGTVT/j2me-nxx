package javax.bluetooth;

public class UUID {
    private static final String BASE = "0000000000001000800000805F9B34FB";
    private final String hex;   // 32 chữ số hex, viết hoa

    public UUID(long uuidValue) {
        if (uuidValue < 0 || uuidValue > 0xffffffffL) {
            throw new IllegalArgumentException();
        }
        String v = Long.toHexString(uuidValue).toUpperCase();
        while (v.length() < 8) {
            v = "0" + v;
        }
        hex = v + BASE.substring(8);
    }

    public UUID(String uuidValue, boolean shortUUID) {
        if (uuidValue == null) {
            throw new NullPointerException();
        }
        int len = uuidValue.length();
        if (len == 0 || (shortUUID && len > 8) || (!shortUUID && len > 32)) {
            throw new IllegalArgumentException();
        }
        for (int i = 0; i < len; i++) {
            if (Character.digit(uuidValue.charAt(i), 16) < 0) {
                throw new NumberFormatException(uuidValue);
            }
        }
        String v = uuidValue.toUpperCase();
        if (shortUUID) {
            while (v.length() < 8) {
                v = "0" + v;
            }
            hex = v + BASE.substring(8);
        } else {
            while (v.length() < 32) {
                v = "0" + v;
            }
            hex = v;
        }
    }

    public String toString() {
        // Bỏ số 0 ở đầu như cài đặt gốc
        int i = 0;
        while (i < hex.length() - 1 && hex.charAt(i) == '0') {
            i++;
        }
        return hex.substring(i);
    }

    public boolean equals(Object o) {
        return o instanceof UUID && ((UUID) o).hex.equals(hex);
    }

    public int hashCode() {
        return hex.hashCode();
    }
}
