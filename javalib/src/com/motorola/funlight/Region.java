package com.motorola.funlight;

public interface Region {
    int getColor();

    int setColor(int color);

    int getControl();

    void releaseControl();

    int getID();
}
