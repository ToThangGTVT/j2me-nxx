package j2menx;

// Ngôn ngữ giao diện MIDP theo cài đặt của app (thuộc tính j2menx.lang)
public final class Lang {
    private static int en = -1;

    private Lang() {
    }

    public static boolean english() {
        if (en < 0) {
            en = "en".equals(System.getProperty("j2menx.lang")) ? 1 : 0;
        }
        return en == 1;
    }

    public static String t(String vi, String english) {
        return english() ? english : vi;
    }
}
