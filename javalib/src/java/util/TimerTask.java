package java.util;

public abstract class TimerTask implements Runnable {
    Object lock = new Object();
    boolean cancelled;
    boolean scheduled;
    long nextTime;
    long period;
    boolean fixedRate;

    protected TimerTask() {
    }

    public abstract void run();

    public boolean cancel() {
        synchronized (lock) {
            boolean r = scheduled && !cancelled && (period > 0 || nextTime > 0);
            cancelled = true;
            return r;
        }
    }

    public long scheduledExecutionTime() {
        synchronized (lock) {
            return period < 0 ? nextTime + period : nextTime - period;
        }
    }
}
