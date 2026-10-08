# j2me-nxx

<p align="center"><a href="https://discord.gg/skHsYt8GAa"><img src="https://img.shields.io/badge/Discord-J2ME--NXX-5865F2?logo=discord&logoColor=white" alt="Discord"></a></p>

> 💬 **Tham gia [Discord J2ME-NXX](https://discord.gg/skHsYt8GAa)** để thảo luận, hỏi đáp, báo lỗi, gửi file crash và góp ý tính năng mới.
>
> 💬 **Join the [J2ME-NXX Discord](https://discord.gg/skHsYt8GAa)** to chat, ask questions, report bugs, share crash files and suggest new features.

*[English below](#english)*

Trình giả lập J2ME (Java ME / MIDP 2.0) cho Nintendo Switch, viết bằng C trên devkitPro + SDL2.
Chạy file `.jar` của ứng dụng, game điện thoại Java cũ trực tiếp trên Switch (homebrew `.nro`).

## Kiến trúc

| Thư mục | Nội dung |
|---|---|
| `source/vm/` | Máy ảo Java tự viết: đọc class file, trình thông dịch bytecode (đủ ~200 opcode, kể cả `jsr/ret`), green thread + monitor, GC mark-sweep, đọc JAR (zip + zlib) |
| `source/vm/aot*` | Chế độ AOT thử nghiệm (bật trong Cài đặt): dịch bytecode sang mã máy ARM64 khi nạp lớp, chạy chung với trình thông dịch |
| `javalib/src/` | Thư viện CLDC 1.1 / MIDP 2.0 viết bằng Java: `java.lang/util/io`, `lcdui`, `lcdui.game`, `rms`, `media`, API Nokia (`FullCanvas`, `DirectGraphics`) |
| `source/midp/` | Native của MIDP: vẽ phần mềm (hình, ảnh PNG/JPEG/GIF/BMP, chữ qua SDL_ttf), hàng đợi sự kiện, RecordStore lưu ra thẻ SD, âm thanh (trộn WAV/MP3 + tổng hợp MIDI/tone), socket/HTTP/TLS |
| `source/third_party/` | `stb_image.h` (JPEG/GIF/BMP), `dr_mp3.h` (MP3), đều public domain; `tsf.h` (TinySoundFont, MIT); SoundFont `TimGM6mb.sf2` (GPL v2, nhúng vào binary); font Google Sans (OFL) có đủ chữ tiếng Việt |
| `source/ui/` | Giao diện dựng bằng [borealis](https://github.com/xfangfang/borealis) (submodule `library/borealis`): cài đặt, ánh xạ phím, cập nhật, gửi game từ điện thoại |
| `source/` | Danh sách ứng dụng (`menu.c`), phiên chạy ứng dụng (`emu.c`), lớp vẽ `gfx.c` trên NanoVG cho danh sách ứng dụng / màn hình chạy game / phím ảo / trình xem video, giải mã video qua FFmpeg (`video_dec.c`), lớp nền tảng Switch/desktop |
| `resources/` | Tài nguyên của borealis (chữ gợi ý nút vi/en, icon): nằm trong romfs của `.nro` |
| `tests/` | MIDlet để kiểm tra: `demo-midlet` (Canvas, Sprite, Form, List, Alert, RMS), `audio-midlet` (MIDI, WAV, MP3, tone), `net-midlet` (socket, HTTP), `https-midlet` (HTTPS, ssl://), `m3g-midlet` (3D), `video-midlet` (video trên Canvas, trong Form, `platformRequest`) |

Thư viện Java được biên dịch bằng `javac` lúc build rồi nhúng vào binary dưới dạng `classlib.jar`.

## Build

Cần: [devkitPro](https://devkitpro.org/wiki/Getting_Started) (gói `switch-dev`), JDK (`javac`, `jar`), CMake.

```bash
git submodule update --init library/borealis
sudo dkp-pacman -S switch-dev switch-sdl2 switch-sdl2_ttf switch-libpng switch-zlib switch-mbedtls switch-ffmpeg switch-mesa switch-libdrm_nouveau
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

Biến môi trường: `J2ME_NX_GAMES` (thư mục ứng dụng, mặc định `./games`), `J2ME_NX_DATA` (log, save, cài đặt; mặc định `./data`).

### CLion

- Profile Switch: CMake options `-DCMAKE_TOOLCHAIN_FILE=/opt/devkitpro/cmake/Switch.cmake`, Environment `DEVKITPRO=/opt/devkitpro`. Build target `j2me-nxx_nro` (hoặc Build Project).
- Profile Desktop: CMake options `-DJ2ME_NX_DESKTOP=ON`, chạy được bằng nút Run.

## Tải bản build

Bản phát hành có sẵn ở trang [Releases](https://github.com/ToThangGTVT/j2me-nxx/releases): giải nén zip vào gốc thẻ SD.
GitHub Actions tự build mỗi lần push; push tag `v*` (vd `git tag v0.1.0 && git push origin v0.1.0`) sẽ tạo bản phát hành mới.

## Dùng trên Switch

1. Chép `j2me-nxx.nro` vào `sdmc:/switch/`.
2. Chép ứng dụng `.jar` (và `.jad` cùng tên nếu có) vào `sdmc:/switch/j2me-nxx/games/`. Có thể chia thư mục con (tối đa 3 cấp), app tự tạo thư mục `games` ở lần chạy đầu.
3. Mở bằng hbmenu. Dữ liệu save ở `sdmc:/switch/j2me-nxx/rms/`, log ở `sdmc:/switch/j2me-nxx/log.txt`.

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
| − (2 lần) | Thoát ứng dụng |

Đây là ánh xạ mặc định. Đổi được ở **Cài đặt > Ánh xạ phím** (cho mọi ứng dụng) hoặc **Tùy chọn ứng dụng > Ánh xạ phím** (riêng ứng dụng đó, nút để "Mặc định" thì theo cài đặt chung): chọn nút Switch trong danh sách, bấm **A** (hoặc chạm) rồi chọn phím điện thoại. **Y** đưa nút đang chọn về mặc định, **X** đưa tất cả về mặc định. Bảng phím khi chạy hiện đúng theo ánh xạ đang dùng.

Màn hình cảm ứng được chuyển thành sự kiện pointer.

**Phím ảo trên màn hình**: bật **Cài đặt > Phím ảo trên màn hình** (hoặc riêng từng ứng dụng ở **Tùy chọn ứng dụng**: Mặc định / Bật / Tắt) thì khi chạy có bàn phím điện thoại kiểu Nokia trên màn hình cảm ứng: cần điều khiển bên trái (đi theo ánh xạ của stick trái, kéo chéo bấm 2 hướng), phím mềm trái / phải, Xoá (C), Fire (OK) và bàn phím số 0–9, `*`, `#` bên phải. Chạm nhiều phím cùng lúc được. Bố cục đặt theo màn hình Switch (dùng chung cho mọi ứng dụng, không đổi theo cỡ màn hình của ứng dụng), chỉnh ở **Cài đặt > Bố cục phím ảo**: kéo phím để di chuyển, kéo chấm ở góc dưới phải để đổi kích thước, chỉnh bo góc, độ rõ, ẩn / hiện từng phím; khi bật **Hít**, phím kéo tới gần phím khác hoặc mép màn hình thì tự hít sát vào. Tay cầm: **L / R** chọn phím, **D-pad** di chuyển, **X / Y** to / nhỏ, **A** ẩn / hiện. **Mặc định** (chạm 2 lần) đưa cả bố cục về ban đầu. Khi phím ảo bật thì bảng phím bên trái không hiện.

**Bàn phím ảo QWERTY**: bật **Cài đặt > Bong bóng bàn phím ảo** thì khi chạy có bong bóng nhỏ ở góc phải (kéo để di chuyển). Chạm vào bong bóng để mở bàn phím nổi có hàng số, chữ cái, `* # , . `, phím cách, Shift (chạm 2 lần = khoá chữ hoa; Shift + hàng số ra `! @ # $ ...`), Del (phím xoá `-8`) và Enter (phím Fire). Chữ và ký hiệu gửi đúng mã ký tự như máy có bàn phím QWERTY. Kéo thanh trên cùng để dời bàn phím, nút **×** thu về bong bóng.

**Báo cáo crash**: mỗi lần app bị sập hoặc ứng dụng Java lỗi (exception không ai bắt), J2ME-NXX ghi 1 file `sdmc:/switch/j2me-nxx/crash/crash-<ngày>-<giờ>.txt` gồm phiên bản, ứng dụng đang chạy, lý do, stack trace Java, thanh ghi CPU + backtrace (khi app sập) và log gần nhất. Lần mở app sau sẽ báo tên file ở góc màn hình. Gửi file này kèm khi báo lỗi; địa chỉ dạng `j2me-nxx.elf + 0x...` đổi ra tên hàm bằng `aarch64-none-elf-addr2line -f -C -e j2me-nxx.elf <offset>` với file `.elf` đính kèm trong bản phát hành tương ứng.

**Cập nhật**: mỗi lần mở app, J2ME-NXX hỏi GitHub Releases xem có bản mới không (tắt ở **Cài đặt > Tự kiểm tra bản mới**). Có bản mới thì hiện hộp thoại kèm ghi chú phát hành: **A** tải `j2me-nxx.nro` về (có thanh tiến trình, tốc độ, thời gian còn lại; **B** để huỷ), file cũ chỉ bị thay khi đã tải đủ và kiểm tra đúng là file `.nro`, xong bấm **A** để khởi động lại vào bản mới. Chọn "Để sau" thì danh sách ứng dụng có nhãn "Bản mới", bấm **B** để cập nhật lúc khác.

**SoundFont**: nhạc MIDI phát bằng SoundFont. App có sẵn `TimGM6mb` (~6 MB, GPL v2) nhúng trong file `.nro`, không cần chép gì thêm. Mặc định **tắt** (dùng bộ tổng hợp sóng), bật ở **Cài đặt > SoundFont MIDI**. Muốn dùng SoundFont General MIDI khác thì chép file `.sf2` vào `sdmc:/switch/j2me-nxx/soundfonts/` rồi chọn ở cùng mục đó: "Tự động" dùng file `.sf2` đầu tiên theo tên (không có thì dùng bản có sẵn), "TimGM6mb (có sẵn)" luôn dùng bản nhúng, "Tắt" dùng bộ tổng hợp sóng cũ (nhẹ hơn). File `.sf2` được nạp cả vào RAM, nên chọn file nhỏ (dưới ~50 MB).

**Gửi ứng dụng, video từ điện thoại (Upload)**: ở danh sách ứng dụng bấm **R**, Switch hiện mã QR. Điện thoại (cùng mạng Wi-Fi với Switch) quét mã để mở trang tải lên, chọn một hoặc nhiều file ứng dụng `.jar` / `.jad` hoặc video, file được lưu thẳng vào thư mục `games` (có thanh tiến trình trên cả điện thoại và Switch). Không quét được mã thì gõ địa chỉ hiện bên cạnh (dạng `http://192.168.x.x:8080/`) vào trình duyệt. Bấm **B** để đóng, danh sách tự quét lại.

**Xoá ứng dụng, video**: chọn file trong danh sách rồi bấm **L**, hộp xác nhận hiện tên và dung lượng file, bấm **A** để xoá (kèm file `.jad` cùng tên nếu có), **B** để huỷ. Dữ liệu save và tuỳ chọn riêng của ứng dụng vẫn được giữ lại, chép lại ứng dụng là dùng tiếp được.

**Xem video**: chép file `.3gp`, `.mp4`, `.avi`, `.mkv`, `.flv`, `.mpg`, `.wmv`... vào cùng thư mục `games`, chúng hiện trong danh sách với biểu tượng ▶. Khi xem: **A** phát / dừng, **trái / phải** tua 10 giây, **L / R** tua 1 phút, **lên / xuống** âm lượng, **B** thoát.

Danh sách ứng dụng hiện tên, nhà phát hành, phiên bản và icon đọc từ `MANIFEST.MF` / `.jad` của từng ứng dụng (đọc dần khi cuộn tới). JAR thiếu `MIDlet-1` được đánh dấu cảnh báo.

Trong danh sách ứng dụng (**A** mở):
- **X**: Cài đặt chung (chia thẻ Màn hình ứng dụng / Điều khiển / Chữ và âm thanh / Hệ thống): giới hạn FPS, kích thước màn hình mặc định (có sẵn 20 cỡ, dọc/ngang, tuỳ chỉnh), hiện chú thích phím khi chạy, ánh xạ phím, cỡ chữ (75–300%), chữ mịn (khử răng cưa, nên bật cho Opera Mini), ngôn ngữ (Tiếng Việt / English).
- **−**: Tùy chọn riêng cho ứng dụng đang chọn (FPS, kích thước màn hình, kiểu phím, ánh xạ phím, cỡ chữ, chữ mịn), lưu ở `sdmc:/switch/j2me-nxx/options/<tên>.ini`.

Kích thước màn hình được chọn theo thứ tự: tuỳ chọn riêng của ứng dụng > `Nokia-MIDlet-Original-Display-Size` trong MANIFEST/JAD > cài đặt chung (mặc định 240x320).

## Trạng thái

- Đã chạy: Canvas / GameCanvas, Sprite / TiledLayer / LayerManager, Image (PNG, JPEG, GIF, BMP), Font, Form / List / Alert / TextBox (bàn phím ảo của Switch), RecordStore, Timer, thread / wait / notify.
- Âm thanh: WAV (PCM 8/16-bit, IMA ADPCM), MP3, MIDI (phát bằng SoundFont `.sf2` qua TinySoundFont; không có file `.sf2` thì tổng hợp bằng sóng cơ bản + trống), ToneControl, `Manager.playTone`, `com.nokia.mid.sound.Sound`. AMR, AAC, M4A và tiếng trong 3GP/MP4 giải mã bằng FFmpeg.
- Video (MMAPI `VideoControl`): 3GP / MP4 (H.263, MPEG-4, H.264...) từ JAR, `file://` hoặc `http://`; vẽ đè lên Canvas (`USE_DIRECT_VIDEO`, cả toàn màn hình) hoặc trong Form (`USE_GUI_PRIMITIVE`), `getSnapshot` (PNG), lặp, tua. Chưa có camera (`capture://`).
- `MIDlet.platformRequest`: link video (`http://`, `file:///`...) phát bằng trình xem video đè lên app (B để quay lại), trang web mở bằng trình duyệt có sẵn của Switch (cần chạy hbmenu ở chế độ full RAM). Dùng cho app như JTube (chọn Playback method: Via browser).
- Mạng: `socket://`, `http://`, `https://`, `ssl://` (TLS qua mbedTLS, không kiểm tra chứng chỉ), `datagram://` (UDP).
- File: JSR-75 FileConnection với ổ `C:/`, `E:/` trong sandbox riêng của từng ứng dụng (`sdmc:/switch/j2me-nxx/files/<tên>/`).
- API của hãng: Nokia UI (`FullCanvas`, `DirectGraphics`, `Sound`), Siemens (`com.siemens.mp.game/ui/io/gsm`), Samsung (`com.samsung.util`), Motorola (`funlight`, `multimedia`).
- Giả lập để ứng dụng không lỗi thiếu lớp: Bluetooth (JSR-82), SMS (JSR-120, gửi luôn báo lỗi), `PushRegistry`.
- Kiểu phím theo hãng (Nokia, Sony Ericsson, Samsung, Motorola, Siemens, LG) trong Cài đặt / Tùy chọn ứng dụng; JAR nhiều MIDlet có hộp chọn MIDlet.
- 3D: JSR-184 M3G (`javax.microedition.m3g`) với bộ dựng hình phần mềm (`source/midp/m3g.c`): Z-buffer, texture có hiệu chỉnh phối cảnh, chiếu sáng theo đỉnh (ambient/directional/omni/spot), fog, blend, Sprite3D, Skinned/MorphingMesh, animation keyframe, `Loader` đọc file `.m3g` (kể cả section nén zlib), `Group.pick`.
- Chưa có: MascotCapsule 3D (game Sony Ericsson), JSR-226 SVG, camera, cảm biến.

### Test tự động trên desktop

Bản desktop đọc vài biến môi trường để chạy kịch bản (tính theo ms từ lúc ứng dụng chạy):

```bash
J2ME_NX_KEYS="1500:-6,2000:-5" J2ME_NX_SHOTS="3000:/tmp/a.bmp" J2ME_NX_QUIT=4000 ./build-desktop/j2me-nxx game.jar
```

`J2ME_NX_TAPS="1000:640:500"` chạm chuột tại (x, y) trên màn hình app 1280x720, `J2ME_NX_APPSHOT=<file.bmp>` chụp màn hình app (danh sách ứng dụng) rồi thoát, `J2ME_NX_AUDIO_DUMP=<file>` ghi luồng âm thanh (PCM 16-bit mono 48000Hz) ra file, `J2ME_NX_SCREEN=settings` / `lang` / `keybind` mở thẳng màn hình cài đặt / chọn ngôn ngữ / ánh xạ phím, `J2ME_NX_PRESS="1500:Return,2000:Down"` bấm phím bàn phím (Enter = A, Esc = B, mũi tên; S = X, A = Y, Q = L, W = R, Tab = −, `=` là +), `J2ME_NX_FAKE_VERSION=0.1.0` giả làm bản cũ để thử cập nhật (bản desktop chỉ tải `.nro` về thư mục dữ liệu, hoặc `J2ME_NX_UPDATE_PATH`).

---

## English

A J2ME (Java ME / MIDP 2.0) emulator for Nintendo Switch, written in C with devkitPro + SDL2.
It runs `.jar` files of old Java phone apps and games directly on the Switch (homebrew `.nro`).

### Architecture

| Folder | Contents |
|---|---|
| `source/vm/` | Custom Java VM: class file loader, bytecode interpreter (all ~200 opcodes, including `jsr/ret`), green threads + monitors, mark-sweep GC, JAR reader (zip + zlib) |
| `source/vm/aot*` | Experimental AOT mode (enable in Settings): compiles bytecode to ARM64 machine code at class load, running alongside the interpreter |
| `javalib/src/` | CLDC 1.1 / MIDP 2.0 library written in Java: `java.lang/util/io`, `lcdui`, `lcdui.game`, `rms`, `media`, Nokia API (`FullCanvas`, `DirectGraphics`) |
| `source/midp/` | MIDP natives: software rendering (shapes, PNG/JPEG/GIF/BMP images, text via SDL_ttf), event queue, RecordStore saved to the SD card, audio (WAV/MP3 mixing + MIDI/tone synthesis), socket/HTTP/TLS |
| `source/third_party/` | `stb_image.h` (JPEG/GIF/BMP), `dr_mp3.h` (MP3), both public domain; `tsf.h` (TinySoundFont, MIT); `TimGM6mb.sf2` SoundFont (GPL v2, embedded in the binary); Google Sans font (OFL) with full Vietnamese coverage |
| `source/ui/` | UI built with [borealis](https://github.com/xfangfang/borealis) (submodule `library/borealis`): settings, button mapping, updates, sending apps from a phone |
| `source/` | App list (`menu.c`), app session (`emu.c`), `gfx.c` drawing layer on NanoVG for the app list / running app / on-screen keypad / video player, FFmpeg video decoding (`video_dec.c`), Switch/desktop platform layer |
| `resources/` | borealis resources (vi/en button hint strings, icons): packed into the `.nro` romfs |
| `tests/` | Test MIDlets: `demo-midlet` (Canvas, Sprite, Form, List, Alert, RMS), `audio-midlet` (MIDI, WAV, MP3, tone), `net-midlet` (socket, HTTP), `https-midlet` (HTTPS, ssl://), `m3g-midlet` (3D), `video-midlet` (video on a Canvas, in a Form, `platformRequest`) |

The Java library is compiled with `javac` at build time and embedded in the binary as `classlib.jar`.

### Build

Requirements: [devkitPro](https://devkitpro.org/wiki/Getting_Started) (`switch-dev` package), a JDK (`javac`, `jar`), CMake.

```bash
git submodule update --init library/borealis
sudo dkp-pacman -S switch-dev switch-sdl2 switch-sdl2_ttf switch-libpng switch-zlib switch-mbedtls switch-ffmpeg switch-mesa switch-libdrm_nouveau
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

Environment variables: `J2ME_NX_GAMES` (apps folder, default `./games`), `J2ME_NX_DATA` (logs, saves, settings; default `./data`).

#### CLion

- Switch profile: CMake options `-DCMAKE_TOOLCHAIN_FILE=/opt/devkitpro/cmake/Switch.cmake`, environment `DEVKITPRO=/opt/devkitpro`. Build the `j2me-nxx_nro` target (or Build Project).
- Desktop profile: CMake options `-DJ2ME_NX_DESKTOP=ON`, runnable with the Run button.

### Download

Prebuilt releases are on the [Releases](https://github.com/ToThangGTVT/j2me-nxx/releases) page: extract the zip to the root of your SD card.
GitHub Actions builds on every push; pushing a `v*` tag (e.g. `git tag v0.1.0 && git push origin v0.1.0`) creates a new release.

### Using it on the Switch

1. Copy `j2me-nxx.nro` to `sdmc:/switch/`.
2. Copy your `.jar` apps (and the matching `.jad` if you have one) to `sdmc:/switch/j2me-nxx/games/`. Subfolders are supported (up to 3 levels); the `games` folder is created on first launch.
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
| − (twice) | Exit app |

This is the default mapping. Change it in **Settings > Button mapping** (all apps) or **App options > Button mapping** (that app only; buttons left on "Default" follow the global settings): pick a Switch button in the list, press **A** (or tap it) and choose the phone key. **Y** resets the selected button, **X** resets them all. The in-app key panel follows the active mapping.

The touch screen is mapped to pointer events.

**On-screen keypad**: enable **Settings > On-screen keypad** (or per app in **App options**: Default / On / Off) to get a Nokia-style phone keypad on the touch screen while running: a joystick on the left (follows the left stick mapping, diagonals press two directions), left / right soft keys, Clear (C), Fire (OK) and the 0–9, `*`, `#` keypad on the right. Multi-touch works. The layout is placed on the Switch screen (shared by all apps, independent of the app screen size); edit it in **Settings > On-screen keypad layout**: drag a key to move it, drag the dot in its bottom-right corner to resize, set rounded corners and opacity, hide / show each key; with **Snap** on, keys dragged near another key or the screen edge snap against it. Controller: **L / R** pick a key, **D-pad** move, **X / Y** bigger / smaller, **A** hide / show. **Default** (tap twice) resets the whole layout. The key help panel on the left is hidden while the keypad is on.

**QWERTY virtual keyboard**: enable **Settings > Virtual keyboard bubble** to get a small bubble in the bottom-right corner while running (drag to move). Tap it to open a floating keyboard with a number row, letters, `* # , .`, space, Shift (double-tap = caps lock; Shift + number row gives `! @ # $ ...`), Del (clear key `-8`) and Enter (Fire). Letters and symbols are sent as character codes like on QWERTY phones. Drag the top bar to move the keyboard; **×** collapses it back to the bubble.

**Crash reports**: whenever the emulator crashes or a Java app fails (uncaught exception), J2ME-NXX writes one file `sdmc:/switch/j2me-nxx/crash/crash-<date>-<time>.txt` with the version, running app, reason, Java stack trace, CPU registers + backtrace (for app crashes) and the latest log lines. The next launch shows the file name in a notification. Attach it when reporting a bug; addresses like `j2me-nxx.elf + 0x...` can be turned into function names with `aarch64-none-elf-addr2line -f -C -e j2me-nxx.elf <offset>` using the `.elf` attached to the matching release.

**Updates**: each time the app starts, J2ME-NXX asks GitHub Releases whether a newer version exists (turn off in **Settings > Check for updates**). If there is one, a dialog shows the release notes: **A** downloads `j2me-nxx.nro` (with a progress bar, speed and time left; **B** cancels), the old file is only replaced once the download is complete and verified to be an `.nro`, then **A** restarts into the new version. Choosing "Later" leaves a "New version" badge in the app list; press **B** there to update later.

**SoundFont**: MIDI music is played with a SoundFont. `TimGM6mb` (~6 MB, GPL v2) is embedded in the `.nro`, so nothing extra needs to be copied. It is **off** by default (wave synth); turn it on in **Settings > MIDI SoundFont**. To use another General MIDI SoundFont, copy a `.sf2` file to `sdmc:/switch/j2me-nxx/soundfonts/` and pick it in the same setting: "Auto" uses the first `.sf2` file by name (or the built-in one if there is none), "TimGM6mb (built-in)" always uses the embedded one, "Off" uses the old wave synth (lighter). The whole `.sf2` is loaded into RAM, so prefer small files (under ~50 MB).

**Sending apps and videos from your phone (Upload)**: press **R** in the app list and the Switch shows a QR code. Scan it with a phone on the same Wi-Fi network to open the upload page, then pick one or more `.jar` / `.jad` apps or videos; they are saved straight into the `games` folder (with progress on both the phone and the Switch). If scanning does not work, type the address shown next to the code (like `http://192.168.x.x:8080/`) into the browser. Press **B** to close; the list is rescanned automatically.

**Deleting apps and videos**: select a file in the list and press **L**; a confirmation shows its name and size. Press **A** to delete it (along with the matching `.jad`, if any) or **B** to cancel. Save data and per-app options are kept, so copying the app back lets you continue where you left off.

**Watching videos**: copy `.3gp`, `.mp4`, `.avi`, `.mkv`, `.flv`, `.mpg`, `.wmv`... files into the same `games` folder; they show up in the list with a ▶ icon. While watching: **A** play / pause, **left / right** seek 10 s, **L / R** seek 1 min, **up / down** volume, **B** exit.

The app list shows the name, vendor, version and icon read from each app's `MANIFEST.MF` / `.jad` (loaded lazily as you scroll). JARs without `MIDlet-1` are flagged with a warning.

In the app list (**A** opens):
- **X**: Global settings (tabs App screen / Controls / Text and sound / System): FPS limit, default screen size (20 presets, portrait/landscape, custom), show key hints while running, button mapping, font size (75–300%), smooth (anti-aliased) text, recommended for Opera Mini, language (Tiếng Việt / English).
- **−**: Options for the selected app (FPS, screen size, key layout, button mapping, font size, smooth text), saved to `sdmc:/switch/j2me-nxx/options/<name>.ini`.

Screen size is chosen in this order: the app's own options > `Nokia-MIDlet-Original-Display-Size` in MANIFEST/JAD > global settings (default 240x320).

### Status

- Working: Canvas / GameCanvas, Sprite / TiledLayer / LayerManager, Image (PNG, JPEG, GIF, BMP), Font, Form / List / Alert / TextBox (Switch software keyboard), RecordStore, Timer, threads / wait / notify.
- Audio: WAV (8/16-bit PCM, IMA ADPCM), MP3, MIDI (played with a `.sf2` SoundFont via TinySoundFont; without one, synthesized with basic waveforms + drums), ToneControl, `Manager.playTone`, `com.nokia.mid.sound.Sound`. AMR, AAC, M4A and the audio track of 3GP/MP4 are decoded with FFmpeg.
- Video (MMAPI `VideoControl`): 3GP / MP4 (H.263, MPEG-4, H.264...) from the JAR, `file://` or `http://`; drawn over a Canvas (`USE_DIRECT_VIDEO`, including full screen) or inside a Form (`USE_GUI_PRIMITIVE`), `getSnapshot` (PNG), looping, seeking. No camera (`capture://`) yet.
- `MIDlet.platformRequest`: video links (`http://`, `file:///`...) play in the video player on top of the app (B to go back), web pages open in the Switch's built-in browser (requires hbmenu in full RAM mode). Useful for apps like JTube (set Playback method to Via browser).
- Networking: `socket://`, `http://`, `https://`, `ssl://` (TLS via mbedTLS, certificates are not verified), `datagram://` (UDP).
- Files: JSR-75 FileConnection with `C:/` and `E:/` drives in a per-app sandbox (`sdmc:/switch/j2me-nxx/files/<name>/`).
- Vendor APIs: Nokia UI (`FullCanvas`, `DirectGraphics`, `Sound`), Siemens (`com.siemens.mp.game/ui/io/gsm`), Samsung (`com.samsung.util`), Motorola (`funlight`, `multimedia`).
- Stubbed so apps don't fail on missing classes: Bluetooth (JSR-82), SMS (JSR-120, sending always reports an error), `PushRegistry`.
- Per-vendor key layouts (Nokia, Sony Ericsson, Samsung, Motorola, Siemens, LG) in Settings / App options; JARs with several MIDlets show a MIDlet picker.
- 3D: JSR-184 M3G (`javax.microedition.m3g`) with a software renderer (`source/midp/m3g.c`): Z-buffer, perspective-correct textures, per-vertex lighting (ambient/directional/omni/spot), fog, blending, Sprite3D, Skinned/MorphingMesh, keyframe animation, a `Loader` for `.m3g` files (including zlib-compressed sections), `Group.pick`.
- Not yet: MascotCapsule 3D (Sony Ericsson games), JSR-226 SVG, camera, sensors.

#### Automated testing on desktop

The desktop build reads a few environment variables to run a script (times in ms since the app started):

```bash
J2ME_NX_KEYS="1500:-6,2000:-5" J2ME_NX_SHOTS="3000:/tmp/a.bmp" J2ME_NX_QUIT=4000 ./build-desktop/j2me-nxx game.jar
```

`J2ME_NX_TAPS="1000:640:500"` clicks at (x, y) on the 1280x720 app screen, `J2ME_NX_APPSHOT=<file.bmp>` takes a screenshot of the app (app list) and exits, `J2ME_NX_AUDIO_DUMP=<file>` writes the audio stream (16-bit mono PCM, 48000 Hz) to a file, `J2ME_NX_SCREEN=settings` / `lang` / `keybind` opens the settings / language / button mapping screen directly, `J2ME_NX_PRESS="1500:Return,2000:Down"` presses keyboard keys (Enter = A, Esc = B, arrows; S = X, A = Y, Q = L, W = R, Tab = −, `=` is +), `J2ME_NX_FAKE_VERSION=0.1.0` pretends to be an older version to test updating (the desktop build only downloads the `.nro` into the data folder, or `J2ME_NX_UPDATE_PATH`).
