# j2me-nxx

Trình giả lập J2ME (Java ME / MIDP 2.0) cho Nintendo Switch, viết bằng C trên devkitPro + SDL2.
Chạy file `.jar` của game điện thoại Java cũ trực tiếp trên Switch (homebrew `.nro`).

## Kiến trúc

| Thư mục | Nội dung |
|---|---|
| `source/vm/` | Máy ảo Java tự viết: đọc class file, trình thông dịch bytecode (đủ ~200 opcode, kể cả `jsr/ret`), green thread + monitor, GC mark-sweep, đọc JAR (zip + zlib) |
| `javalib/src/` | Thư viện CLDC 1.1 / MIDP 2.0 viết bằng Java: `java.lang/util/io`, `lcdui`, `lcdui.game`, `rms`, `media` (chưa có tiếng), API Nokia (`FullCanvas`, `DirectGraphics`) |
| `source/midp/` | Native của MIDP: vẽ phần mềm (hình, ảnh PNG/JPEG/GIF/BMP, chữ qua SDL_ttf), hàng đợi sự kiện, RecordStore lưu ra thẻ SD, âm thanh (trộn WAV/MP3 + tổng hợp MIDI/tone), socket/HTTP/TLS |
| `source/third_party/` | `stb_image.h` (JPEG/GIF/BMP), `dr_mp3.h` (MP3), đều public domain; font Google Sans (OFL) có đủ chữ tiếng Việt |
| `source/` | App: danh sách game, cài đặt, phiên chạy game (`emu.c`), lớp nền tảng Switch/desktop |
| `tests/` | MIDlet để kiểm tra: `demo-midlet` (Canvas, Sprite, Form, List, Alert, RMS), `audio-midlet` (MIDI, WAV, MP3, tone), `net-midlet` (socket, HTTP), `https-midlet` (HTTPS, ssl://), `m3g-midlet` (3D) |

Thư viện Java được biên dịch bằng `javac` lúc build rồi nhúng vào binary dưới dạng `classlib.jar`.

## Build

Cần: [devkitPro](https://devkitpro.org/wiki/Getting_Started) (gói `switch-dev`), JDK (`javac`, `jar`), CMake.

```bash
sudo dkp-pacman -S switch-dev switch-sdl2 switch-sdl2_ttf switch-libpng switch-zlib switch-mbedtls
export DEVKITPRO=/opt/devkitpro
cmake -B build -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/Switch.cmake
cmake --build build
```

Kết quả: `build/j2me-nx.nro`. Thiếu mbedTLS thì vẫn build được, chỉ không có `https://` / `ssl://`.

### Bản desktop (test nhanh trên Mac/Linux)

```bash
brew install sdl2 sdl2_ttf libpng mbedtls pkgconf
cmake -B build-desktop -DJ2ME_NX_DESKTOP=ON
cmake --build build-desktop
./build-desktop/j2me-nx path/to/game.jar
```

Biến môi trường: `J2ME_NX_GAMES` (thư mục game, mặc định `./games`), `J2ME_NX_DATA` (log, save, cài đặt; mặc định `./data`).

### CLion

- Profile Switch: CMake options `-DCMAKE_TOOLCHAIN_FILE=/opt/devkitpro/cmake/Switch.cmake`, Environment `DEVKITPRO=/opt/devkitpro`. Build target `j2me-nx_nro` (hoặc Build Project).
- Profile Desktop: CMake options `-DJ2ME_NX_DESKTOP=ON`, chạy được bằng nút Run.

## Tải bản build

Bản phát hành có sẵn ở trang [Releases](https://github.com/ToThangGTVT/j2me-nxx/releases): giải nén zip vào gốc thẻ SD.
GitHub Actions tự build mỗi lần push; push tag `v*` (vd `git tag v0.1.0 && git push origin v0.1.0`) sẽ tạo bản phát hành mới.

## Dùng trên Switch

1. Chép `j2me-nx.nro` vào `sdmc:/switch/`.
2. Chép game `.jar` (và `.jad` cùng tên nếu có) vào `sdmc:/switch/j2me-nx/games/`. Có thể chia thư mục con (tối đa 3 cấp), app tự tạo thư mục `games` ở lần chạy đầu.
3. Mở bằng hbmenu. Save game ở `sdmc:/switch/j2me-nx/rms/`, log ở `sdmc:/switch/j2me-nx/log.txt`.

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

Danh sách game hiện tên, nhà phát hành, phiên bản và icon đọc từ `MANIFEST.MF` / `.jad` của từng game (đọc dần khi cuộn tới). JAR thiếu `MIDlet-1` được đánh dấu cảnh báo.

Trong danh sách game:
- **X**: Cài đặt chung: giới hạn FPS, kích thước màn hình mặc định (có sẵn 20 cỡ, dọc/ngang, tuỳ chỉnh), hiện chú thích phím khi chơi, ngôn ngữ (Tiếng Việt / English).
- **−**: Tuỳ chọn riêng cho game đang chọn (FPS, kích thước màn hình), lưu ở `sdmc:/switch/j2me-nx/games/<tên>.ini`.

Kích thước màn hình được chọn theo thứ tự: tuỳ chọn riêng của game > `Nokia-MIDlet-Original-Display-Size` trong MANIFEST/JAD > cài đặt chung (mặc định 240x320).

## Trạng thái

- Đã chạy: Canvas / GameCanvas, Sprite / TiledLayer / LayerManager, Image (PNG, JPEG, GIF, BMP), Font, Form / List / Alert / TextBox (bàn phím ảo của Switch), RecordStore, Timer, thread / wait / notify.
- Âm thanh: WAV (PCM 8/16-bit, IMA ADPCM), MP3, MIDI (tổng hợp bằng sóng cơ bản + trống, không cần soundfont), ToneControl, `Manager.playTone`, `com.nokia.mid.sound.Sound`. AMR chưa phát được (game vẫn chạy, chỉ im lặng).
- Mạng: `socket://`, `http://`, `https://`, `ssl://` (TLS qua mbedTLS, không kiểm tra chứng chỉ), `datagram://` (UDP).
- File: JSR-75 FileConnection với ổ `C:/`, `E:/` trong sandbox riêng của từng game (`sdmc:/switch/j2me-nx/files/<game>/`).
- API của hãng: Nokia UI (`FullCanvas`, `DirectGraphics`, `Sound`), Siemens (`com.siemens.mp.game/ui/io/gsm`), Samsung (`com.samsung.util`), Motorola (`funlight`, `multimedia`).
- Giả lập để game không lỗi thiếu lớp: Bluetooth (JSR-82), SMS (JSR-120, gửi luôn báo lỗi), `PushRegistry`.
- Kiểu phím theo hãng (Nokia, Sony Ericsson, Samsung, Motorola, Siemens, LG) trong Cài đặt / Tuỳ chọn game; JAR nhiều MIDlet có hộp chọn MIDlet.
- 3D: JSR-184 M3G (`javax.microedition.m3g`) với bộ dựng hình phần mềm (`source/midp/m3g.c`): Z-buffer, texture có hiệu chỉnh phối cảnh, chiếu sáng theo đỉnh (ambient/directional/omni/spot), fog, blend, Sprite3D, Skinned/MorphingMesh, animation keyframe, `Loader` đọc file `.m3g` (kể cả section nén zlib), `Group.pick`.
- Chưa có: MascotCapsule 3D (game Sony Ericsson), JSR-226 SVG, AMR, cảm biến.

### Test tự động trên desktop

Bản desktop đọc vài biến môi trường để chạy kịch bản (tính theo ms từ lúc game chạy):

```bash
J2ME_NX_KEYS="1500:-6,2000:-5" J2ME_NX_SHOTS="3000:/tmp/a.bmp" J2ME_NX_QUIT=4000 ./build-desktop/j2me-nx game.jar
```

`J2ME_NX_APPSHOT=<file.bmp>` chụp màn hình app (danh sách game) rồi thoát, `J2ME_NX_AUDIO_DUMP=<file>` ghi luồng âm thanh (PCM 16-bit mono 22050Hz) ra file, `J2ME_NX_SCREEN=settings` mở thẳng màn hình cài đặt.
