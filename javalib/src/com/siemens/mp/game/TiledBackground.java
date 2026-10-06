package com.siemens.mp.game;

import javax.microedition.lcdui.Graphics;
import javax.microedition.lcdui.Image;

// Nền ghép ô 8x8 của Siemens. Ô 0 = trong suốt, 1 = trắng, 2 = đen, 3+ = ô thứ (n-3) trong ảnh
public class TiledBackground extends GraphicObject {
    private static final int TILE = 8;
    private final Image tiles;
    private final byte[] map;
    private final int mapW, mapH;
    private int posX, posY;

    public TiledBackground(Image pixels, Image mask, byte[] map, int widthInTiles, int heightInTiles) {
        this.tiles = pixels;
        this.map = map;
        this.mapW = widthInTiles;
        this.mapH = heightInTiles;
    }

    public TiledBackground(byte[] pixels, byte[] mask, byte[] map, int widthInTiles, int heightInTiles) {
        this(Sprite.bitImage(pixels, 0, TILE, pixels.length / 1, false), null, map, widthInTiles, heightInTiles);
    }

    public TiledBackground(ExtendedImage pixels, ExtendedImage mask, byte[] map, int widthInTiles, int heightInTiles) {
        this(pixels.getImage(), mask == null ? null : mask.getImage(), map, widthInTiles, heightInTiles);
    }

    public void setPositionInMap(int x, int y) {
        posX = x;
        posY = y;
    }

    public int getPositionInMapX() {
        return posX;
    }

    public int getPositionInMapY() {
        return posY;
    }

    void paint(Graphics g, int ox, int oy) {
        int cw = g.getClipWidth(), ch = g.getClipHeight();
        int c0 = Math.max(0, posX / TILE), r0 = Math.max(0, posY / TILE);
        int c1 = Math.min(mapW - 1, (posX + cw) / TILE + 1), r1 = Math.min(mapH - 1, (posY + ch) / TILE + 1);
        int tilesPerCol = tiles.getHeight() / TILE;
        for (int r = r0; r <= r1; r++) {
            for (int c = c0; c <= c1; c++) {
                int t = map[r * mapW + c] & 0xff;
                int dx = ox + c * TILE - posX, dy = oy + r * TILE - posY;
                if (t == 0) {
                    continue;
                }
                if (t == 1 || t == 2) {
                    g.setColor(t == 1 ? 0xffffff : 0);
                    g.fillRect(dx, dy, TILE, TILE);
                    continue;
                }
                t -= 3;
                if (t < tilesPerCol) {
                    g.drawRegion(tiles, 0, t * TILE, TILE, TILE, 0, dx, dy, Graphics.TOP | Graphics.LEFT);
                }
            }
        }
    }
}
