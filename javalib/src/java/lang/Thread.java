package java.lang;

public class Thread implements Runnable {
    public static final int MIN_PRIORITY = 1;
    public static final int NORM_PRIORITY = 5;
    public static final int MAX_PRIORITY = 10;

    private static int counter;

    private long vmThread;
    private Runnable target;
    private String name;
    private int priority = NORM_PRIORITY;
    private boolean started;

    public Thread() {
        this(null, null);
    }

    public Thread(Runnable target) {
        this(target, null);
    }

    public Thread(String name) {
        this(null, name);
    }

    public Thread(Runnable target, String name) {
        this.target = target;
        this.name = name != null ? name : "Thread-" + (counter++);
    }

    public void run() {
        if (target != null) {
            target.run();
        }
    }

    public synchronized void start() {
        if (started) {
            throw new IllegalThreadStateException();
        }
        started = true;
        start0();
    }

    private native void start0();

    public static native Thread currentThread();

    public static native void sleep(long millis) throws InterruptedException;

    public static native void yield();

    public static native int activeCount();

    public final native boolean isAlive();

    public void interrupt() {
        interrupt0();
    }

    private native void interrupt0();

    public final void join() throws InterruptedException {
        synchronized (this) {
            while (isAlive()) {
                wait(0);
            }
        }
    }

    public final void setPriority(int p) {
        if (p < MIN_PRIORITY || p > MAX_PRIORITY) {
            throw new IllegalArgumentException();
        }
        priority = p;
    }

    public final int getPriority() {
        return priority;
    }

    public final String getName() {
        return name;
    }

    public String toString() {
        return "Thread[" + name + "," + priority + "]";
    }
}
