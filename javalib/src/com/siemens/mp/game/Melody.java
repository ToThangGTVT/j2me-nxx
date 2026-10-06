package com.siemens.mp.game;

import j2menx.AudioPlayer;
import javax.microedition.media.MediaException;

// Giai điệu tạo bởi MelodyComposer, phát qua ToneControl
public class Melody {
    private static Melody playing;
    private final AudioPlayer player;

    Melody(byte[] toneSequence) {
        player = new AudioPlayer(null, "audio/x-tone-seq");
        player.setSequence(toneSequence);
    }

    public void play() {
        stop();
        try {
            player.start();
            playing = this;
        } catch (MediaException e) {
            // bỏ qua
        }
    }

    public static void stop() {
        if (playing != null) {
            try {
                playing.player.stop();
            } catch (MediaException e) {
                // bỏ qua
            }
            playing = null;
        }
    }
}
