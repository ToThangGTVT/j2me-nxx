## J2ME-NX v0.4.0

**Video** arrives: a built-in video player, video inside J2ME apps and games, and AMR/AAC audio, all powered by FFmpeg.

### 🎬 New
- **Video player**: copy `.3gp`, `.mp4`, `.avi`, `.mkv`, `.flv`, `.mpg`, `.wmv`... files into the `games` folder; they appear in the list with a ▶ icon. Full screen with the correct aspect ratio, progress bar and time. **A** play / pause, **left / right** seek 10 s, **L / R** seek 1 min, **up / down** volume, **B** exit.
- **Video in J2ME apps (MMAPI `VideoControl`)**: 3GP / MP4 (H.263, MPEG-4, H.264...) from the JAR, `file://` or `http://`. Video drawn over a Canvas (including full screen) or inside a Form, snapshots (`getSnapshot`), looping, seeking and volume. Picture is synced to the audio.
- **AMR, AAC and M4A audio**: games whose sounds were silent because they used AMR can now play them.

### 🔧 Changed
- Decoding large sounds no longer makes the game's audio stutter.
- The `.nro` is larger (about 23 MB) because it bundles FFmpeg.

### 📌 Notes
- Run hbmenu in **full RAM mode** (hold **R** while launching any game) instead of from the Album. Album (applet) mode has limited RAM and CPU, which can make games stutter.
- When reporting a bug or stutter, enable **Show FPS** and attach `log.txt`.

### Installation
- **Zip** (recommended): extract `j2me-nx-v0.4.0.zip` to the root of your SD card, then copy your `.jar` games to `sdmc:/switch/j2me-nx/games/` (subfolders are supported).
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
