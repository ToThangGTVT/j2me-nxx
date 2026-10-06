package j2menx;

// Bàn phím ảo của hệ thống (swkbd trên Switch)
public final class Keyboard {
    private Keyboard() {
    }

    // Trả về chuỗi mới, hoặc null nếu người dùng huỷ
    public static String show(String title, String text, int maxSize, int constraints) {
        return show0(title == null ? "" : title, text == null ? "" : text, maxSize, constraints & 0xffff);
    }

    private static native String show0(String title, String text, int maxSize, int type);
}
