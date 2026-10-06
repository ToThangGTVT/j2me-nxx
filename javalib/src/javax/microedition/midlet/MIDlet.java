package javax.microedition.midlet;

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

    public final boolean platformRequest(String url) {
        System.out.println("platformRequest: " + url);
        return false;
    }

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
