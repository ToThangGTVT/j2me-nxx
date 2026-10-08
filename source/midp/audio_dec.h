// Giải mã âm thanh nén cho AudioPlayer: AMR-NB / AMR-WB (.amr, 3GP), AAC (ADTS .aac, M4A / MP4 / 3GP)
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Dữ liệu có phải định dạng giải mã được ở đây không (nhìn phần đầu file)
bool audio_dec_probe(const uint8_t *data, size_t size);
// Giải mã cả file thành PCM 16-bit mono (malloc, người gọi free). rate: tần số mẫu gốc
bool audio_dec_decode(const uint8_t *data, size_t size, int16_t **pcm, size_t *frames, int *rate);
