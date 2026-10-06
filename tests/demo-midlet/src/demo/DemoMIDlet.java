package demo;

import javax.microedition.lcdui.*;
import javax.microedition.midlet.MIDlet;
import javax.microedition.rms.RecordStore;

public class DemoMIDlet extends MIDlet implements CommandListener {
    private Display display;
    private DemoCanvas canvas;
    private List menu;
    private Form form;
    private final Command back = new Command("Quay lai", Command.BACK, 1);
    private final Command exit = new Command("Thoat", Command.EXIT, 1);
    int launches;

    protected void startApp() {
        if (display == null) {
            display = Display.getDisplay(this);
            launches = bumpLaunchCounter();
            canvas = new DemoCanvas(this);
            menu = new List("Menu", List.IMPLICIT, new String[] { "Tiep tuc", "Form", "Alert", "Thoat" }, null);
            menu.addCommand(back);
            menu.setCommandListener(this);
        }
        display.setCurrent(canvas);
        canvas.start();
    }

    protected void pauseApp() {
    }

    protected void destroyApp(boolean unconditional) {
        canvas.stop();
    }

    private int bumpLaunchCounter() {
        try {
            RecordStore rs = RecordStore.openRecordStore("demo", true);
            int n = 0;
            if (rs.getNumRecords() > 0) {
                byte[] b = rs.getRecord(1);
                n = ((b[0] & 0xff) << 8) | (b[1] & 0xff);
                n++;
                rs.setRecord(1, new byte[] { (byte) (n >> 8), (byte) n }, 0, 2);
            } else {
                n = 1;
                rs.addRecord(new byte[] { 0, 1 }, 0, 2);
            }
            rs.closeRecordStore();
            return n;
        } catch (Exception e) {
            e.printStackTrace();
            return -1;
        }
    }

    void showMenu() {
        display.setCurrent(menu);
    }

    public void commandAction(Command c, Displayable d) {
        if (c == back) {
            display.setCurrent(canvas);
        } else if (c == exit) {
            destroyApp(true);
            notifyDestroyed();
        } else if (d == menu && c == List.SELECT_COMMAND) {
            switch (menu.getSelectedIndex()) {
            case 0:
                display.setCurrent(canvas);
                break;
            case 1:
                if (form == null) {
                    form = new Form("Form demo");
                    form.append(new StringItem("Xin chao", "Day la Form cua J2ME-NX."));
                    form.append(new TextField("Ten", "Switch", 20, TextField.ANY));
                    form.append(new ChoiceGroup("Chon", Choice.EXCLUSIVE, new String[] { "Mot", "Hai", "Ba" }, null));
                    form.append(new Gauge("Am luong", true, 10, 5));
                    form.addCommand(back);
                    form.addCommand(exit);
                    form.setCommandListener(this);
                }
                display.setCurrent(form);
                break;
            case 2:
                Alert a = new Alert("Alert", "Tu dong dong sau 2 giay", null, AlertType.INFO);
                display.setCurrent(a, canvas);
                break;
            default:
                destroyApp(true);
                notifyDestroyed();
            }
        }
    }
}
