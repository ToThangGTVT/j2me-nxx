# Chuyển file nhị phân thành mảng C: cmake -DIN=... -DOUT=... -DNAME=... -P bin2c.cmake
file(READ "${IN}" hex HEX)
string(LENGTH "${hex}" hex_len)
math(EXPR size "${hex_len} / 2")
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," bytes "${hex}")
file(WRITE "${OUT}" "// Sinh tự động từ ${IN}\n#include <stddef.h>\nconst unsigned char ${NAME}[] = {${bytes}};\nconst size_t ${NAME}_size = ${size};\n")
