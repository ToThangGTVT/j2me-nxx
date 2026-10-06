package java.util;

public class Timer {
    private final Vector queue = new Vector();
    private boolean cancelled;
    private final Thread thread;

    public Timer() {
        thread = new Thread(new Runnable() {
            public void run() {
                loop();
            }
        }, "Timer");
        thread.start();
    }

    private void add(TimerTask task, long time, long period, boolean fixedRate) {
        if (time < 0) {
            throw new IllegalArgumentException();
        }
        synchronized (queue) {
            if (cancelled) {
                throw new IllegalStateException("Timer cancelled");
            }
            synchronized (task.lock) {
                if (task.scheduled || task.cancelled) {
                    throw new IllegalStateException("Task already scheduled or cancelled");
                }
                task.scheduled = true;
                task.nextTime = time;
                task.period = period;
                task.fixedRate = fixedRate;
            }
            queue.addElement(task);
            queue.notify();
        }
    }

    public void schedule(TimerTask task, long delay) {
        if (delay < 0) {
            throw new IllegalArgumentException();
        }
        add(task, System.currentTimeMillis() + delay, 0, false);
    }

    public void schedule(TimerTask task, Date time) {
        add(task, time.getTime(), 0, false);
    }

    public void schedule(TimerTask task, long delay, long period) {
        if (delay < 0 || period <= 0) {
            throw new IllegalArgumentException();
        }
        add(task, System.currentTimeMillis() + delay, period, false);
    }

    public void schedule(TimerTask task, Date first, long period) {
        if (period <= 0) {
            throw new IllegalArgumentException();
        }
        add(task, first.getTime(), period, false);
    }

    public void scheduleAtFixedRate(TimerTask task, long delay, long period) {
        if (delay < 0 || period <= 0) {
            throw new IllegalArgumentException();
        }
        add(task, System.currentTimeMillis() + delay, period, true);
    }

    public void scheduleAtFixedRate(TimerTask task, Date first, long period) {
        if (period <= 0) {
            throw new IllegalArgumentException();
        }
        add(task, first.getTime(), period, true);
    }

    public void cancel() {
        synchronized (queue) {
            cancelled = true;
            queue.removeAllElements();
            queue.notify();
        }
    }

    private void loop() {
        while (true) {
            TimerTask task = null;
            synchronized (queue) {
                while (!cancelled && queue.isEmpty()) {
                    try {
                        queue.wait();
                    } catch (InterruptedException e) {
                        // bỏ qua
                    }
                }
                if (cancelled) {
                    return;
                }
                // Tìm task đến hạn sớm nhất
                long now = System.currentTimeMillis();
                TimerTask best = null;
                for (int i = queue.size() - 1; i >= 0; i--) {
                    TimerTask t = (TimerTask) queue.elementAt(i);
                    if (t.cancelled) {
                        queue.removeElementAt(i);
                        continue;
                    }
                    if (best == null || t.nextTime < best.nextTime) {
                        best = t;
                    }
                }
                if (best == null) {
                    continue;
                }
                long wait = best.nextTime - now;
                if (wait > 0) {
                    try {
                        queue.wait(wait);
                    } catch (InterruptedException e) {
                        // bỏ qua
                    }
                    continue;
                }
                task = best;
                if (task.period > 0) {
                    task.nextTime = task.fixedRate ? task.nextTime + task.period : now + task.period;
                } else {
                    queue.removeElement(task);
                }
            }
            try {
                task.run();
            } catch (Throwable e) {
                e.printStackTrace();
            }
        }
    }
}
