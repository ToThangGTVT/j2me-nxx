package j2menx;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.Enumeration;
import java.util.Vector;
import javax.microedition.io.Connector;
import javax.microedition.io.file.ConnectionClosedException;
import javax.microedition.io.file.FileConnection;
import javax.microedition.io.file.IllegalModeException;

public class FileConn implements FileConnection {
    private static final long TOTAL = 512L * 1024 * 1024;

    private String virt;        // vd "E:/game/save.dat" hoặc "E:/game/"
    private String host;
    private final int mode;
    private boolean closed;

    static {
        FileIO.ensureRoots();
    }

    public FileConn(String url, int mode) throws IOException {
        this.mode = mode;
        if (!url.startsWith("file:///")) {
            throw new IllegalArgumentException("URL khong hop le: " + url);
        }
        setVirt(decode(url.substring(8)));
    }

    private void setVirt(String v) throws IOException {
        String h = FileIO.hostPath(v);
        if (h == null) {
            throw new IllegalArgumentException("Duong dan khong hop le: " + v);
        }
        virt = v;
        host = h.endsWith("/") ? h.substring(0, h.length() - 1) : h;
        String root = v.indexOf('/') >= 0 ? v.substring(0, v.indexOf('/') + 1) : v + "/";
        boolean known = false;
        for (int i = 0; i < FileIO.ROOTS.length; i++) {
            known |= FileIO.ROOTS[i].equalsIgnoreCase(root);
        }
        if (!known) {
            throw new javax.microedition.io.ConnectionNotFoundException("Khong co o dia " + root);
        }
    }

    private static String decode(String s) {
        StringBuffer sb = new StringBuffer();
        for (int i = 0; i < s.length(); i++) {
            char c = s.charAt(i);
            if (c == '%' && i + 2 < s.length()) {
                try {
                    sb.append((char) Integer.parseInt(s.substring(i + 1, i + 3), 16));
                    i += 2;
                    continue;
                } catch (NumberFormatException e) {
                    // giữ nguyên
                }
            }
            sb.append(c);
        }
        return sb.toString();
    }

    private void checkOpen() throws IOException {
        if (closed) {
            throw new ConnectionClosedException();
        }
    }

    private void checkRead() throws IOException {
        checkOpen();
        if (mode == Connector.WRITE) {
            throw new IllegalModeException();
        }
    }

    private void checkWrite() throws IOException {
        checkOpen();
        if (mode == Connector.READ) {
            throw new IllegalModeException();
        }
    }

    public boolean isOpen() {
        return !closed;
    }

    public InputStream openInputStream() throws IOException {
        checkRead();
        byte[] data = FileIO.read0(host);
        if (data == null) {
            throw new IOException("Khong doc duoc " + virt);
        }
        return new ByteArrayInputStream(data);
    }

    public DataInputStream openDataInputStream() throws IOException {
        return new DataInputStream(openInputStream());
    }

    public OutputStream openOutputStream() throws IOException {
        return openOutputStream(0);
    }

    public DataOutputStream openDataOutputStream() throws IOException {
        return new DataOutputStream(openOutputStream());
    }

    // Ghi vào bộ đệm; đẩy xuống file khi flush / close
    public OutputStream openOutputStream(final long byteOffset) throws IOException {
        checkWrite();
        if (!FileIO.exists0(host) || FileIO.isDir0(host)) {
            throw new IOException("File khong ton tai: " + virt);
        }
        return new ByteArrayOutputStream() {
            private long pos = byteOffset;

            public synchronized void flush() throws IOException {
                if (count > 0) {
                    if (!FileIO.write0(host, buf, count, pos, false)) {
                        throw new IOException("Khong ghi duoc " + virt);
                    }
                    pos += count;
                    count = 0;
                }
            }

            public synchronized void close() {
                try {
                    flush();
                } catch (IOException e) {
                    // bỏ qua
                }
            }
        };
    }

    public long totalSize() { return TOTAL; }
    public long availableSize() { return TOTAL / 2; }
    public long usedSize() { return TOTAL / 2; }

    public long directorySize(boolean includeSubDirs) throws IOException {
        checkOpen();
        if (!isDirectory()) {
            throw new IOException("Khong phai thu muc");
        }
        String[] names = FileIO.list0(host);
        long total = 0;
        for (int i = 0; names != null && i < names.length; i++) {
            if (!names[i].endsWith("/")) {
                total += FileIO.size0(host + "/" + names[i]);
            }
        }
        return total;
    }

    public long fileSize() throws IOException {
        checkRead();
        if (isDirectory()) {
            throw new IOException("La thu muc");
        }
        return FileIO.exists0(host) ? FileIO.size0(host) : -1;
    }

