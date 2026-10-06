package mt;
import javax.microedition.lcdui.*;
import javax.microedition.m3g.*;
import javax.microedition.midlet.*;
public class Cube3D extends MIDlet {
    protected void startApp() { Display.getDisplay(this).setCurrent(new C()); }
    protected void pauseApp() {} protected void destroyApp(boolean u) {}
    static class C extends Canvas implements Runnable {
        World world; Mesh cube; Graphics3D g3d = Graphics3D.getInstance(); int angle; long t0 = System.currentTimeMillis(); int frames, fps;
        C() {
            setFullScreenMode(true);
            short[] v = { -1,-1,1, 1,-1,1, -1,1,1, 1,1,1,  1,-1,-1, -1,-1,-1, 1,1,-1, -1,1,-1,
                          -1,-1,-1, -1,-1,1, -1,1,-1, -1,1,1,  1,-1,1, 1,-1,-1, 1,1,1, 1,1,-1,
                          -1,1,1, 1,1,1, -1,1,-1, 1,1,-1,  -1,-1,-1, 1,-1,-1, -1,-1,1, 1,-1,1 };
            byte[] n = new byte[72]; int[][] fn = {{0,0,127},{0,0,-127},{-127,0,0},{127,0,0},{0,127,0},{0,-127,0}};
            for (int f = 0; f < 6; f++) for (int k = 0; k < 4; k++) { n[(f*4+k)*3]=(byte)fn[f][0]; n[(f*4+k)*3+1]=(byte)fn[f][1]; n[(f*4+k)*3+2]=(byte)fn[f][2]; }
            short[] t = new short[48]; for (int i = 0; i < 6; i++) { short[] q = {0,1, 1,1, 0,0, 1,0}; System.arraycopy(q, 0, t, i*8, 8); }
            VertexArray pos = new VertexArray(24, 3, 2); pos.set(0, 24, v);
            VertexArray nrm = new VertexArray(24, 3, 1); nrm.set(0, 24, n);
            VertexArray tc = new VertexArray(24, 2, 2); tc.set(0, 24, t);
            VertexBuffer vb = new VertexBuffer(); vb.setPositions(pos, 1, null); vb.setNormals(nrm); vb.setTexCoords(0, tc, 1, null);
            int[] strips = {4,4,4,4,4,4}; int[] idx = new int[24]; for (int i = 0; i < 24; i++) idx[i] = i;
            IndexBuffer ib = new TriangleStripArray(idx, strips);
            Image img = Image.createImage(64, 64); Graphics ig = img.getGraphics();
            for (int y = 0; y < 8; y++) for (int x = 0; x < 8; x++) { ig.setColor(((x + y) & 1) == 0 ? 0xffcc00 : 0x2266ff); ig.fillRect(x*8, y*8, 8, 8); }
            Texture2D tex = new Texture2D(new Image2D(Image2D.RGB, img));
            tex.setBlending(Texture2D.FUNC_MODULATE);
            Material mat = new Material(); mat.setColor(Material.DIFFUSE, 0xffffffff); mat.setColor(Material.SPECULAR, 0xffffff); mat.setShininess(40);
            Appearance ap = new Appearance(); ap.setMaterial(mat); ap.setTexture(0, tex);
            cube = new Mesh(vb, ib, ap);
            world = new World();
            Camera cam = new Camera(); cam.setPerspective(60, getWidth() / (float) getHeight(), 0.1f, 100);
            cam.setTranslation(0, 0, 5); world.addChild(cam); world.setActiveCamera(cam);
            Light l = new Light(); l.setMode(Light.DIRECTIONAL); l.setOrientation(-30, 1, 1, 0); world.addChild(l);
            Light amb = new Light(); amb.setMode(Light.AMBIENT); amb.setIntensity(0.4f); world.addChild(amb);
            Background bg = new Background(); bg.setColor(0x203048); world.setBackground(bg);
            world.addChild(cube);
            Image si = Image.createImage(16, 16); Graphics sg = si.getGraphics(); sg.setColor(0xff4040); sg.fillArc(0, 0, 16, 16, 0, 360);
            Appearance sa = new Appearance(); CompositingMode cm = new CompositingMode(); cm.setBlending(CompositingMode.ALPHA); sa.setCompositingMode(cm);
            Sprite3D sp = new Sprite3D(false, new Image2D(Image2D.RGB, si), sa); sp.setTranslation(1.6f, 1.6f, 0); world.addChild(sp);
            new Thread(this).start();
        }
        public void run() { while (true) { angle = (angle + 2) % 360; repaint(); serviceRepaints(); try { Thread.sleep(5); } catch (InterruptedException e) {} } }
        protected void paint(Graphics g) {
            cube.setOrientation(angle, 1, 1, 0.3f);
            g3d.bindTarget(g); try { g3d.render(world); } finally { g3d.releaseTarget(); }
            frames++; long now = System.currentTimeMillis(); if (now - t0 >= 1000) { fps = frames; frames = 0; t0 = now; }
            g.setColor(0xffffff); g.drawString("M3G FPS " + fps, 4, 4, Graphics.TOP | Graphics.LEFT);
        }
    }
}
