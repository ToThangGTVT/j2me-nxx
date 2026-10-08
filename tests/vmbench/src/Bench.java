// Đo tốc độ trình thông dịch và kiểm tra kết quả: mỗi bài trả về 1 checksum,
// tối ưu xong checksum phải giữ nguyên.
import java.util.Hashtable;
import java.util.Vector;

public class Bench {
    // --- dữ liệu dùng chung

    interface Shape {
        int area();
    }

    static abstract class Base implements Shape {
        int x, y;
        long big;
        double scale = 1.5;

        Base(int x, int y) {
            this.x = x;
            this.y = y;
        }

        int sum() {
            return x + y;
        }

        abstract int kind();
    }

    static class Rect extends Base {
        int w, h;

        Rect(int x, int y, int w, int h) {
            super(x, y);
            this.w = w;
            this.h = h;
        }

        public int area() {
            return w * h;
        }

        int kind() {
            return 1;
        }

        int sum() {
            return super.sum() + w;
        }
    }

    static class Circle extends Base {
        int r;

        Circle(int x, int y, int r) {
            super(x, y);
            this.r = r;
        }

        public int area() {
            return 3 * r * r;
        }

        int kind() {
            return 2;
        }
    }

    static class Counter {
        private int n;

        synchronized void inc() {
            n++;
        }

        private int get() {
            return n;
        }

        int value() {
            return get();
        }
    }

    static class Lazy {
        static int[] table;
        static long seed;

        static {
            table = new int[64];
            for (int i = 0; i < 64; i++)
                table[i] = i * i + 7;
            seed = 0x123456789L;
        }
    }

    static int sCount;
    static long sLong;
    static double sDouble;
    static Object sObj;

    // --- các bài đo

    // Vòng lặp số nguyên thuần, biến cục bộ
    static int loops(int n) {
        int a = 1, b = 0;
        for (int i = 0; i < n; i++) {
            a = a * 31 + i;
            b ^= a >>> 3;
            if ((i & 7) == 0)
                b += i % 13;
        }
        return a ^ b;
    }

    // Đọc ghi field object (giống di chuyển sprite)
    static int fields(int n) {
        Rect r = new Rect(1, 2, 3, 4);
        Circle c = new Circle(5, 6, 7);
        for (int i = 0; i < n; i++) {
            r.x += r.w;
            r.y -= c.r;
            c.x = r.x ^ c.y;
            c.big += r.x;
            r.scale = r.scale * 0.5 + 1.0;
            if (r.x > 10000)
                r.x -= 20000;
        }
        return r.x + r.y + c.x + (int) c.big + (int) (r.scale * 1000);
    }

    // Field static, cả long / double / tham chiếu, và lớp có <clinit>
    static int statics(int n) {
        sCount = 0;
        sLong = 0;
        sDouble = 0;
        for (int i = 0; i < n; i++) {
            sCount += Lazy.table[i & 63];
            sLong += Lazy.seed ^ i;
            sDouble += 0.25;
            sObj = (i & 1) == 0 ? null : Lazy.table;
        }
        return sCount + (int) (sLong >>> 7) + (int) sDouble + (sObj == null ? 0 : 1);
    }

    // Gọi method ảo / interface / private / super / synchronized
    static int calls(int n) {
        Base[] objs = { new Rect(1, 1, 2, 3), new Circle(2, 2, 4), new Rect(3, 3, 5, 1) };
        Shape[] shapes = new Shape[] { objs[0], objs[1], objs[2] };
        Counter cnt = new Counter();
        int acc = 0;
        for (int i = 0; i < n; i++) {
            Base b = objs[i % 3];
            acc += b.sum() + b.kind();
            acc += shapes[(i + 1) % 3].area();
            acc += max(i & 15, 9);
            if ((i & 3) == 0)
                cnt.inc();
        }
        return acc + cnt.value();
    }

    static int max(int a, int b) {
        return a > b ? a : b;
    }

    // Mảng 1 và 2 chiều (bản đồ ô, buffer điểm ảnh)
    static int arrays(int n) {
        int[][] map = new int[32][48];
        byte[] buf = new byte[1024];
        char[] chars = new char[256];
        short[] shorts = new short[256];
        long[] longs = new long[64];
        int acc = 0;
        for (int k = 0; k < n; k++) {
            for (int y = 0; y < 32; y++) {
                int[] row = map[y];
                for (int x = 0; x < 48; x++)
                    row[x] += x * y + k;
            }
            for (int i = 0; i < buf.length; i++)
                buf[i] = (byte) (buf[i] + i);
            for (int i = 0; i < chars.length; i++) {
                chars[i] = (char) (chars[i] + 3);
                shorts[i] = (short) (shorts[i] - 5);
            }
            longs[k & 63] += k;
        }
        for (int y = 0; y < 32; y++)
            for (int x = 0; x < 48; x++)
                acc += map[y][x];
        for (int i = 0; i < buf.length; i++)
            acc += buf[i];
        acc += chars[7] + shorts[9] + (int) longs[5];
        return acc;
    }

