package javax.microedition.midlet;

import javax.microedition.io.ConnectionNotFoundException;

public abstract class MIDlet {
    private static MIDlet instance;

    protected MIDlet() {
        instance = this;
    }

    static MIDlet getInstance() {
        return instance;
    }

    protected abstract void startApp() throws MIDletStateChangeException;

    protected abstract void pauseApp();

    protected abstract void destroyApp(boolean unconditional) throws MIDletStateChangeException;

    public final void notifyDestroyed() {
        notifyDestroyed0();
    }

    public final void notifyPaused() {
    }

    public final void resumeRequest() {
    }

    public final String getAppProperty(String key) {
        if (key == null) {
            throw new NullPointerException();
        }
        return getAppProperty0(key);
    }

    // Trang web (http / https): mở trình duyệt. Chuỗi rỗng = huỷ yêu cầu đang chờ.
    // Luôn trả về false (không cần thoát MIDlet).
    public final boolean platformRequest(String url) throws ConnectionNotFoundException {
        if (url == null) {
            throw new NullPointerException();
        }
        System.out.println("platformRequest: " + url);
        String u = url.trim();
        String lower = u.toLowerCase();
        if (u.length() > 0 && !lower.startsWith("http://") && !lower.startsWith("https://")) {
            throw new ConnectionNotFoundException("Khong ho tro " + url);   // file:, tel:, sms:, mailto:...
        }
        platformRequest0(u);
        return false;
    }

    private static native void platformRequest0(String url);

    public final int checkPermission(String permission) {
        return 1;
    }

    private static native String getAppProperty0(String key);

    static native void notifyDestroyed0();

    // --- Vòng đời, được Launcher gọi

    void doStart() throws MIDletStateChangeException {
        startApp();
    }

    void doPause() {
        pauseApp();
    }

    void doDestroy() {
        try {
            destroyApp(true);
        } catch (Throwable e) {
            e.printStackTrace();
        }
    }
}
