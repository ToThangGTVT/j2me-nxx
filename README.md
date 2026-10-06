# j2me-nxx

Trình giả lập J2ME (Java ME / MIDP 2.0) cho Nintendo Switch, viết bằng C trên devkitPro + SDL2.
Chạy file `.jar` của game điện thoại Java cũ trực tiếp trên Switch (homebrew `.nro`).

## Kiến trúc

| Thư mục | Nội dung |
|---|---|
| `source/vm/` | Máy ảo Java tự viết: đọc class file, trình thông dịch bytecode (đủ ~200 opcode, kể cả `jsr/ret`), green thread + monitor, GC mark-sweep, đọc JAR (zip + zlib) |
| `javalib/src/` | Thư viện CLDC 1.1 / MIDP 2.0 viết bằng Java: `java.lang/util/io`, `lcdui`, `lcdui.game`, `rms`, `media` (chưa có tiếng), API Nokia (`FullCanvas`, `DirectGraphics`) |
| `source/midp/` | Native của MIDP: vẽ phần mềm (hình, ảnh PNG/JPEG/GIF/BMP, chữ qua SDL_ttf), hàng đợi sự kiện, RecordStore lưu ra thẻ SD, âm thanh (trộn WAV + tổng hợp MIDI/tone), socket/HTTP |
| `source/` | App: danh sách game, cài đặt, phiên chạy game (`emu.c`), lớp nền tảng Switch/desktop |
| `tests/` | MIDlet để kiểm tra: `demo-midlet` (Canvas, Sprite, Form, List, Alert, RMS), `audio-midlet` (MIDI, WAV, tone), `net-midlet` (socket, HTTP) |

Thư viện Java được biên dịch bằng `javac` lúc build rồi nhúng vào binary dưới dạng `classlib.jar`.

## Build

Cần: [devkitPro](https://devkitpro.org/wiki/Getting_Started) (gói `switch-dev`), JDK (`javac`, `jar`), CMake.

```bash
sudo dkp-pacman -S switch-dev switch-sdl2 switch-sdl2_ttf switch-libpng switch-zlib
export DEVKITPRO=/opt/devkitpro
cmake -B build -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/Switch.cmake
cmake --build build
```

Kết quả: `build/j2me-nx.nro`.

### Bản desktop (test nhanh trên Mac/Linux)

```bash
brew install sdl2 sdl2_ttf libpng pkgconf
cmake -B build-desktop -DJ2ME_NX_DESKTOP=ON
cmake --build build-desktop
./build-desktop/j2me-nx path/to/game.jar
```

Biến môi trường: `J2ME_NX_GAMES` (thư mục game, mặc định `./games`), `J2ME_NX_DATA` (log, save, cài đặt; mặc định `./data`).

### CLion

- Profile Switch: CMake options `-DCMAKE_TOOLCHAIN_FILE=/opt/devkitpro/cmake/Switch.cmake`, Environment `DEVKITPRO=/opt/devkitpro`. Build target `j2me-nx_nro` (hoặc Build Project).
- Profile Desktop: CMake options `-DJ2ME_NX_DESKTOP=ON`, chạy được bằng nút Run.

## Dùng trên Switch

1. Chép `j2me-nx.nro` vào `sdmc:/switch/`.
2. Chép game `.jar` (và `.jad` nếu có) vào `sdmc:/switch/j2me-nx/games/`.
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

Trong danh sách game:
- **X**: Cài đặt chung (giới hạn FPS, kích thước màn hình mặc định).
- **−**: Tuỳ chọn riêng cho game đang chọn (FPS, kích thước màn hình), lưu ở `sdmc:/switch/j2me-nx/games/<tên>.ini`.

Kích thước màn hình được chọn theo thứ tự: tuỳ chọn riêng của game > `Nokia-MIDlet-Original-Display-Size` trong MANIFEST/JAD > cài đặt chung (mặc định 240x320).

## Trạng thái

- Đã chạy: Canvas / GameCanvas, Sprite / TiledLayer / LayerManager, Image (PNG, JPEG, GIF, BMP), Font, Form / List / Alert / TextBox (bàn phím ảo của Switch), RecordStore, Timer, thread / wait / notify.
- Âm thanh: WAV (PCM 8/16-bit, IMA ADPCM), MIDI (tổng hợp bằng sóng cơ bản + trống, không cần soundfont), ToneControl, `Manager.playTone`, `com.nokia.mid.sound.Sound`. MP3/AMR chưa phát được (game vẫn chạy, chỉ im lặng).
- Mạng: `socket://` và `http://` (chưa có `https://`, `datagram://`).

### Test tự động trên desktop

Bản desktop đọc vài biến môi trường để chạy kịch bản (tính theo ms từ lúc game chạy):

```bash
J2ME_NX_KEYS="1500:-6,2000:-5" J2ME_NX_SHOTS="3000:/tmp/a.bmp" J2ME_NX_QUIT=4000 ./build-desktop/j2me-nx game.jar
```

`J2ME_NX_AUDIO_DUMP=<file>` ghi luồng âm thanh (PCM 16-bit mono 22050Hz) ra file, `J2ME_NX_SCREEN=settings` mở thẳng màn hình cài đặt.
