## J2ME-NX v0.3.0

This release focuses on **smoothness**, **sharpness** and **3D graphics**.

### ✨ New
- **M3G 3D graphics (JSR-184)**: full API, software renderer (Z-buffer, perspective-correct textures, lighting, fog, blending), and a `Loader` for `.m3g` files.
- **Show FPS** (Settings → Show FPS): displays FPS, CPU % and memory in the corner of the screen, and writes a performance log to `sdmc:/switch/j2me-nx/log.txt`.
- **Old Motorola key layout** (T720/V300…), detected automatically for Motorola MIDP 1.0 games.
- **Scaling modes**: Sharp (default) / Pixel / Pixel (integer).

### ⚡ Smoother
- The Java VM now runs on **its own thread on CPU core 1**, in parallel with rendering, so its work is no longer cut into slices by the display refresh.
- The render thread never waits for the VM, which fixes stutter even when the FPS counter looks fine.
- Fixed games that wait for the next frame with `Thread.yield()` burning all available CPU (Ninja School 3 and others).
- The VM clock is now accurate to the millisecond on Switch.
- Faster drawing at large screen sizes.

### 🖼️ Sharper
- **Sharp-bilinear** scaling: crisp, even pixels at non-integer scales (e.g. 240x320 → 2.25x). Previously the image was blurry.
- In-game text is rendered as pixel text like on real phones, so it no longer blurs when scaled up.

### 📌 Notes
- Run hbmenu in **full RAM mode** (hold **R** while launching any game) instead of from the Album. Album (applet) mode has limited RAM and CPU, which can make games stutter.
- When reporting a bug or stutter, enable **Show FPS** and attach `log.txt`.

### Installation
- **Zip** (recommended): extract `j2me-nx-v0.3.0.zip` to the root of your SD card, then copy your `.jar` games to `sdmc:/switch/j2me-nx/games/` (subfolders are supported).
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
