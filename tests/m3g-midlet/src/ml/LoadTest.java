package ml;
import javax.microedition.lcdui.*;
import javax.microedition.m3g.*;
import javax.microedition.midlet.*;
public class LoadTest extends MIDlet {
    protected void startApp() {
        try {
            Object3D[] roots = Loader.load("/scene.m3g");
            System.out.println("roots=" + roots.length + " first=" + roots[0].getClass().getName());
            final World w = (World) roots[0];
            Mesh m = (Mesh) w.find(42);
            System.out.println("mesh=" + (m != null) + " tracks=" + m.getAnimationTrackCount() + " cam=" + (w.getActiveCamera() != null)
                + " bg=" + Integer.toHexString(w.getBackground().getColor()) + " children=" + w.getChildCount());
            final long start = System.currentTimeMillis();
            Display.getDisplay(this).setCurrent(new Canvas() {
                { setFullScreenMode(true); new Thread() { public void run() { while (true) { repaint(); serviceRepaints(); try { Thread.sleep(16); } catch (InterruptedException e) {} } } }.start(); }
                protected void paint(Graphics g) {
                    w.animate((int) (System.currentTimeMillis() - start));
                    Graphics3D g3 = Graphics3D.getInstance();
                    g3.bindTarget(g); try { g3.render(w); } finally { g3.releaseTarget(); }
                }
            });
        } catch (Throwable e) { System.out.println("Loader loi: " + e); e.printStackTrace(); }
    }
    protected void pauseApp() {} protected void destroyApp(boolean u) {}
}
