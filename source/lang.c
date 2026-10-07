#include "lang.h"

#include "gfx.h"

#include <string.h>

static Lang current = LANG_VI;

static const char *const strings[S_COUNT][LANG_COUNT] = {
    [S_APP_SUBTITLE]   = { "Trình giả lập J2ME cho Nintendo Switch", "J2ME emulator for Nintendo Switch" },
    [S_GAME_COUNT]     = { "%d game", "%d games" },
    [S_FOLDER]         = { "Thư mục: %s", "Folder: %s" },
    [S_NO_MIDLET]      = { "Không có MIDlet-1 trong MANIFEST: có thể không chạy được",
                           "No MIDlet-1 in MANIFEST: may not run" },
    [S_MENU_HINTS]     = { ICON_A " Chơi   " ICON_MINUS " Tùy chọn game   " ICON_X " Cài đặt   " ICON_Y " Quét lại   " ICON_R " Gửi game   " ICON_L " Xoá   " ICON_PLUS " Thoát",
                           ICON_A " Play   " ICON_MINUS " Game options   " ICON_X " Settings   " ICON_Y " Rescan   " ICON_R " Send games   " ICON_L " Delete   " ICON_PLUS " Exit" },

    [S_PICK_MIDLET]    = { "Chọn MIDlet:  " ICON_A " Chạy   " ICON_B " Quay lại", "Choose MIDlet:  " ICON_A " Run   " ICON_B " Back" },

    [S_EMPTY_TITLE]    = { "Chưa có game nào", "No games yet" },
    [S_EMPTY_COPY]     = { "Chép file game .jar (và .jad cùng tên nếu có) vào thư mục:",
                           "Copy your .jar games (and the matching .jad if any) to:" },
    [S_EMPTY_SUBDIR]   = { "Có thể chia thư mục con, ví dụ: games/RPG/game.jar",
                           "Subfolders are supported, e.g. games/RPG/game.jar" },
    [S_EMPTY_VIDEO]    = { "File video (.mp4, .3gp...) để cùng chỗ cũng mở được",
                           "Video files (.mp4, .3gp...) placed there can be played too" },
    [S_EMPTY_RESCAN]   = { "Chép xong nhấn " ICON_Y " để quét lại", "Then press " ICON_Y " to rescan" },

    [S_ERROR_FMT]      = { "Lỗi: %s", "Error: %s" },
    [S_RESCANNED]      = { "Đã quét lại: %d file", "Rescanned: %d files" },
    [S_SETTINGS_SAVED] = { "Đã lưu cài đặt", "Settings saved" },
    [S_GAME_EXITED]    = { "Đã thoát game", "Game closed" },

    [S_ERR_SYSLIB]     = { "Thư viện hệ thống bị hỏng", "System library is corrupted" },
    [S_ERR_OPEN_JAR]   = { "Không mở được file JAR", "Cannot open the JAR file" },
    [S_ERR_NO_MIDLET]  = { "JAR không có MIDlet-1 trong MANIFEST", "JAR has no MIDlet-1 in MANIFEST" },
    [S_ERR_VM]         = { "Lỗi khởi động VM: %s", "VM startup error: %s" },
    [S_ERR_MIDLET]     = { "Không chạy được MIDlet: %s", "Cannot start MIDlet: %s" },
    [S_GAME_ENDED]     = { "Game đã kết thúc (không còn thread nào chạy)", "Game ended (no threads left)" },
    [S_GAME_CRASHED]   = { "Game lỗi, đã ghi crash/%s", "Game crashed, saved crash/%s" },
    [S_APP_CRASHED_BEFORE] = { "Lần trước app bị sập, đã ghi crash/%s", "The app crashed last time, saved crash/%s" },

    [S_EXIT_CONFIRM]   = { "Nhấn - (hoặc Esc) lần nữa để thoát game", "Press - (or Esc) again to exit the game" },
    [S_SCREEN_INFO_FPS] = { "%dx%d  -  giới hạn %d FPS", "%dx%d  -  %d FPS limit" },
    [S_HELP_DPAD]      = { "Điều hướng", "Directions" },
    [S_HELP_SOFT_RIGHT] = { "Phím mềm phải", "Right soft key" },
    [S_HELP_SOFT_LEFT] = { "Phím mềm trái", "Left soft key" },
    [S_HELP_STICK_CLICK] = { "Bấm L / R stick", "Click L / R stick" },
    [S_HELP_EXIT]      = { "Thoát game", "Exit game" },

    [S_SETTINGS]       = { "Cài đặt", "Settings" },
    [S_GAME_OPTIONS]   = { "Tùy chọn game", "Game options" },
    [S_FPS_LIMIT]      = { "Giới hạn FPS", "FPS limit" },
    [S_FPS_HINT]       = { "Số khung hình tối đa mỗi giây của game. Giúp game chạy đúng tốc độ và đỡ tốn pin.",
                           "Maximum frames per second of the game. Keeps games at the right speed and saves battery." },
    [S_UNLIMITED]      = { "Không giới hạn", "Unlimited" },
    [S_DEFAULT_FMT]    = { "Mặc định (%s)", "Default (%s)" },
    [S_SCREEN_SIZE]    = { "Kích thước màn hình", "Screen size" },
    [S_SCREEN_SIZE_DEFAULT] = { "Kích thước màn hình mặc định", "Default screen size" },
    [S_SCREEN_SIZE_HINT_GAME] = { "Tự động: lấy từ MANIFEST của game, nếu không có thì dùng kích thước mặc định. "
                                  "Chọn 'Tùy chỉnh' để nhập kích thước bất kỳ.",
                                  "Auto: taken from the game's MANIFEST, otherwise the default size. "
                                  "Choose 'Custom' to enter any size." },
    [S_SCREEN_SIZE_HINT] = { "Dùng cho game không khai báo kích thước. Phổ biến nhất là 240x320. "
                             "Chọn 'Tùy chỉnh' để nhập kích thước bất kỳ.",
                             "Used for games that do not declare a size. The most common is 240x320. "
                             "Choose 'Custom' to enter any size." },
    [S_AUTO]           = { "Tự động", "Auto" },
    [S_CUSTOM]         = { "Tùy chỉnh", "Custom" },
    [S_ORIENTATION]    = { "Hướng màn hình", "Orientation" },
    [S_ORIENT_HINT]    = { "Dọc: cao hơn rộng (điện thoại thường). Ngang: rộng hơn cao (vd 320x240, 640x360).",
                           "Portrait: taller than wide (common phones). Landscape: wider than tall (e.g. 320x240)." },
    [S_PORTRAIT]       = { "Dọc", "Portrait" },
    [S_LANDSCAPE]      = { "Ngang", "Landscape" },
    [S_SQUARE]         = { "Vuông", "Square" },
    [S_WIDTH]          = { "Chiều rộng", "Width" },
    [S_HEIGHT]         = { "Chiều cao", "Height" },
    [S_SIZE_EDIT_HINT] = { "Trái/Phải: +-1, L/R: +-10, A: nhập số. Giới hạn 64 - 1280.",
                           "Left/Right: +-1, L/R: +-10, A: type a number. Range 64 - 1280." },
    [S_KB_WIDTH]       = { "Chiều rộng màn hình", "Screen width" },
    [S_KB_HEIGHT]      = { "Chiều cao màn hình", "Screen height" },
    [S_LANGUAGE]       = { "Ngôn ngữ / Language", "Language / Ngôn ngữ" },
    [S_LANGUAGE_HINT]  = { "Ngôn ngữ của giao diện. Game cũng nhận microedition.locale tương ứng.",
                           "Interface language. Games also receive the matching microedition.locale." },
    [S_KEYMAP]         = { "Kiểu phím", "Key layout" },
    [S_KEYMAP_HINT]    = { "Mã phím mềm / điều hướng theo hãng điện thoại mà game được làm cho. "
                           "Dùng khi phím mềm hoặc phím Fire không ăn.",
                           "Soft key / navigation codes of the phone brand the game was made for. "
                           "Use it when soft keys or Fire do not work." },
    [S_SCALE_MODE]     = { "Kiểu phóng to", "Scaling" },
    [S_SCALE_HINT]     = { "Phóng màn hình game bằng GPU. Sắc nét: giữ điểm ảnh rõ mà vẫn đều (khuyên dùng). "
                           "Điểm ảnh: nét nhất nhưng điểm ảnh có thể to nhỏ không đều. Số nguyên: đúng 2x/3x, có viền.",
                           "The game screen is upscaled by the GPU. Sharp: crisp, even pixels (recommended). "
                           "Pixel: crispest but pixels may be uneven. Integer: exact 2x/3x with borders." },
    [S_SCALE_SMOOTH]   = { "Sắc nét", "Sharp" },
    [S_SCALE_SHARP]    = { "Điểm ảnh", "Pixel" },
    [S_SCALE_INTEGER]  = { "Điểm ảnh (số nguyên)", "Pixel (integer)" },
    [S_SHOW_HELP]      = { "Hiện chú thích phím khi chơi", "Show key help while playing" },
    [S_SHOW_HELP_HINT] = { "Bảng phím ở bên trái màn hình game.", "Key map shown left of the game screen." },
    [S_SHOW_FPS]       = { "Hiện FPS", "Show FPS" },
    [S_SHOW_FPS_HINT]  = { "Góc trên trái: số khung hình game vẽ mỗi giây, % CPU dùng để chạy game, bộ nhớ Java và RAM của cả app.",
                           "Top left: frames the game draws per second, CPU % spent running the game, Java memory and the app's total RAM." },
    [S_FONT_SCALE]     = { "Cỡ chữ", "Font size" },
    [S_FONT_SCALE_HINT] = { "Phóng to / thu nhỏ chữ trong game so với cỡ gốc. Hợp với Opera Mini, nhất là khi tăng "
                            "kích thước màn hình (vd 480x800 + 200%). Game tự tính bố cục theo cỡ chữ mới.",
                            "Scale in-game text relative to the original size. Useful for Opera Mini, especially with a "
                            "larger screen size (e.g. 480x800 + 200%). Apps lay out text using the new size." },
    [S_SMOOTH_TEXT]    = { "Chữ mịn", "Smooth text" },
    [S_SMOOTH_TEXT_HINT] = { "Khử răng cưa chữ trong game. Nên bật cho ứng dụng nhiều chữ như Opera Mini; "
                             "tắt thì chữ vẽ điểm ảnh như điện thoại thật.",
                             "Anti-aliased in-game text. Recommended for text-heavy apps like Opera Mini; "
                             "when off, text is drawn as pixels like on real phones." },
    [S_CHECK_UPDATE]   = { "Tự kiểm tra bản mới", "Check for updates" },
    [S_CHECK_UPDATE_HINT] = { "Mỗi lần mở app, hỏi GitHub xem có bản J2ME-NXX mới không; có thì hỏi tải về và tự "
                              "thay file .nro. Cần kết nối Internet.",
                              "Each time the app starts, ask GitHub whether a newer J2ME-NXX exists; if so, offer to "
                              "download it and replace the .nro file. Needs an Internet connection." },
    [S_UPDATE_BADGE]   = { "Bản mới v%s  " ICON_B " Cập nhật", "New v%s  " ICON_B " Update" },
    [S_UPDATE_TITLE]   = { "Có bản mới: v%s", "New version: v%s" },
    [S_UPDATE_CURRENT] = { "Đang dùng v%s", "You have v%s" },
    [S_UPDATE_PROMPT_HINTS] = { ICON_A " Tải về và cập nhật   " ICON_B " Để sau", ICON_A " Download and update   " ICON_B " Later" },
    [S_UPDATE_FAILED]  = { "Cập nhật lỗi: %s", "Update failed: %s" },
    [S_UPDATE_FAILED_HINTS] = { ICON_A " Thử lại   " ICON_B " Đóng", ICON_A " Retry   " ICON_B " Close" },
    [S_UPDATE_DOWNLOADING] = { "Đang tải v%s...", "Downloading v%s..." },
    [S_UPDATE_CANCELLING] = { "Đang huỷ...", "Cancelling..." },
    [S_UPDATE_ETA]     = { "còn %d:%02d", "%d:%02d left" },
    [S_UPDATE_KEEP_OPEN] = { "Đừng thoát app hay tắt máy khi đang tải. File .nro cũ chỉ bị thay khi đã tải xong và "
                             "kiểm tra hợp lệ.",
                             "Do not exit the app or turn off the console while downloading. The old .nro is only "
                             "replaced once the download is complete and verified." },
    [S_UPDATE_CANCEL_HINT] = { ICON_B " Huỷ", ICON_B " Cancel" },
    [S_UPDATE_DONE]    = { "Đã cập nhật lên v%s", "Updated to v%s" },
    [S_UPDATE_DONE_INFO] = { "File .nro đã được thay bằng bản mới. Khởi động lại app để dùng bản mới.",
                             "The .nro file has been replaced. Restart the app to use the new version." },
    [S_UPDATE_DONE_HINTS] = { ICON_A " Khởi động lại   " ICON_B " Để sau", ICON_A " Restart   " ICON_B " Later" },
    [S_UPDATE_DONE_DESKTOP] = { "Bản desktop chỉ tải file .nro về thư mục dữ liệu để thử, không tự thay app.",
                                "The desktop build only downloads the .nro to the data folder for testing." },
    [S_UPDATE_CLOSE_HINT] = { ICON_A " Đóng", ICON_A " Close" },
    [S_VKB_BUBBLE]     = { "Bong bóng bàn phím ảo", "Virtual keyboard bubble" },
    [S_VKB_BUBBLE_HINT] = { "Khi chơi có bong bóng nhỏ trên màn hình cảm ứng (kéo để di chuyển). Chạm vào để mở bàn phím "
                            "QWERTY nổi có hàng số, gõ chữ cho game / ứng dụng; nút × thu bàn phím về bong bóng.",
                            "Shows a small bubble on the touch screen while playing (drag to move). Tap it to open a "
                            "floating QWERTY keyboard with a number row for typing in games / apps; × collapses it back." },
    [S_SOUNDFONT]      = { "SoundFont MIDI", "MIDI SoundFont" },
    [S_SOUNDFONT_HINT] = { "Phát nhạc MIDI bằng SoundFont cho giống nhạc cụ thật: có sẵn TimGM6mb, hoặc chép file .sf2 vào "
                           "sdmc:/switch/j2me-nxx/soundfonts/. Tắt: bộ tổng hợp sóng, nhẹ hơn.",
                           "Play MIDI music with a SoundFont for real-instrument sound: TimGM6mb is built in, or copy a .sf2 "
                           "file to sdmc:/switch/j2me-nxx/soundfonts/. Off: wave synth, lighter." },
    [S_SOUNDFONT_AUTO] = { "Tự động (%s)", "Auto (%s)" },
    [S_SOUNDFONT_NONE] = { "Tắt - tổng hợp sóng", "Off - wave synth" },
    [S_SOUNDFONT_BUILTIN] = { "TimGM6mb (có sẵn)", "TimGM6mb (built-in)" },
    [S_SYSTEM_FONT]    = { "Font hệ thống", "System font" },
    [S_SYSTEM_FONT_HINT] = { "Chữ trong game dùng font của máy (Switch: font hệ thống có chữ Nhật, Trung, Hàn), "
                             "tự mịn và vẽ nét theo độ phân giải màn hình. Tắt thì dùng font của app; ký tự font "
                             "đang dùng không có luôn được lấy từ font kia.",
                             "In-game text uses the device font (Switch: system fonts with Japanese, Chinese, Korean), "
                             "always smooth and drawn sharp at screen resolution. When off, the app font is used; "
                             "characters missing from either font are taken from the other." },
    [S_ON]             = { "Bật", "On" },
    [S_OFF]            = { "Tắt", "Off" },
    [S_SETTINGS_HINTS] = { ICON_LEFT ICON_RIGHT " Đổi giá trị   " ICON_A " Chọn / nhập số   " ICON_B " Lưu và quay lại",
                           ICON_LEFT ICON_RIGHT " Change   " ICON_A " Select / enter number   " ICON_B " Save and back" },
    [S_VIDEO_TAG]      = { "Video", "Video" },
    [S_VIDEO_HINTS]    = { ICON_A " Phát / dừng   " ICON_LEFT ICON_RIGHT " Tua 10 giây   " ICON_L "/" ICON_R " Tua 1 phút   " ICON_UP ICON_DOWN " Âm lượng   " ICON_B " Thoát",
                           ICON_A " Play / pause   " ICON_LEFT ICON_RIGHT " Seek 10 s   " ICON_L "/" ICON_R " Seek 1 min   " ICON_UP ICON_DOWN " Volume   " ICON_B " Exit" },
    [S_VIDEO_PAUSED]   = { "Tạm dừng", "Paused" },
    [S_VIDEO_ENDED]    = { "Hết", "Ended" },
    [S_VOLUME_FMT]     = { "Âm lượng %d%%", "Volume %d%%" },
    [S_ERR_VIDEO]      = { "Không mở được video", "Cannot open video" },
    [S_ERR_NO_VIDEO_BUILD] = { "Bản build này không có FFmpeg: không xem được video",
                               "This build has no FFmpeg: video is not supported" },
    [S_VIDEO_NO_OPTIONS] = { "Video không có tuỳ chọn riêng", "Videos have no options" },
    [S_LINK_OPENING]   = { "Đang mở liên kết...   " ICON_B " Huỷ", "Opening link...   " ICON_B " Cancel" },
    [S_LINK_FAILED]    = { "Không mở được liên kết", "Cannot open link" },
    [S_BROWSER_NEEDS_APP] = { "Muốn mở trình duyệt, hãy chạy hbmenu ở chế độ full RAM (giữ R khi mở một game)",
                              "To open the browser, run hbmenu in full RAM mode (hold R while launching a game)" },
    [S_EMPTY_UPLOAD]   = { "Hoặc nhấn " ICON_R " để gửi game, video từ điện thoại bằng mã QR",
                           "Or press " ICON_R " to send games and videos from your phone with a QR code" },
    [S_UPLOAD_TITLE]   = { "Gửi game, video từ điện thoại", "Send games and videos from your phone" },
    [S_UPLOAD_STEP1]   = { "1. Điện thoại và Switch dùng chung một mạng Wi-Fi",
                           "1. Connect your phone to the same Wi-Fi as the Switch" },
    [S_UPLOAD_STEP2]   = { "2. Quét mã QR bằng camera (hoặc mở địa chỉ bên dưới)",
                           "2. Scan the QR code with the camera (or open the address below)" },
    [S_UPLOAD_STEP3]   = { "3. Chọn game .jar (và .jad nếu có) hoặc video để gửi lên",
                           "3. Choose .jar games (and .jad if any) or videos to send" },
    [S_UPLOAD_WAITING] = { "Đang chờ điện thoại...", "Waiting for your phone..." },
    [S_UPLOAD_RECEIVING] = { "Đang nhận %s", "Receiving %s" },
    [S_UPLOAD_RECEIVED] = { "Đã nhận %d file, mới nhất: %s", "Received %d file(s), latest: %s" },
    [S_UPLOAD_NO_NET]  = { "Switch chưa kết nối mạng. Hãy kết nối Wi-Fi rồi mở lại.",
                           "The Switch is not connected to a network. Connect to Wi-Fi and try again." },
    [S_UPLOAD_SERVER_FAILED] = { "Không mở được cổng mạng: %s", "Cannot open a network port: %s" },
    [S_UPLOAD_WRITE_FAILED] = { "Không ghi được file vào thẻ nhớ", "Cannot write the file to the SD card" },
    [S_UPLOAD_HINTS]   = { ICON_B " Đóng", ICON_B " Close" },
    [S_UPLOAD_DONE_STATUS] = { "Đã nhận %d file từ điện thoại", "Received %d file(s) from your phone" },
    [S_DELETE_GAME]    = { "Xoá game này?", "Delete this game?" },
    [S_DELETE_VIDEO]   = { "Xoá video này?", "Delete this video?" },
    [S_DELETE_KEEP_SAVE] = { "Dữ liệu save và tuỳ chọn riêng của game vẫn được giữ lại.",
                             "Save data and per-game options are kept." },
    [S_DELETE_HINTS]   = { ICON_A " Xoá   " ICON_B " Huỷ", ICON_A " Delete   " ICON_B " Cancel" },
    [S_DELETED]        = { "Đã xoá: %s", "Deleted: %s" },
    [S_DELETE_FAILED]  = { "Không xoá được: %s", "Could not delete: %s" },
    [S_WEB_TITLE]      = { "Gửi game, video lên Switch", "Send games and videos to the Switch" },
    [S_WEB_INTRO]      = { "Chọn game .jar (và .jad nếu có) hoặc video (.mp4, .3gp, .mkv...). File được lưu vào "
                           "thư mục games và hiện trong danh sách khi đóng màn hình gửi game trên Switch.",
                           "Choose .jar games (and .jad if any) or videos (.mp4, .3gp, .mkv...). Files are saved to the "
                           "games folder and show up in the list once you close the send screen on the Switch." },
    [S_WEB_CHOOSE]     = { "Chọn game / video", "Choose games / videos" },
    [S_WEB_WAITING]    = { "Chờ gửi", "Waiting" },
    [S_WEB_SENDING]    = { "Đang gửi", "Sending" },
    [S_WEB_DONE]       = { "Đã gửi", "Sent" },
    [S_WEB_FAILED]     = { "Lỗi", "Failed" },
    [S_WEB_NETWORK]    = { "mất kết nối với Switch", "lost connection to the Switch" },
    [S_WEB_WRONG_TYPE] = { "Chỉ nhận game .jar / .jad hoặc video", "Only .jar / .jad games or videos are accepted" },
    [S_WEB_ALL_DONE]   = { "Xong! Đóng màn hình gửi game trên Switch để thấy file mới trong danh sách.",
                           "Done! Close the send screen on the Switch to see the new files in the list." },
};

void lang_set(Lang l) {
    if (l >= 0 && l < LANG_COUNT)
        current = l;
}

Lang lang_get(void) {
    return current;
}

const char *lang_code(Lang l) {
    return l == LANG_EN ? "en" : "vi";
}

const char *lang_name(Lang l) {
    return l == LANG_EN ? "English" : "Tiếng Việt";
}

Lang lang_from_code(const char *code) {
    return code && strncmp(code, "en", 2) == 0 ? LANG_EN : LANG_VI;
}

const char *tr(StrId id) {
    const char *s = id >= 0 && id < S_COUNT ? strings[id][current] : NULL;
    return s ? s : "?";
}