    public boolean canRead() { return exists(); }
    public boolean canWrite() { return exists(); }
    public boolean isHidden() { return getName().startsWith("."); }
    public void setReadable(boolean r) throws IOException { checkWrite(); }
    public void setWritable(boolean w) throws IOException { checkWrite(); }
    public void setHidden(boolean h) throws IOException { checkWrite(); }

    public Enumeration list() throws IOException {
        return list("*", false);
    }

    public Enumeration list(String filter, boolean includeHidden) throws IOException {
        checkRead();
        if (!isDirectory()) {
            throw new IOException("Khong phai thu muc: " + virt);
        }
        String[] names = FileIO.list0(host);
        Vector v = new Vector();
        for (int i = 0; names != null && i < names.length; i++) {
            String n = names[i];
            if (!includeHidden && n.startsWith(".")) {
                continue;
            }
            if (matches(filter, n.endsWith("/") ? n.substring(0, n.length() - 1) : n)) {
                v.addElement(n);
            }
        }
        return v.elements();
    }

    // Lọc kiểu "*.txt" (chỉ hỗ trợ dấu *)
    private static boolean matches(String filter, String name) {
        if (filter == null || filter.equals("*")) {
            return true;
        }
        String f = filter.toLowerCase(), n = name.toLowerCase();
        int star = f.indexOf('*');
        if (star < 0) {
            return f.equals(n);
        }
        return n.startsWith(f.substring(0, star)) && n.endsWith(f.substring(star + 1))
                && n.length() >= f.length() - 1;
    }

    public void create() throws IOException {
        checkWrite();
        if (virt.endsWith("/")) {
            throw new IOException("Ten file khong hop le");
        }
        if (FileIO.exists0(host)) {
            throw new IOException("File da ton tai: " + virt);
        }
        if (!FileIO.create0(host)) {
            throw new IOException("Khong tao duoc " + virt);
        }
    }

    public void mkdir() throws IOException {
        checkWrite();
        if (FileIO.exists0(host)) {
            throw new IOException("Da ton tai: " + virt);
        }
        if (!FileIO.mkdir0(host)) {
            throw new IOException("Khong tao duoc thu muc " + virt);
        }
    }

    public boolean exists() {
        return !closed && FileIO.exists0(host);
    }

    public boolean isDirectory() {
        return !closed && FileIO.isDir0(host);
    }

    public void delete() throws IOException {
        checkWrite();
        if (!FileIO.delete0(host)) {
            throw new IOException("Khong xoa duoc " + virt);
        }
    }

    public void rename(String newName) throws IOException {
        checkWrite();
        if (newName == null) {
            throw new NullPointerException();
        }
        if (newName.indexOf('/') >= 0) {
            throw new IllegalArgumentException();
        }
        String parentVirt = virt.endsWith("/") ? virt.substring(0, virt.length() - 1) : virt;
        parentVirt = parentVirt.substring(0, parentVirt.lastIndexOf('/') + 1);
        String newVirt = parentVirt + newName + (virt.endsWith("/") ? "/" : "");
        String newHost = FileIO.hostPath(newVirt);
        if (newHost == null || !FileIO.rename0(host, newHost.endsWith("/") ? newHost.substring(0, newHost.length() - 1) : newHost)) {
            throw new IOException("Khong doi ten duoc " + virt);
        }
        setVirt(newVirt);
    }

    public void truncate(long byteOffset) throws IOException {
        checkWrite();
        if (!FileIO.truncate0(host, byteOffset)) {
            throw new IOException("Khong cat duoc " + virt);
        }
    }

    public void setFileConnection(String fileName) throws IOException {
        checkOpen();
        if (!isDirectory()) {
            throw new IOException("Ket noi hien tai khong phai thu muc");
        }
        String base = virt.endsWith("/") ? virt : virt + "/";
        if (fileName.equals("..")) {
            String p = base.substring(0, base.length() - 1);
            int i = p.lastIndexOf('/');
            if (i < 0) {
                throw new IOException("Da o goc");
            }
            setVirt(p.substring(0, i + 1));
        } else {
            setVirt(base + fileName);
        }
    }

    public String getName() {
        String p = virt.endsWith("/") ? virt.substring(0, virt.length() - 1) : virt;
        int i = p.lastIndexOf('/');
        String n = i >= 0 ? p.substring(i + 1) : "";
        return virt.endsWith("/") && n.length() > 0 ? n + "/" : n;
    }

    public String getPath() {
        String p = virt.endsWith("/") ? virt.substring(0, virt.length() - 1) : virt;
        int i = p.lastIndexOf('/');
        return "/" + (i >= 0 ? p.substring(0, i + 1) : p + "/");
    }

    public String getURL() {
        return "file:///" + virt;
    }

    public long lastModified() {
        return FileIO.exists0(host) ? FileIO.modified0(host) : 0;
    }

    public DataInputStream openDataInputStream0() throws IOException {
        return openDataInputStream();
    }

    public void close() {
        closed = true;
    }
}
