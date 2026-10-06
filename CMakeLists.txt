# J2ME-NX
#
# Switch (mặc định): cần CMake option
#   -DCMAKE_TOOLCHAIN_FILE=/opt/devkitpro/cmake/Switch.cmake
#   và biến môi trường DEVKITPRO=/opt/devkitpro
#
# Desktop (test nhanh trên Mac): CMake option -DJ2ME_NX_DESKTOP=ON
#   cần: brew install sdl2 sdl2_ttf libpng pkgconf
#
# Cả hai đều cần JDK (javac, jar) để biên dịch thư viện Java trong javalib/.
cmake_minimum_required(VERSION 3.13)

option(J2ME_NX_DESKTOP "Build ban desktop thay vi Switch" OFF)

if(NOT J2ME_NX_DESKTOP AND NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{DEVKITPRO}/cmake/Switch.cmake")
    if(NOT EXISTS "${CMAKE_TOOLCHAIN_FILE}")
        set(CMAKE_TOOLCHAIN_FILE "/opt/devkitpro/cmake/Switch.cmake")
    endif()
endif()

project(j2me-nx VERSION 0.3.1 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(APP_TITLE  "J2ME-NX")
set(APP_AUTHOR "toquangthang")

# ---------------------------------------------------------------------------
# Thư viện CLDC/MIDP viết bằng Java -> classlib.jar -> nhúng vào binary

find_program(JAVAC_EXE javac HINTS "$ENV{JAVA_HOME}/bin" PATHS /usr/bin /usr/local/bin /opt/homebrew/bin
             NO_CMAKE_FIND_ROOT_PATH)
find_program(JAR_EXE jar HINTS "$ENV{JAVA_HOME}/bin" PATHS /usr/bin /usr/local/bin /opt/homebrew/bin
             NO_CMAKE_FIND_ROOT_PATH)
if(NOT JAVAC_EXE OR NOT JAR_EXE)
    message(FATAL_ERROR "Can JDK (javac, jar) de bien dich thu vien Java trong javalib/")
endif()

file(GLOB_RECURSE JAVALIB_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/javalib/src/*.java")
set(CLASSLIB_DIR "${CMAKE_BINARY_DIR}/classlib")
set(CLASSLIB_JAR "${CMAKE_BINARY_DIR}/classlib.jar")
set(CLASSLIB_C   "${CMAKE_BINARY_DIR}/classlib_data.c")

add_custom_command(
    OUTPUT "${CLASSLIB_JAR}"
    COMMAND "${CMAKE_COMMAND}" -E rm -rf "${CLASSLIB_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CLASSLIB_DIR}"
    # bootclasspath trỏ vào thư mục rỗng: tự biên dịch java.lang riêng
    COMMAND "${JAVAC_EXE}" -source 8 -target 8 -bootclasspath "${CLASSLIB_DIR}" -Xlint:-options -nowarn
            -implicit:none -encoding UTF-8 -g:source,lines -d "${CLASSLIB_DIR}" ${JAVALIB_SOURCES}
    COMMAND "${JAR_EXE}" cf "${CLASSLIB_JAR}" -C "${CLASSLIB_DIR}" .
    DEPENDS ${JAVALIB_SOURCES}
    COMMENT "Bien dich thu vien Java (CLDC/MIDP)"
    VERBATIM)

add_custom_command(
    OUTPUT "${CLASSLIB_C}"
    COMMAND "${CMAKE_COMMAND}" -DIN=${CLASSLIB_JAR} -DOUT=${CLASSLIB_C} -DNAME=classlib_jar
            -P "${CMAKE_SOURCE_DIR}/cmake/bin2c.cmake"
    DEPENDS "${CLASSLIB_JAR}" "${CMAKE_SOURCE_DIR}/cmake/bin2c.cmake"
    COMMENT "Nhung classlib.jar"
    VERBATIM)

# Font giao diện (Google Sans, OFL) nhúng vào binary
set(UI_FONT   "${CMAKE_SOURCE_DIR}/source/third_party/fonts/GoogleSans-Regular.ttf")
set(UI_FONT_C "${CMAKE_BINARY_DIR}/ui_font_data.c")
add_custom_command(
    OUTPUT "${UI_FONT_C}"
    COMMAND "${CMAKE_COMMAND}" -DIN=${UI_FONT} -DOUT=${UI_FONT_C} -DNAME=ui_font_ttf
            -P "${CMAKE_SOURCE_DIR}/cmake/bin2c.cmake"
    DEPENDS "${UI_FONT}" "${CMAKE_SOURCE_DIR}/cmake/bin2c.cmake"
    COMMENT "Nhung font giao dien"
    VERBATIM)

# ---------------------------------------------------------------------------

add_executable(j2me-nx
    source/main.c
    source/platform.c
    source/gfx.c
    source/input.c
    source/game_list.c
    source/menu.c
    source/emu.c
    source/manifest.c
    source/lang.c
    source/keymap.c
    source/settings.c
    source/settings_screen.c
    source/vm/class.c
    source/vm/heap.c
    source/vm/interp.c
    source/vm/jstring.c
    source/vm/native.c
    source/vm/native_lang.c
    source/vm/thread.c
    source/vm/vm.c
    source/vm/zip.c
    source/midp/midp.c
    source/midp/graphics.c
    source/midp/net.c
    source/midp/tls.c
    source/midp/audio.c
    source/midp/fileio.c
    source/midp/m3g.c
    source/third_party/stb_image.c
    source/third_party/dr_mp3.c
    "${CLASSLIB_C}"
    "${UI_FONT_C}"
)

target_compile_options(j2me-nx PRIVATE -Wall)
target_compile_definitions(j2me-nx PRIVATE APP_VERSION_STR="${PROJECT_VERSION}")
if(NOT CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "Debug")
    # Interpreter rất chậm khi không tối ưu
    target_compile_options(j2me-nx PRIVATE -O2)
endif()

# SDL2 + SDL2_ttf + libpng + zlib qua pkg-config. Trên Switch, Switch.cmake trỏ pkg-config
# vào portlibs của devkitPro và luôn trả về cờ link tĩnh.
find_package(PkgConfig REQUIRED)
pkg_check_modules(DEPS sdl2 SDL2_ttf libpng zlib)
if(NOT DEPS_FOUND)
    if(NINTENDO_SWITCH)
        message(FATAL_ERROR "Thieu thu vien cho Switch. Chay: sudo dkp-pacman -S switch-sdl2 switch-sdl2_ttf switch-libpng switch-zlib")
    else()
        message(FATAL_ERROR "Thieu thu vien tren may. Chay: brew install sdl2 sdl2_ttf libpng pkgconf")
    endif()
endif()

# mbedTLS cho https:// / ssl:// (không bắt buộc: thiếu thì build không có TLS)
if(NINTENDO_SWITCH)
    find_library(MBEDTLS_LIB mbedtls)
    find_library(MBEDX509_LIB mbedx509)
    find_library(MBEDCRYPTO_LIB mbedcrypto)
    if(MBEDTLS_LIB AND MBEDX509_LIB AND MBEDCRYPTO_LIB)
        set(TLS_LIBS ${MBEDTLS_LIB} ${MBEDX509_LIB} ${MBEDCRYPTO_LIB})
    endif()
else()
    pkg_check_modules(MBEDTLS mbedtls mbedx509 mbedcrypto)
    if(MBEDTLS_FOUND)
        target_include_directories(j2me-nx PRIVATE ${MBEDTLS_INCLUDE_DIRS})
        set(TLS_LIBS ${MBEDTLS_LINK_LIBRARIES})
    endif()
endif()
if(TLS_LIBS)
    message(STATUS "HTTPS: dung mbedTLS")
else()
    message(WARNING "Khong tim thay mbedTLS: build khong co HTTPS. Switch: sudo dkp-pacman -S switch-mbedtls, Mac: brew install mbedtls")
    target_compile_definitions(j2me-nx PRIVATE J2ME_NX_NO_TLS)
endif()

target_include_directories(j2me-nx PRIVATE ${DEPS_INCLUDE_DIRS})
target_compile_options(j2me-nx PRIVATE ${DEPS_CFLAGS_OTHER})

if(NINTENDO_SWITCH)
    # mbedTLS phải đứng trước libnx trong lệnh link
    target_link_libraries(j2me-nx PRIVATE ${TLS_LIBS} ${DEPS_LDFLAGS})

    nx_generate_nacp(j2me-nx.nacp
        NAME    "${APP_TITLE}"
        AUTHOR  "${APP_AUTHOR}"
        VERSION "${PROJECT_VERSION}"
    )

    nx_create_nro(j2me-nx
        NACP j2me-nx.nacp
    )
else()
    target_link_libraries(j2me-nx PRIVATE ${TLS_LIBS} ${DEPS_LINK_LIBRARIES} m)
endif()
