package javax.microedition.m3g;

public class AnimationTrack extends Object3D {
    public static final int ALPHA = 256;
    public static final int AMBIENT_COLOR = 257;
    public static final int COLOR = 258;
    public static final int CROP = 259;
    public static final int DENSITY = 260;
    public static final int DIFFUSE_COLOR = 261;
    public static final int EMISSIVE_COLOR = 262;
    public static final int FAR_DISTANCE = 263;
    public static final int FIELD_OF_VIEW = 264;
    public static final int INTENSITY = 265;
    public static final int MORPH_WEIGHTS = 266;
    public static final int NEAR_DISTANCE = 267;
    public static final int ORIENTATION = 268;
    public static final int PICKABILITY = 269;
    public static final int SCALE = 270;
    public static final int SHININESS = 271;
    public static final int SPECULAR_COLOR = 272;
    public static final int SPOT_ANGLE = 273;
    public static final int SPOT_EXPONENT = 274;
    public static final int TRANSLATION = 275;
    public static final int VISIBILITY = 276;

    private final KeyframeSequence sequence;
    private final int property;
    private AnimationController controller;

    public AnimationTrack(KeyframeSequence sequence, int property) {
        if (sequence == null) {
            throw new NullPointerException();
        }
        if (property < ALPHA || property > VISIBILITY) {
            throw new IllegalArgumentException();
        }
        this.sequence = sequence;
        this.property = property;
    }

    Object3D duplicateImpl() {
        AnimationTrack t = new AnimationTrack(sequence, property);
        t.controller = controller;
        return t;
    }

    int getReferencesImpl(Object3D[] out) {
        int n = super.getReferencesImpl(out);
        n = addRef(out, n, sequence);
        n = addRef(out, n, controller);
        return n;
    }

    public void setController(AnimationController c) {
        controller = c;
    }

    public AnimationController getController() { return controller; }
    public KeyframeSequence getKeyframeSequence() { return sequence; }
    public int getTargetProperty() { return property; }

    float activeWeight(int worldTime) {
        if (controller == null || !controller.isActive(worldTime)) {
            return 0;
        }
        return controller.getWeight();
    }

    void sample(int worldTime, float[] out) {
        sequence.sample(controller.sequenceTime(worldTime), out);
    }
}
