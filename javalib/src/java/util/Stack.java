package java.util;

public class Stack extends Vector {
    public Stack() {
    }

    public Object push(Object item) {
        addElement(item);
        return item;
    }

    public synchronized Object pop() {
        Object o = peek();
        removeElementAt(size() - 1);
        return o;
    }

    public synchronized Object peek() {
        if (size() == 0) {
            throw new EmptyStackException();
        }
        return elementAt(size() - 1);
    }

    public boolean empty() {
        return size() == 0;
    }

    public synchronized int search(Object o) {
        int i = lastIndexOf(o);
        return i >= 0 ? size() - i : -1;
    }
}
