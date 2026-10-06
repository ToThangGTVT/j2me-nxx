package java.lang;

public class OutOfMemoryError extends VirtualMachineError {
    public OutOfMemoryError() {
        super();
    }

    public OutOfMemoryError(String message) {
        super(message);
    }
}
