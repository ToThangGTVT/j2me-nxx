package hs;
import java.io.*;
import javax.microedition.io.*;
import javax.microedition.lcdui.*;
import javax.microedition.midlet.*;
public class HttpsTest extends MIDlet implements Runnable {
    protected void startApp() { Display.getDisplay(this).setCurrent(new Form("HTTPS")); new Thread(this).start(); }
    public void run() {
        try {
            long t0 = System.currentTimeMillis();
            HttpsConnection hc = (HttpsConnection) Connector.open("https://example.com/");
            int code = hc.getResponseCode();
            InputStream in = hc.openInputStream();
            ByteArrayOutputStream bo = new ByteArrayOutputStream(); byte[] buf = new byte[1024]; int n;
            while ((n = in.read(buf, 0, buf.length)) > 0) bo.write(buf, 0, n);
            String body = new String(bo.toByteArray(), "UTF-8");
            int ti = body.indexOf("<title>");
            SecurityInfo si = hc.getSecurityInfo();
            System.out.println("HTTPS " + code + " bytes=" + bo.size() + " title=" + (ti >= 0 ? body.substring(ti + 7, body.indexOf("</title>")) : "?")
                + " " + si.getProtocolName() + " " + si.getProtocolVersion() + " " + si.getCipherSuite() + " " + (System.currentTimeMillis() - t0) + "ms");
            hc.close();
        } catch (Throwable e) { System.out.println("HTTPS loi: " + e); e.printStackTrace(); }
        try {
            SecureConnection sc = (SecureConnection) Connector.open("ssl://example.com:443");
            OutputStream o = sc.openOutputStream();
            o.write("HEAD / HTTP/1.1\r\nHost: example.com\r\nConnection: close\r\n\r\n".getBytes());
            InputStream in = sc.openInputStream();
            StringBuffer sb = new StringBuffer(); int c;
            while ((c = in.read()) >= 0 && c != '\n') sb.append((char) c);
            System.out.println("SSL raw: " + sb.toString().trim());
            sc.close();
        } catch (Throwable e) { System.out.println("SSL loi: " + e); }
        notifyDestroyed();
    }
    protected void pauseApp() {} protected void destroyApp(boolean u) {}
}
