package java.lang;

import java.io.ByteArrayInputStream;
import java.io.InputStream;

public final class Class {
    private long vmClass;

    private Class() {
    }

    public static native Class forName(String className) throws ClassNotFoundException;

    public Object newInstance() throws InstantiationException, IllegalAccessException {
        Object o = allocInstance();
        initInstance(o);
        return o;
    }

    private native Object allocInstance() throws InstantiationException, IllegalAccessException;

    private native void initInstance(Object o);

    public native boolean isInstance(Object obj);

    public native boolean isAssignableFrom(Class cls);

    public native boolean isInterface();

    public native boolean isArray();

    public native String getName();

    public String toString() {
        return (isInterface() ? "interface " : "class ") + getName();
    }

    public InputStream getResourceAsStream(String name) {
        if (name == null) {
            return null;
        }
        if (name.startsWith("/")) {
            name = name.substring(1);
        } else {
            String n = getName();
            int i = n.lastIndexOf('.');
            if (i >= 0) {
                name = n.substring(0, i).replace('.', '/') + "/" + name;
            }
        }
        byte[] data = getResourceData(name);
        return data == null ? null : new ByteArrayInputStream(data);
    }

    static native byte[] getResourceData(String name);
}
