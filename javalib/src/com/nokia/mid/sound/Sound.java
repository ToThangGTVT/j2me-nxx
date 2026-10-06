package com.nokia.mid.sound;

import j2menx.AudioPlayer;
import javax.microedition.media.MediaException;
import javax.microedition.media.Player;
import javax.microedition.media.PlayerListener;

public class Sound {
    public static final int FORMAT_TONE = 1;
    public static final int FORMAT_WAV = 5;
    public static final int SOUND_PLAYING = 0;
    public static final int SOUND_STOPPED = 1;
    public static final int SOUND_UNINITIALIZED = 3;

    private int gain = 255;
    private SoundListener listener;
    private AudioPlayer player;
    private int freq;
    private long duration;
    private int state = SOUND_UNINITIALIZED;

    public Sound(int freq, long duration) {
        init(freq, duration);
    }

    public Sound(byte[] data, int type) {
        init(data, type);
    }

    public void init(int freq, long duration) {
        release();
        this.freq = freq;
        this.duration = duration;
        state = SOUND_STOPPED;
    }

    public void init(byte[] data, int type) {
        release();
        if (type == FORMAT_WAV) {
            player = new AudioPlayer(data, "audio/x-wav");
        } else {
            // Định dạng ringtone OTA của Nokia: thử như MIDI, không được thì im lặng
            player = new AudioPlayer(data, "audio/midi");
        }
        player.addPlayerListener(new PlayerListener() {
            public void playerUpdate(Player p, String event, Object data) {
                if (event == END_OF_MEDIA || event == STOPPED) {
                    state = SOUND_STOPPED;
                    if (listener != null) {
                        listener.soundStateChanged(Sound.this, SOUND_STOPPED);
                    }
                }
            }
        });
        state = SOUND_STOPPED;
    }

    public void play(int loop) {
        if (player == null) {
            if (freq > 0 && duration > 0) {
                // Đổi tần số sang nốt MIDI gần nhất
                int note = (int) Math.floor(69 + 12 * Math.log(freq / 440.0) / Math.log(2) + 0.5);
                AudioPlayer.playTone(Math.max(0, Math.min(127, note)), (int) duration, gain * 100 / 255);
            }
            return;
        }
        try {
            player.setLoopCount(loop == 0 ? -1 : loop);
            player.setLevel(gain * 100 / 255);
            player.start();
            state = SOUND_PLAYING;
            if (listener != null) {
                listener.soundStateChanged(this, SOUND_PLAYING);
            }
        } catch (MediaException e) {
            state = SOUND_STOPPED;
        }
    }

    public void stop() {
        if (player != null) {
            try {
                player.stop();
            } catch (MediaException e) {
                // bỏ qua
            }
        }
        state = SOUND_STOPPED;
    }

    public void release() {
        if (player != null) {
            player.close();
            player = null;
        }
        state = SOUND_UNINITIALIZED;
    }

    public void resume() {
        play(1);
    }

    public int getState() {
        return state;
    }

    public void setGain(int gain) {
        this.gain = Math.max(0, Math.min(255, gain));
        if (player != null) {
            player.setLevel(this.gain * 100 / 255);
        }
    }

    public int getGain() {
        return gain;
    }

    public static int getConcurrentSoundCount(int type) {
        return 4;
    }

    public static int[] getSupportedFormats() {
        return new int[] { FORMAT_TONE, FORMAT_WAV };
    }

    public void setSoundListener(SoundListener listener) {
        this.listener = listener;
    }
}
