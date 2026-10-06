package javax.microedition.lcdui;

import java.util.Calendar;
import java.util.Date;
import java.util.TimeZone;

public class DateField extends Item {
    public static final int DATE = 1;
    public static final int TIME = 2;
    public static final int DATE_TIME = 3;

    private Date date;
    private int mode;

    public DateField(String label, int mode) {
        this(label, mode, null);
    }

    public DateField(String label, int mode, TimeZone timeZone) {
        super(label);
        this.mode = mode;
    }

    public Date getDate() {
        return date;
    }

    public void setDate(Date date) {
        this.date = date;
        repaint0();
    }

    public int getInputMode() {
        return mode;
    }

    public void setInputMode(int mode) {
        this.mode = mode;
    }

    int paintBody(Graphics g, int y, int w, boolean focused) {
        String s = "--";
        if (date != null) {
            Calendar c = Calendar.getInstance();
            c.setTime(date);
            s = c.toString();
        }
        g.setFont(Screen.plainFont());
        g.setColor(Screen.FG);
        g.drawString(s, 6, y + 2, Graphics.TOP | Graphics.LEFT);
        return bodyHeight(w);
    }

    int bodyHeight(int w) {
        return Screen.plainFont().getHeight() + 4;
    }
}
