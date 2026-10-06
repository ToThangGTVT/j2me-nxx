package java.util;

public abstract class Calendar {
    public static final int ERA = 0, YEAR = 1, MONTH = 2, WEEK_OF_YEAR = 3, WEEK_OF_MONTH = 4, DATE = 5,
            DAY_OF_MONTH = 5, DAY_OF_YEAR = 6, DAY_OF_WEEK = 7, DAY_OF_WEEK_IN_MONTH = 8, AM_PM = 9, HOUR = 10,
            HOUR_OF_DAY = 11, MINUTE = 12, SECOND = 13, MILLISECOND = 14, ZONE_OFFSET = 15, DST_OFFSET = 16;
    public static final int SUNDAY = 1, MONDAY = 2, TUESDAY = 3, WEDNESDAY = 4, THURSDAY = 5, FRIDAY = 6,
            SATURDAY = 7;
    public static final int JANUARY = 0, FEBRUARY = 1, MARCH = 2, APRIL = 3, MAY = 4, JUNE = 5, JULY = 6,
            AUGUST = 7, SEPTEMBER = 8, OCTOBER = 9, NOVEMBER = 10, DECEMBER = 11;
    public static final int AM = 0, PM = 1;

    protected int[] fields = new int[17];
    protected boolean[] isSet = new boolean[17];
    protected long time;
    private TimeZone zone;

    protected Calendar() {
        zone = TimeZone.getDefault();
    }

    public static synchronized Calendar getInstance() {
        Calendar c = new Greg();
        c.setTimeInMillis(System.currentTimeMillis());
        return c;
    }

    public static synchronized Calendar getInstance(TimeZone zone) {
        Calendar c = new Greg();
        c.zone = zone;
        c.setTimeInMillis(System.currentTimeMillis());
        return c;
    }

    public final Date getTime() {
        return new Date(getTimeInMillis());
    }

    public final void setTime(Date date) {
        setTimeInMillis(date.getTime());
    }

    protected long getTimeInMillis() {
        return time;
    }

    protected void setTimeInMillis(long millis) {
        time = millis;
        computeFields();
    }

    public final int get(int field) {
        return fields[field];
    }

    public final void set(int field, int value) {
        fields[field] = value;
        isSet[field] = true;
        computeTime();
        computeFields();
    }

    public void setTimeZone(TimeZone value) {
        zone = value;
        computeFields();
    }

    public TimeZone getTimeZone() {
        return zone;
    }

    protected abstract void computeFields();

    protected abstract void computeTime();

    public boolean equals(Object o) {
        return o instanceof Calendar && ((Calendar) o).time == time;
    }

    public boolean before(Object when) {
        return when instanceof Calendar && time < ((Calendar) when).time;
    }

    public boolean after(Object when) {
        return when instanceof Calendar && time > ((Calendar) when).time;
    }

    public String toString() {
        return fields[YEAR] + "-" + pad(fields[MONTH] + 1) + "-" + pad(fields[DATE]) + " " + pad(fields[HOUR_OF_DAY])
                + ":" + pad(fields[MINUTE]) + ":" + pad(fields[SECOND]);
    }

    private static String pad(int v) {
        return v < 10 ? "0" + v : String.valueOf(v);
    }

    // Lịch Gregory đơn giản (không DST)
    static class Greg extends Calendar {
        private static final int[] DAYS = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

        private static boolean leap(int y) {
            return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
        }

        private static long daysFromEpoch(int y, int m, int d) {
            // Thuật toán days_from_civil (Howard Hinnant)
            y -= m <= 2 ? 1 : 0;
            long era = (y >= 0 ? y : y - 399) / 400;
            long yoe = y - era * 400;
            long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
            long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
            return era * 146097 + doe - 719468;
        }

        protected void computeFields() {
            long local = time + getTimeZone().getRawOffset();
            long days = local >= 0 ? local / 86400000L : (local - 86399999L) / 86400000L;
            long ms = local - days * 86400000L;
            long z = days + 719468;
            long era = (z >= 0 ? z : z - 146096) / 146097;
            long doe = z - era * 146097;
            long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
            long y = yoe + era * 400;
            long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
            long mp = (5 * doy + 2) / 153;
            long d = doy - (153 * mp + 2) / 5 + 1;
            long m = mp + (mp < 10 ? 3 : -9);
            if (m <= 2) {
                y++;
            }
            fields[ERA] = 1;
            fields[YEAR] = (int) y;
            fields[MONTH] = (int) m - 1;
            fields[DATE] = (int) d;
            int dow = (int) ((days % 7 + 11) % 7);
            fields[DAY_OF_WEEK] = dow + 1;
            int yday = 0;
            for (int i = 0; i < m - 1; i++) {
                yday += DAYS[i] + (i == 1 && leap((int) y) ? 1 : 0);
            }
            fields[DAY_OF_YEAR] = yday + (int) d;
            fields[HOUR_OF_DAY] = (int) (ms / 3600000);
            fields[HOUR] = fields[HOUR_OF_DAY] % 12;
            fields[AM_PM] = fields[HOUR_OF_DAY] >= 12 ? PM : AM;
            fields[MINUTE] = (int) (ms / 60000 % 60);
            fields[SECOND] = (int) (ms / 1000 % 60);
            fields[MILLISECOND] = (int) (ms % 1000);
            fields[ZONE_OFFSET] = getTimeZone().getRawOffset();
            fields[DST_OFFSET] = 0;
            fields[WEEK_OF_YEAR] = (fields[DAY_OF_YEAR] - 1) / 7 + 1;
            fields[WEEK_OF_MONTH] = (fields[DATE] - 1) / 7 + 1;
            fields[DAY_OF_WEEK_IN_MONTH] = (fields[DATE] - 1) / 7 + 1;
        }

        protected void computeTime() {
            int hour = isSet[HOUR] && !isSet[HOUR_OF_DAY] ? fields[HOUR] + (fields[AM_PM] == PM ? 12 : 0)
                    : fields[HOUR_OF_DAY];
            long days = daysFromEpoch(fields[YEAR], fields[MONTH] + 1, fields[DATE]);
            long local = days * 86400000L + hour * 3600000L + fields[MINUTE] * 60000L + fields[SECOND] * 1000L
                    + fields[MILLISECOND];
            time = local - getTimeZone().getRawOffset();
            for (int i = 0; i < isSet.length; i++) {
                isSet[i] = false;
            }
        }
    }
}
