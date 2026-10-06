package j2menx;

import java.io.IOException;
import java.io.InterruptedIOException;
import java.util.Date;
import javax.wireless.messaging.BinaryMessage;
import javax.wireless.messaging.Message;
import javax.wireless.messaging.MessageConnection;
import javax.wireless.messaging.MessageListener;
import javax.wireless.messaging.TextMessage;

// sms://: tạo được tin nhắn nhưng không gửi / nhận được (Switch không có SIM)
public class SmsConn implements MessageConnection {
    private final String address;
    private boolean closed;

    public SmsConn(String url) {
        address = url;
    }

    private static class Msg implements TextMessage, BinaryMessage {
        String addr;
        String text;
        byte[] data;

        public String getAddress() { return addr; }
        public void setAddress(String a) { addr = a; }
        public Date getTimestamp() { return null; }
        public String getPayloadText() { return text; }
        public void setPayloadText(String t) { text = t; }
        public byte[] getPayloadData() { return data; }
        public void setPayloadData(byte[] d) { data = d; }
    }

    public Message newMessage(String type) {
        return newMessage(type, address.indexOf("//:") >= 0 ? null : address);
    }

    public Message newMessage(String type, String addr) {
        if (!TEXT_MESSAGE.equals(type) && !BINARY_MESSAGE.equals(type)) {
            throw new IllegalArgumentException("Loai tin nhan khong ho tro: " + type);
        }
        Msg m = new Msg();
        m.addr = addr;
        return m;
    }

    public void send(Message msg) throws IOException, InterruptedIOException {
        if (closed) {
            throw new IOException("Connection closed");
        }
        if (msg == null) {
            throw new NullPointerException();
        }
        System.out.println("SMS bi bo qua (khong ho tro): " + msg.getAddress());
        throw new IOException("Khong gui duoc SMS: may khong co SIM");
    }

    public Message receive() throws IOException, InterruptedIOException {
        throw new InterruptedIOException("Khong nhan duoc SMS");
    }

    public void setMessageListener(MessageListener l) throws IOException {
    }

    public int numberOfSegments(Message msg) {
        return 1;
    }

    public void close() {
        closed = true;
    }
}
