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
        return new String[] { "audio/midi", "audio/x-wav", "audio/mpeg", "audio/amr", "audio/mp4", "audio/aac",
                "audio/3gpp", "audio/x-tone-seq", "video/3gpp", "video/mp4", "video/mpeg4" };
    }

    public static String[] getSupportedProtocols(String contentType) {
        return new String[] { "device", "resource", "file", "http", "https" };
    }

    // Đoán kiểu theo đuôi file của locator
    private static String typeOf(String locator) {
        String l = locator.toLowerCase();
        int q = l.indexOf('?');
        if (q >= 0) {
            l = l.substring(0, q);
        }
        if (l.endsWith(".3gp") || l.endsWith(".3g2")) {
            return "video/3gpp";
        }
        if (l.endsWith(".mp4") || l.endsWith(".m4v")) {
            return "video/mp4";
        }
        if (l.endsWith(".amr")) {
            return "audio/amr";
        }
        if (l.endsWith(".mp3")) {
            return "audio/mpeg";
        }
        if (l.endsWith(".mid") || l.endsWith(".midi")) {
            return "audio/midi";
        }
        if (l.endsWith(".wav")) {
            return "audio/x-wav";
        }
        return null;
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
            return createPlayer(in, typeOf(locator));
        }
        String lower = locator.toLowerCase();
        if (lower.startsWith("capture:")) {
            throw new MediaException("Khong ho tro camera / ghi am");
        }
        if (lower.startsWith("file:") || lower.startsWith("http:") || lower.startsWith("https:")) {
            javax.microedition.io.InputConnection c = (javax.microedition.io.InputConnection)
                    javax.microedition.io.Connector.open(locator, javax.microedition.io.Connector.READ);
            try {
                InputStream in = c.openInputStream();
                try {
                    return createPlayer(in, typeOf(locator));
                } finally {
                    in.close();
                }
            } finally {
                c.close();
            }
        }
        if (lower.startsWith("rtsp:")) {
            throw new MediaException("Khong ho tro " + locator);
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
        byte[] data = bo.toByteArray();
        if (j2menx.VideoPlayer.isVideo(data, type)) {
            return new j2menx.VideoPlayer(data, type);
        }
        return new j2menx.AudioPlayer(data, type);
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
