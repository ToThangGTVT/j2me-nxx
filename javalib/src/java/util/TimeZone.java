package java.util;

public abstract class TimeZone {
    private static TimeZone defaultZone;

    public TimeZone() {
    }

    public abstract int getOffset(int era, int year, int month, int day, int dayOfWeek, int millis);

    public abstract int getRawOffset();

    public abstract boolean useDaylightTime();

    public String getID() {
        return "GMT";
    }

    public static synchronized TimeZone getDefault() {
        if (defaultZone == null) {
            defaultZone = new SimpleZone(getDefaultOffset0(), "GMT");
        }
        return defaultZone;
    }

    public static TimeZone getTimeZone(String id) {
        if (id == null) {
            throw new NullPointerException();
        }
        if (id.startsWith("GMT") && id.length() > 4) {
            try {
                int sign = id.charAt(3) == '-' ? -1 : 1;
                String rest = id.substring(4);
                int colon = rest.indexOf(':');
                int h = Integer.parseInt(colon >= 0 ? rest.substring(0, colon) : rest);
                int m = colon >= 0 ? Integer.parseInt(rest.substring(colon + 1)) : 0;
                return new SimpleZone(sign * (h * 3600000 + m * 60000), id);
            } catch (NumberFormatException e) {
                // rơi xuống GMT
            }
        }
        if (id.equals("GMT") || id.equals("UTC")) {
            return new SimpleZone(0, id);
        }
        return getDefault();
    }

    public static String[] getAvailableIDs() {
        return new String[] { "GMT", "UTC" };
    }

    private static native int getDefaultOffset0();

    static class SimpleZone extends TimeZone {
        private final int offset;
        private final String id;

        SimpleZone(int offset, String id) {
            this.offset = offset;
            this.id = id;
        }

        public int getOffset(int era, int year, int month, int day, int dayOfWeek, int millis) {
            return offset;
        }

        public int getRawOffset() {
            return offset;
        }

        public boolean useDaylightTime() {
            return false;
        }

        public String getID() {
            return id;
        }
    }
}
