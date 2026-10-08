package j2menx;

// Ngôn ngữ giao diện MIDP theo cài đặt của app (thuộc tính j2menx.lang).
// Chuỗi từng ngôn ngữ nằm ở LangVi / LangEn, đúng thứ tự các hằng số dưới đây.
public final class Lang {
    public static final int CREATE_MIDLET_FAILED = 0;
    public static final int START_APP_FAILED = 1;
    public static final int EDIT = 2;
    public static final int SELECT = 3;
    public static final int CANCEL = 4;
    public static final int ERROR = 5;
    static final int COUNT = 6;

    private static String[] strings;

    private Lang() {
    }

    public static boolean english() {
        return table() == LangEn.STRINGS;
    }

    public static String t(int id) {
        return table()[id];
    }

    private static String[] table() {
        if (strings == null) {
            strings = "en".equals(System.getProperty("j2menx.lang")) ? LangEn.STRINGS : LangVi.STRINGS;
        }
        return strings;
    }
}
