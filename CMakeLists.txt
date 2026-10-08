# J2ME-NXX
#
# Switch (mặc định): cần CMake option
#   -DCMAKE_TOOLCHAIN_FILE=/opt/devkitpro/cmake/Switch.cmake
#   và biến môi trường DEVKITPRO=/opt/devkitpro
#
# Desktop (test nhanh trên Mac): CMake option -DJ2ME_NX_DESKTOP=ON
#   cần: brew install sdl2 sdl2_ttf libpng pkgconf
#
# Cả hai đều cần JDK (javac, jar) để biên dịch thư viện Java trong javalib/.
# Giao diện dùng borealis (submodule library/borealis: git submodule update --init),
# chạy trên SDL2 + OpenGL ở cả Switch và desktop.
cmake_minimum_required(VERSION 3.13)

option(J2ME_NX_DESKTOP "Build ban desktop thay vi Switch" OFF)

if(NOT J2ME_NX_DESKTOP AND NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{DEVKITPRO}/cmake/Switch.cmake")
    if(NOT EXISTS "${CMAKE_TOOLCHAIN_FILE}")
        set(CMAKE_TOOLCHAIN_FILE "/opt/devkitpro/cmake/Switch.cmake")
    endif()
endif()

project(j2me-nxx VERSION 0.8.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(APP_TITLE  "J2ME-NXX")
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

# ---------------------------------------------------------------------------
# borealis: khung giao diện. Switch và desktop đều dùng SDL2 (cửa sổ, OpenGL) + NanoVG.
# Tài nguyên (i18n, icon) ở resources/: Switch nhúng vào romfs của .nro, desktop chép cạnh file chạy.

set(BOREALIS_DIR "${CMAKE_SOURCE_DIR}/library/borealis/library")
if(NOT EXISTS "${BOREALIS_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "Thieu borealis. Chay: git submodule update --init library/borealis")
endif()
if(NINTENDO_SWITCH)
    set(PLATFORM_SWITCH ON)
else()
    set(PLATFORM_DESKTOP ON)
    set(USE_SYSTEM_SDL2 ON)
    if(APPLE)
        # borealis đặt mục tiêu macOS 10.15 khi build cả gói; ở đây theo máy đang build
        set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
    endif()
endif()
set(USE_SDL2 ON)
set(BRLS_RESOURCES_DIR "${CMAKE_BINARY_DIR}")
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
include("${BOREALIS_DIR}/cmake/commonOption.cmake")
add_subdirectory("${BOREALIS_DIR}" borealis EXCLUDE_FROM_ALL)
if(PLATFORM_SWITCH)
    # Bản Switch của borealis chọn SDLVideoContext theo __SDL2__ (CMake của borealis chưa tự thêm)
    target_compile_definitions(borealis PUBLIC __SDL2__)
    # borealis include <SDL2/SDL.h>; CMake của nó trông vào cờ của toolchain riêng (không dùng ở đây).
    # Để sau cùng (-isystem): portlibs có thể có tinyxml2 / fmt khác phiên bản với bản đi kèm borealis
    target_include_directories(borealis SYSTEM AFTER PUBLIC "${DEVKITPRO}/portlibs/switch/include")
endif()
if(NOT CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(borealis PRIVATE -O2)
endif()

set(RESOURCES_DIR "${CMAKE_SOURCE_DIR}/resources")

# ---------------------------------------------------------------------------
# Giải mã âm thanh nén cho AudioPlayer: AAC / M4A (faad2, GPL v2+), AMR-NB / AMR-WB (opencore-amr,
# Apache 2.0). File MP4 / M4A / 3GP bóc tách bằng minimp4.h (CC0). Mã nguồn nằm sẵn trong source/third_party.

set(FAAD_DIR "${CMAKE_SOURCE_DIR}/source/third_party/faad2")
file(GLOB FAAD_SOURCES "${FAAD_DIR}/libfaad/*.c")
add_library(faad STATIC ${FAAD_SOURCES})
target_include_directories(faad PUBLIC "${FAAD_DIR}/include" PRIVATE "${FAAD_DIR}/libfaad")
target_compile_definitions(faad PRIVATE HAVE_INTTYPES_H=1 HAVE_MEMCPY=1 HAVE_STRING_H=1 HAVE_STRINGS_H=1
    HAVE_SYS_STAT_H=1 HAVE_SYS_TYPES_H=1 HAVE_LRINTF=1 PACKAGE_VERSION="2.11.4")
target_compile_options(faad PRIVATE -O2 -w)

set(AMRNB_SOURCES
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/amrnb/wrapper.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/dec/src/agc.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/dec/src/amrdecode.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/dec/src/a_refl.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/dec/src/b_cn_cod.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/dec/src/bgnscd.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/dec/src/c_g_aver.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/dec/src/d1035pf.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/dec/src/d2_11pf.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/dec/src/d2_9pf.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/dec/src/d3_14pf.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/dec/src/d4_17pf.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/dec/src/d8_31pf.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/dec/src/dec_amr.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/dec/src/dec_gain.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/dec/src/dec_input_format_tab.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/dec/src/dec_lag3.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/dec/src/dec_lag6.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/dec/src/d_gain_c.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/dec/src/d_gain_p.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/dec/src/d_plsf_3.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/dec/src/d_plsf_5.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/dec/src/d_plsf.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/dec/src/dtx_dec.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/dec/src/ec_gains.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/dec/src/ex_ctrl.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/dec/src/if2_to_ets.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/dec/src/int_lsf.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/dec/src/lsp_avg.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/dec/src/ph_disp.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/dec/src/post_pro.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/dec/src/preemph.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/dec/src/pstfilt.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/dec/src/qgain475_tab.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/dec/src/sp_dec.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/dec/src/wmf_to_ets.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/common/src/add.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/common/src/az_lsp.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/common/src/bitno_tab.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/common/src/bitreorder_tab.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/common/src/c2_9pf_tab.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/common/src/div_s.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/common/src/extract_h.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/common/src/extract_l.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/common/src/gains_tbl.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/common/src/gc_pred.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/common/src/get_const_tbls.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/common/src/gmed_n.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/common/src/gray_tbl.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/common/src/grid_tbl.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/common/src/int_lpc.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/common/src/inv_sqrt.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/common/src/inv_sqrt_tbl.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/common/src/l_deposit_h.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/common/src/l_deposit_l.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/common/src/log2.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/common/src/log2_norm.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/common/src/log2_tbl.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/common/src/lsfwt.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/common/src/l_shr_r.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/common/src/lsp_az.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/common/src/lsp.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/common/src/lsp_lsf.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/common/src/lsp_lsf_tbl.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/common/src/lsp_tab.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/common/src/mult_r.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/common/src/negate.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/common/src/norm_l.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/common/src/norm_s.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/common/src/overflow_tbl.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/common/src/ph_disp_tab.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/common/src/pow2.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/common/src/pow2_tbl.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/common/src/pred_lt.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/common/src/q_plsf_3.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/common/src/q_plsf_3_tbl.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/common/src/q_plsf_5.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/common/src/q_plsf_5_tbl.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/common/src/q_plsf.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/common/src/qua_gain_tbl.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/common/src/reorder.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/common/src/residu.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/common/src/round.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/common/src/set_zero.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/common/src/shr.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/common/src/shr_r.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/common/src/sqrt_l.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/common/src/sqrt_l_tbl.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/common/src/sub.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/common/src/syn_filt.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/common/src/weight_a.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_nb/common/src/window_tab.cpp
)
set(AMRWB_SOURCES
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/amrwb/wrapper.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_wb/dec/src/agc2_amr_wb.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_wb/dec/src/band_pass_6k_7k.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_wb/dec/src/dec_acelp_2p_in_64.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_wb/dec/src/dec_acelp_4p_in_64.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_wb/dec/src/dec_alg_codebook.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_wb/dec/src/dec_gain2_amr_wb.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_wb/dec/src/deemphasis_32.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_wb/dec/src/dtx_decoder_amr_wb.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_wb/dec/src/get_amr_wb_bits.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_wb/dec/src/highpass_400hz_at_12k8.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_wb/dec/src/highpass_50hz_at_12k8.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_wb/dec/src/homing_amr_wb_dec.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_wb/dec/src/interpolate_isp.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_wb/dec/src/isf_extrapolation.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_wb/dec/src/isp_az.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_wb/dec/src/isp_isf.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_wb/dec/src/lagconceal.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_wb/dec/src/low_pass_filt_7k.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_wb/dec/src/median5.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_wb/dec/src/mime_io.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_wb/dec/src/noise_gen_amrwb.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_wb/dec/src/normalize_amr_wb.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_wb/dec/src/oversamp_12k8_to_16k.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_wb/dec/src/phase_dispersion.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_wb/dec/src/pit_shrp.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_wb/dec/src/pred_lt4.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_wb/dec/src/preemph_amrwb_dec.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_wb/dec/src/pvamrwbdecoder.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_wb/dec/src/pvamrwb_math_op.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_wb/dec/src/q_gain2_tab.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_wb/dec/src/qisf_ns.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_wb/dec/src/qisf_ns_tab.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_wb/dec/src/qpisf_2s.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_wb/dec/src/qpisf_2s_tab.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_wb/dec/src/scale_signal.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_wb/dec/src/synthesis_amr_wb.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_wb/dec/src/voice_factor.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_wb/dec/src/wb_syn_filt.cpp
    ${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr/opencore/codecs_v2/audio/gsm_amr/amr_wb/dec/src/weight_amrwb_lpc.cpp
)
set(AMR_DIR "${CMAKE_SOURCE_DIR}/source/third_party/opencore-amr")
set(GSM_AMR "${AMR_DIR}/opencore/codecs_v2/audio/gsm_amr")
add_library(amrnb STATIC ${AMRNB_SOURCES})
target_include_directories(amrnb PUBLIC "${AMR_DIR}/amrnb" PRIVATE "${AMR_DIR}/oscl" "${GSM_AMR}/amr_nb/dec/src"
    "${GSM_AMR}/amr_nb/dec/include" "${GSM_AMR}/amr_nb/common/include" "${GSM_AMR}/common/dec/include"
    "${GSM_AMR}/amr_nb/enc/src")
target_compile_definitions(amrnb PRIVATE DISABLE_AMRNB_ENCODER)
add_library(amrwb STATIC ${AMRWB_SOURCES})
target_include_directories(amrwb PUBLIC "${AMR_DIR}/amrwb" PRIVATE "${AMR_DIR}/oscl" "${GSM_AMR}/amr_wb/dec/src"
    "${GSM_AMR}/amr_wb/dec/include" "${GSM_AMR}/common/dec/include")
foreach(lib amrnb amrwb)
    # Mã C++ cũ (dùng register): không biên dịch theo C++17 được
    set_target_properties(${lib} PROPERTIES CXX_STANDARD 11)
    target_compile_options(${lib} PRIVATE -O2 -w)
endforeach()

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

# SoundFont General MIDI có sẵn (TimGM6mb, GPL v2) nhúng vào binary bằng .incbin
set(SOUNDFONT_SF2 "${CMAKE_SOURCE_DIR}/source/third_party/soundfont/TimGM6mb.sf2")
set(SOUNDFONT_C   "${CMAKE_SOURCE_DIR}/source/third_party/soundfont/soundfont_data.c")
set_source_files_properties("${SOUNDFONT_C}" PROPERTIES
    COMPILE_DEFINITIONS "SOUNDFONT_FILE=\"${SOUNDFONT_SF2}\""
    OBJECT_DEPENDS "${SOUNDFONT_SF2}")

# ---------------------------------------------------------------------------

add_executable(j2me-nxx
    source/ui/app.cpp
    source/ui/common.cpp
    source/ui/screen_activity.cpp
    source/ui/main_activity.cpp
    source/ui/settings_activity.cpp
    source/ui/keybind_activity.cpp
    source/ui/lang_activity.cpp
    source/ui/update_activity.cpp
    source/ui/upload_activity.cpp
    source/platform.c
    source/gfx.c
    source/input.c
    source/game_list.c
    source/emu.c
    source/manifest.c
    source/lang.c
    source/lang_vi.c
    source/lang_en.c
    source/keymap.c
    source/keybind.c
    source/settings.c
    source/qr.c
    source/upload.c
    source/vkb.c
    source/vpad.c
    source/vpad_screen.c
    source/http.c
    source/crash.c
    source/update.c
    source/vm/aot_arm64.c
    source/vm/aot_mem.c
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
    source/midp/audio_dec.c
    source/midp/fileio.c
    source/midp/m3g.c
    source/third_party/dr_mp3.c
    "${SOUNDFONT_C}"
    "${CLASSLIB_C}"
    "${UI_FONT_C}"
)

target_compile_options(j2me-nxx PRIVATE -Wall)
target_include_directories(j2me-nxx PRIVATE source)
target_link_libraries(j2me-nxx PRIVATE borealis faad amrnb amrwb)
target_compile_definitions(j2me-nxx PRIVATE APP_VERSION_STR="${PROJECT_VERSION}")
if(NOT CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "Debug")
    # Interpreter rất chậm khi không tối ưu
    target_compile_options(j2me-nxx PRIVATE -O2)
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
        target_include_directories(j2me-nxx PRIVATE ${MBEDTLS_INCLUDE_DIRS})
        set(TLS_LIBS ${MBEDTLS_LINK_LIBRARIES})
    endif()
endif()
if(TLS_LIBS)
    message(STATUS "HTTPS: dung mbedTLS")
else()
    message(WARNING "Khong tim thay mbedTLS: build khong co HTTPS. Switch: sudo dkp-pacman -S switch-mbedtls, Mac: brew install mbedtls")
    target_compile_definitions(j2me-nxx PRIVATE J2ME_NX_NO_TLS)
endif()

target_include_directories(j2me-nxx PRIVATE ${DEPS_INCLUDE_DIRS})
target_compile_options(j2me-nxx PRIVATE ${DEPS_CFLAGS_OTHER})

if(NINTENDO_SWITCH)
    # mbedTLS phải đứng trước libnx trong lệnh link
    target_link_libraries(j2me-nxx PRIVATE ${TLS_LIBS} ${DEPS_LDFLAGS})

    nx_generate_nacp(j2me-nxx.nacp
        NAME    "${APP_TITLE}"
        AUTHOR  "${APP_AUTHOR}"
        VERSION "${PROJECT_VERSION}"
    )

    nx_create_nro(j2me-nxx
        NACP j2me-nxx.nacp
        ROMFS "${RESOURCES_DIR}"
    )
else()
    target_link_libraries(j2me-nxx PRIVATE ${TLS_LIBS} ${DEPS_LINK_LIBRARIES} m)
    # Tài nguyên của borealis cạnh file chạy (BRLS_RESOURCES = <thư mục build>/resources/)
    add_custom_command(TARGET j2me-nxx POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_directory "${RESOURCES_DIR}" "${CMAKE_BINARY_DIR}/resources"
        COMMENT "Chep tai nguyen giao dien")
endif()

# ---------------------------------------------------------------------------
# vmbench: đo tốc độ trình thông dịch trên desktop (không cần cửa sổ)
#   cmake --build <dir> --target vmbench && <dir>/vmbench <dir>/bench.jar
if(NOT NINTENDO_SWITCH)
    file(GLOB VMBENCH_JAVA CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/tests/vmbench/src/*.java")
    set(VMBENCH_DIR "${CMAKE_BINARY_DIR}/vmbench_classes")
    set(VMBENCH_JAR "${CMAKE_BINARY_DIR}/bench.jar")
    add_custom_command(
        OUTPUT "${VMBENCH_JAR}"
        COMMAND "${CMAKE_COMMAND}" -E rm -rf "${VMBENCH_DIR}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${VMBENCH_DIR}"
        COMMAND "${JAVAC_EXE}" -source 8 -target 8 -bootclasspath "${CLASSLIB_DIR}" -Xlint:-options -nowarn
                -encoding UTF-8 -g:source,lines -d "${VMBENCH_DIR}" ${VMBENCH_JAVA}
        COMMAND "${JAR_EXE}" cf "${VMBENCH_JAR}" -C "${VMBENCH_DIR}" .
        DEPENDS ${VMBENCH_JAVA} "${CLASSLIB_JAR}"
        COMMENT "Bien dich bench.jar"
        VERBATIM)
    add_custom_target(vmbench_jar DEPENDS "${VMBENCH_JAR}")

    add_executable(vmbench EXCLUDE_FROM_ALL
        tests/vmbench/vmbench.c
        source/vm/aot_arm64.c
        source/vm/aot_mem.c
        source/vm/class.c
        source/vm/heap.c
        source/vm/interp.c
        source/vm/jstring.c
        source/vm/native.c
        source/vm/native_lang.c
        source/vm/thread.c
        source/vm/vm.c
        source/vm/zip.c
        "${CLASSLIB_C}")
    add_dependencies(vmbench vmbench_jar)
    target_include_directories(vmbench PRIVATE source)
    target_compile_options(vmbench PRIVATE -O2 -Wall)
    target_link_libraries(vmbench PRIVATE ${DEPS_LINK_LIBRARIES} m)
endif()
