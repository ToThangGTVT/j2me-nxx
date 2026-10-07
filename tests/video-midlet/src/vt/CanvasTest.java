package vt;

import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Display;
import javax.microedition.lcdui.Graphics;
import javax.microedition.media.Manager;
import javax.microedition.media.Player;
import javax.microedition.media.PlayerListener;
import javax.microedition.media.control.VideoControl;
import javax.microedition.midlet.MIDlet;

// Video vẽ đè lên Canvas tại (32, 60); phím 5 bật/tắt toàn màn hình, # chụp khung hình
public class CanvasTest extends MIDlet implements PlayerListener {
    private Player player;
    private VideoControl vc;
    private String status = "...";
    private int ends;
    private int snapshotBytes = -1;
    private final Canvas canvas = new Canvas() {
        protected void paint(Graphics g) {
            g.setColor(0x203040);
            g.fillRect(0, 0, getWidth(), getHeight());
            g.setColor(0xffffff);
            g.drawString("Video test: " + status, 4, 4, Graphics.TOP | Graphics.LEFT);
            long t = player != null ? player.getMediaTime() / 1000 : 0;
            long d = player != null ? player.getDuration() / 1000 : 0;
            g.drawString(t + " / " + d + " ms  end " + ends, 4, 24, Graphics.TOP | Graphics.LEFT);
            if (vc != null) {
                g.drawString("src " + vc.getSourceWidth() + "x" + vc.getSourceHeight() + " snap " + snapshotBytes, 4,
                        getHeight() - 20, Graphics.TOP | Graphics.LEFT);
            }
        }

        protected void keyPressed(int key) {
            try {
                if (key == KEY_NUM5) {
                    full = !full;
                    vc.setDisplayFullScreen(full);
                } else if (key == KEY_POUND) {
                    snapshotBytes = vc.getSnapshot(null).length;
                }
            } catch (Exception e) {
                status = e.toString();
            }
            repaint();
        }
    };
    private boolean full;

    protected void startApp() {
        Display.getDisplay(this).setCurrent(canvas);
        try {
            player = Manager.createPlayer(getClass().getResourceAsStream("/clip.3gp"), "video/3gpp");
            player.addPlayerListener(this);
            player.realize();
            vc = (VideoControl) player.getControl("VideoControl");
            vc.initDisplayMode(VideoControl.USE_DIRECT_VIDEO, canvas);
            vc.setDisplayLocation(32, 60);
            vc.setVisible(true);
            player.setLoopCount(2);
            player.start();
            status = player.getClass().getName();
        } catch (Exception e) {
            status = e.toString();
        }
        new Thread() {
            public void run() {
                while (true) {
                    canvas.repaint();
                    try {
                        Thread.sleep(250);
                    } catch (InterruptedException e) {
                        return;
                    }
                }
            }
        }.start();
    }

    public void playerUpdate(Player p, String event, Object data) {
        if (event == END_OF_MEDIA) {
            ends++;
        }
        status = event;
    }

    protected void pauseApp() {
    }

    protected void destroyApp(boolean unconditional) {
        if (player != null) {
            player.close();
        }
    }
}
