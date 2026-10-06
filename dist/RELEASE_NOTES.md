## J2ME-NX v0.3.1

Bug-fix release.

### 🐛 Fixed
- **Opera Mini froze on the loading screen** (and any app that calls `repaint()` while holding its own lock). The event thread held the paint lock while calling the app's `paint()`, which waited for the app's lock, while the app's thread held that lock and waited for the paint lock in `repaint()`. `repaint()` now never blocks, like on real phones.

### 🔍 Diagnostics
- With **Show FPS** enabled, `log.txt` now also records the state of every Java thread every 5 seconds (running, sleeping, waiting on which lock, and in which method). If a game freezes, please attach this log to your bug report.

### 📌 Notes
- Run hbmenu in **full RAM mode** (hold **R** while launching any game) instead of from the Album. Album (applet) mode has limited RAM and CPU, which can make games stutter.
- When reporting a bug or stutter, enable **Show FPS** and attach `log.txt`.

### Installation
- **Zip** (recommended): extract `j2me-nx-v0.3.1.zip` to the root of your SD card, then copy your `.jar` games to `sdmc:/switch/j2me-nx/games/` (subfolders are supported).
- **NRO only**: copy `j2me-nx.nro` to `sdmc:/switch/`; the `games` folder is created on first launch.

Launch **J2ME-NX** from hbmenu.

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
