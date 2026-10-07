package javax.microedition.media.control;

import javax.microedition.media.Control;

public interface FramePositioningControl extends Control {
    int seek(int frameNumber);

    int skip(int framesToSkip);

    long mapFrameToTime(int frameNumber);

    int mapTimeToFrame(long mediaTime);
}
