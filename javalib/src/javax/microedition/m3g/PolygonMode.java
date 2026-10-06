package javax.microedition.m3g;

public class PolygonMode extends Object3D {
    public static final int CULL_BACK = 160;
    public static final int CULL_FRONT = 161;
    public static final int CULL_NONE = 162;
    public static final int SHADE_FLAT = 164;
    public static final int SHADE_SMOOTH = 165;
    public static final int WINDING_CCW = 168;
    public static final int WINDING_CW = 169;

    int culling = CULL_BACK, shading = SHADE_SMOOTH, winding = WINDING_CCW;
    boolean twoSided, localCamera, perspective = true;

    public PolygonMode() {
    }

    Object3D duplicateImpl() {
        PolygonMode p = new PolygonMode();
        p.culling = culling;
        p.shading = shading;
        p.winding = winding;
        p.twoSided = twoSided;
        p.localCamera = localCamera;
        p.perspective = perspective;
        return p;
    }

    public void setCulling(int mode) {
        if (mode < CULL_BACK || mode > CULL_NONE) {
            throw new IllegalArgumentException();
        }
        culling = mode;
    }

    public int getCulling() { return culling; }

    public void setWinding(int mode) {
        if (mode != WINDING_CCW && mode != WINDING_CW) {
            throw new IllegalArgumentException();
        }
        winding = mode;
    }

    public int getWinding() { return winding; }

    public void setShading(int mode) {
        if (mode != SHADE_FLAT && mode != SHADE_SMOOTH) {
            throw new IllegalArgumentException();
        }
        shading = mode;
    }

    public int getShading() { return shading; }
    public void setTwoSidedLightingEnable(boolean e) { twoSided = e; }
    public boolean isTwoSidedLightingEnabled() { return twoSided; }
    public void setLocalCameraLightingEnable(boolean e) { localCamera = e; }
    public boolean isLocalCameraLightingEnabled() { return localCamera; }
    public void setPerspectiveCorrectionEnable(boolean e) { perspective = e; }
    public boolean isPerspectiveCorrectionEnabled() { return perspective; }
}
