package com.nokia.mid.sound;

public class Sound {
    public static final int FORMAT_TONE = 1;
    public static final int FORMAT_WAV = 5;
    public static final int SOUND_PLAYING = 0;
    public static final int SOUND_STOPPED = 1;
    public static final int SOUND_UNINITIALIZED = 3;

    private int gain = 255;
    private SoundListener listener;

    public Sound(int freq, long duration) {
    }

    public Sound(byte[] data, int type) {
    }

    public void init(int freq, long duration) {
    }

    public void init(byte[] data, int type) {
    }

    public void play(int loop) {
        if (listener != null) {
            listener.soundStateChanged(this, SOUND_STOPPED);
        }
    }

    public void stop() {
    }

    public void release() {
    }

    public void resume() {
    }

    public int getState() {
        return SOUND_STOPPED;
    }

    public void setGain(int gain) {
        this.gain = gain;
    }

    public int getGain() {
        return gain;
    }

    public static int getConcurrentSoundCount(int type) {
        return 1;
    }

    public static int[] getSupportedFormats() {
        return new int[] { FORMAT_TONE, FORMAT_WAV };
    }

    public void setSoundListener(SoundListener listener) {
        this.listener = listener;
    }
}