    // Đệ quy: chi phí đẩy / bỏ frame
    static int fib(int n) {
        return n < 2 ? n : fib(n - 1) + fib(n - 2);
    }

    // Thư viện: String, StringBuffer, Vector, Hashtable
    static int library(int n) {
        Vector v = new Vector();
        Hashtable h = new Hashtable();
        int acc = 0;
        for (int i = 0; i < n; i++) {
            StringBuffer sb = new StringBuffer();
            sb.append("item").append(i).append('-').append(i * 7);
            String s = sb.toString();
            acc += s.length() + s.charAt(s.length() - 1) + s.hashCode();
            if (v.size() < 200)
                v.addElement(s);
            h.put(new Integer(i & 255), s);
            Object o = h.get(new Integer((i * 3) & 255));
            if (o != null)
                acc += ((String) o).indexOf('-');
        }
        return acc + v.size() + h.size();
    }

    // Exception: ném và bắt, kể cả từ frame sâu hơn
    static int exceptions(int n) {
        int acc = 0;
        int[] small = new int[4];
        try {
            acc += deep(0);
        } catch (StackOverflowError e) {
            acc += 1000;
        }
        for (int i = 0; i < n; i++) {
            try {
                acc += small[i & 7];
            } catch (ArrayIndexOutOfBoundsException e) {
                acc += 3;
            }
            try {
                acc += 100 / (i & 3);
            } catch (ArithmeticException e) {
                acc += 5;
            }
            try {
                thrower(i);
            } catch (IllegalStateException e) {
                acc += 7;
            } finally {
                acc++;
            }
            Object o = (i & 1) == 0 ? (Object) "s" : (Object) small;
            try {
                acc += ((String) o).length();
            } catch (ClassCastException e) {
                acc += 11;
            }
        }
        return acc;
    }

    static int deep(int depth) {
        return deep(depth + 1) + 1;
    }

    static void thrower(int i) {
        if ((i % 5) == 0)
            throw new IllegalStateException();
    }

    // long / float / double, switch, instanceof
    static int mixed(int n) {
        long l = 1;
        float f = 0.5f;
        double d = 2.0;
        int acc = 0;
        Object[] things = { "a", new Integer(1), new Rect(0, 0, 1, 1), null };
        for (int i = 0; i < n; i++) {
            l = l * 6364136223846793005L + 1442695040888963407L;
            f = f * 1.0001f + (float) (i & 3);
            d = d / 1.5 + i;
            switch (i & 7) {
            case 0: acc += 1; break;
            case 1: acc += 3; break;
            case 2: acc -= 2; break;
            case 5: acc ^= 9; break;
            default: acc += (int) (l >>> 60);
            }
            switch ((int) (l >>> 58)) {
            case 3: acc += 17; break;
            case 40: acc += 19; break;
            case 1000: acc += 23; break;
            }
            Object o = things[i & 3];
            if (o instanceof String)
                acc++;
            else if (o instanceof Shape)
                acc += 2;
        }
        return acc + (int) l + (int) f + (int) d;
    }

    // Nhiều thread: producer / consumer qua wait / notify, kèm 1 thread tính toán không nhường lượt
    static class Box {
        private int value;
        private boolean full;

        synchronized void put(int v) throws InterruptedException {
            while (full)
                wait();
            value = v;
            full = true;
            notifyAll();
        }

        synchronized int take() throws InterruptedException {
            while (!full)
                wait();
            full = false;
            notifyAll();
            return value;
        }
    }

    static int threads(final int n) {
        final Box box = new Box();
        final int[] busyResult = new int[1];
        Thread producer = new Thread() {
            public void run() {
                try {
                    for (int i = 0; i < n; i++)
                        box.put(i * 3 + 1);
                } catch (InterruptedException e) {
                }
            }
        };
        Thread busy = new Thread() {
            public void run() {
                busyResult[0] = loops(n * 20);
            }
        };
        producer.start();
        busy.start();
        int acc = 0;
        try {
            for (int i = 0; i < n; i++)
                acc = acc * 31 + box.take();
            producer.join();
            busy.join();
        } catch (InterruptedException e) {
            return -1;
        }
        return acc + busyResult[0];
    }

    // --- điểm vào cho vmbench: kết quả để ở field result

    public static int result;

    public static void main(int which, int n) {
        result = run(which, n);
    }

    static int run(int which, int n) {
        switch (which) {
        case 0: return loops(n);
        case 1: return fields(n);
        case 2: return statics(n);
        case 3: return calls(n);
        case 4: return arrays(n);
        case 5: return fib(n);
        case 6: return library(n);
        case 7: return exceptions(n);
        case 8: return mixed(n);
        case 9: return threads(n);
        }
        return -1;
    }
}
