package j2menx;

import javax.microedition.lcdui.CustomItem;
import javax.microedition.lcdui.Graphics;

// Item hiện video trong Form (VideoControl.USE_GUI_PRIMITIVE)
final class VideoItem extends CustomItem {
    private final VideoPlayer player;
    private int[] buf;

    VideoItem(VideoPlayer player) {
        super(null);
        this.player = player;
    }

    protected int getMinContentWidth() {
        return player.getDisplayWidth();
    }

    protected int getMinContentHeight() {
        return player.getDisplayHeight();
    }

    protected int getPrefContentWidth(int height) {
        return player.getDisplayWidth();
    }

    protected int getPrefContentHeight(int width) {
        return player.getDisplayHeight();
    }

    protected void paint(Graphics g, int w, int h) {
        int dw = player.getDisplayWidth(), dh = player.getDisplayHeight();
        if (buf == null || buf.length != dw * dh) {
            buf = new int[dw * dh];
        }
        if (player.copyFrame(buf, dw, dh)) {
            g.drawRGB(buf, 0, dw, 0, 0, dw, dh, false);
        } else {
            g.setColor(0);
            g.fillRect(0, 0, dw, dh);
        }
    }

    void frameChanged() {
        repaint();
    }

    void sizeChanged() {
        invalidate();
    }
}
