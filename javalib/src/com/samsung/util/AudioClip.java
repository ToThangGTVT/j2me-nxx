package com.samsung.util;

import j2menx.AudioPlayer;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import javax.microedition.media.MediaException;

public class AudioClip {
    public static final int TYPE_MMF = 1;
    public static final int TYPE_MP3 = 2;
    public static final int TYPE_MIDI = 3;

    private final AudioPlayer player;

    public AudioClip(int type, String filename) throws IOException {
        InputStream in = AudioClip.class.getResourceAsStream(filename.startsWith("/") ? filename : "/" + filename);
        if (in == null) {
            throw new IOException("Khong tim thay " + filename);
        }
        ByteArrayOutputStream bo = new ByteArrayOutputStream();
        byte[] buf = new byte[4096];
        int n;
        while ((n = in.read(buf, 0, buf.length)) > 0) {
            bo.write(buf, 0, n);
        }
        player = new AudioPlayer(bo.toByteArray(), mime(type));
    }

    public AudioClip(int type, byte[] audioData, int audioOffset, int audioLength) {
        byte[] d = new byte[audioLength];
        System.arraycopy(audioData, audioOffset, d, 0, audioLength);
        player = new AudioPlayer(d, mime(type));
    }

    private static String mime(int type) {
        return type == TYPE_MP3 ? "audio/mpeg" : type == TYPE_MMF ? "application/vnd.smaf" : "audio/midi";
    }

    public static boolean isSupported() {
        return true;
    }

    // loop: 0 = phát 1 lần, 255 = lặp mãi; volume 0..5
    public void play(int loop, int volume) {
        try {
            player.stop();
            player.setMediaTime(0);
        } catch (Exception e) {
            // bỏ qua
        }
        try {
            player.setLoopCount(loop == 255 ? -1 : loop + 1);
            player.setLevel(Math.max(0, Math.min(5, volume)) * 20);
            player.start();
        } catch (MediaException e) {
            // bỏ qua
        }
    }

    public void stop() {
        try {
            player.stop();
        } catch (MediaException e) {
            // bỏ qua
        }
    }

    public void pause() {
        stop();
    }

    public void resume() {
        try {
            player.start();
        } catch (MediaException e) {
            // bỏ qua
        }
    }
}
