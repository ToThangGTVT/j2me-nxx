package javax.microedition.media;

import java.io.IOException;
import java.io.InputStream;

public final class Manager {
    public static final String TONE_DEVICE_LOCATOR = "device://tone";
    public static final String MIDI_DEVICE_LOCATOR = "device://midi";

    private static final TimeBase systemTimeBase = new TimeBase() {
        public long getTime() {
            return System.currentTimeMillis() * 1000;
        }
    };

    private Manager() {
    }

    public static String[] getSupportedContentTypes(String protocol) {
        return new String[] { "audio/midi", "audio/x-wav", "audio/mpeg", "audio/amr", "audio/x-tone-seq" };
    }

    public static String[] getSupportedProtocols(String contentType) {
        return new String[] { "device", "resource" };
    }

    public static Player createPlayer(String locator) throws IOException, MediaException {
        if (locator == null) {
            throw new IllegalArgumentException();
        }
        if (locator.startsWith("resource:")) {
            InputStream in = Manager.class.getResourceAsStream(locator.substring(9));
            if (in == null) {
                throw new MediaException("Khong tim thay " + locator);
            }
            return createPlayer(in, null);
        }
        return new j2menx.AudioPlayer(null, locator.startsWith(TONE_DEVICE_LOCATOR) ? "audio/x-tone-seq" : "audio/midi");
    }

    public static Player createPlayer(InputStream stream, String type) throws IOException, MediaException {
        if (stream == null) {
            throw new IllegalArgumentException();
        }
        java.io.ByteArrayOutputStream bo = new java.io.ByteArrayOutputStream();
        byte[] buf = new byte[4096];
        int n;
        while ((n = stream.read(buf, 0, buf.length)) > 0) {
            bo.write(buf, 0, n);
        }
        return new j2menx.AudioPlayer(bo.toByteArray(), type);
    }

    public static void playTone(int note, int duration, int volume) throws MediaException {
        if (note < 0 || note > 127 || duration <= 0) {
            throw new IllegalArgumentException();
        }
        j2menx.AudioPlayer.playTone(note, duration, volume);
    }

    public static TimeBase getSystemTimeBase() {
        return systemTimeBase;
    }
}
