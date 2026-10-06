package demo;

import java.io.IOException;
import java.util.Random;
import javax.microedition.lcdui.*;
import javax.microedition.lcdui.game.*;

public class DemoCanvas extends GameCanvas implements Runnable {
    private final DemoMIDlet midlet;
    private volatile boolean running;
    private final int[] bx = new int[6], by = new int[6], vx = new int[6], vy = new int[6], col = new int[6];
    private Sprite sprite;
    private Image logo;
    private String lastKey = "-";
    private int frames, fps;
    private long fpsTime;
    private int px, py;

    public DemoCanvas(DemoMIDlet m) {
        super(false);
        midlet = m;
        setFullScreenMode(true);
        Random r = new Random(7);
        for (int i = 0; i < bx.length; i++) {
            bx[i] = r.nextInt(200) + 10;
            by[i] = r.nextInt(250) + 30;
            vx[i] = r.nextInt(5) - 2 | 1;
            vy[i] = r.nextInt(5) - 2 | 1;
            col[i] = 0x404040 | r.nextInt(0xffffff);
        }
        try {
            logo = Image.createImage("/logo.png");
            // 4 frame 32x32 tạo từ logo bằng các phép biến đổi
            Image strip = Image.createImage(128, 32);
            Graphics g = strip.getGraphics();
            g.setColor(0xff00ff);
            g.fillRect(0, 0, 128, 32);
            for (int i = 0; i < 4; i++) {
                g.drawRegion(logo, 0, 0, 32, 32, new int[] { 0, 5, 3, 6 }[i], i * 32, 0, Graphics.TOP | Graphics.LEFT);
            }
            sprite = new Sprite(logo);
        } catch (IOException e) {
            e.printStackTrace();
        }
        px = getWidth() / 2;
        py = getHeight() / 2;
    }

    void start() {
        if (!running) {
            running = true;
            new Thread(this).start();
        }
    }

    void stop() {
        running = false;
    }

    protected void keyPressed(int keyCode) {
        lastKey = getKeyName(keyCode) + " (" + keyCode + ")";
        if (keyCode == -6) {
            midlet.showMenu();
        }
    }

    protected void pointerPressed(int x, int y) {
        px = x;
        py = y;
        lastKey = "cham " + x + "," + y;
    }

    public void run() {
        Graphics g = getGraphics();
        int w = getWidth(), h = getHeight();
        int angle = 0;
        fpsTime = System.currentTimeMillis();
        while (running) {
            int keys = getKeyStates();
            if ((keys & LEFT_PRESSED) != 0) px -= 3;
            if ((keys & RIGHT_PRESSED) != 0) px += 3;
            if ((keys & UP_PRESSED) != 0) py -= 3;
            if ((keys & DOWN_PRESSED) != 0) py += 3;

            for (int y = 0; y < h; y += 4) {
                g.setColor(0x10 + y * 0x30 / h, 0x20 + y * 0x40 / h, 0x50 + y * 0x60 / h);
                g.fillRect(0, y, w, 4);
            }
            for (int i = 0; i < bx.length; i++) {
                bx[i] += vx[i];
                by[i] += vy[i];
                if (bx[i] < 8 || bx[i] > w - 8) vx[i] = -vx[i];
                if (by[i] < 8 || by[i] > h - 8) vy[i] = -vy[i];
                g.setColor(col[i]);
                g.fillArc(bx[i] - 8, by[i] - 8, 16, 16, 0, 360);
            }
            g.setColor(0xffff00);
            g.drawArc(w / 2 - 40, h / 2 - 40, 80, 80, angle, 270);
            g.fillTriangle(10, h - 40, 40, h - 70, 70, h - 40);
            g.setColor(0x00ff80);
            g.fillRoundRect(w - 80, h - 70, 70, 30, 12, 12);
            g.setColor(0);
            g.drawString("RoundRect", w - 45, h - 55, Graphics.HCENTER | Graphics.BASELINE);

            if (sprite != null) {
                sprite.setTransform(new int[] { Sprite.TRANS_NONE, Sprite.TRANS_ROT90, Sprite.TRANS_ROT180,
                        Sprite.TRANS_ROT270 }[(angle / 30) % 4]);
                sprite.setRefPixelPosition(px, py);
                sprite.defineReferencePixel(16, 16);
                sprite.paint(g);
            }

            g.setColor(0xffffff);
            g.setFont(Font.getFont(Font.FACE_SYSTEM, Font.STYLE_BOLD, Font.SIZE_LARGE));
            g.drawString("J2ME-NX Demo", w / 2, 6, Graphics.HCENTER | Graphics.TOP);
            g.setFont(Font.getFont(Font.FACE_SYSTEM, Font.STYLE_PLAIN, Font.SIZE_SMALL));
            g.drawString("FPS: " + fps + "   lan chay: " + midlet.launches, 4, 32, Graphics.TOP | Graphics.LEFT);
            g.drawString("Phim: " + lastKey, 4, 48, Graphics.TOP | Graphics.LEFT);
            g.drawString("Mem: " + (Runtime.getRuntime().freeMemory() / 1024) + "K free", 4, 64, Graphics.TOP | Graphics.LEFT);
            g.drawString("Menu", 4, h - 2, Graphics.BOTTOM | Graphics.LEFT);

            flushGraphics();
            angle = (angle + 6) % 360;
            frames++;
            long now = System.currentTimeMillis();
            if (now - fpsTime >= 1000) {
                fps = frames;
                frames = 0;
                fpsTime = now;
            }
            try {
                Thread.sleep(5);
            } catch (InterruptedException e) {
                // bỏ qua
            }
        }
    }
}
