package java.lang;

public class StackOverflowError extends VirtualMachineError {
    public StackOverflowError() {
        super();
    }

    public StackOverflowError(String message) {
        super(message);
    }
}
