package at;
import javax.microedition.lcdui.*;
import javax.microedition.media.*;
import javax.microedition.media.control.*;
import javax.microedition.midlet.*;
public class AudioTest extends MIDlet implements PlayerListener, Runnable {
    protected void startApp() { Display.getDisplay(this).setCurrent(new Form("Audio")); new Thread(this).start(); }
    public void playerUpdate(Player p, String ev, Object d) { System.out.println("EVENT " + p.getContentType() + " " + ev + " t=" + p.getMediaTime()); }
    public void run() {
        try {
            Player mid = Manager.createPlayer(getClass().getResourceAsStream("/song.mid"), "audio/midi");
            mid.addPlayerListener(this); mid.realize();
            System.out.println("MIDI duration us=" + mid.getDuration());
            mid.start();
            Thread.sleep(3000);
            Player wav = Manager.createPlayer(getClass().getResourceAsStream("/beep.wav"), "audio/x-wav");
            wav.addPlayerListener(this); wav.realize(); System.out.println("WAV duration us=" + wav.getDuration());
            wav.start(); Thread.sleep(600);
            ToneControl tc = (ToneControl) Manager.createPlayer(Manager.TONE_DEVICE_LOCATOR).getControl("ToneControl");
            Player tp = (Player) tc; tc.setSequence(new byte[]{ToneControl.VERSION,1,ToneControl.TEMPO,30,67,16,69,16,71,32});
            tp.addPlayerListener(this); tp.start(); Thread.sleep(1500);
            Manager.playTone(72, 300, 80); Thread.sleep(500);
            Player mp3 = Manager.createPlayer(getClass().getResourceAsStream("/tone.mp3"), "audio/mpeg");
            mp3.addPlayerListener(this); mp3.realize(); System.out.println("MP3 duration us=" + mp3.getDuration());
            mp3.start(); Thread.sleep(1200);
        } catch (Throwable e) { e.printStackTrace(); }
        notifyDestroyed();
    }
    protected void pauseApp() {} protected void destroyApp(boolean u) {}
}
