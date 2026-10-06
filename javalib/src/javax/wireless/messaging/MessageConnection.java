package javax.wireless.messaging;

import java.io.IOException;
import java.io.InterruptedIOException;
import javax.microedition.io.Connection;

public interface MessageConnection extends Connection {
    String TEXT_MESSAGE = "text";
    String BINARY_MESSAGE = "binary";
    String MULTIPART_MESSAGE = "multipart";

    Message newMessage(String type);

    Message newMessage(String type, String address);

    void send(Message msg) throws IOException, InterruptedIOException;

    Message receive() throws IOException, InterruptedIOException;

    void setMessageListener(MessageListener l) throws IOException;

    int numberOfSegments(Message msg);
}
