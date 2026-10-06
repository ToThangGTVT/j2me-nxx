package java.lang;

public class ExceptionInInitializerError extends LinkageError {
    public ExceptionInInitializerError() {
        super();
    }

    public ExceptionInInitializerError(String message) {
        super(message);
    }
}
