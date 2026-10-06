package nt;
import java.io.*;
import javax.microedition.io.*;
import javax.microedition.lcdui.*;
import javax.microedition.midlet.*;
public class NetTest extends MIDlet implements Runnable {
    protected void startApp() {
        Form f = new Form("Net"); f.append("dang test...");
        Display.getDisplay(this).setCurrent(f);
        new Thread(this).start();
    }
    public void run() {
        try {
            SocketConnection sc = (SocketConnection) Connector.open("socket://127.0.0.1:5555");
            DataOutputStream out = sc.openDataOutputStream();
            DataInputStream in = sc.openDataInputStream();
            out.writeUTF("ping tieng Viet có dấu");
            out.flush();
            System.out.println("SOCKET echo: " + in.readUTF() + " local port>0: " + (sc.getLocalPort() > 0));
            sc.close();
        } catch (Throwable e) { System.out.println("SOCKET loi: " + e); }
        try {
            HttpConnection hc = (HttpConnection) Connector.open("http://127.0.0.1:8765/hello.txt");
            int code = hc.getResponseCode();
            InputStream in = hc.openInputStream();
            ByteArrayOutputStream bo = new ByteArrayOutputStream(); int c;
            while ((c = in.read()) >= 0) bo.write(c);
            System.out.println("HTTP " + code + " type=" + hc.getType() + " body=" + new String(bo.toByteArray(), "UTF-8").trim());
            hc.close();
        } catch (Throwable e) { System.out.println("HTTP loi: " + e); }
        try { Connector.open("socket://127.0.0.1:1"); System.out.println("??"); } catch (IOException e) { System.out.println("Ket noi cong dong bao loi dung: " + e); }
        notifyDestroyed();
    }
    protected void pauseApp() {}
    protected void destroyApp(boolean u) {}
}
