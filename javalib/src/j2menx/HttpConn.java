package j2menx;

import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.Vector;
import javax.microedition.io.HttpConnection;

// HTTP/1.1 tối giản trên socket (chưa hỗ trợ https)
public class HttpConn implements HttpConnection {
    private final String url;
    private final String host;
    private final int port;
    private final String file;
    private final String query;
    private final String ref;
    private String method = GET;
    private final Vector reqKeys = new Vector();
    private final Vector reqValues = new Vector();
    private ByteArrayOutputStream body;

    private SocketConn sock;
    private int responseCode = -1;
    private String responseMessage;
    private final Vector respKeys = new Vector();
    private final Vector respValues = new Vector();
    private InputStream bodyIn;
    private boolean closed;

    public HttpConn(String url) throws IOException {
        this.url = url;
        String rest = url.substring(url.indexOf("://") + 3);
        int hashPos = rest.indexOf('#');
        ref = hashPos >= 0 ? rest.substring(hashPos + 1) : null;
        if (hashPos >= 0) {
            rest = rest.substring(0, hashPos);
        }
        int slash = rest.indexOf('/');
        String hostPort = slash >= 0 ? rest.substring(0, slash) : rest;
        String path = slash >= 0 ? rest.substring(slash) : "/";
        int q = path.indexOf('?');
        file = q >= 0 ? path.substring(0, q) : path;
        query = q >= 0 ? path.substring(q + 1) : null;
        int colon = hostPort.lastIndexOf(':');
        if (colon >= 0) {
            host = hostPort.substring(0, colon);
            port = Integer.parseInt(hostPort.substring(colon + 1));
        } else {
            host = hostPort;
            port = 80;
        }
        setRequestProperty("Host", port == 80 ? host : host + ":" + port);
        setRequestProperty("User-Agent", "Nokia6300/2.0 Profile/MIDP-2.0 Configuration/CLDC-1.1");
    }

    private void ensureSetup() throws IOException {
        if (closed) {
            throw new IOException("Connection closed");
        }
        if (responseCode >= 0) {
            throw new IOException("Already connected");
        }
    }

    public String getURL() { return url; }
    public String getProtocol() { return "http"; }
    public String getHost() { return host; }
    public String getFile() { return file; }
    public String getRef() { return ref; }
    public String getQuery() { return query; }
    public int getPort() { return port; }
    public String getRequestMethod() { return method; }

    public void setRequestMethod(String m) throws IOException {
        ensureSetup();
        method = m;
    }

    public String getRequestProperty(String key) {
        for (int i = 0; i < reqKeys.size(); i++) {
            if (((String) reqKeys.elementAt(i)).equalsIgnoreCase(key)) {
                return (String) reqValues.elementAt(i);
            }
        }
        return null;
    }

    public void setRequestProperty(String key, String value) throws IOException {
        ensureSetup();
        for (int i = 0; i < reqKeys.size(); i++) {
            if (((String) reqKeys.elementAt(i)).equalsIgnoreCase(key)) {
                reqValues.setElementAt(value, i);
                return;
            }
        }
        reqKeys.addElement(key);
        reqValues.addElement(value);
    }

    private void connect() throws IOException {
        if (responseCode >= 0) {
            return;
        }
        if (closed) {
            throw new IOException("Connection closed");
        }
        sock = new SocketConn(host, port);
        StringBuffer sb = new StringBuffer();
        sb.append(method).append(' ').append(file);
        if (query != null) {
            sb.append('?').append(query);
        }
        sb.append(" HTTP/1.1\r\n");
        byte[] data = body != null ? body.toByteArray() : null;
        if (data != null && getRequestProperty("Content-Length") == null) {
            reqKeys.addElement("Content-Length");
            reqValues.addElement(String.valueOf(data.length));
        }
        for (int i = 0; i < reqKeys.size(); i++) {
            sb.append(reqKeys.elementAt(i)).append(": ").append(reqValues.elementAt(i)).append("\r\n");
        }
        sb.append("Connection: close\r\n\r\n");
        OutputStream out = sock.openOutputStream();
        out.write(sb.toString().getBytes("ISO-8859-1"));
        if (data != null) {
            out.write(data);
        }

        InputStream in = sock.openInputStream();
        String status = readLine(in);
        int sp1 = status.indexOf(' ');
        int sp2 = sp1 >= 0 ? status.indexOf(' ', sp1 + 1) : -1;
        try {
            responseCode = Integer.parseInt(sp2 > 0 ? status.substring(sp1 + 1, sp2) : status.substring(sp1 + 1));
        } catch (RuntimeException e) {
            throw new IOException("HTTP response hong: " + status);
        }
        responseMessage = sp2 > 0 ? status.substring(sp2 + 1) : "";
        String line;
        while ((line = readLine(in)).length() > 0) {
            int c = line.indexOf(':');
            if (c > 0) {
                respKeys.addElement(line.substring(0, c).trim());
                respValues.addElement(line.substring(c + 1).trim());
            }
        }
        String te = getHeaderField("Transfer-Encoding");
        String cl = getHeaderField("Content-Length");
        if (te != null && te.toLowerCase().indexOf("chunked") >= 0) {
            bodyIn = new Chunked(in);
        } else if (cl != null) {
            bodyIn = new Limited(in, Long.parseLong(cl.trim()));
        } else {
            bodyIn = in;
        }
    }

