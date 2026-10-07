## J2ME-NX v0.4.1

Apps can now open links: videos play in the built-in video player, web pages open in the Switch browser.

### ✨ New
- **`MIDlet.platformRequest`** (previously ignored):
  - **Video links** (`http://`, `rtsp://`, files on the app's memory card) play in the video player on top of the app. Press **B** to go back to the app. While the video is shown, keys do not reach the app and the game's sound is paused.
  - **Web pages** open in the Switch's built-in browser. This needs hbmenu in **full RAM mode** (hold **R** while launching a game); otherwise J2ME-NX shows a hint. On desktop builds the default browser is used.
  - `tel:`, `sms:` and `mailto:` links report "not supported" to the app, as on phones without those features.
- **JTube** (YouTube client): browsing works; for playback set *Settings → Playback method → Via browser*. Videos play when the selected Invidious server is up. With *Via 2yxa.mobi*, the 2yxa page opens in the Switch browser.

### 📌 Known limitations
- The Switch FFmpeg build has no `https://`: HTTPS video links open in the Switch browser instead of the video player.
- Network streams are read on the render thread, so a slow connection can make the picture stall; a stream that sends nothing for 15 seconds is stopped.

### 📌 Notes
- Run hbmenu in **full RAM mode** (hold **R** while launching any game) instead of from the Album. Album (applet) mode has limited RAM and CPU, which can make games stutter.
- When reporting a bug or stutter, enable **Show FPS** and attach `log.txt`.

### Installation
- **Zip** (recommended): extract `j2me-nx-v0.4.1.zip` to the root of your SD card, then copy your `.jar` games to `sdmc:/switch/j2me-nx/games/` (subfolders are supported).
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
