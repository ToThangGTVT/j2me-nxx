package java.lang;

public class Runtime {
    private static final Runtime instance = new Runtime();

    private Runtime() {
    }

    public static Runtime getRuntime() {
        return instance;
    }

    public void exit(int status) {
        System.exit(status);
    }

    public native long freeMemory();

    public native long totalMemory();

    public void gc() {
        System.gc();
    }
}
