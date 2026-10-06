package java.util;

public class Date {
    private long time;

    public Date() {
        this(System.currentTimeMillis());
    }

    public Date(long date) {
        time = date;
    }

    public long getTime() {
        return time;
    }

    public void setTime(long time) {
        this.time = time;
    }

    public boolean equals(Object o) {
        return o instanceof Date && ((Date) o).time == time;
    }

    public int hashCode() {
        return (int) time ^ (int) (time >> 32);
    }

    public String toString() {
        Calendar c = Calendar.getInstance();
        c.setTime(this);
        return c.toString();
    }
}
