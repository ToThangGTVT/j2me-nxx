// Chuỗi giao diện theo ngôn ngữ (Tiếng Việt / English)
#pragma once

typedef enum {
    LANG_VI,
    LANG_EN,
    LANG_COUNT,
} Lang;

typedef enum {
    // Danh sách game
    S_APP_SUBTITLE,
    S_GAME_COUNT,           // %d
    S_FOLDER,               // %s
    S_NO_MIDLET,
    S_MENU_HINTS,
    S_PICK_MIDLET,
    // Chưa có game
    S_EMPTY_TITLE,
    S_EMPTY_COPY,
    S_EMPTY_SUBDIR,
    S_EMPTY_VIDEO,
    S_EMPTY_RESCAN,
    // Thông báo ở thanh dưới
    S_ERROR_FMT,            // %s
    S_RESCANNED,            // %d
    S_SETTINGS_SAVED,
    S_GAME_EXITED,
    // Lỗi khi chạy game
    S_ERR_SYSLIB,
    S_ERR_OPEN_JAR,
    S_ERR_NO_MIDLET,
    S_ERR_VM,               // %s
    S_ERR_MIDLET,           // %s
    S_GAME_ENDED,
    S_GAME_CRASHED,         // %s
    S_APP_CRASHED_BEFORE,   // %s
    // Trong game
    S_EXIT_CONFIRM,
    S_SCREEN_INFO_FPS,      // %d %d %d
    S_HELP_DPAD,
    S_HELP_SOFT_RIGHT,
    S_HELP_SOFT_LEFT,
    S_HELP_EXIT,
    // Cài đặt
    S_SETTINGS,
    S_GAME_OPTIONS,
    S_FPS_LIMIT,
    S_FPS_HINT,
    S_UNLIMITED,
    S_DEFAULT_FMT,          // %s
    S_SCREEN_SIZE,
    S_SCREEN_SIZE_DEFAULT,
    S_SCREEN_SIZE_HINT_GAME,
    S_SCREEN_SIZE_HINT,
    S_AUTO,
    S_CUSTOM,
    S_ORIENTATION,
    S_ORIENT_HINT,
    S_PORTRAIT,
    S_LANDSCAPE,
    S_SQUARE,
    S_WIDTH,
    S_HEIGHT,
    S_SIZE_EDIT_HINT,
    S_KB_WIDTH,
    S_KB_HEIGHT,
    S_LANGUAGE,
    S_LANGUAGE_HINT,
    S_KEYMAP,
    S_KEYMAP_HINT,
    S_KEYBIND,
    S_KEYBIND_HINT,
    S_KEYBIND_HINT_APP,
    S_KEYBIND_DEFAULT,
    S_KEYBIND_CHANGED,      // %d
    S_KEYBIND_INHERIT,
    S_KEYBIND_OWN,          // %d
    S_KEYBIND_GLOBAL,
    S_KEYBIND_HINTS,
    S_KEYBIND_PHONE,
    S_BTN_LSTICK,
    S_BTN_RSTICK,
    S_BTN_RS_UP,
    S_BTN_RS_DOWN,
    S_BTN_RS_LEFT,
    S_BTN_RS_RIGHT,
    S_KEY_NONE,
    S_KEY_UP,
    S_KEY_DOWN,
    S_KEY_LEFT,
    S_KEY_RIGHT,
    S_KEY_CLEAR,
    S_SCALE_MODE,
    S_SCALE_HINT,
    S_SCALE_SMOOTH,
    S_SCALE_SHARP,
    S_SCALE_INTEGER,
    S_SHOW_HELP,
    S_SHOW_HELP_HINT,
    S_SHOW_FPS,
    S_SHOW_FPS_HINT,
    S_FONT_SCALE,
    S_FONT_SCALE_HINT,
    S_SMOOTH_TEXT,
    S_SMOOTH_TEXT_HINT,
    S_CHECK_UPDATE,
    S_CHECK_UPDATE_HINT,
    S_UPDATE_BADGE,         // %s
    S_UPDATE_TITLE,         // %s
    S_UPDATE_CURRENT,       // %s
    S_UPDATE_PROMPT_HINTS,
    S_UPDATE_FAILED,        // %s
    S_UPDATE_FAILED_HINTS,
    S_UPDATE_DOWNLOADING,   // %s
    S_UPDATE_CANCELLING,
    S_UPDATE_ETA,           // %d %02d
    S_UPDATE_KEEP_OPEN,
    S_UPDATE_CANCEL_HINT,
    S_UPDATE_DONE,          // %s
    S_UPDATE_DONE_INFO,
    S_UPDATE_DONE_HINTS,
    S_UPDATE_DONE_DESKTOP,
    S_UPDATE_CLOSE_HINT,
    S_VKB_BUBBLE,
    S_VKB_BUBBLE_HINT,
    // Phím ảo trên màn hình
    S_VPAD,
    S_VPAD_HINT,
    S_VPAD_HINT_APP,
    S_AOT,
    S_AOT_HINT,
    S_AOT_HINT_APP,
    S_AOT_UNSUPPORTED,
    S_VPAD_LAYOUT,
    S_VPAD_LAYOUT_HINT,
    S_VPAD_CUSTOMIZED,      // %s
    S_VPAD_STICK,
    S_VPAD_KEY_FMT,         // %s
    S_VPAD_PICK,
    S_VPAD_HIDE,
    S_VPAD_SHOW,
    S_VPAD_RADIUS,          // %d
    S_VPAD_OPACITY,         // %d
    S_VPAD_SNAP,            // %s
    S_VPAD_RESET,
    S_VPAD_RESET_CONFIRM,
    S_VPAD_DONE,
    S_VPAD_EDIT_HELP,
    S_SOUNDFONT,
    S_SOUNDFONT_HINT,
    S_SOUNDFONT_AUTO,       // %s
    S_SOUNDFONT_NONE,
    S_SOUNDFONT_BUILTIN,
    S_SYSTEM_FONT,
    S_SYSTEM_FONT_HINT,
    S_ON,
    S_OFF,
    S_SETTINGS_HINTS,
    S_VIDEO_TAG,
    S_VIDEO_HINTS,
    S_VIDEO_PAUSED,
    S_VIDEO_ENDED,
    S_VOLUME_FMT,           // %d
    S_ERR_VIDEO,
    S_ERR_NO_VIDEO_BUILD,
    S_VIDEO_NO_OPTIONS,
    S_LINK_OPENING,
    S_LINK_FAILED,
    S_BROWSER_NEEDS_APP,
    // Gửi game từ điện thoại (mã QR)
    S_EMPTY_UPLOAD,
    S_UPLOAD_TITLE,
    S_UPLOAD_STEP1,
    S_UPLOAD_STEP2,
    S_UPLOAD_STEP3,
    S_UPLOAD_WAITING,
    S_UPLOAD_RECEIVING,     // %s
    S_UPLOAD_RECEIVED,      // %d %s
    S_UPLOAD_NO_NET,
    S_UPLOAD_SERVER_FAILED, // %s
    S_UPLOAD_WRITE_FAILED,
    S_UPLOAD_HINTS,
    S_UPLOAD_DONE_STATUS,   // %d
    // Xoá file
    S_DELETE_GAME,
    S_DELETE_VIDEO,
    S_DELETE_KEEP_SAVE,
    S_DELETE_HINTS,
    S_DELETED,              // %s
    S_DELETE_FAILED,        // %s
    // Trang web trên điện thoại
    S_WEB_TITLE,
    S_WEB_INTRO,
    S_WEB_CHOOSE,
    S_WEB_WAITING,
    S_WEB_SENDING,
    S_WEB_DONE,
    S_WEB_FAILED,
    S_WEB_NETWORK,
    S_WEB_WRONG_TYPE,
    S_WEB_ALL_DONE,
    // Giao diện borealis
    S_ACT_OPEN,
    S_ACT_OPTIONS,
    S_ACT_RESCAN,
    S_ACT_UPLOAD,
    S_ACT_DELETE,
    S_ACT_UPDATE,
    S_ACT_CANCEL,
    S_ACT_DOWNLOAD,
    S_ACT_RETRY,
    S_ACT_RESTART,
    S_ACT_LATER,
    S_ACT_CLOSE,
    S_ACT_DEFAULT,
    S_ACT_RESET_ALL,
    S_SEC_SCREEN,
    S_SEC_CONTROLS,
    S_SEC_TEXT,
    S_SEC_SYSTEM,
    S_UPDATE_NEW,
    S_PICK_MIDLET_TITLE,
    S_SIZE_INPUT_HINT,
    S_KEYBIND_INFO,
    S_KEYBIND_INFO_APP,
    S_LANG_RESTART,
    S_COUNT,
} StrId;

void lang_set(Lang l);
Lang lang_get(void);
const char *lang_code(Lang l);          // "vi" / "en"
const char *lang_name(Lang l);          // tên hiển thị
Lang lang_from_code(const char *code);
const char *tr(StrId id);
