## J2ME-NXX v0.5.0

The app is now called **J2ME-NXX**, and everything lives in its own folder `sdmc:/switch/j2me-nxx/`.

### ⚠️ Breaking changes
- **New folder.** Games are read from `sdmc:/switch/j2me-nxx/games/`. The old `sdmc:/switch/j2me-nx/games/` folder is no longer scanned: move your `.jar` files over.
- **Saves and settings start fresh.** Saves (`rms/`), game files (`files/`), `settings.ini` and `log.txt` are now under `sdmc:/switch/j2me-nxx/`. Data from `sdmc:/switch/j2me-nx/` is not migrated.
- Per-game options moved from `games/<name>.ini` to `options/<name>.ini`, so the `games` folder only holds your games.
- The file is now `j2me-nxx.nro`. Delete the old `sdmc:/switch/j2me-nx.nro` (and the `sdmc:/switch/j2me-nx/` folder once your games are moved) so hbmenu does not show two apps.

### ✨ Changed
- The fake demo game list is gone. With no games yet, the list shows how to add them: the folder to copy `.jar` files into, subfolders, video files, and **Y** to rescan.
- The `games` folder is created on first launch; the zip no longer ships a placeholder text file.

### Installation
- **Zip** (recommended): extract `j2me-nxx-v0.5.0.zip` to the root of your SD card, then copy your `.jar` games to `sdmc:/switch/j2me-nxx/games/` (subfolders are supported).
- **NRO only**: copy `j2me-nxx.nro` to `sdmc:/switch/`; the `games` folder is created on first launch.

Launch **J2ME-NXX** from hbmenu.

### Controls

| Button | J2ME key |
|---|---|
| D-pad / left stick | Directions |
| A | Fire (5) |
| B, R | Right soft key |
| L, + | Left soft key |
| Y / X | `*` / `#` |
| ZL / ZR | 1 / 3 |
| Right stick | 2 4 6 8 |
| − (twice) | Exit game |

In the game list: **X** settings, **−** per-game options (FPS, screen size, key layout), **Y** rescan.
