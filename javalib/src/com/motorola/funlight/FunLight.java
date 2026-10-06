package com.motorola.funlight;

// Đèn trang trí của Motorola: chỉ ghi nhớ màu, không có đèn thật
public class FunLight {
    public static final int BLACK = 0x000000;
    public static final int WHITE = 0xffffff;
    public static final int RED = 0xff0000;
    public static final int GREEN = 0x00ff00;
    public static final int BLUE = 0x0000ff;
    public static final int OFF = BLACK;
    public static final int ON = WHITE;
    public static final int SUCCESS = 0;
    public static final int QUEUED = 1;
    public static final int IGNORED = 2;

    private static final Region[] regions = new Region[4];

    static {
        for (int i = 0; i < regions.length; i++) {
            final int id = i + 1;
            regions[i] = new Region() {
                private int color;

                public int getColor() { return color; }
                public int setColor(int c) { color = c; return SUCCESS; }
                public int getControl() { return SUCCESS; }
                public void releaseControl() { }
                public int getID() { return id; }
            };
        }
    }

    private FunLight() {
    }

    public static Region getRegion(int id) {
        return id >= 1 && id <= regions.length ? regions[id - 1] : null;
    }

    public static Region[] getRegions() {
        return regions;
    }

    public static Region[] getRegionsIDs() {
        return regions;
    }

    public static int setColor(int color) {
        for (int i = 0; i < regions.length; i++) {
            regions[i].setColor(color);
        }
        return SUCCESS;
    }

    public static int getControl() {
        return SUCCESS;
    }

    public static void releaseControl() {
    }
}
