#include "qr.h"

#include <stdbool.h>
#include <string.h>

// Theo ISO/IEC 18004, cách làm giống thư viện QR Code generator của Nayuki
#define MAX_VERSION 10

// Mức M, phiên bản 1-10: số byte sửa lỗi mỗi khối và số khối
static const int ecc_per_block[MAX_VERSION + 1] = { 0, 10, 16, 26, 18, 24, 16, 18, 22, 22, 26 };
static const int num_blocks[MAX_VERSION + 1] = { 0, 1, 1, 1, 2, 2, 4, 4, 4, 5, 5 };

typedef struct {
    int size;
    uint8_t *mod;           // ô tối
    uint8_t func[QR_MAX_SIZE * QR_MAX_SIZE];   // ô thuộc hoa văn cố định (không chứa dữ liệu)
} Qr;

static void set_func(Qr *q, int x, int y, bool dark) {
    q->mod[y * q->size + x] = dark;
    q->func[y * q->size + x] = 1;
}

// Số ô dành cho dữ liệu + sửa lỗi (trừ hoa văn cố định, thông tin định dạng / phiên bản)
static int raw_data_modules(int ver) {
    int r = (16 * ver + 128) * ver + 64;
    if (ver >= 2) {
        int n = ver / 7 + 2;
        r -= (25 * n - 10) * n - 55;
        if (ver >= 7)
            r -= 36;
    }
    return r;
}

static int data_codewords(int ver) {
    return raw_data_modules(ver) / 8 - ecc_per_block[ver] * num_blocks[ver];
}

// ---- Reed-Solomon trên GF(256), đa thức 0x11D ----

static uint8_t gf_mul(uint8_t x, uint8_t y) {
    int z = 0;
    for (int i = 7; i >= 0; i--) {
        z = (z << 1) ^ ((z >> 7) * 0x11D);
        z ^= ((y >> i) & 1) * x;
    }
    return (uint8_t)z;
}

static void rs_divisor(int degree, uint8_t *out) {
    memset(out, 0, (size_t)degree);
    out[degree - 1] = 1;
    uint8_t root = 1;
    for (int i = 0; i < degree; i++) {
        for (int j = 0; j < degree; j++) {
            out[j] = gf_mul(out[j], root);
            if (j + 1 < degree)
                out[j] ^= out[j + 1];
        }
        root = gf_mul(root, 0x02);
    }
}

static void rs_remainder(const uint8_t *data, int len, const uint8_t *div, int degree, uint8_t *out) {
    memset(out, 0, (size_t)degree);
    for (int i = 0; i < len; i++) {
        uint8_t factor = data[i] ^ out[0];
        memmove(out, out + 1, (size_t)degree - 1);
        out[degree - 1] = 0;
        for (int j = 0; j < degree; j++)
            out[j] ^= gf_mul(div[j], factor);
    }
}

// ---- Hoa văn cố định ----

static void draw_finder(Qr *q, int cx, int cy) {
    for (int dy = -4; dy <= 4; dy++) {
        for (int dx = -4; dx <= 4; dx++) {
            int x = cx + dx, y = cy + dy;
            if (x < 0 || y < 0 || x >= q->size || y >= q->size)
                continue;
            int adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy;
            int d = adx > ady ? adx : ady;
            set_func(q, x, y, d != 2 && d != 4);
        }
    }
}

static void draw_alignment(Qr *q, int cx, int cy) {
    for (int dy = -2; dy <= 2; dy++) {
        for (int dx = -2; dx <= 2; dx++) {
            int adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy;
            set_func(q, cx + dx, cy + dy, (adx > ady ? adx : ady) != 1);
        }
    }
}

static void draw_format(Qr *q, int mask) {
    int data = (0 << 3) | mask;     // mức M = 00
    int rem = data;
    for (int i = 0; i < 10; i++)
        rem = (rem << 1) ^ ((rem >> 9) * 0x537);
    int bits = ((data << 10) | rem) ^ 0x5412;
    int n = q->size;
#define BIT(i) (((bits) >> (i)) & 1)
    for (int i = 0; i <= 5; i++)
        set_func(q, 8, i, BIT(i));
    set_func(q, 8, 7, BIT(6));
    set_func(q, 8, 8, BIT(7));
    set_func(q, 7, 8, BIT(8));
    for (int i = 9; i < 15; i++)
        set_func(q, 14 - i, 8, BIT(i));
    for (int i = 0; i < 8; i++)
        set_func(q, n - 1 - i, 8, BIT(i));
    for (int i = 8; i < 15; i++)
        set_func(q, 8, n - 15 + i, BIT(i));
#undef BIT
    set_func(q, 8, n - 8, true);    // ô tối cố định
}

