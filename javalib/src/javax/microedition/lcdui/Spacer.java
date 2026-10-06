package javax.microedition.lcdui;

public class Spacer extends Item {
    private int minW, minH;

    public Spacer(int minWidth, int minHeight) {
        super(null);
        minW = minWidth;
        minH = minHeight;
    }

    public void setMinimumSize(int w, int h) {
        minW = w;
        minH = h;
    }

    public void setLabel(String label) {
        throw new IllegalStateException();
    }

    int paintBody(Graphics g, int y, int w, boolean focused) {
        return minH;
    }

    int bodyHeight(int w) {
        return minH;
    }
}
