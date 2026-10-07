## J2ME-NXX v0.6.4

💬 Join the [J2ME-NXX Discord](https://discord.gg/skHsYt8GAa) to chat, ask questions, report bugs and suggest features.

### ✨ New
- **System font** (Settings, and per game with **−**). In-game text uses the Switch system fonts, which include Japanese, Chinese and Korean. Text is always smooth and drawn sharp at screen resolution instead of being upscaled from 11-16 px.
- **Font fallback.** Characters missing from one font are taken from the other: with System font off, the app font is used and the system fonts fill in Chinese / Japanese / Korean; with it on, the app font fills in Vietnamese.

### 🔧 Changed
- **Smooth text** works as before (anti-aliased at the game's resolution). It is hidden while System font is on, since system font text is always smooth.

### Installation
- **Zip** (recommended): extract `j2me-nxx-v0.6.4.zip` to the root of your SD card, then copy your `.jar` games to `sdmc:/switch/j2me-nxx/games/` (subfolders are supported).
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
