package javax.microedition.lcdui;

import java.util.Vector;

// Lớp cơ sở cho Form / List / TextBox / Alert: tự vẽ giao diện đơn giản
public abstract class Screen extends Displayable {
    static final int SOFTBAR_H = 22;
    static final int TITLE_H = 22;
    static final int BG = 0xffffff;
    static final int FG = 0x000000;
    static final int ACCENT = 0x2060c0;

    int scrollY;

    Screen() {
    }

    static Font smallFont() {
        return Font.getFont(Font.FACE_SYSTEM, Font.STYLE_PLAIN, Font.SIZE_SMALL);
    }

    static Font boldFont() {
        return Font.getFont(Font.FACE_SYSTEM, Font.STYLE_BOLD, Font.SIZE_MEDIUM);
    }

    static Font plainFont() {
        return Font.getFont(Font.FACE_SYSTEM, Font.STYLE_PLAIN, Font.SIZE_MEDIUM);
    }

    static void paintSoftBar(Graphics g, String left, String right) {
        int w = Display.screenW, h = Display.screenH;
        Font f = boldFont();
        g.setFont(f);
        g.setColor(0x202020);
        g.fillRect(0, h - SOFTBAR_H, w, SOFTBAR_H);
        g.setColor(0xffffff);
        int ty = h - SOFTBAR_H + (SOFTBAR_H - f.getHeight()) / 2;
        if (left != null) {
            g.drawString(left, 4, ty, Graphics.TOP | Graphics.LEFT);
        }
        if (right != null) {
            g.drawString(right, w - 4, ty, Graphics.TOP | Graphics.RIGHT);
        }
    }

    static Vector wrap(String text, Font f, int width) {
        Vector lines = new Vector();
        if (text == null) {
            return lines;
        }
        int len = text.length();
        int start = 0;
        while (start < len) {
            int nl = text.indexOf('\n', start);
            int end = nl < 0 ? len : nl;
            String para = text.substring(start, end);
            if (para.length() == 0) {
                lines.addElement("");
            }
            while (para.length() > 0) {
                int fit = para.length();
                while (fit > 1 && f.stringWidth(para.substring(0, fit)) > width) {
                    int sp = para.lastIndexOf(' ', fit - 1);
                    fit = sp > 0 ? sp : fit - 1;
                }
                lines.addElement(para.substring(0, fit));
                para = para.substring(fit);
                if (para.startsWith(" ")) {
                    para = para.substring(1);
                }
            }
            start = end + 1;
        }
        return lines;
    }

    // Vẽ đoạn văn có xuống dòng, trả về chiều cao đã dùng
    static int drawWrapped(Graphics g, String text, int x, int y, int width) {
        Font f = g.getFont();
        Vector lines = wrap(text, f, width);
        for (int i = 0; i < lines.size(); i++) {
            g.drawString((String) lines.elementAt(i), x, y + i * f.getHeight(), Graphics.TOP | Graphics.LEFT);
        }
        return lines.size() * f.getHeight();
    }

    static int wrappedHeight(String text, Font f, int width) {
        return wrap(text, f, width).size() * f.getHeight();
    }

    int contentTop() {
        return TITLE_H + (ticker != null ? 18 : 0);
    }

    int contentHeight() {
        return Display.screenH - contentTop() - SOFTBAR_H;
    }

    void paintScreen(Graphics g) {
        int w = Display.screenW, h = Display.screenH;
        g.setColor(BG);
        g.fillRect(0, 0, w, h);

        int top = contentTop();
        g.setClip(0, top, w, contentHeight());
        g.translate(0, top - scrollY);
        paintContent(g, w);
        g.reset();

        // Thanh tiêu đề
        g.setColor(ACCENT);
        g.fillRect(0, 0, w, TITLE_H);
        g.setColor(0xffffff);
        Font bf = boldFont();
        g.setFont(bf);
        if (title != null) {
            g.drawString(title, 4, (TITLE_H - bf.getHeight()) / 2, Graphics.TOP | Graphics.LEFT);
        }
        if (ticker != null) {
            g.setColor(0xffffc0);
            g.fillRect(0, TITLE_H, w, 18);
            g.setColor(0);
            g.setFont(smallFont());
            g.drawString(ticker.getString(), 4, TITLE_H + 2, Graphics.TOP | Graphics.LEFT);
        }
        paintSoftBar(g, softLeftLabel(), rightLabel());
    }

    String softLeftLabel() {
        return leftLabel();
    }

    // Vẽ nội dung từ y = 0, chiều rộng w
    abstract void paintContent(Graphics g, int w);

    // Giữ đoạn [y, y + h) trong vùng nhìn thấy
    void ensureVisible(int y, int h) {
        int ch = contentHeight();
        if (y < scrollY) {
            scrollY = y;
        } else if (y + h > scrollY + ch) {
            scrollY = y + h - ch;
        }
        if (scrollY < 0) {
            scrollY = 0;
        }
    }
}
