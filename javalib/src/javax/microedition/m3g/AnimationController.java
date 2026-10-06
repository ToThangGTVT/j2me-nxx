package javax.microedition.m3g;

public class AnimationController extends Object3D {
    private int activeStart, activeEnd;
    private float weight = 1;
    private float speed = 1;
    private float refSequenceTime;
    private int refWorldTime;

    public AnimationController() {
    }

    Object3D duplicateImpl() {
        AnimationController c = new AnimationController();
        c.activeStart = activeStart;
        c.activeEnd = activeEnd;
        c.weight = weight;
        c.speed = speed;
        c.refSequenceTime = refSequenceTime;
        c.refWorldTime = refWorldTime;
        return c;
    }

    public void setActiveInterval(int start, int end) {
        if (start > end) {
            throw new IllegalArgumentException();
        }
        activeStart = start;
        activeEnd = end;
    }

    public int getActiveIntervalStart() { return activeStart; }
    public int getActiveIntervalEnd() { return activeEnd; }

    public void setSpeed(float speed, int worldTime) {
        refSequenceTime = sequenceTime(worldTime);
        refWorldTime = worldTime;
        this.speed = speed;
    }

    public float getSpeed() { return speed; }

    public void setPosition(float sequenceTime, int worldTime) {
        refSequenceTime = sequenceTime;
        refWorldTime = worldTime;
    }

    public float getPosition(int worldTime) {
        return sequenceTime(worldTime);
    }

    public int getRefWorldTime() { return refWorldTime; }

    public void setWeight(float weight) {
        if (weight < 0) {
            throw new IllegalArgumentException();
        }
        this.weight = weight;
    }

    public float getWeight() { return weight; }

    float sequenceTime(int worldTime) {
        return refSequenceTime + speed * (worldTime - refWorldTime);
    }

    boolean isActive(int worldTime) {
        return activeStart == activeEnd || (worldTime >= activeStart && worldTime < activeEnd);
    }
}
