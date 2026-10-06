package com.siemens.mp.ui;

import java.io.IOException;

// Các hàm tiện ích tạo ảnh của Siemens, đều trả về Image của MIDP
public class Image {
    private Image() {
    }

    public static javax.microedition.lcdui.Image createImageWithoutScaling(String name) throws IOException {
        return javax.microedition.lcdui.Image.createImage(name);
    }

    public static javax.microedition.lcdui.Image createImageWithScaling(String name) throws IOException {
        return javax.microedition.lcdui.Image.createImage(name);
    }

    public static javax.microedition.lcdui.Image createImageFromBitmap(byte[] bytes, int width, int height) {
        int[] argb = new int[width * height];
        int bpr = (width + 7) / 8;
        for (int r = 0; r < height; r++) {
            for (int c = 0; c < width; c++) {
                int bit = (bytes[r * bpr + c / 8] >> (7 - (c & 7))) & 1;
                argb[r * width + c] = bit != 0 ? 0xff000000 : 0xffffffff;
            }
        }
        return javax.microedition.lcdui.Image.createRGBImage(argb, width, height, false);
    }

    public static javax.microedition.lcdui.Image mirrorImageHorizontally(javax.microedition.lcdui.Image img) {
        return javax.microedition.lcdui.Image.createImage(img, 0, 0, img.getWidth(), img.getHeight(), 2);
    }

    public static javax.microedition.lcdui.Image mirrorImageVertically(javax.microedition.lcdui.Image img) {
        return javax.microedition.lcdui.Image.createImage(img, 0, 0, img.getWidth(), img.getHeight(), 1);
    }
}
