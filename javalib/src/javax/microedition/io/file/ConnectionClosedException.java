package javax.microedition.io.file;

import java.io.IOException;

public class ConnectionClosedException extends IOException {
    public ConnectionClosedException() {
        super();
    }

    public ConnectionClosedException(String detailMessage) {
        super(detailMessage);
    }
}
