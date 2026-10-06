package com.siemens.mp.io;

import j2menx.FileIO;
import java.io.IOException;

// File API của Siemens, lưu ở ổ C: trong sandbox của game
public class File {
    private static final int MAX = 16;
    private static final String[] paths = new String[MAX];
    private static final int[] positions = new int[MAX];

    public File() {
    }

    private static String host(String name) throws IOException {
        while (name.startsWith("/")) {
            name = name.substring(1);
        }
        String h = FileIO.hostPath("C:/" + name);
        if (h == null) {
            throw new IOException("Ten file khong hop le: " + name);
        }
        return h;
    }

    public int open(String fileName) throws IOException {
        String h = host(fileName);
        if (!FileIO.exists0(h)) {
            FileIO.create0(h);
        }
        for (int i = 0; i < MAX; i++) {
            if (paths[i] == null) {
                paths[i] = h;
                positions[i] = 0;
                return i;
            }
        }
        throw new IOException("Qua nhieu file dang mo");
    }

    private static String path(int fd) throws IOException {
        if (fd < 0 || fd >= MAX || paths[fd] == null) {
            throw new IOException("File chua mo");
        }
        return paths[fd];
    }

    public int close(int fd) throws IOException {
        path(fd);
        paths[fd] = null;
        return 0;
    }

    public int read(int fd, byte[] buf, int offset, int numBytes) throws IOException {
        byte[] data = FileIO.read0(path(fd));
        if (data == null) {
            return -1;
        }
        int n = Math.min(numBytes, data.length - positions[fd]);
        if (n <= 0) {
            return -1;
        }
        System.arraycopy(data, positions[fd], buf, offset, n);
        positions[fd] += n;
        return n;
    }

    public int write(int fd, byte[] buf, int offset, int numBytes) throws IOException {
        byte[] part = new byte[numBytes];
        System.arraycopy(buf, offset, part, 0, numBytes);
        if (!FileIO.write0(path(fd), part, numBytes, positions[fd], false)) {
            throw new IOException("Khong ghi duoc");
        }
        positions[fd] += numBytes;
        return numBytes;
    }

    public int seek(int fd, int seekpos) throws IOException {
        path(fd);
        positions[fd] = seekpos;
        return seekpos;
    }

    public int length(int fd) throws IOException {
        return (int) FileIO.size0(path(fd));
    }

    public static int exists(String fileName) throws IOException {
        return FileIO.exists0(host(fileName)) ? 1 : -1;
    }

    public static int delete(String fileName) throws IOException {
        return FileIO.delete0(host(fileName)) ? 1 : -1;
    }

    public static int rename(String source, String dest) throws IOException {
        return FileIO.rename0(host(source), host(dest)) ? 1 : -1;
    }

    public static int copy(String source, String dest) throws IOException {
        byte[] d = FileIO.read0(host(source));
        if (d == null) {
            return -1;
        }
        String h = host(dest);
        FileIO.create0(h);
        return FileIO.write0(h, d, d.length, 0, true) ? 1 : -1;
    }

    public static int spaceAvailable() {
        return 4 * 1024 * 1024;
    }

    public static boolean isDirectory(String pathName) throws IOException {
        return FileIO.isDir0(host(pathName));
    }
}
