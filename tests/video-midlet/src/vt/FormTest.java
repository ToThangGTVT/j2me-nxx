package vt;

import javax.microedition.lcdui.Display;
import javax.microedition.lcdui.Form;
import javax.microedition.lcdui.Item;
import javax.microedition.lcdui.StringItem;
import javax.microedition.media.Manager;
import javax.microedition.media.Player;
import javax.microedition.media.control.VideoControl;
import javax.microedition.midlet.MIDlet;

// Video trong Form (USE_GUI_PRIMITIVE)
public class FormTest extends MIDlet {
    private Player player;

    protected void startApp() {
        Form f = new Form("Video trong Form");
        f.append(new StringItem(null, "Clip 3GP:"));
        try {
            player = Manager.createPlayer(getClass().getResourceAsStream("/clip.3gp"), "video/3gpp");
            player.realize();
            VideoControl vc = (VideoControl) player.getControl("VideoControl");
            f.append((Item) vc.initDisplayMode(VideoControl.USE_GUI_PRIMITIVE, null));
            player.setLoopCount(-1);
            player.start();
        } catch (Exception e) {
            f.append(new StringItem(null, e.toString()));
        }
        f.append(new StringItem(null, "Duoi video"));
        Display.getDisplay(this).setCurrent(f);
    }

    protected void pauseApp() {
    }

    protected void destroyApp(boolean unconditional) {
        if (player != null) {
            player.close();
        }
    }
}
