## J2ME-NX v0.3.2

Better text for text-heavy apps like **Opera Mini**, and memory info in the FPS overlay.

### ✨ New
- **Font size** (Settings → Font size, or per game with **−**): scale in-game text from 75% to 300%, like J2ME Loader. Apps lay out text using the new size, so nothing overflows. For Opera Mini, try a larger screen size together with a larger font, e.g. **480x800 + 200%**, for big and crisp text.
- **Smooth text** (Settings → Smooth text, or per game): anti-aliased text instead of pixel text. Off by default so games keep the authentic phone look; turn it on for Opera Mini and other text-heavy apps.
- **Show FPS** now also shows memory: `Java` (Java heap) and `RAM` (the whole app's memory, in use / available). The RAM limit tells you whether hbmenu is running in full RAM mode or Album mode. `log.txt` records RAM every second too.

### 🔧 Changed
- The Settings screen fits more rows on screen.

### 📌 Notes
- Run hbmenu in **full RAM mode** (hold **R** while launching any game) instead of from the Album. Album (applet) mode has limited RAM and CPU, which can make games stutter.
- When reporting a bug or stutter, enable **Show FPS** and attach `log.txt`.

### Installation
- **Zip** (recommended): extract `j2me-nx-v0.3.2.zip` to the root of your SD card, then copy your `.jar` games to `sdmc:/switch/j2me-nx/games/` (subfolders are supported).
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
