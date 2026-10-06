package javax.microedition.lcdui.game;

import javax.microedition.lcdui.Graphics;
import javax.microedition.lcdui.Image;

public class Sprite extends Layer {
    public static final int TRANS_NONE = 0;
    public static final int TRANS_ROT90 = 5;
    public static final int TRANS_ROT180 = 3;
    public static final int TRANS_ROT270 = 6;
    public static final int TRANS_MIRROR = 2;
    public static final int TRANS_MIRROR_ROT90 = 7;
    public static final int TRANS_MIRROR_ROT180 = 1;
    public static final int TRANS_MIRROR_ROT270 = 4;

    private Image image;
    private int frameW, frameH;
    private int cols, rawFrames;
    private int[] sequence;
    private int seqIndex;
    private int refX, refY;
    private int transform;
    private int colX, colY, colW, colH;

    public Sprite(Image image) {
        super(image.getWidth(), image.getHeight());
        init(image, image.getWidth(), image.getHeight());
    }

    public Sprite(Image image, int frameWidth, int frameHeight) {
        super(frameWidth, frameHeight);
        init(image, frameWidth, frameHeight);
    }

    public Sprite(Sprite s) {
        super(s.width, s.height);
        image = s.image;
        frameW = s.frameW;
        frameH = s.frameH;
        cols = s.cols;
        rawFrames = s.rawFrames;
        sequence = s.sequence;
        seqIndex = s.seqIndex;
        refX = s.refX;
        refY = s.refY;
        transform = s.transform;
        colX = s.colX;
        colY = s.colY;
        colW = s.colW;
        colH = s.colH;
        x = s.x;
        y = s.y;
        visible = s.visible;
    }

    private void init(Image img, int fw, int fh) {
        if (fw <= 0 || fh <= 0 || img.getWidth() % fw != 0 || img.getHeight() % fh != 0) {
            throw new IllegalArgumentException();
        }
        image = img;
        frameW = fw;
        frameH = fh;
        cols = img.getWidth() / fw;
        rawFrames = cols * (img.getHeight() / fh);
        sequence = null;
        seqIndex = 0;
        colX = 0;
        colY = 0;
        colW = fw;
        colH = fh;
        width = fw;
        height = fh;
        transform = TRANS_NONE;
    }

    public void setImage(Image img, int frameWidth, int frameHeight) {
        int oldSeq = seqIndex;
        int[] oldSequence = sequence;
        int tr = transform;
        int rx = refX, ry = refY;
        setTransform(TRANS_NONE);
        boolean sameFrames = img.getWidth() / frameWidth * (img.getHeight() / frameHeight) >= rawFrames;
        init(img, frameWidth, frameHeight);
        if (sameFrames && oldSequence != null) {
            sequence = oldSequence;
            seqIndex = oldSeq;
        }
        refX = rx;
        refY = ry;
        setTransform(tr);
    }

    public void defineReferencePixel(int x, int y) {
        refX = x;
        refY = y;
    }

    // Toạ độ điểm tham chiếu sau biến đổi (tương đối góc trên trái của sprite)
    private int transformedRefX() {
        return tx(refX, refY, transform, frameW, frameH);
    }

    private int transformedRefY() {
        return ty(refX, refY, transform, frameW, frameH);
    }

    private static int tx(int px, int py, int t, int w, int h) {
        switch (t) {
        case TRANS_MIRROR: return w - 1 - px;
        case TRANS_MIRROR_ROT180: return px;
        case TRANS_ROT180: return w - 1 - px;
        case TRANS_ROT90: return h - 1 - py;
        case TRANS_ROT270: return py;
        case TRANS_MIRROR_ROT90: return h - 1 - py;
        case TRANS_MIRROR_ROT270: return py;
        default: return px;
        }
    }

    private static int ty(int px, int py, int t, int w, int h) {
        switch (t) {
        case TRANS_MIRROR: return py;
        case TRANS_MIRROR_ROT180: return h - 1 - py;
        case TRANS_ROT180: return h - 1 - py;
        case TRANS_ROT90: return px;
        case TRANS_ROT270: return w - 1 - px;
        case TRANS_MIRROR_ROT90: return w - 1 - px;
        case TRANS_MIRROR_ROT270: return px;
        default: return py;
        }
    }

