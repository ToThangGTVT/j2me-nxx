package j2menx;

import java.util.Vector;
import javax.microedition.media.Control;
import javax.microedition.media.MediaException;
import javax.microedition.media.Player;
import javax.microedition.media.PlayerListener;
import javax.microedition.media.TimeBase;
import javax.microedition.media.control.ToneControl;
import javax.microedition.media.control.VolumeControl;

// Player chưa phát âm thanh thật: chỉ giữ trạng thái để game chạy đúng luồng
public class AudioPlayer implements Player, VolumeControl, ToneControl {
    private final byte[] data;
    private final String type;
    private int state = UNREALIZED;
    private final Vector listeners = new Vector();
    private int level = 100;
    private boolean muted;
    private TimeBase timeBase;

    public AudioPlayer(byte[] data, String type) {
        this.data = data;
        this.type = type;
    }

    private void checkClosed() {
        if (state == CLOSED) {
            throw new IllegalStateException();
        }
    }

    private void notify(String event, Object data) {
        for (int i = 0; i < listeners.size(); i++) {
            ((PlayerListener) listeners.elementAt(i)).playerUpdate(this, event, data);
        }
    }

    public void realize() throws MediaException {
        checkClosed();
        if (state < REALIZED) {
            state = REALIZED;
        }
    }

    public void prefetch() throws MediaException {
        realize();
        if (state < PREFETCHED) {
            state = PREFETCHED;
        }
    }

    public void start() throws MediaException {
        prefetch();
        if (state != STARTED) {
            state = STARTED;
            notify(PlayerListener.STARTED, new Long(0));
            // Không có âm thanh: coi như phát xong ngay
            state = PREFETCHED;
            notify(PlayerListener.END_OF_MEDIA, new Long(0));
        }
    }

    public void stop() throws MediaException {
        checkClosed();
        if (state == STARTED) {
            state = PREFETCHED;
            notify(PlayerListener.STOPPED, new Long(0));
        }
    }

    public void deallocate() {
        checkClosed();
        if (state >= PREFETCHED) {
            state = REALIZED;
        }
    }

    public void close() {
        if (state != CLOSED) {
            state = CLOSED;
            notify(PlayerListener.CLOSED, null);
        }
    }

    public long setMediaTime(long now) throws MediaException {
        checkClosed();
        return 0;
    }

    public long getMediaTime() {
        return 0;
    }

    public int getState() {
        return state;
    }

    public long getDuration() {
        return TIME_UNKNOWN;
    }

    public String getContentType() {
        return type;
    }

    public void setLoopCount(int count) {
    }

    public void addPlayerListener(PlayerListener l) {
        if (l != null && !listeners.contains(l)) {
            listeners.addElement(l);
        }
    }

    public void removePlayerListener(PlayerListener l) {
        listeners.removeElement(l);
    }

    public void setTimeBase(TimeBase master) throws MediaException {
        timeBase = master;
    }

    public TimeBase getTimeBase() {
        return timeBase;
    }

    public Control[] getControls() {
        return new Control[] { this };
    }

    public Control getControl(String controlType) {
        if (controlType.endsWith("VolumeControl") || controlType.endsWith("ToneControl")) {
            return this;
        }
        return null;
    }

    public void setMute(boolean mute) {
        muted = mute;
    }

    public boolean isMuted() {
        return muted;
    }

    public int setLevel(int l) {
        level = Math.max(0, Math.min(100, l));
        return level;
    }

    public int getLevel() {
        return level;
    }

    public void setSequence(byte[] sequence) {
    }
}
