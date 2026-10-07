## J2ME-NXX v0.6.2

💬 Join the [J2ME-NXX Discord](https://discord.gg/skHsYt8GAa) to chat, ask questions, report bugs and suggest features.

### ✨ New
- **Crash reports.** Every crash now writes its own file to `sdmc:/switch/j2me-nxx/crash/crash-<date>-<time>.txt`:
  - **App crash**: CPU registers, faulting address, backtrace (as `j2me-nxx.elf + 0x...`) and the Java method that was running.
  - **Java game error** (uncaught exception): exception and stack trace with line numbers, one file per play session.
  - **Game failed to start**: the error, and the file name is shown in the error message.
  - Each report also includes the app version, the running game and the latest log lines.
  - After an app crash, the next launch shows the report's file name in the bottom bar. Please attach it when reporting a bug.
- Each release now also ships `j2me-nxx-<version>-elf.zip`, used to turn crash addresses into function names (`aarch64-none-elf-addr2line -f -C -e j2me-nxx.elf <offset>`).

### 🔧 Changed
- The logo from v0.6.1 has been removed: the game list header is text-only again and hbmenu shows the default homebrew icon.

### Installation
- **Zip** (recommended): extract `j2me-nxx-v0.6.2.zip` to the root of your SD card, then copy your `.jar` games to `sdmc:/switch/j2me-nxx/games/` (subfolders are supported).
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