static void draw_version(Qr *q, int ver) {
    if (ver < 7)
        return;
    int rem = ver;
    for (int i = 0; i < 12; i++)
        rem = (rem << 1) ^ ((rem >> 11) * 0x1F25);
    long bits = ((long)ver << 12) | rem;
    for (int i = 0; i < 18; i++) {
        bool bit = (bits >> i) & 1;
        int a = q->size - 11 + i % 3, b = i / 3;
        set_func(q, a, b, bit);
        set_func(q, b, a, bit);
    }
}

static void draw_function_patterns(Qr *q, int ver) {
    int n = q->size;
    for (int i = 0; i < n; i++) {
        set_func(q, 6, i, i % 2 == 0);
        set_func(q, i, 6, i % 2 == 0);
    }
    draw_finder(q, 3, 3);
    draw_finder(q, n - 4, 3);
    draw_finder(q, 3, n - 4);

    if (ver >= 2) {
        int count = ver / 7 + 2;
        int step = (ver * 4 + count * 2 + 1) / (count * 2 - 2) * 2;
        int pos[7];
        pos[0] = 6;
        for (int i = count - 1, p = n - 7; i >= 1; i--, p -= step)
            pos[i] = p;
        for (int i = 0; i < count; i++) {
            for (int j = 0; j < count; j++) {
                if ((i == 0 && j == 0) || (i == 0 && j == count - 1) || (i == count - 1 && j == 0))
                    continue;
                draw_alignment(q, pos[i], pos[j]);
            }
        }
    }
    draw_format(q, 0);  // chỗ giữ trước, vẽ lại khi đã chọn mask
    draw_version(q, ver);
}

// Đặt bit dữ liệu theo đường zigzag 2 cột, từ góc dưới phải
static void draw_codewords(Qr *q, const uint8_t *data, int len) {
    int n = q->size;
    int i = 0;
    for (int right = n - 1; right >= 1; right -= 2) {
        if (right == 6)
            right = 5;
        for (int vert = 0; vert < n; vert++) {
            for (int j = 0; j < 2; j++) {
                int x = right - j;
                bool upward = ((right + 1) & 2) == 0;
                int y = upward ? n - 1 - vert : vert;
                if (!q->func[y * n + x] && i < len * 8) {
                    q->mod[y * n + x] = (data[i >> 3] >> (7 - (i & 7))) & 1;
                    i++;
                }
            }
        }
    }
}

static void apply_mask(Qr *q, int mask) {
    int n = q->size;
    for (int y = 0; y < n; y++) {
        for (int x = 0; x < n; x++) {
            bool inv;
            switch (mask) {
            case 0: inv = (x + y) % 2 == 0; break;
            case 1: inv = y % 2 == 0; break;
            case 2: inv = x % 3 == 0; break;
            case 3: inv = (x + y) % 3 == 0; break;
            case 4: inv = (x / 3 + y / 2) % 2 == 0; break;
            case 5: inv = x * y % 2 + x * y % 3 == 0; break;
            case 6: inv = (x * y % 2 + x * y % 3) % 2 == 0; break;
            default: inv = ((x + y) % 2 + x * y % 3) % 2 == 0; break;
            }
            if (inv && !q->func[y * n + x])
                q->mod[y * n + x] ^= 1;
        }
    }
}

