## J2ME-NXX v0.6.3

💬 Join the [J2ME-NXX Discord](https://discord.gg/skHsYt8GAa) to chat, ask questions, report bugs and suggest features.

### ✨ New
- **Switch button icons.** Key hints in the game list, settings, updater, video player and in-game key panel now show the real Switch button icons instead of `(A)`, `(B)`, `(X)`, `(Y)`, `(+)`, `(-)`.

### 🐛 Fixed
- Fixed a crash when opening a second game in the same session while **Show FPS** is on.
- Crash reports now record the real load address, so every address comes with its `j2me-nxx.elf + 0x...` offset (it was always `0x0` before).

### Installation
- **Zip** (recommended): extract `j2me-nxx-v0.6.3.zip` to the root of your SD card, then copy your `.jar` games to `sdmc:/switch/j2me-nxx/games/` (subfolders are supported).
- **NRO only**: copy `j2me-nxx.nro` to `sdmc:/switch/`, replacing the old one.

Already on v0.6.0 or newer: open J2ME-NXX and accept the update prompt (or press **B** in the game list). Coming from v0.5.0 or older: install this version by hand once.

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

In the game list: **X** settings, **−** per-game options (FPS, screen size, key layout), **Y** rescan, **B** update (when a new version is available).
