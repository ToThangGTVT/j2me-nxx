package javax.microedition.lcdui;

public class ImageItem extends Item {
    private Image image;
    private String altText;
    private int appearance;

    public ImageItem(String label, Image img, int layout, String altText) {
        this(label, img, layout, altText, PLAIN);
    }

    public ImageItem(String label, Image img, int layout, String altText, int appearanceMode) {
        super(label);
        this.image = img;
        this.layout = layout;
        this.altText = altText;
        this.appearance = appearanceMode;
    }

    public Image getImage() {
        return image;
    }

    public void setImage(Image img) {
        image = img;
        repaint0();
    }

    public String getAltText() {
        return altText;
    }

    public void setAltText(String t) {
        altText = t;
    }

    public int getAppearanceMode() {
        return appearance;
    }

    int paintBody(Graphics g, int y, int w, boolean focused) {
        if (image == null) {
            return 0;
        }
        int align = layout & 3;
        int x = align == LAYOUT_CENTER ? (w - image.getWidth()) / 2 : align == LAYOUT_RIGHT ? w - image.getWidth() - 4 : 4;
        if (focused) {
            g.setColor(Screen.ACCENT);
            g.drawRect(x - 2, y, image.getWidth() + 3, image.getHeight() + 3);
        }
        g.drawImage(image, x, y + 2, Graphics.TOP | Graphics.LEFT);
        return image.getHeight() + 4;
    }

    int bodyHeight(int w) {
        return image == null ? 0 : image.getHeight() + 4;
    }
}
