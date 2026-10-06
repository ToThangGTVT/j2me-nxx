package javax.microedition.io.file;

import java.util.Enumeration;
import java.util.Vector;

public class FileSystemRegistry {
    private FileSystemRegistry() {
    }

    public static boolean addFileSystemListener(FileSystemListener listener) {
        if (listener == null) {
            throw new NullPointerException();
        }
        return true;
    }

    public static boolean removeFileSystemListener(FileSystemListener listener) {
        if (listener == null) {
            throw new NullPointerException();
        }
        return true;
    }

    public static Enumeration listRoots() {
        Vector v = new Vector();
        String[] roots = j2menx.FileIO.ROOTS;
        for (int i = 0; i < roots.length; i++) {
            v.addElement(roots[i]);
        }
        return v.elements();
    }
}