// Điểm phạt để chọn mask dễ quét nhất: dải cùng màu dài, khối 2x2, mẫu giống ô định vị, lệch tỉ lệ sáng/tối
static long penalty(const Qr *q) {
    int n = q->size;
    long p = 0;
    for (int pass = 0; pass < 2; pass++) {
        for (int a = 0; a < n; a++) {
            int run = 0, prev = -1;
            for (int b = 0; b < n; b++) {
                int v = pass ? q->mod[b * n + a] : q->mod[a * n + b];
                if (v == prev) {
                    run++;
                    if (run == 5)
                        p += 3;
                    else if (run > 5)
                        p++;
                } else {
                    prev = v;
                    run = 1;
                }
            }
            // 1:1:3:1:1 có 4 ô sáng một bên
            static const uint8_t pat[2][11] = { { 1, 0, 1, 1, 1, 0, 1, 0, 0, 0, 0 },
                                                { 0, 0, 0, 0, 1, 0, 1, 1, 1, 0, 1 } };
            for (int b = 0; b + 11 <= n; b++) {
                for (int k = 0; k < 2; k++) {
                    int m = 0;
                    while (m < 11 && (pass ? q->mod[(b + m) * n + a] : q->mod[a * n + b + m]) == pat[k][m])
                        m++;
                    if (m == 11)
                        p += 40;
                }
            }
        }
    }
    int dark = 0;
    for (int y = 0; y < n; y++) {
        for (int x = 0; x < n; x++) {
            int v = q->mod[y * n + x];
            dark += v;
            if (x + 1 < n && y + 1 < n && v == q->mod[y * n + x + 1] && v == q->mod[(y + 1) * n + x] &&
                v == q->mod[(y + 1) * n + x + 1])
                p += 3;
        }
    }
    int total = n * n;
    int k = ((dark * 20 - total * 10) < 0 ? total * 10 - dark * 20 : dark * 20 - total * 10);
    k = (k + total - 1) / total - 1;
    if (k > 0)
        p += k * 10;
    return p;
}

int qr_encode(const char *text, uint8_t *modules) {
    int len = (int)strlen(text);
    int ver = 1;
    for (; ver <= MAX_VERSION; ver++) {
        int count_bits = ver <= 9 ? 8 : 16;
        if (4 + count_bits + len * 8 <= data_codewords(ver) * 8)
            break;
    }
    if (ver > MAX_VERSION)
        return 0;

    // Chuỗi bit: chế độ byte (0100), độ dài, dữ liệu, kết thúc, đệm 0xEC 0x11
    int cap = data_codewords(ver);
    uint8_t data[400] = { 0 };
    int bit = 0;
#define PUT(val, nbits)                                                     \
    for (int b_ = (nbits) - 1; b_ >= 0; b_--, bit++)                        \
        if (((val) >> b_) & 1)                                              \
            data[bit >> 3] |= (uint8_t)(0x80 >> (bit & 7));
    PUT(4, 4);
    PUT(len, ver <= 9 ? 8 : 16);
    for (int i = 0; i < len; i++)
        PUT((uint8_t)text[i], 8);
    int term = cap * 8 - bit < 4 ? cap * 8 - bit : 4;
    bit += term;
    bit = (bit + 7) & ~7;
#undef PUT
    for (int i = bit / 8, pad = 0xEC; i < cap; i++, pad ^= 0xEC ^ 0x11)
        data[i] = (uint8_t)pad;

    // Chia khối, thêm byte sửa lỗi, xen kẽ các khối
    int blocks = num_blocks[ver], ecc_len = ecc_per_block[ver];
    int raw = raw_data_modules(ver) / 8;
    int short_blocks = blocks - raw % blocks;
    int short_len = raw / blocks;   // độ dài khối ngắn (dữ liệu + sửa lỗi)
    uint8_t div[30];
    rs_divisor(ecc_len, div);
    uint8_t blk[5][160];
    for (int i = 0, k = 0; i < blocks; i++) {
        int dlen = short_len - ecc_len + (i < short_blocks ? 0 : 1);
        memset(blk[i], 0, sizeof(blk[i]));
        memcpy(blk[i], data + k, (size_t)dlen);
        k += dlen;
        // Khối ngắn chừa 1 byte trống trước phần sửa lỗi để xen kẽ thẳng hàng
        rs_remainder(blk[i], dlen, div, ecc_len, blk[i] + short_len + 1 - ecc_len);
    }
    uint8_t all[400];
    int out = 0;
    for (int i = 0; i <= short_len; i++) {
        for (int j = 0; j < blocks; j++) {
            if (i != short_len - ecc_len || j >= short_blocks)
                all[out++] = blk[j][i];
        }
    }

    static Qr q;
    memset(&q, 0, sizeof(q));
    q.size = ver * 4 + 17;
    q.mod = modules;
    memset(modules, 0, QR_MAX_SIZE * QR_MAX_SIZE);
    draw_function_patterns(&q, ver);
    draw_codewords(&q, all, out);

    int best = 0;
    long best_p = -1;
    for (int m = 0; m < 8; m++) {
        apply_mask(&q, m);
        draw_format(&q, m);
        long p = penalty(&q);
        if (best_p < 0 || p < best_p) {
            best_p = p;
            best = m;
        }
        apply_mask(&q, m);  // XOR lần nữa để gỡ mask
    }
    apply_mask(&q, best);
    draw_format(&q, best);
    return q.size;
}
