package java.lang;

public class NoClassDefFoundError extends LinkageError {
    public NoClassDefFoundError() {
        super();
    }

    public NoClassDefFoundError(String message) {
        super(message);
    }
}
