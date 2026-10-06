package com.samsung.util;

public class SM {
    private String dest, callback, text;

    public SM() {
    }

    public SM(String dest, String callback, String text) {
        this.dest = dest;
        this.callback = callback;
        this.text = text;
    }

    public void setDestAddress(String d) { dest = d; }
    public String getDestAddress() { return dest; }
    public void setCallbackAddress(String c) { callback = c; }
    public String getCallbackAddress() { return callback; }
    public void setData(String t) { text = t; }
    public String getData() { return text; }
}