    private static String readLine(InputStream in) throws IOException {
        StringBuffer sb = new StringBuffer();
        int c;
        while ((c = in.read()) >= 0 && c != '\n') {
            if (c != '\r') {
                sb.append((char) c);
            }
        }
        return sb.toString();
    }

    public int getResponseCode() throws IOException {
        connect();
        return responseCode;
    }

    public String getResponseMessage() throws IOException {
        connect();
        return responseMessage;
    }

    public long getExpiration() throws IOException {
        return getHeaderFieldDate("Expires", 0);
    }

    public long getDate() throws IOException {
        return getHeaderFieldDate("Date", 0);
    }

    public long getLastModified() throws IOException {
        return getHeaderFieldDate("Last-Modified", 0);
    }

    public String getHeaderField(String name) throws IOException {
        connect();
        for (int i = respKeys.size() - 1; i >= 0; i--) {
            if (((String) respKeys.elementAt(i)).equalsIgnoreCase(name)) {
                return (String) respValues.elementAt(i);
            }
        }
        return null;
    }

    public int getHeaderFieldInt(String name, int def) throws IOException {
        String v = getHeaderField(name);
        try {
            return v == null ? def : Integer.parseInt(v.trim());
        } catch (NumberFormatException e) {
            return def;
        }
    }

    public long getHeaderFieldDate(String name, long def) throws IOException {
        return def;
    }

    public String getHeaderField(int n) throws IOException {
        connect();
        return n >= 0 && n < respValues.size() ? (String) respValues.elementAt(n) : null;
    }

    public String getHeaderFieldKey(int n) throws IOException {
        connect();
        return n >= 0 && n < respKeys.size() ? (String) respKeys.elementAt(n) : null;
    }

    public String getType() {
        try {
            return getHeaderField("Content-Type");
        } catch (IOException e) {
            return null;
        }
    }

    public String getEncoding() {
        try {
            return getHeaderField("Content-Encoding");
        } catch (IOException e) {
            return null;
        }
    }

    public long getLength() {
        try {
            String v = getHeaderField("Content-Length");
            return v == null ? -1 : Long.parseLong(v.trim());
        } catch (Exception e) {
            return -1;
        }
    }

    public InputStream openInputStream() throws IOException {
        connect();
        return bodyIn;
    }

    public DataInputStream openDataInputStream() throws IOException {
        return new DataInputStream(openInputStream());
    }

    public OutputStream openOutputStream() throws IOException {
        ensureSetup();
        if (body == null) {
            body = new ByteArrayOutputStream();
        }
        if (method.equals(GET)) {
            method = POST;
        }
        return body;
    }

    public DataOutputStream openDataOutputStream() throws IOException {
        return new DataOutputStream(openOutputStream());
    }

    public void close() throws IOException {
        closed = true;
        if (sock != null) {
            sock.close();
        }
    }

    private static class Limited extends InputStream {
        private final InputStream in;
        private long left;

        Limited(InputStream in, long len) {
            this.in = in;
            this.left = len;
        }

        public int read() throws IOException {
            if (left <= 0) {
                return -1;
            }
            int c = in.read();
            if (c >= 0) {
                left--;
            }
            return c;
        }

        public int read(byte[] b, int off, int len) throws IOException {
            if (left <= 0) {
                return -1;
            }
            int n = in.read(b, off, (int) Math.min(len, left));
            if (n > 0) {
                left -= n;
            }
            return n;
        }

        public int available() throws IOException {
            return (int) Math.min(left, in.available());
        }
    }

    private static class Chunked extends InputStream {
        private final InputStream in;
        private int left;
        private boolean eof;

        Chunked(InputStream in) {
            this.in = in;
        }

        private boolean next() throws IOException {
            if (eof) {
                return false;
            }
            if (left > 0) {
                return true;
            }
            String line = readLine(in);
            if (line.length() == 0) {
                line = readLine(in);
            }
            int semi = line.indexOf(';');
            if (semi >= 0) {
                line = line.substring(0, semi);
            }
            try {
                left = Integer.parseInt(line.trim(), 16);
            } catch (NumberFormatException e) {
                left = 0;
            }
            if (left == 0) {
                eof = true;
                return false;
            }
            return true;
        }

        public int read() throws IOException {
            if (!next()) {
                return -1;
            }
            int c = in.read();
            if (c >= 0) {
                left--;
            }
            return c;
        }

        public int read(byte[] b, int off, int len) throws IOException {
            if (!next()) {
                return -1;
            }
            int n = in.read(b, off, Math.min(len, left));
            if (n > 0) {
                left -= n;
            }
            return n;
        }
    }
}
