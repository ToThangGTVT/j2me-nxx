package java.lang.ref;

// GC chưa hỗ trợ tham chiếu yếu: giữ như tham chiếu mạnh
public class WeakReference extends Reference {
    public WeakReference(Object referent) {
        super(referent);
    }
}
