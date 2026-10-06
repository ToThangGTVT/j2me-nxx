package j2menx;

import java.io.IOException;

// Truy cập file thật trong thư mục sandbox của game (native: source/midp/fileio.c).
// Đường dẫn ảo "C:/a/b.txt" -> <j2menx.files>/c/a/b.txt
public final class FileIO {
    public static final String[] ROOTS = { "C:/", "E:/" };

    private FileIO() {
    }

    // Đổi đường dẫn ảo (root + phần còn lại, không có "file:///") sang đường dẫn thật; null nếu không hợp lệ
    public static String hostPath(String virt) {
        int slash = virt.indexOf('/');
        String root = slash >= 0 ? virt.substring(0, slash) : virt;
        String rest = slash >= 0 ? virt.substring(slash + 1) : "";
        String r = root.toLowerCase();
        if (r.endsWith(":")) {
            r = r.substring(0, r.length() - 1);
        }
        if (r.length() == 0 || r.indexOf('.') >= 0 || r.indexOf('\\') >= 0) {
            return null;
        }
        // Chặn thoát khỏi sandbox
        String check = "/" + rest + "/";
        if (check.indexOf("/../") >= 0 || check.indexOf("/./") >= 0 || rest.indexOf('\\') >= 0) {
            return null;
        }
        String base = System.getProperty("j2menx.files");
        if (base == null) {
            return null;
        }
        return base + "/" + r + (rest.length() > 0 ? "/" + rest : "");
    }

    static void ensureRoots() {
        String base = System.getProperty("j2menx.files");
        if (base != null) {
            mkdirs0(base);
            for (int i = 0; i < ROOTS.length; i++) {
                mkdirs0(base + "/" + ROOTS[i].substring(0, 1).toLowerCase());
            }
        }
    }

    public static native boolean exists0(String path);
    public static native boolean isDir0(String path);
    public static native long size0(String path);
    public static native long modified0(String path);
    public static native String[] list0(String path);
    public static native boolean mkdir0(String path);
    public static native boolean mkdirs0(String path);
    public static native boolean create0(String path);
    public static native boolean delete0(String path);
    public static native boolean rename0(String from, String to);
    public static native byte[] read0(String path) throws IOException;
    // Ghi data tại offset; truncate = cắt file sau phần vừa ghi
    public static native boolean write0(String path, byte[] data, int len, long offset, boolean truncate);
    public static native boolean truncate0(String path, long len);
}
