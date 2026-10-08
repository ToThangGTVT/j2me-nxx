## J2ME-NXX v0.7.4

💬 Join the [J2ME-NXX Discord](https://discord.gg/skHsYt8GAa) to chat, ask questions, report bugs and suggest features.

### 🐛 Fixed
- No more crash when restarting after an update (or returning to hbmenu) after playing an app with **AOT** on: AOT now frees its memory when J2ME-NXX closes. When updating from v0.7.1 or older, the restart may still crash once if AOT was used; the update is already installed, just open J2ME-NXX again.
- J2ME-NXX no longer crashes when a Java app runs out of memory; the Java app gets an `OutOfMemoryError` instead.
- Better crash logs: startup, update and exit steps are written to `sdmc:/switch/j2me-nxx/app.log` (previous session: `app-prev.log`), and a failed update or startup error saves a crash report in `sdmc:/switch/j2me-nxx/crash/`. Please attach these when reporting a crash.

### Installation
- **Zip** (recommended): extract `j2me-nxx-v0.7.4.zip` to the root of your SD card, then copy your `.jar` apps to `sdmc:/switch/j2me-nxx/games/` (subfolders are supported).
- **NRO only**: copy `j2me-nxx.nro` to `sdmc:/switch/`, replacing the old one.

Already on v0.6.0 or newer: open J2ME-NXX and accept the update prompt (or press **B** in the app list). Coming from v0.5.0 or older: install this version by hand once.

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
| − (twice) | Exit app |

In the app list: **A** open, **X** settings, **−** per-app options (FPS, screen size, key layout), **Y** rescan, **R** upload apps / videos from your phone, **L** delete the selected file, **B** update (when a new version is available).
