package javax.microedition.midlet;

import javax.microedition.lcdui.DisplayAccess;

// VM gọi Launcher.start(tên lớp MIDlet) trên một thread riêng
final class Launcher {
    private Launcher() {
    }

    static void start(String className) {
        MIDlet m;
        try {
            Class c = Class.forName(className);
            m = (MIDlet) c.newInstance();
        } catch (Throwable e) {
            System.out.println("Khong tao duoc MIDlet " + className + ": " + e);
            e.printStackTrace();
            DisplayAccess.fatalError(j2menx.Lang.t(j2menx.Lang.CREATE_MIDLET_FAILED) + e);
            return;
        }
        try {
            m.doStart();
        } catch (Throwable e) {
            System.out.println("startApp loi: " + e);
            e.printStackTrace();
            DisplayAccess.fatalError(j2menx.Lang.t(j2menx.Lang.START_APP_FAILED) + e);
        }
    }

    static void pause() {
        MIDlet m = MIDlet.getInstance();
        if (m != null) {
            m.doPause();
        }
    }

    static void resume() {
        MIDlet m = MIDlet.getInstance();
        if (m != null) {
            try {
                m.doStart();
            } catch (Throwable e) {
                e.printStackTrace();
            }
        }
    }

    static void destroy() {
        MIDlet m = MIDlet.getInstance();
        if (m != null) {
            m.doDestroy();
        }
        MIDlet.notifyDestroyed0();
    }
}
