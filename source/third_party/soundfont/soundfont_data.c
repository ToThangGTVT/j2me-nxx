// Nhúng SoundFont có sẵn vào binary bằng .incbin (nhanh hơn nhiều so với sinh mảng C cho file vài MB).
// SOUNDFONT_FILE: đường dẫn tuyệt đối, do CMakeLists.txt truyền vào.
#ifdef __APPLE__
#define SF_SECTION ".const_data\n"
#define SF_SYM(n) "_" #n
#else
#define SF_SECTION ".section .rodata\n"
#define SF_SYM(n) #n
#endif

__asm__(SF_SECTION
        ".global " SF_SYM(builtin_sf2) "\n"
        ".balign 16\n"
        SF_SYM(builtin_sf2) ":\n"
        ".incbin \"" SOUNDFONT_FILE "\"\n"
        ".global " SF_SYM(builtin_sf2_end) "\n"
        SF_SYM(builtin_sf2_end) ":\n"
        ".byte 0\n");
