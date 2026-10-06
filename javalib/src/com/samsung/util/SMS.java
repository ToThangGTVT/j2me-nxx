package com.samsung.util;

import java.io.IOException;

public class SMS {
    private SMS() {
    }

    public static boolean isSupported() {
        return false;
    }

    public static void send(SM sm) throws IOException {
        throw new IOException("Khong gui duoc SMS: may khong co SIM");
    }
}
