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
    S_NO_JAR,               // %s
    S_FOLDER,               // %s
    S_NO_MIDLET,
    S_MENU_HINTS,
    S_PICK_MIDLET,
    // Thông báo ở thanh dưới
    S_ERROR_FMT,            // %s
    S_RESCANNED,            // %d
    S_DEMO_LIST_COPY,       // %s
    S_DEMO_LIST,
    S_SETTINGS_SAVED,
    S_GAME_EXITED,
    // Lỗi khi chạy game
    S_ERR_SYSLIB,
    S_ERR_OPEN_JAR,
    S_ERR_NO_MIDLET,
    S_ERR_VM,               // %s
    S_ERR_MIDLET,           // %s
    S_GAME_ENDED,
    // Trong game
    S_EXIT_CONFIRM,
    S_SCREEN_INFO_FPS,      // %d %d %d
    S_HELP_DPAD,
    S_HELP_SOFT_RIGHT,
    S_HELP_SOFT_LEFT,
    S_HELP_STICK_CLICK,
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
    S_SHOW_HELP,
    S_SHOW_HELP_HINT,
    S_ON,
    S_OFF,
    S_SETTINGS_HINTS,
    S_COUNT,
} StrId;

void lang_set(Lang l);
Lang lang_get(void);
const char *lang_code(Lang l);          // "vi" / "en"
const char *lang_name(Lang l);          // tên hiển thị
Lang lang_from_code(const char *code);
const char *tr(StrId id);