    public void setRefPixelPosition(int x, int y) {
        this.x = x - transformedRefX();
        this.y = y - transformedRefY();
    }

    public int getRefPixelX() {
        return x + transformedRefX();
    }

    public int getRefPixelY() {
        return y + transformedRefY();
    }

    public void setFrame(int sequenceIndex) {
        if (sequenceIndex < 0 || sequenceIndex >= getFrameSequenceLength()) {
            throw new IndexOutOfBoundsException();
        }
        seqIndex = sequenceIndex;
    }

    public final int getFrame() {
        return seqIndex;
    }

    public int getRawFrameCount() {
        return rawFrames;
    }

    public int getFrameSequenceLength() {
        return sequence != null ? sequence.length : rawFrames;
    }

    public void nextFrame() {
        seqIndex = (seqIndex + 1) % getFrameSequenceLength();
    }

    public void prevFrame() {
        int n = getFrameSequenceLength();
        seqIndex = (seqIndex + n - 1) % n;
    }

    public void setFrameSequence(int[] seq) {
        if (seq == null) {
            sequence = null;
        } else {
            if (seq.length == 0) {
                throw new IllegalArgumentException();
            }
            for (int i = 0; i < seq.length; i++) {
                if (seq[i] < 0 || seq[i] >= rawFrames) {
                    throw new ArrayIndexOutOfBoundsException();
                }
            }
            sequence = new int[seq.length];
            System.arraycopy(seq, 0, sequence, 0, seq.length);
        }
        seqIndex = 0;
    }

    private int rawFrame() {
        return sequence != null ? sequence[seqIndex] : seqIndex;
    }

    public void setTransform(int t) {
        if (t < 0 || t > 7) {
            throw new IllegalArgumentException();
        }
        int oldRx = getRefPixelX(), oldRy = getRefPixelY();
        transform = t;
        boolean swap = (t & 4) != 0;
        width = swap ? frameH : frameW;
        height = swap ? frameW : frameH;
        x = oldRx - transformedRefX();
        y = oldRy - transformedRefY();
    }

    public void defineCollisionRectangle(int x, int y, int w, int h) {
        if (w < 0 || h < 0) {
            throw new IllegalArgumentException();
        }
        colX = x;
        colY = y;
        colW = w;
        colH = h;
    }

    public final void paint(Graphics g) {
        if (!visible) {
            return;
        }
        int f = rawFrame();
        int sx = (f % cols) * frameW;
        int sy = (f / cols) * frameH;
        g.drawRegion(image, sx, sy, frameW, frameH, transform, x, y, Graphics.TOP | Graphics.LEFT);
    }

    // Hình chữ nhật va chạm sau biến đổi, toạ độ màn hình: [x, y, w, h]
    private int[] collisionRect() {
        int x1 = tx(colX, colY, transform, frameW, frameH);
        int y1 = ty(colX, colY, transform, frameW, frameH);
        int x2 = tx(colX + colW - 1, colY + colH - 1, transform, frameW, frameH);
        int y2 = ty(colX + colW - 1, colY + colH - 1, transform, frameW, frameH);
        int l = Math.min(x1, x2), t = Math.min(y1, y2);
        return new int[] { x + l, y + t, Math.abs(x2 - x1) + 1, Math.abs(y2 - y1) + 1 };
    }

    private static boolean overlap(int[] a, int[] b) {
        return a[0] < b[0] + b[2] && b[0] < a[0] + a[2] && a[1] < b[1] + b[3] && b[1] < a[1] + a[3];
    }

