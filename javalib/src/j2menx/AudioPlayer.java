package j2menx;

import java.util.Hashtable;
import java.util.Vector;
import javax.microedition.media.Control;
import javax.microedition.media.MediaException;
import javax.microedition.media.Player;
import javax.microedition.media.PlayerListener;
import javax.microedition.media.TimeBase;
import javax.microedition.media.control.ToneControl;
import javax.microedition.media.control.VolumeControl;

// Player phát WAV / MIDI / MP3 / tone qua bộ trộn native (source/midp/audio.c).
// Định dạng không đọc được (AMR, AAC...) thì giả lập trạng thái, không có tiếng.
public class AudioPlayer implements Player, VolumeControl, ToneControl {
    private static final Hashtable active = new Hashtable();

    private byte[] data;
    private final String type;
    private int handle;
    private int state = UNREALIZED;
    private final Vector listeners = new Vector();
    private int level = 100;
    private boolean muted;
    private int loopCount = 1;
    private TimeBase timeBase;
    private byte[] toneSequence;

    public AudioPlayer(byte[] data, String type) {
        this.data = data;
        this.type = type;
    }

    private void checkClosed() {
        if (state == CLOSED) {
            throw new IllegalStateException("Player closed");
        }
    }

    private void fire(String event, Object eventData) {
        for (int i = 0; i < listeners.size(); i++) {
            try {
                ((PlayerListener) listeners.elementAt(i)).playerUpdate(this, event, eventData);
            } catch (Throwable e) {
                e.printStackTrace();
            }
        }
    }

    // Display gọi khi native báo phát xong
    public static void mediaEnded(int handle) {
        AudioPlayer p = (AudioPlayer) active.get(new Integer(handle));
        if (p != null && p.state == STARTED) {
            p.state = PREFETCHED;
            p.fire(PlayerListener.END_OF_MEDIA, new Long(p.getMediaTime()));
        }
    }

    public void realize() throws MediaException {
        checkClosed();
        if (state >= REALIZED) {
            return;
        }
        if (handle == 0) {
            if (toneSequence != null) {
                handle = createTone0(toneSequence);
            } else if (data != null) {
                handle = create0(data);
            }
            if (handle != 0) {
                active.put(new Integer(handle), this);
                setLoop0(handle, loopCount);
                setVolume0(handle, muted ? 0 : level);
            }
        }
        state = REALIZED;
    }

    public void prefetch() throws MediaException {
        realize();
        if (state < PREFETCHED) {
            state = PREFETCHED;
        }
    }

    public void start() throws MediaException {
        prefetch();
        if (state == STARTED) {
            return;
        }
        state = STARTED;
        fire(PlayerListener.STARTED, new Long(getMediaTime()));
        if (handle != 0) {
            start0(handle);
        } else {
            // Không phát được: coi như phát xong ngay
            state = PREFETCHED;
            fire(PlayerListener.END_OF_MEDIA, new Long(0));
        }
    }

    public void stop() throws MediaException {
        checkClosed();
        if (state == STARTED) {
            if (handle != 0) {
                stop0(handle);
            }
            state = PREFETCHED;
            fire(PlayerListener.STOPPED, new Long(getMediaTime()));
        }
    }

    public void deallocate() {
        checkClosed();
        if (state == STARTED) {
            try {
                stop();
            } catch (MediaException e) {
                // bỏ qua
            }
        }
        if (state >= PREFETCHED) {
            state = REALIZED;
        }
    }

    public void close() {
        if (state == CLOSED) {
            return;
        }
        if (handle != 0) {
            active.remove(new Integer(handle));
            close0(handle);
            handle = 0;
        }
        state = CLOSED;
        data = null;
        fire(PlayerListener.CLOSED, null);
    }

    public long setMediaTime(long now) throws MediaException {
        checkClosed();
        if (state == UNREALIZED) {
            throw new IllegalStateException();
        }
        return handle != 0 ? setTime0(handle, now) : 0;
    }

    public long getMediaTime() {
        return handle != 0 ? getTime0(handle) : 0;
    }

    public int getState() {
        return state;
    }

    public long getDuration() {
        return handle != 0 ? duration0(handle) : TIME_UNKNOWN;
    }

    public String getContentType() {
        return type;
    }

    public void setLoopCount(int count) {
        checkClosed();
        if (count == 0) {
            throw new IllegalArgumentException();
        }
        loopCount = count;
        if (handle != 0) {
            setLoop0(handle, count);
        }
    }

    public void addPlayerListener(PlayerListener l) {
        checkClosed();
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
        if (handle != 0) {
            setVolume0(handle, muted ? 0 : level);
        }
    }

    public boolean isMuted() {
        return muted;
    }

    public int setLevel(int l) {
        level = Math.max(0, Math.min(100, l));
        if (handle != 0 && !muted) {
            setVolume0(handle, level);
        }
        fire(PlayerListener.VOLUME_CHANGED, this);
        return level;
    }

    public int getLevel() {
        return level;
    }

    public void setSequence(byte[] sequence) {
        if (state >= PREFETCHED) {
            throw new IllegalStateException();
        }
        if (sequence == null) {
            throw new IllegalArgumentException();
        }
        toneSequence = sequence;
        if (handle != 0) {
            active.remove(new Integer(handle));
            close0(handle);
            handle = 0;
            state = UNREALIZED;
        }
    }

    public static void playTone(int note, int duration, int volume) {
        playTone0(note, duration, volume);
    }

    static native int create0(byte[] data);
    private static native int createTone0(byte[] seq);
    static native void start0(int h);
    static native void stop0(int h);
    static native void setLoop0(int h, int count);
    static native void setVolume0(int h, int level);
    static native long getTime0(int h);
    static native long setTime0(int h, long us);
    static native long duration0(int h);
    static native void close0(int h);
    private static native void playTone0(int note, int duration, int volume);
}
