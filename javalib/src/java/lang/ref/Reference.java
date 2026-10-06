package java.lang.ref;

public abstract class Reference {
    private Object referent;

    Reference(Object referent) {
        this.referent = referent;
    }

    public Object get() {
        return referent;
    }

    public void clear() {
        referent = null;
    }
}
