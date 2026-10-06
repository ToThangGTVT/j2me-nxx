package com.siemens.mp.gsm;

import java.io.IOException;

public class SMS {
    private SMS() {
    }

    public static int send(String number, String data) throws IOException {
        throw new IOException("Khong gui duoc SMS: may khong co SIM");
    }

    public static int send(String number, byte[] data) throws IOException {
        throw new IOException("Khong gui duoc SMS: may khong co SIM");
    }
}
