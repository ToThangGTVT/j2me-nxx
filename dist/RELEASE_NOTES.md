## J2ME-NXX v0.6.0

Better MIDI music, a touch QWERTY keyboard, and in-app updates.

### ✨ New
- **In-app updates.** On launch, J2ME-NXX checks GitHub for a newer release and shows its notes. Press **A** to download the new `j2me-nxx.nro` with a progress bar (size, %, speed, time left; **B** cancels). The old file is only replaced once the download is complete and verified, then **A** restarts into the new version. Pick "Later" and a **New version** badge stays in the game list; press **B** there to update. Can be turned off in **Settings > Check for updates**.
- **SoundFont MIDI.** MIDI music, tone sequences and `playTone` can be played with a real General MIDI SoundFont instead of the simple wave synth. `TimGM6mb` (~6 MB, GPL v2) is built into the `.nro`, nothing to copy. You can also drop your own `.sf2` files into `sdmc:/switch/j2me-nxx/soundfonts/`. Turn it on in **Settings > MIDI SoundFont** (off by default): *Auto* uses the first `.sf2` on the SD card or the built-in one, *TimGM6mb (built-in)*, *Off*, or a specific file.
- **QWERTY virtual keyboard bubble.** Enable **Settings > Virtual keyboard bubble** to get a small draggable bubble while playing. Tap it to open a floating keyboard with a number row, letters, `* # , .`, space, Shift (double-tap for caps lock; Shift + numbers gives `! @ # $ ...`), Del and Enter. Keys are sent as character codes like on QWERTY phones, handy for typing names, chat and Opera Mini. **×** collapses it back to the bubble.

### 🔧 Changed
- Audio is now mixed at 48 kHz (the Switch's native rate) instead of 22050 Hz. SoundFont instruments no longer sound harsh/buzzy on the speakers.
- Loud passages are softly compressed instead of hard-clipped.

### Installation
- **Zip** (recommended): extract `j2me-nxx-v0.6.0.zip` to the root of your SD card, then copy your `.jar` games to `sdmc:/switch/j2me-nxx/games/` (subfolders are supported).
- **NRO only**: copy `j2me-nxx.nro` to `sdmc:/switch/`, replacing the old one.

Coming from v0.5.0 or older: install this version by hand once; later versions can be installed from inside the app.

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
