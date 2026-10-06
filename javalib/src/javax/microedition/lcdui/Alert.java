package javax.microedition.lcdui;

public class Alert extends Screen {
    public static final int FOREVER = -2;
    public static final Command DISMISS_COMMAND = new Command("Done", Command.OK, 0);

    private String text;
    private Image image;
    private AlertType type;
    private int timeout = 2000;
    private Displayable next;
    private Gauge indicator;
    private int showId;

    public Alert(String title) {
        this(title, null, null, null);
    }

    public Alert(String title, String alertText, Image alertImage, AlertType alertType) {
        this.title = title;
        this.text = alertText;
        this.image = alertImage;
        this.type = alertType;
    }

    public int getDefaultTimeout() {
        return 2000;
    }

    public int getTimeout() {
        return timeout;
    }

    public void setTimeout(int time) {
        if (time <= 0 && time != FOREVER) {
            throw new IllegalArgumentException();
        }
        timeout = time;
    }

    public AlertType getType() {
        return type;
    }

    public void setType(AlertType type) {
        this.type = type;
    }

    public String getString() {
        return text;
    }

    public void setString(String str) {
        text = str;
        repaint0();
    }

    public Image getImage() {
        return image;
    }

    public void setImage(Image img) {
        image = img;
        repaint0();
    }

    public void setIndicator(Gauge indicator) {
        this.indicator = indicator;
    }

    public Gauge getIndicator() {
        return indicator;
    }

    void setNext(Displayable d) {
        if (d != this) {
            next = d;
        }
    }

    private boolean hasUserCommands() {
        return !commands.isEmpty();
    }

    private void dismiss() {
        if (hasUserCommands() && listener != null) {
            return;
        }
        if (next != null) {
            Display.get().setCurrent0(next);
        }
    }

    String softLeftLabel() {
        return hasUserCommands() ? leftLabel() : "OK";
    }

    void keyEvent(int type, int code) {
        if (type != Display.EV_KEY_PRESSED) {
            return;
        }
        int a = Canvas.actionOf(code);
        if (a == Canvas.UP) {
            scrollY = Math.max(0, scrollY - 20);
            repaint0();
        } else if (a == Canvas.DOWN) {
            scrollY += 20;
            repaint0();
        } else if (!hasUserCommands() && (a == Canvas.FIRE || code == Canvas.KEY_SOFT_LEFT)) {
            dismiss();
        }
    }

    void showNotify0() {
        scrollY = 0;
        if (timeout == FOREVER || hasUserCommands()) {
            return;
        }
        final int id = ++showId;
        final int t = timeout;
        new Thread() {
            public void run() {
                try {
                    Thread.sleep(t);
                } catch (InterruptedException e) {
                    // bỏ qua
                }
                if (id == showId && shown) {
                    dismiss();
                }
            }
        }.start();
    }

    void paintContent(Graphics g, int w) {
        int y = 6;
        if (image != null) {
            g.drawImage(image, w / 2, y, Graphics.TOP | Graphics.HCENTER);
            y += image.getHeight() + 6;
        }
        if (text != null) {
            g.setColor(FG);
            g.setFont(plainFont());
            y += drawWrapped(g, text, 6, y, w - 12);
        }
        if (indicator != null) {
            indicator.paintBar(g, 6, y + 4, w - 12);
        }
    }
}