    // Pixel (sx, sy) toạ độ màn hình có đục không
    private boolean opaqueAt(int px, int py) {
        int lx = px - x, ly = py - y;
        if (lx < 0 || ly < 0 || lx >= width || ly >= height) {
            return false;
        }
        // Đảo ngược biến đổi để ra toạ độ trong frame
        int fx, fy;
        switch (transform) {
        case TRANS_MIRROR: fx = frameW - 1 - lx; fy = ly; break;
        case TRANS_MIRROR_ROT180: fx = lx; fy = frameH - 1 - ly; break;
        case TRANS_ROT180: fx = frameW - 1 - lx; fy = frameH - 1 - ly; break;
        case TRANS_ROT90: fx = ly; fy = frameH - 1 - lx; break;
        case TRANS_ROT270: fx = frameW - 1 - ly; fy = lx; break;
        case TRANS_MIRROR_ROT90: fx = frameW - 1 - ly; fy = frameH - 1 - lx; break;
        case TRANS_MIRROR_ROT270: fx = ly; fy = lx; break;
        default: fx = lx; fy = ly; break;
        }
        if (fx < colX || fy < colY || fx >= colX + colW || fy >= colY + colH) {
            return false;
        }
        int f = rawFrame();
        int sx = (f % cols) * frameW + fx;
        int sy = (f / cols) * frameH + fy;
        int[] px1 = new int[1];
        image.getRGB(px1, 0, 1, sx, sy, 1, 1);
        return (px1[0] >>> 24) != 0;
    }

    public final boolean collidesWith(Sprite s, boolean pixelLevel) {
        if (!visible || !s.visible) {
            return false;
        }
        int[] a = collisionRect(), b = s.collisionRect();
        if (!overlap(a, b)) {
            return false;
        }
        if (!pixelLevel) {
            return true;
        }
        int l = Math.max(a[0], b[0]), t = Math.max(a[1], b[1]);
        int r = Math.min(a[0] + a[2], b[0] + b[2]), bt = Math.min(a[1] + a[3], b[1] + b[3]);
        for (int py = t; py < bt; py++) {
            for (int px = l; px < r; px++) {
                if (opaqueAt(px, py) && s.opaqueAt(px, py)) {
                    return true;
                }
            }
        }
        return false;
    }

    public final boolean collidesWith(TiledLayer t, boolean pixelLevel) {
        if (!visible || !t.visible) {
            return false;
        }
        int[] a = collisionRect();
        int cw = t.getCellWidth(), ch = t.getCellHeight();
        int c0 = Math.max(0, (a[0] - t.x) / cw), r0 = Math.max(0, (a[1] - t.y) / ch);
        int c1 = Math.min(t.getColumns() - 1, (a[0] + a[2] - 1 - t.x) / cw);
        int r1 = Math.min(t.getRows() - 1, (a[1] + a[3] - 1 - t.y) / ch);
        for (int r = r0; r <= r1; r++) {
            for (int c = c0; c <= c1; c++) {
                if (t.getCell(c, r) == 0) {
                    continue;
                }
                if (!pixelLevel) {
                    return true;
                }
                int cx = t.x + c * cw, cy = t.y + r * ch;
                int l = Math.max(a[0], cx), tp = Math.max(a[1], cy);
                int rr = Math.min(a[0] + a[2], cx + cw), bb = Math.min(a[1] + a[3], cy + ch);
                for (int py = tp; py < bb; py++) {
                    for (int px = l; px < rr; px++) {
                        if (opaqueAt(px, py) && t.opaqueAt(px, py)) {
                            return true;
                        }
                    }
                }
            }
        }
        return false;
    }

    public final boolean collidesWith(Image img, int ix, int iy, boolean pixelLevel) {
        if (!visible) {
            return false;
        }
        int[] a = collisionRect();
        int[] b = { ix, iy, img.getWidth(), img.getHeight() };
        if (!overlap(a, b)) {
            return false;
        }
        if (!pixelLevel) {
            return true;
        }
        int l = Math.max(a[0], b[0]), t = Math.max(a[1], b[1]);
        int r = Math.min(a[0] + a[2], b[0] + b[2]), bt = Math.min(a[1] + a[3], b[1] + b[3]);
        int[] px1 = new int[1];
        for (int py = t; py < bt; py++) {
            for (int px = l; px < r; px++) {
                img.getRGB(px1, 0, 1, px - ix, py - iy, 1, 1);
                if ((px1[0] >>> 24) != 0 && opaqueAt(px, py)) {
                    return true;
                }
            }
        }
        return false;
    }
}
