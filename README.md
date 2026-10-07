# j2me-nxx

*[English below](#english)*

Trình giả lập J2ME (Java ME / MIDP 2.0) cho Nintendo Switch, viết bằng C trên devkitPro + SDL2.
Chạy file `.jar` của game điện thoại Java cũ trực tiếp trên Switch (homebrew `.nro`).

## Kiến trúc

| Thư mục | Nội dung |
|---|---|
| `source/vm/` | Máy ảo Java tự viết: đọc class file, trình thông dịch bytecode (đủ ~200 opcode, kể cả `jsr/ret`), green thread + monitor, GC mark-sweep, đọc JAR (zip + zlib) |
| `javalib/src/` | Thư viện CLDC 1.1 / MIDP 2.0 viết bằng Java: `java.lang/util/io`, `lcdui`, `lcdui.game`, `rms`, `media`, API Nokia (`FullCanvas`, `DirectGraphics`) |
| `source/midp/` | Native của MIDP: vẽ phần mềm (hình, ảnh PNG/JPEG/GIF/BMP, chữ qua SDL_ttf), hàng đợi sự kiện, RecordStore lưu ra thẻ SD, âm thanh (trộn WAV/MP3 + tổng hợp MIDI/tone), socket/HTTP/TLS |
| `source/third_party/` | `stb_image.h` (JPEG/GIF/BMP), `dr_mp3.h` (MP3), đều public domain; `tsf.h` (TinySoundFont, MIT); SoundFont `TimGM6mb.sf2` (GPL v2, nhúng vào binary); font Google Sans (OFL) có đủ chữ tiếng Việt |
| `source/` | App: danh sách game, cài đặt, phiên chạy game (`emu.c`), giải mã video qua FFmpeg (`video_dec.c`), trình xem video (`video_screen.c`), lớp nền tảng Switch/desktop |
| `tests/` | MIDlet để kiểm tra: `demo-midlet` (Canvas, Sprite, Form, List, Alert, RMS), `audio-midlet` (MIDI, WAV, MP3, tone), `net-midlet` (socket, HTTP), `https-midlet` (HTTPS, ssl://), `m3g-midlet` (3D), `video-midlet` (video trên Canvas, trong Form, `platformRequest`) |

Thư viện Java được biên dịch bằng `javac` lúc build rồi nhúng vào binary dưới dạng `classlib.jar`.

## Build

Cần: [devkitPro](https://devkitpro.org/wiki/Getting_Started) (gói `switch-dev`), JDK (`javac`, `jar`), CMake.

```bash
sudo dkp-pacman -S switch-dev switch-sdl2 switch-sdl2_ttf switch-libpng switch-zlib switch-mbedtls switch-ffmpeg
export DEVKITPRO=/opt/devkitpro
cmake -B build -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/Switch.cmake
cmake --build build
```

Kết quả: `build/j2me-nxx.nro`. Thiếu mbedTLS thì vẫn build được, chỉ không có `https://` / `ssl://`; thiếu FFmpeg thì không có video và AMR/AAC.

### Bản desktop (test nhanh trên Mac/Linux)

```bash
brew install sdl2 sdl2_ttf libpng mbedtls ffmpeg pkgconf
cmake -B build-desktop -DJ2ME_NX_DESKTOP=ON
cmake --build build-desktop
./build-desktop/j2me-nxx path/to/game.jar     # hoặc file video .mp4 / .3gp
```

Biến môi trường: `J2ME_NX_GAMES` (thư mục game, mặc định `./games`), `J2ME_NX_DATA` (log, save, cài đặt; mặc định `./data`).

### CLion

- Profile Switch: CMake options `-DCMAKE_TOOLCHAIN_FILE=/opt/devkitpro/cmake/Switch.cmake`, Environment `DEVKITPRO=/opt/devkitpro`. Build target `j2me-nxx_nro` (hoặc Build Project).
- Profile Desktop: CMake options `-DJ2ME_NX_DESKTOP=ON`, chạy được bằng nút Run.

## Tải bản build

Bản phát hành có sẵn ở trang [Releases](https://github.com/ToThangGTVT/j2me-nxx/releases): giải nén zip vào gốc thẻ SD.
GitHub Actions tự build mỗi lần push; push tag `v*` (vd `git tag v0.1.0 && git push origin v0.1.0`) sẽ tạo bản phát hành mới.

## Dùng trên Switch

1. Chép `j2me-nxx.nro` vào `sdmc:/switch/`.
2. Chép game `.jar` (và `.jad` cùng tên nếu có) vào `sdmc:/switch/j2me-nxx/games/`. Có thể chia thư mục con (tối đa 3 cấp), app tự tạo thư mục `games` ở lần chạy đầu.
3. Mở bằng hbmenu. Save game ở `sdmc:/switch/j2me-nxx/rms/`, log ở `sdmc:/switch/j2me-nxx/log.txt`.

| Nút | Phím J2ME |
|---|---|
| D-pad / stick trái | Lên / xuống / trái / phải |
| A | Fire (5) |
| B, R | Phím mềm phải |
| L, + | Phím mềm trái |
| Y / X | `*` / `#` |
| ZL / ZR | 1 / 3 |
| Stick phải | 2 4 6 8 |
| Bấm stick trái / phải | 5 / 0 |
| − (2 lần) | Thoát game |

Màn hình cảm ứng được chuyển thành sự kiện pointer.


**SoundFont**: nhạc MIDI phát bằng SoundFont. App có sẵn `TimGM6mb` (~6 MB, GPL v2) nhúng trong file `.nro`, không cần chép gì thêm. Mặc định **tắt** (dùng bộ tổng hợp sóng), bật ở **Cài đặt > SoundFont MIDI**. Muốn dùng SoundFont General MIDI khác thì chép file `.sf2` vào `sdmc:/switch/j2me-nxx/soundfonts/` rồi chọn ở cùng mục đó: "Tự động" dùng file `.sf2` đầu tiên theo tên (không có thì dùng bản có sẵn), "TimGM6mb (có sẵn)" luôn dùng bản nhúng, "Tắt" dùng bộ tổng hợp sóng cũ (nhẹ hơn). File `.sf2` được nạp cả vào RAM, nên chọn file nhỏ (dưới ~50 MB).

**Xem video**: chép file `.3gp`, `.mp4`, `.avi`, `.mkv`, `.flv`, `.mpg`, `.wmv`... vào cùng thư mục `games`, chúng hiện trong danh sách với biểu tượng ▶. Khi xem: **A** phát / dừng, **trái / phải** tua 10 giây, **L / R** tua 1 phút, **lên / xuống** âm lượng, **B** thoát.

Danh sách game hiện tên, nhà phát hành, phiên bản và icon đọc từ `MANIFEST.MF` / `.jad` của từng game (đọc dần khi cuộn tới). JAR thiếu `MIDlet-1` được đánh dấu cảnh báo.

Trong danh sách game:
- **X**: Cài đặt chung: giới hạn FPS, kích thước màn hình mặc định (có sẵn 20 cỡ, dọc/ngang, tuỳ chỉnh), hiện chú thích phím khi chơi, cỡ chữ (75–300%), chữ mịn (khử răng cưa, nên bật cho Opera Mini), ngôn ngữ (Tiếng Việt / English).
- **−**: Tuỳ chọn riêng cho game đang chọn (FPS, kích thước màn hình, kiểu phím, cỡ chữ, chữ mịn), lưu ở `sdmc:/switch/j2me-nxx/options/<tên>.ini`.

Kích thước màn hình được chọn theo thứ tự: tuỳ chọn riêng của game > `Nokia-MIDlet-Original-Display-Size` trong MANIFEST/JAD > cài đặt chung (mặc định 240x320).

## Trạng thái

- Đã chạy: Canvas / GameCanvas, Sprite / TiledLayer / LayerManager, Image (PNG, JPEG, GIF, BMP), Font, Form / List / Alert / TextBox (bàn phím ảo của Switch), RecordStore, Timer, thread / wait / notify.
- Âm thanh: WAV (PCM 8/16-bit, IMA ADPCM), MP3, MIDI (phát bằng SoundFont `.sf2` qua TinySoundFont; không có file `.sf2` thì tổng hợp bằng sóng cơ bản + trống), ToneControl, `Manager.playTone`, `com.nokia.mid.sound.Sound`. AMR, AAC, M4A và tiếng trong 3GP/MP4 giải mã bằng FFmpeg.
- Video (MMAPI `VideoControl`): 3GP / MP4 (H.263, MPEG-4, H.264...) từ JAR, `file://` hoặc `http://`; vẽ đè lên Canvas (`USE_DIRECT_VIDEO`, cả toàn màn hình) hoặc trong Form (`USE_GUI_PRIMITIVE`), `getSnapshot` (PNG), lặp, tua. Chưa có camera (`capture://`).
- `MIDlet.platformRequest`: link video (`http://`, `file:///`...) phát bằng trình xem video đè lên app (B để quay lại), trang web mở bằng trình duyệt có sẵn của Switch (cần chạy hbmenu ở chế độ full RAM). Dùng cho app như JTube (chọn Playback method: Via browser).
- Mạng: `socket://`, `http://`, `https://`, `ssl://` (TLS qua mbedTLS, không kiểm tra chứng chỉ), `datagram://` (UDP).
- File: JSR-75 FileConnection với ổ `C:/`, `E:/` trong sandbox riêng của từng game (`sdmc:/switch/j2me-nxx/files/<game>/`).
- API của hãng: Nokia UI (`FullCanvas`, `DirectGraphics`, `Sound`), Siemens (`com.siemens.mp.game/ui/io/gsm`), Samsung (`com.samsung.util`), Motorola (`funlight`, `multimedia`).
- Giả lập để game không lỗi thiếu lớp: Bluetooth (JSR-82), SMS (JSR-120, gửi luôn báo lỗi), `PushRegistry`.
- Kiểu phím theo hãng (Nokia, Sony Ericsson, Samsung, Motorola, Siemens, LG) trong Cài đặt / Tuỳ chọn game; JAR nhiều MIDlet có hộp chọn MIDlet.
- 3D: JSR-184 M3G (`javax.microedition.m3g`) với bộ dựng hình phần mềm (`source/midp/m3g.c`): Z-buffer, texture có hiệu chỉnh phối cảnh, chiếu sáng theo đỉnh (ambient/directional/omni/spot), fog, blend, Sprite3D, Skinned/MorphingMesh, animation keyframe, `Loader` đọc file `.m3g` (kể cả section nén zlib), `Group.pick`.
- Chưa có: MascotCapsule 3D (game Sony Ericsson), JSR-226 SVG, camera, cảm biến.

### Test tự động trên desktop

Bản desktop đọc vài biến môi trường để chạy kịch bản (tính theo ms từ lúc game chạy):

```bash
J2ME_NX_KEYS="1500:-6,2000:-5" J2ME_NX_SHOTS="3000:/tmp/a.bmp" J2ME_NX_QUIT=4000 ./build-desktop/j2me-nxx game.jar
```

`J2ME_NX_APPSHOT=<file.bmp>` chụp màn hình app (danh sách game) rồi thoát, `J2ME_NX_AUDIO_DUMP=<file>` ghi luồng âm thanh (PCM 16-bit mono 48000Hz) ra file, `J2ME_NX_SCREEN=settings` mở thẳng màn hình cài đặt.

---

## English

A J2ME (Java ME / MIDP 2.0) emulator for Nintendo Switch, written in C with devkitPro + SDL2.
It runs `.jar` files of old Java phone games directly on the Switch (homebrew `.nro`).

### Architecture

| Folder | Contents |
|---|---|
| `source/vm/` | Custom Java VM: class file loader, bytecode interpreter (all ~200 opcodes, including `jsr/ret`), green threads + monitors, mark-sweep GC, JAR reader (zip + zlib) |
| `javalib/src/` | CLDC 1.1 / MIDP 2.0 library written in Java: `java.lang/util/io`, `lcdui`, `lcdui.game`, `rms`, `media`, Nokia API (`FullCanvas`, `DirectGraphics`) |
| `source/midp/` | MIDP natives: software rendering (shapes, PNG/JPEG/GIF/BMP images, text via SDL_ttf), event queue, RecordStore saved to the SD card, audio (WAV/MP3 mixing + MIDI/tone synthesis), socket/HTTP/TLS |
| `source/third_party/` | `stb_image.h` (JPEG/GIF/BMP), `dr_mp3.h` (MP3), both public domain; `tsf.h` (TinySoundFont, MIT); `TimGM6mb.sf2` SoundFont (GPL v2, embedded in the binary); Google Sans font (OFL) with full Vietnamese coverage |
| `source/` | App: game list, settings, game session (`emu.c`), FFmpeg video decoding (`video_dec.c`), video player (`video_screen.c`), Switch/desktop platform layer |
| `tests/` | Test MIDlets: `demo-midlet` (Canvas, Sprite, Form, List, Alert, RMS), `audio-midlet` (MIDI, WAV, MP3, tone), `net-midlet` (socket, HTTP), `https-midlet` (HTTPS, ssl://), `m3g-midlet` (3D), `video-midlet` (video on a Canvas, in a Form, `platformRequest`) |

The Java library is compiled with `javac` at build time and embedded in the binary as `classlib.jar`.

### Build

Requirements: [devkitPro](https://devkitpro.org/wiki/Getting_Started) (`switch-dev` package), a JDK (`javac`, `jar`), CMake.

```bash
sudo dkp-pacman -S switch-dev switch-sdl2 switch-sdl2_ttf switch-libpng switch-zlib switch-mbedtls switch-ffmpeg
export DEVKITPRO=/opt/devkitpro
cmake -B build -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/Switch.cmake
cmake --build build
```

Output: `build/j2me-nxx.nro`. It still builds without mbedTLS, just without `https://` / `ssl://`; without FFmpeg there is no video and no AMR/AAC.

#### Desktop build (quick testing on Mac/Linux)

```bash
brew install sdl2 sdl2_ttf libpng mbedtls ffmpeg pkgconf
cmake -B build-desktop -DJ2ME_NX_DESKTOP=ON
cmake --build build-desktop
./build-desktop/j2me-nxx path/to/game.jar     # or a .mp4 / .3gp video file
```

Environment variables: `J2ME_NX_GAMES` (games folder, default `./games`), `J2ME_NX_DATA` (logs, saves, settings; default `./data`).

#### CLion

- Switch profile: CMake options `-DCMAKE_TOOLCHAIN_FILE=/opt/devkitpro/cmake/Switch.cmake`, environment `DEVKITPRO=/opt/devkitpro`. Build the `j2me-nxx_nro` target (or Build Project).
- Desktop profile: CMake options `-DJ2ME_NX_DESKTOP=ON`, runnable with the Run button.

### Download

Prebuilt releases are on the [Releases](https://github.com/ToThangGTVT/j2me-nxx/releases) page: extract the zip to the root of your SD card.
GitHub Actions builds on every push; pushing a `v*` tag (e.g. `git tag v0.1.0 && git push origin v0.1.0`) creates a new release.

### Using it on the Switch

1. Copy `j2me-nxx.nro` to `sdmc:/switch/`.
2. Copy your `.jar` games (and the matching `.jad` if you have one) to `sdmc:/switch/j2me-nxx/games/`. Subfolders are supported (up to 3 levels); the `games` folder is created on first launch.
3. Launch from hbmenu. Saves go to `sdmc:/switch/j2me-nxx/rms/`, the log to `sdmc:/switch/j2me-nxx/log.txt`.

| Button | J2ME key |
|---|---|
| D-pad / left stick | Up / down / left / right |
| A | Fire (5) |
| B, R | Right soft key |
| L, + | Left soft key |
| Y / X | `*` / `#` |
| ZL / ZR | 1 / 3 |
| Right stick | 2 4 6 8 |
| Left / right stick click | 5 / 0 |
| − (twice) | Exit game |

The touch screen is mapped to pointer events.


**SoundFont**: MIDI music is played with a SoundFont. `TimGM6mb` (~6 MB, GPL v2) is embedded in the `.nro`, so nothing extra needs to be copied. It is **off** by default (wave synth); turn it on in **Settings > MIDI SoundFont**. To use another General MIDI SoundFont, copy a `.sf2` file to `sdmc:/switch/j2me-nxx/soundfonts/` and pick it in the same setting: "Auto" uses the first `.sf2` file by name (or the built-in one if there is none), "TimGM6mb (built-in)" always uses the embedded one, "Off" uses the old wave synth (lighter). The whole `.sf2` is loaded into RAM, so prefer small files (under ~50 MB).

**Watching videos**: copy `.3gp`, `.mp4`, `.avi`, `.mkv`, `.flv`, `.mpg`, `.wmv`... files into the same `games` folder; they show up in the list with a ▶ icon. While watching: **A** play / pause, **left / right** seek 10 s, **L / R** seek 1 min, **up / down** volume, **B** exit.

The game list shows the name, vendor, version and icon read from each game's `MANIFEST.MF` / `.jad` (loaded lazily as you scroll). JARs without `MIDlet-1` are flagged with a warning.

In the game list:
- **X**: Global settings: FPS limit, default screen size (20 presets, portrait/landscape, custom), show key hints while playing, font size (75–300%), smooth (anti-aliased) text, recommended for Opera Mini, language (Tiếng Việt / English).
- **−**: Options for the selected game (FPS, screen size, key layout, font size, smooth text), saved to `sdmc:/switch/j2me-nxx/options/<name>.ini`.

Screen size is chosen in this order: the game's own options > `Nokia-MIDlet-Original-Display-Size` in MANIFEST/JAD > global settings (default 240x320).

### Status

- Working: Canvas / GameCanvas, Sprite / TiledLayer / LayerManager, Image (PNG, JPEG, GIF, BMP), Font, Form / List / Alert / TextBox (Switch software keyboard), RecordStore, Timer, threads / wait / notify.
- Audio: WAV (8/16-bit PCM, IMA ADPCM), MP3, MIDI (played with a `.sf2` SoundFont via TinySoundFont; without one, synthesized with basic waveforms + drums), ToneControl, `Manager.playTone`, `com.nokia.mid.sound.Sound`. AMR, AAC, M4A and the audio track of 3GP/MP4 are decoded with FFmpeg.
- Video (MMAPI `VideoControl`): 3GP / MP4 (H.263, MPEG-4, H.264...) from the JAR, `file://` or `http://`; drawn over a Canvas (`USE_DIRECT_VIDEO`, including full screen) or inside a Form (`USE_GUI_PRIMITIVE`), `getSnapshot` (PNG), looping, seeking. No camera (`capture://`) yet.
- `MIDlet.platformRequest`: video links (`http://`, `file:///`...) play in the video player on top of the app (B to go back), web pages open in the Switch's built-in browser (requires hbmenu in full RAM mode). Useful for apps like JTube (set Playback method to Via browser).
- Networking: `socket://`, `http://`, `https://`, `ssl://` (TLS via mbedTLS, certificates are not verified), `datagram://` (UDP).
- Files: JSR-75 FileConnection with `C:/` and `E:/` drives in a per-game sandbox (`sdmc:/switch/j2me-nxx/files/<game>/`).
- Vendor APIs: Nokia UI (`FullCanvas`, `DirectGraphics`, `Sound`), Siemens (`com.siemens.mp.game/ui/io/gsm`), Samsung (`com.samsung.util`), Motorola (`funlight`, `multimedia`).
- Stubbed so games don't fail on missing classes: Bluetooth (JSR-82), SMS (JSR-120, sending always reports an error), `PushRegistry`.
- Per-vendor key layouts (Nokia, Sony Ericsson, Samsung, Motorola, Siemens, LG) in Settings / Game options; JARs with several MIDlets show a MIDlet picker.
- 3D: JSR-184 M3G (`javax.microedition.m3g`) with a software renderer (`source/midp/m3g.c`): Z-buffer, perspective-correct textures, per-vertex lighting (ambient/directional/omni/spot), fog, blending, Sprite3D, Skinned/MorphingMesh, keyframe animation, a `Loader` for `.m3g` files (including zlib-compressed sections), `Group.pick`.
- Not yet: MascotCapsule 3D (Sony Ericsson games), JSR-226 SVG, camera, sensors.

#### Automated testing on desktop

The desktop build reads a few environment variables to run a script (times in ms since the game started):

```bash
J2ME_NX_KEYS="1500:-6,2000:-5" J2ME_NX_SHOTS="3000:/tmp/a.bmp" J2ME_NX_QUIT=4000 ./build-desktop/j2me-nxx game.jar
```

`J2ME_NX_APPSHOT=<file.bmp>` takes a screenshot of the app (game list) and exits, `J2ME_NX_AUDIO_DUMP=<file>` writes the audio stream (16-bit mono PCM, 48000 Hz) to a file, `J2ME_NX_SCREEN=settings` opens the settings screen directly.
