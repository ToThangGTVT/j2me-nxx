## J2ME-NXX v0.6.5

💬 Join the [J2ME-NXX Discord](https://discord.gg/skHsYt8GAa) to chat, ask questions, report bugs and suggest features.

### ✨ New
- **Send games and videos from your phone.** Press **R** in the game list and the Switch shows a QR code. Scan it with a phone on the same Wi-Fi to open an upload page, pick one or more `.jar` / `.jad` games or videos, and they are saved straight into the `games` folder, with progress on both the phone and the Switch. The list is rescanned when you close the screen. If scanning does not work, type the address shown next to the code (like `http://192.168.x.x:8080/`) into the phone's browser.
- **Delete games and videos.** Select a file and press **L**, then **A** to confirm. The matching `.jad` is deleted too. Save data and per-game options are kept, so copying the game back lets you continue where you left off.

### 🔧 Changed
- **Settings scroll.** The settings list now scrolls with a scrollbar instead of squeezing every row onto one screen.
- **L / R** no longer page through the game list (they now delete / send files). Use **left / right** to page.

### Installation
- **Zip** (recommended): extract `j2me-nxx-v0.6.5.zip` to the root of your SD card, then copy your `.jar` games to `sdmc:/switch/j2me-nxx/games/` (subfolders are supported).
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

In the game list: **X** settings, **−** per-game options (FPS, screen size, key layout), **Y** rescan, **R** send games / videos from your phone, **L** delete the selected file, **B** update (when a new version is available).
