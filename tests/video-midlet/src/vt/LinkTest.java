package vt;

import java.io.InputStream;
import java.io.OutputStream;
import javax.microedition.io.Connector;
import javax.microedition.io.file.FileConnection;
import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Display;
import javax.microedition.lcdui.Graphics;
import javax.microedition.midlet.MIDlet;

// MIDlet.platformRequest: phím 5 mở file:///E:/clip.3gp (chép từ JAR), phím * mở URL trong
// thuộc tính Test-URL (vd chạy "python3 -m http.server 8765" trong thư mục res)
public class LinkTest extends MIDlet {
    private String status = "5: file   *: Test-URL";
    private int keys;

    private final Canvas canvas = new Canvas() {
        protected void paint(Graphics g) {
            g.setColor(0x302020);
            g.fillRect(0, 0, getWidth(), getHeight());
            g.setColor(0xffffff);
            g.drawString("platformRequest test", 4, 4, Graphics.TOP | Graphics.LEFT);
            g.drawString(status, 4, 24, Graphics.TOP | Graphics.LEFT);
            g.drawString("keys " + keys, 4, 44, Graphics.TOP | Graphics.LEFT);
        }

        protected void keyPressed(int key) {
            keys++;
            try {
                if (key == KEY_NUM5) {
                    copyClip();
                    platformRequest("file:///E:/clip.3gp");
                    status = "file: ok";
                } else if (key == KEY_STAR) {
                    String url = getAppProperty("Test-URL");
                    platformRequest(url != null ? url : "http://127.0.0.1:8765/clip.3gp");
                    status = "url: ok";
                } else if (key == KEY_POUND) {
                    platformRequest("tel:123");
                }
            } catch (Exception e) {
                status = e.toString();
            }
            repaint();
        }
    };

    private void copyClip() throws Exception {
        FileConnection fc = (FileConnection) Connector.open("file:///E:/clip.3gp");
        if (!fc.exists()) {
            fc.create();
        }
        InputStream in = getClass().getResourceAsStream("/clip.3gp");
        OutputStream out = fc.openOutputStream();
        byte[] buf = new byte[4096];
        int n;
        while ((n = in.read(buf)) > 0) {
            out.write(buf, 0, n);
        }
        out.close();
        in.close();
        fc.close();
    }

    protected void startApp() {
        Display.getDisplay(this).setCurrent(canvas);
    }

    protected void pauseApp() {
    }

    protected void destroyApp(boolean unconditional) {
    }
}
