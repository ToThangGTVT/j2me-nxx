package j2menx;

import java.util.Vector;
import javax.microedition.lcdui.Canvas;
import javax.microedition.media.Control;
import javax.microedition.media.MediaException;
import javax.microedition.media.Player;
import javax.microedition.media.PlayerListener;
import javax.microedition.media.TimeBase;
import javax.microedition.media.control.GUIControl;
import javax.microedition.media.control.VideoControl;
import javax.microedition.media.control.VolumeControl;

// Player video (3GP, MP4...): hình giải mã bằng FFmpeg (source/midp/video.c), vẽ đè lên Canvas
// (USE_DIRECT_VIDEO) hoặc trong Item của Form (USE_GUI_PRIMITIVE). Tiếng phát qua AudioPlayer,
// hình chạy theo đồng hồ của tiếng.
public class VideoPlayer implements Player, VideoControl, VolumeControl, Runnable {
    private static final int MODE_NONE = -1;

    private byte[] data;
    private final String type;
    private int vh;                 // handle video (video.c)
    private int ah;                 // handle tiếng (audio.c), 0 nếu không có tiếng
    private long audioDuration = -1;
    private int state = UNREALIZED;
    private final Vector listeners = new Vector();
    private int level = 100;
    private boolean muted;
    private int loopCount = 1;
    private int loopsPlayed;
    private TimeBase timeBase;

    private int displayMode = MODE_NONE;
    private Canvas canvas;
    private VideoItem item;
    private int dispX, dispY, dispW, dispH;
    private boolean visible;
    private boolean fullScreen;
    private boolean overlayShown;

    // Đồng hồ (micro giây): khi đang phát, mediaTime = now - wallBase
    private long wallBase;
    private long pausedTime;
    private boolean atEnd;
    private Thread worker;

    public VideoPlayer(byte[] data, String type) {
        this.data = data;
        this.type = type;
    }

