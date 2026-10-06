package javax.microedition.lcdui.game;

import javax.microedition.lcdui.Graphics;
import javax.microedition.lcdui.Image;

public class TiledLayer extends Layer {
    private final int rows, cols;
    private final int[] cells;
    private Image image;
    private int cellW, cellH;
    private int tilesPerRow;
    private int staticCount;
    private int[] animated = new int[4];
    private int animatedCount;

    public TiledLayer(int columns, int rows, Image image, int tileWidth, int tileHeight) {
        super(columns * tileWidth, rows * tileHeight);
        if (columns <= 0 || rows <= 0) {
            throw new IllegalArgumentException();
        }
        this.cols = columns;
        this.rows = rows;
        cells = new int[columns * rows];
        setImage(image, tileWidth, tileHeight);
    }

    private void setImage(Image img, int tw, int th) {
        if (tw <= 0 || th <= 0 || img.getWidth() % tw != 0 || img.getHeight() % th != 0) {
            throw new IllegalArgumentException();
        }
        image = img;
        cellW = tw;
        cellH = th;
        tilesPerRow = img.getWidth() / tw;
        staticCount = tilesPerRow * (img.getHeight() / th);
        width = cols * tw;
        height = rows * th;
    }

    public void setStaticTileSet(Image img, int tw, int th) {
        int oldCount = staticCount;
        setImage(img, tw, th);
        if (staticCount < oldCount) {
            for (int i = 0; i < cells.length; i++) {
                cells[i] = 0;
            }
            animatedCount = 0;
        }
    }

    public int createAnimatedTile(int staticTileIndex) {
        if (staticTileIndex < 0 || staticTileIndex > staticCount) {
            throw new IndexOutOfBoundsException();
        }
        if (animatedCount == animated.length) {
            int[] n = new int[animated.length * 2];
            System.arraycopy(animated, 0, n, 0, animatedCount);
            animated = n;
        }
        animated[animatedCount++] = staticTileIndex;
        return -animatedCount;
    }

    public void setAnimatedTile(int animatedTileIndex, int staticTileIndex) {
        int i = -animatedTileIndex - 1;
        if (i < 0 || i >= animatedCount || staticTileIndex < 0 || staticTileIndex > staticCount) {
            throw new IndexOutOfBoundsException();
        }
        animated[i] = staticTileIndex;
    }

    public int getAnimatedTile(int animatedTileIndex) {
        int i = -animatedTileIndex - 1;
        if (i < 0 || i >= animatedCount) {
            throw new IndexOutOfBoundsException();
        }
        return animated[i];
    }

    public void setCell(int col, int row, int tileIndex) {
        if (col < 0 || col >= cols || row < 0 || row >= rows) {
            throw new IndexOutOfBoundsException();
        }
        if (tileIndex > staticCount || -tileIndex > animatedCount) {
            throw new IndexOutOfBoundsException();
        }
        cells[row * cols + col] = tileIndex;
    }

    public int getCell(int col, int row) {
        if (col < 0 || col >= cols || row < 0 || row >= rows) {
            throw new IndexOutOfBoundsException();
        }
        return cells[row * cols + col];
    }

    public void fillCells(int col, int row, int numCols, int numRows, int tileIndex) {
        if (numCols < 0 || numRows < 0 || col < 0 || row < 0 || col + numCols > cols || row + numRows > rows) {
            throw new IndexOutOfBoundsException();
        }
        for (int r = row; r < row + numRows; r++) {
            for (int c = col; c < col + numCols; c++) {
                cells[r * cols + c] = tileIndex;
            }
        }
    }

    public final int getCellWidth() {
        return cellW;
    }

    public final int getCellHeight() {
        return cellH;
    }

    public final int getColumns() {
        return cols;
    }

    public final int getRows() {
        return rows;
    }

    private int resolve(int tile) {
        return tile < 0 ? animated[-tile - 1] : tile;
    }

    boolean opaqueAt(int px, int py) {
        int lx = px - x, ly = py - y;
        if (lx < 0 || ly < 0 || lx >= width || ly >= height) {
            return false;
        }
        int t = resolve(cells[(ly / cellH) * cols + lx / cellW]);
        if (t == 0) {
            return false;
        }
        t--;
        int sx = (t % tilesPerRow) * cellW + lx % cellW;
        int sy = (t / tilesPerRow) * cellH + ly % cellH;
        int[] p = new int[1];
        image.getRGB(p, 0, 1, sx, sy, 1, 1);
        return (p[0] >>> 24) != 0;
    }

    public final void paint(Graphics g) {
        if (!visible) {
            return;
        }
        // Chỉ vẽ các ô nằm trong vùng clip
        int clipX = g.getClipX(), clipY = g.getClipY();
        int c0 = Math.max(0, (clipX - x) / cellW);
        int r0 = Math.max(0, (clipY - y) / cellH);
        int c1 = Math.min(cols - 1, (clipX + g.getClipWidth() - x - 1) / cellW);
        int r1 = Math.min(rows - 1, (clipY + g.getClipHeight() - y - 1) / cellH);
        for (int r = r0; r <= r1; r++) {
            for (int c = c0; c <= c1; c++) {
                int t = resolve(cells[r * cols + c]);
                if (t == 0) {
                    continue;
                }
                t--;
                g.drawRegion(image, (t % tilesPerRow) * cellW, (t / tilesPerRow) * cellH, cellW, cellH, 0,
                        x + c * cellW, y + r * cellH, Graphics.TOP | Graphics.LEFT);
            }
        }
    }
}
