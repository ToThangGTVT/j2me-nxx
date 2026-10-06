package java.lang;

public class Throwable {
    private String detailMessage;
    private Object vmTrace;

    public Throwable() {
        fillInStackTrace0();
    }

    public Throwable(String message) {
        detailMessage = message;
        fillInStackTrace0();
    }

    private native void fillInStackTrace0();

    public String getMessage() {
        return detailMessage;
    }

    public String getLocalizedMessage() {
        return getMessage();
    }

    public String toString() {
        String n = getClass().getName();
        String m = getLocalizedMessage();
        return m != null ? n + ": " + m : n;
    }

    public void printStackTrace() {
        printStackTrace0();
    }

    private native void printStackTrace0();
}