    // Dữ liệu có phải video FFmpeg đọc được không (nhận dạng nhanh theo chữ ký file trước)
    public static boolean isVideo(byte[] d, String type) {
        if (d == null || d.length < 16) {
            return false;
        }
        boolean sig = (d[4] == 'f' && d[5] == 't' && d[6] == 'y' && d[7] == 'p')               // MP4, 3GP, MOV
                || (d[0] == 'R' && d[1] == 'I' && d[2] == 'F' && d[3] == 'F' && d[8] == 'A' && d[9] == 'V')
                || ((d[0] & 0xff) == 0x1a && (d[1] & 0xff) == 0x45 && (d[2] & 0xff) == 0xdf)  // MKV, WebM
                || (d[0] == 'F' && d[1] == 'L' && d[2] == 'V')
                || (d[0] == 0 && d[1] == 0 && d[2] == 1 && (d[3] & 0xff) == 0xba)              // MPEG-PS
                || ((d[0] & 0xff) == 0x30 && (d[1] & 0xff) == 0x26 && (d[2] & 0xff) == 0xb2);  // ASF, WMV
        if (!sig && (type == null || !type.startsWith("video/"))) {
            return false;
        }
        return (probe0(d) & 1) != 0;
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

    private static long now() {
        return System.currentTimeMillis() * 1000;
    }

    // ------------------------------------------------------------------
    // Player

    public synchronized void realize() throws MediaException {
        checkClosed();
        if (state >= REALIZED) {
            return;
        }
        vh = open0(data);
        if (vh == 0) {
            throw new MediaException("Khong mo duoc video");
        }
        ah = AudioPlayer.create0(data);
        if (ah != 0) {
            AudioPlayer.setLoop0(ah, 1);
            AudioPlayer.setVolume0(ah, muted ? 0 : level);
            audioDuration = AudioPlayer.duration0(ah);
        }
        data = null;
        dispW = width0(vh);
        dispH = height0(vh);
        state = REALIZED;
    }

    public synchronized void prefetch() throws MediaException {
        realize();
        if (state < PREFETCHED) {
            frame0(vh, pausedTime);     // có sẵn khung hình đầu để hiện trước khi phát
            state = PREFETCHED;
        }
    }

    public void start() throws MediaException {
        long t;
        synchronized (this) {
            prefetch();
            if (state == STARTED) {
                return;
            }
            if (atEnd) {
                seekTo(0);
            }
            state = STARTED;
            wallBase = now() - pausedTime;
            if (ah != 0 && pausedTime < audioDuration) {
                AudioPlayer.setTime0(ah, pausedTime);
                AudioPlayer.start0(ah);
            }
            updateDisplay();
            worker = new Thread(this);
            worker.start();
            t = pausedTime;
        }
        fire(PlayerListener.STARTED, new Long(t));
    }

    public void stop() throws MediaException {
        long t;
        synchronized (this) {
            checkClosed();
            if (state != STARTED) {
                return;
            }
            pausedTime = mediaTime();
            state = PREFETCHED;
            worker = null;
            if (ah != 0) {
                AudioPlayer.stop0(ah);
            }
            t = pausedTime;
        }
        fire(PlayerListener.STOPPED, new Long(t));
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
        synchronized (this) {
            if (state >= PREFETCHED) {
                state = REALIZED;
            }
        }
    }

    public void close() {
        synchronized (this) {
            if (state == CLOSED) {
                return;
            }
            worker = null;
            if (vh != 0) {
                close0(vh);     // gỡ luôn lớp phủ
                vh = 0;
            }
            if (ah != 0) {
                AudioPlayer.close0(ah);
                ah = 0;
            }
            state = CLOSED;
            data = null;
        }
        fire(PlayerListener.CLOSED, null);
    }

    private void seekTo(long us) {
        seek0(vh, us);
        frame0(vh, us);
        if (ah != 0) {
            AudioPlayer.setTime0(ah, us);
        }
        atEnd = false;
    }

    public synchronized long setMediaTime(long us) throws MediaException {
        checkClosed();
        if (state == UNREALIZED) {
            throw new IllegalStateException();
        }
        if (us < 0) {
            us = 0;
        }
        long dur = getDuration();
        if (dur > 0 && us > dur) {
            us = dur;
        }
        seekTo(us);
        if (state == STARTED) {
            wallBase = now() - us;
            if (ah != 0 && us < audioDuration) {
                AudioPlayer.start0(ah);
            }
        } else {
            pausedTime = us;
        }
        return us;
    }

    // Đồng hồ theo giờ thực, chỉnh theo vị trí của tiếng khi lệch quá 80ms
    private long mediaTime() {
        if (state != STARTED) {
            return pausedTime;
        }
        long t = now() - wallBase;
        if (ah != 0) {
            long a = AudioPlayer.getTime0(ah);
            if (a > 0 && a < audioDuration && Math.abs(a - t) > 80000) {
                wallBase = now() - a;
                t = a;
            }
        }
        return t;
    }

    public synchronized long getMediaTime() {
        return state == CLOSED ? TIME_UNKNOWN : mediaTime();
    }

    public int getState() {
        return state;
    }

    public synchronized long getDuration() {
        long d = vh != 0 ? duration0(vh) : -1;
        if (audioDuration > d) {
            d = audioDuration;
        }
        return d > 0 ? d : TIME_UNKNOWN;
    }

    public String getContentType() {
        return type != null ? type : "video/mp4";
    }

    public void setLoopCount(int count) {
        checkClosed();
        if (count == 0 || state == STARTED) {
            throw count == 0 ? (RuntimeException) new IllegalArgumentException() : new IllegalStateException();
        }
        loopCount = count;
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
        if (controlType == null) {
            throw new IllegalArgumentException();
        }
        if (state == UNREALIZED) {
            throw new IllegalStateException();
        }
        if (controlType.endsWith("VideoControl") || controlType.endsWith("GUIControl")
                || controlType.endsWith("VolumeControl")) {
            return this;
        }
        return null;
    }

    // ------------------------------------------------------------------
    // Luồng chạy hình

    public void run() {
        while (true) {
            int sleep;
            boolean ended = false;
            long endTime = 0;
            VideoItem it;
            synchronized (this) {
                if (state != STARTED || worker != Thread.currentThread()) {
                    return;
                }
                if (displayMode == VideoControl.USE_DIRECT_VIDEO && canvas.isShown() != overlayShown) {
                    updateDisplay();
                }
                long t = mediaTime();
                long next = frame0(vh, t);
                it = item;
                if (next >= 0) {
                    sleep = (int) ((next - t) / 1000);
                } else if (ah != 0 && t < audioDuration) {
                    sleep = 20;         // hết hình nhưng tiếng còn
                } else {
                    ended = true;
                    endTime = t;
                    sleep = 0;
                    if (loopCount == -1 || ++loopsPlayed < loopCount) {
                        seekTo(0);
                        wallBase = now();
                        if (ah != 0) {
                            AudioPlayer.start0(ah);
                        }
                    } else {
                        loopsPlayed = 0;
                        state = PREFETCHED;
                        pausedTime = endTime;
                        atEnd = true;
                        worker = null;
                        if (ah != 0) {
                            AudioPlayer.stop0(ah);
                        }
                    }
                }
            }
            if (it != null) {
                it.frameChanged();
            }
            if (ended) {
                fire(PlayerListener.END_OF_MEDIA, new Long(endTime));
            }
            try {
                Thread.sleep(sleep < 1 ? 1 : sleep > 50 ? 50 : sleep);
            } catch (InterruptedException e) {
                return;
            }
        }
    }

    // ------------------------------------------------------------------
    // VideoControl

    private void checkMode() {
        checkClosed();
        if (displayMode == MODE_NONE) {
            throw new IllegalStateException("initDisplayMode chua goi");
        }
    }

    public synchronized Object initDisplayMode(int mode, Object arg) {
        checkClosed();
        if (displayMode != MODE_NONE) {
            throw new IllegalStateException();
        }
        if (mode == GUIControl.USE_GUI_PRIMITIVE) {
            if (arg != null && !"javax.microedition.lcdui.Item".equals(arg)) {
                throw new IllegalArgumentException();
            }
            displayMode = mode;
            item = new VideoItem(this);
            return item;
        }
        if (mode == VideoControl.USE_DIRECT_VIDEO) {
            if (!(arg instanceof Canvas)) {
                throw new IllegalArgumentException();
            }
            displayMode = mode;
            canvas = (Canvas) arg;
            updateDisplay();
            return null;
        }
        throw new IllegalArgumentException();
    }

    // Đẩy vị trí / trạng thái hiện của lớp phủ xuống native
    private void updateDisplay() {
        if (displayMode != VideoControl.USE_DIRECT_VIDEO || vh == 0) {
            return;
        }
        boolean show = visible && state != CLOSED && canvas.isShown();
        display0(vh, getDisplayX(), getDisplayY(), getDisplayWidth(), getDisplayHeight(), show);
        overlayShown = canvas.isShown();
    }

    public synchronized void setDisplayLocation(int x, int y) {
        checkMode();
        if (displayMode == VideoControl.USE_DIRECT_VIDEO) {
            dispX = x;
            dispY = y;
            updateDisplay();
        }
    }

    // Toàn màn hình: giữ tỉ lệ, căn giữa Canvas
    private int[] fullRect() {
        int cw = canvas != null ? canvas.getWidth() : dispW, ch = canvas != null ? canvas.getHeight() : dispH;
        int sw = width0(vh), sh = height0(vh);
        if (sw <= 0 || sh <= 0) {
            return new int[] { 0, 0, cw, ch };
        }
        int w = cw, h = sh * cw / sw;
        if (h > ch) {
            h = ch;
            w = sw * ch / sh;
        }
        return new int[] { (cw - w) / 2, (ch - h) / 2, w, h };
    }

    public synchronized int getDisplayX() {
        checkMode();
        return fullScreen ? fullRect()[0] : displayMode == VideoControl.USE_DIRECT_VIDEO ? dispX : 0;
    }

    public synchronized int getDisplayY() {
        checkMode();
        return fullScreen ? fullRect()[1] : displayMode == VideoControl.USE_DIRECT_VIDEO ? dispY : 0;
    }

    public synchronized void setVisible(boolean v) {
        checkMode();
        visible = v;
        updateDisplay();
    }

    public synchronized void setDisplaySize(int w, int h) throws MediaException {
        checkMode();
        if (w < 1 || h < 1) {
            throw new IllegalArgumentException();
        }
        dispW = w;
        dispH = h;
        fullScreen = false;
        updateDisplay();
        if (item != null) {
            item.sizeChanged();
        }
    }

    public synchronized void setDisplayFullScreen(boolean b) throws MediaException {
        checkMode();
        fullScreen = b;
        updateDisplay();
    }

    public synchronized int getSourceWidth() {
        checkClosed();
        return vh != 0 ? width0(vh) : 0;
    }

    public synchronized int getSourceHeight() {
        checkClosed();
        return vh != 0 ? height0(vh) : 0;
    }

    public synchronized int getDisplayWidth() {
        checkMode();
        return fullScreen ? fullRect()[2] : dispW;
    }

    public synchronized int getDisplayHeight() {
        checkMode();
        return fullScreen ? fullRect()[3] : dispH;
    }

    public synchronized byte[] getSnapshot(String imageType) throws MediaException {
        checkMode();
        byte[] png = snapshot0(vh);
        if (png == null) {
            throw new MediaException("Chua co khung hinh");
        }
        return png;
    }

    // Item trong Form lấy khung hình hiện tại
    synchronized boolean copyFrame(int[] buf, int w, int h) {
        return vh != 0 && copyFrame0(vh, buf, w, h);
    }

    // ------------------------------------------------------------------
    // VolumeControl

    public synchronized void setMute(boolean mute) {
        muted = mute;
        if (ah != 0) {
            AudioPlayer.setVolume0(ah, muted ? 0 : level);
        }
    }

    public boolean isMuted() {
        return muted;
    }

    public int setLevel(int l) {
        synchronized (this) {
            level = Math.max(0, Math.min(100, l));
            if (ah != 0 && !muted) {
                AudioPlayer.setVolume0(ah, level);
            }
        }
        fire(PlayerListener.VOLUME_CHANGED, this);
        return level;
    }

    public int getLevel() {
        return level;
    }

    static native int probe0(byte[] data);
    private static native int open0(byte[] data);
    private static native int width0(int h);
    private static native int height0(int h);
    private static native long duration0(int h);
    private static native long frame0(int h, long nowUs);
    private static native boolean seek0(int h, long us);
    private static native void display0(int h, int x, int y, int w, int hgt, boolean visible);
    private static native boolean copyFrame0(int h, int[] dst, int w, int hgt);
    private static native byte[] snapshot0(int h);
    private static native void close0(int h);
}
