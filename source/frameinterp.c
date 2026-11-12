#include "frameinterp.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define BS 8                // cạnh khối
#define MAXV 24             // vector tối đa mỗi chiều (điểm ảnh / khung game)
#define GLOBAL_R 16         // tầm tìm chuyển động chung
#define BAD_SAD 12          // sai khác độ sáng trung bình (0..255) quá mức này: khối không khớp
#define REFINE_R 8          // khối không khớp: tìm hết các vector trong tầm này quanh 0 và quanh chuyển động chung
#define PIX_T 40            // điểm ảnh: lệch màu (kênh lệch nhiều nhất) quá mức này là không khớp
#define WIN_T 16            // lệch màu trung bình trong ô 3x3 quanh điểm quá mức này là không khớp
#define PIX_EXACT 6         // vector của khối đã khớp tới mức này (trung bình ô 3x3) thì khỏi thử vector khác
#define MAX_MISS 0.3        // quá tỉ lệ này điểm ảnh không khớp: đổi cảnh, không nội suy
#define MIN_GAP 22.0        // game đã >= ~45 khung/giây: khỏi nội suy
#define MAX_GAP 200.0       // khung cách nhau quá xa (menu, khựng): hiện thẳng
#define MAX_PIXELS (360 * 640)  // màn hình lớn hơn: dựng khung mỗi lần vẽ tốn quá nhiều CPU
#define MAX_HI_PIXELS (1280 * 720)  // khung nét cao lớn hơn: như trên

typedef struct {
    int8_t x, y;
} Vec;

static int w, h, nbx, nby;
static uint32_t *prev, *cur, *out;
static uint8_t *lprev, *lcur;       // độ sáng, để so khối
static Vec *field;                  // vector của từng khối, ở thời điểm giữa 2 khung
static uint8_t *bad;
// Từng điểm ảnh chọn vector trong số vector của khối nó và các khối quanh (ranh giới vật chuyển động khác
// nền không bị cắt theo khối). miss: không vector nào khớp (vật đổi hình, chỗ bị che): lấy khung gần hơn.
static Vec *pvec;
static uint8_t *pmiss;
static int *proj;                   // tổng độ sáng theo cột / hàng của 2 khung
// Khung nét cao (chữ font hệ thống) gấp hk lần: dựng khung giữa ở độ phân giải này, vector nhân hk
static int hk;
static uint32_t *hprev, *hcur, *hout;
static bool hi_prev, hi_cur;        // khung trước / sau có bản nét cao
static double t_cur, gap_avg;
static bool have_cur, pending, active, show_cur;
static unsigned upd_i;

void frameinterp_reset(void) {
    free(prev);
    free(cur);
    free(out);
    free(lprev);
    free(lcur);
    free(field);
    free(bad);
    free(proj);
    free(pvec);
    free(pmiss);
    pvec = NULL;
    pmiss = NULL;
    free(hprev);
    free(hcur);
    free(hout);
    hprev = hcur = hout = NULL;
    hk = 0;
    hi_prev = hi_cur = false;
    prev = cur = out = NULL;
    lprev = lcur = NULL;
    field = NULL;
    bad = NULL;
    proj = NULL;
    w = h = nbx = nby = 0;
    gap_avg = 0;
    have_cur = pending = active = show_cur = false;
}

static bool alloc(int fw, int fh) {
    frameinterp_reset();
    size_t n = (size_t)fw * fh;
    int bx = (fw + BS - 1) / BS, by = (fh + BS - 1) / BS;
    prev = malloc(n * 4);
    cur = malloc(n * 4);
    out = malloc(n * 4);
    lprev = malloc(n);
    lcur = malloc(n);
    field = calloc((size_t)bx * by, sizeof(Vec));
    bad = calloc((size_t)bx * by, 1);
    proj = malloc((size_t)(fw + fh) * 2 * sizeof(int));
    pvec = malloc(n * sizeof(Vec));
    pmiss = malloc(n);
    if (!prev || !cur || !out || !lprev || !lcur || !field || !bad || !proj || !pvec || !pmiss) {
        frameinterp_reset();
        return false;
    }
    w = fw;
    h = fh;
    nbx = bx;
    nby = by;
    return true;
}

static void luma(const uint32_t *src, uint8_t *dst) {
    size_t n = (size_t)w * h;
    for (size_t i = 0; i < n; i++) {
        uint32_t p = src[i];
        dst[i] = (uint8_t)((((p >> 16) & 0xff) + ((p >> 7) & 0x1fe) + (p & 0xff)) >> 2);
    }
}

static bool alloc_hi(int k) {
    free(hprev);
    free(hcur);
    free(hout);
    size_t n = (size_t)w * k * h * k;
    hprev = malloc(n * 4);
    hcur = malloc(n * 4);
    hout = malloc(n * 4);
    hi_prev = hi_cur = false;
    if (!hprev || !hcur || !hout) {
        frameinterp_reset();
        return false;
    }
    hk = k;
    return true;
}

bool frameinterp_push(const uint32_t *fb, int fw, int fh, const uint32_t *hi, int k, double now) {
    if (!hi || k < 2)
        hi = NULL;
    if ((size_t)fw * fh > MAX_PIXELS || (hi && (size_t)fw * k * fh * k > MAX_HI_PIXELS)) {
        frameinterp_reset();
        return false;
    }
    if ((fw != w || fh != h || !cur) && !alloc(fw, fh))
        return false;
    if (hi && k != hk && !alloc_hi(k))
        return false;
    size_t n = (size_t)w * h, hn = n * hk * hk;
    // Game vẽ lại y hệt khung cũ: không tính là khung mới (đang chuyển cảnh thì chuyển tiếp)
    if (have_cur && memcmp(fb, cur, n * 4) == 0 && (hi != NULL) == hi_cur && (!hi || memcmp(hi, hcur, hn * 4) == 0))
        return true;
    double gap = have_cur ? now - t_cur : 1e9;
    uint32_t *t = prev;
    prev = cur;
    cur = t;
    uint8_t *l = lprev;
    lprev = lcur;
    lcur = l;
    memcpy(cur, fb, n * 4);
    luma(cur, lcur);
    if (hk) {
        t = hprev;
        hprev = hcur;
        hcur = t;
        hi_prev = hi_cur;
        hi_cur = hi != NULL;
        if (hi)
            memcpy(hcur, hi, hn * 4);
    }
    t_cur = now;
    if (gap < 250)
        gap_avg = gap_avg > 0 ? gap_avg * 0.75 + gap * 0.25 : gap;
    // Khung trước / sau phải cùng loại (cùng có hoặc cùng không có bản nét cao)
    pending = have_cur && gap <= MAX_GAP && gap_avg >= MIN_GAP && gap_avg <= MAX_GAP && hi_prev == hi_cur;
    have_cur = true;
    active = false;
    show_cur = !pending;
    return true;
}

static inline int clampi(int v, int lo, int hi) {
    return v < lo ? lo : v > hi ? hi : v;
}

// Sai khác giữa khung trước dời -a và khung sau dời v-a (a = v/2) trên khối, chỉ xét hàng chẵn.
// Vượt limit thì thôi tính.
static int block_sad(int x0, int y0, int bw, int bh, Vec v, int limit) {
    int ax = v.x >> 1, ay = v.y >> 1;
    int cx = v.x - ax, cy = v.y - ay;
    bool inside = x0 - ax >= 0 && x0 + bw - ax <= w && y0 - ay >= 0 && y0 + bh - ay <= h &&
                  x0 + cx >= 0 && x0 + bw + cx <= w && y0 + cy >= 0 && y0 + bh + cy <= h;
    int sad = 0;
    if (inside) {
        for (int y = 0; y < bh; y += 2) {
            const uint8_t *p = lprev + (size_t)(y0 + y - ay) * w + x0 - ax;
            const uint8_t *c = lcur + (size_t)(y0 + y + cy) * w + x0 + cx;
            for (int x = 0; x < bw; x++)
                sad += abs(p[x] - c[x]);
            if (sad >= limit)
                return sad;
        }
        return sad;
    }
    // Mép màn hình: chỉ so điểm có ở cả 2 khung (nội dung cuộn vào / ra chỉ có ở 1 khung), quy về đủ số điểm
    int count = 0, total = 0;
    for (int y = 0; y < bh; y += 2) {
        int py = y0 + y - ay, qy = y0 + y + cy;
        for (int x = 0; x < bw; x++) {
            int px = x0 + x - ax, qx = x0 + x + cx;
            total++;
            if (py < 0 || py >= h || qy < 0 || qy >= h || px < 0 || px >= w || qx < 0 || qx >= w)
                continue;
            sad += abs(lprev[(size_t)py * w + px] - lcur[(size_t)qy * w + qx]);
            count++;
        }
    }
    if (count * 2 < total)
        return 1 << 28;
    return sad * total / count;
}

// b[i + s] ≈ a[i]: tìm s, ưu tiên s nhỏ khi bằng nhau
static int best_shift(const int *a, const int *b, int n, int r) {
    long long best = -1;
    int best_s = 0;
    for (int k = 0; k <= 2 * r; k++) {
        int s = (k + 1) / 2 * (k & 1 ? 1 : -1);
        int lo = s < 0 ? -s : 0, hi = s > 0 ? n - s : n;
        if (hi - lo < n / 2)
            continue;
        long long sum = 0;
        for (int i = lo; i < hi; i++)
            sum += llabs((long long)a[i] - b[i + s]);
        long long cost = sum * 1024 / (hi - lo);
        if (best < 0 || cost < best) {
            best = cost;
            best_s = s;
        }
    }
    return best_s;
}

// Chuyển động chung (cuộn màn hình): so tổng độ sáng theo cột và theo hàng của 2 khung
static Vec global_motion(void) {
    int *cp = proj, *cc = proj + w, *rp = proj + 2 * w, *rc = proj + 2 * w + h;
    memset(proj, 0, (size_t)(w + h) * 2 * sizeof(int));
    for (int y = 0; y < h; y++) {
        const uint8_t *p = lprev + (size_t)y * w, *c = lcur + (size_t)y * w;
        int sp = 0, sc = 0;
        for (int x = 0; x < w; x++) {
            cp[x] += p[x];
            cc[x] += c[x];
            sp += p[x];
            sc += c[x];
        }
        rp[y] = sp;
        rc[y] = sc;
    }
    Vec g = { (int8_t)best_shift(cp, cc, w, GLOBAL_R), (int8_t)best_shift(rp, rc, h, GLOBAL_R) };
    return g;
}

// Vector khác 0 nhiều khối dùng nhất ở khung trước
static Vec field_mode(void) {
    static uint16_t hist[(2 * MAXV + 1) * (2 * MAXV + 1)];
    memset(hist, 0, sizeof(hist));
    int best = 0;
    Vec m = { 0, 0 };
    for (int i = 0; i < nbx * nby; i++) {
        Vec v = field[i];
        if (!v.x && !v.y)
            continue;
        int k = (v.y + MAXV) * (2 * MAXV + 1) + v.x + MAXV;
        if (++hist[k] > best) {
            best = hist[k];
            m = v;
        }
    }
    return m;
}

static const Vec UPDATES[] = {
    { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 }, { 2, 0 }, { -2, 0 }, { 0, 2 }, { 0, -2 },
    { 3, 0 }, { -3, 0 }, { 0, 3 }, { 0, -3 }, { 1, 1 }, { -1, -1 }, { 1, -1 }, { -1, 1 },
};
#define UPDATE_COUNT (int)(sizeof(UPDATES) / sizeof(UPDATES[0]))

static Vec vec_add(Vec a, Vec d) {
    Vec r = { (int8_t)clampi(a.x + d.x, -MAXV, MAXV), (int8_t)clampi(a.y + d.y, -MAXV, MAXV) };
    return r;
}

// Khối chưa khớp với các ứng viên (vật chuyển động nhanh, khác hẳn nền): tìm hết quanh 0 và quanh chuyển động chung
static void refine(int i, int x0, int y0, int bw, int bh, Vec g, int samples) {
    int best = block_sad(x0, y0, bw, bh, field[i], 1 << 30);
    for (int c = 0; c < 2; c++) {
        Vec o = c ? g : (Vec){ 0, 0 };
        if (c && !g.x && !g.y)
            break;
        for (int dy = -REFINE_R; dy <= REFINE_R; dy++) {
            for (int dx = -REFINE_R; dx <= REFINE_R; dx++) {
                Vec v = vec_add(o, (Vec){ (int8_t)dx, (int8_t)dy });
                int sad = block_sad(x0, y0, bw, bh, v, best);
                if (sad < best) {
                    best = sad;
                    field[i] = v;
                }
            }
        }
    }
    bad[i] = best > BAD_SAD * samples;
}

// Ước lượng vector cho mọi khối: 2 lượt (xuôi rồi ngược) để vector tốt lan ra các khối bên cạnh.
// field đang giữ vector của khung trước làm ứng viên theo thời gian.
static void estimate(void) {
    Vec g1 = global_motion(), g2 = field_mode();
    for (int pass = 0; pass < 2; pass++) {
        int step = pass ? -1 : 1;
        for (int k = 0; k < nbx * nby; k++) {
            int i = pass ? nbx * nby - 1 - k : k;
            int bx = i % nbx, by = i / nbx;
            int x0 = bx * BS, y0 = by * BS;
            int bw = x0 + BS <= w ? BS : w - x0, bh = y0 + BS <= h ? BS : h - y0;
            int samples = bw * ((bh + 1) / 2);
            // Khối đã xét ở lượt này (bên trái / trên khi xuôi): gốc cho vector cập nhật
            Vec sx = bx - step >= 0 && bx - step < nbx ? field[i - step] : field[i];
            Vec sy = by - step >= 0 && by - step < nby ? field[i - step * nbx] : field[i];
            Vec cand[12];
            int pen[12], n = 0;
#define CAND(v, p) (cand[n] = (v), pen[n++] = (p))
            CAND(((Vec){ 0, 0 }), 0);
            CAND(field[i], samples / 4);
            CAND(sx, samples / 4);
            CAND(sy, samples / 4);
            if (bx + step >= 0 && bx + step < nbx)
                CAND(field[i + step], samples / 4);
            if (by + step >= 0 && by + step < nby)
                CAND(field[i + step * nbx], samples / 4);
            CAND(g1, samples / 4);
            CAND(g2, samples / 4);
            CAND(vec_add(sx, UPDATES[upd_i++ % UPDATE_COUNT]), samples);
            CAND(vec_add(sy, UPDATES[upd_i++ % UPDATE_COUNT]), samples);
#undef CAND
            int best = 1 << 30, best_sad = 0;
            Vec best_v = { 0, 0 };
            for (int c = 0; c < n; c++) {
                bool seen = false;
                for (int d = 0; d < c && !seen; d++)
                    seen = cand[d].x == cand[c].x && cand[d].y == cand[c].y;
                if (seen)
                    continue;
                int sad = block_sad(x0, y0, bw, bh, cand[c], best - pen[c]);
                if (sad + pen[c] < best) {
                    best = sad + pen[c];
                    best_sad = sad;
                    best_v = cand[c];
                }
            }
            field[i] = best_v;
            if (pass) {
                bad[i] = best_sad > BAD_SAD * samples;
                if (bad[i])
                    refine(i, x0, y0, bw, bh, g1, samples);
            }
        }
    }
}

// Không rẽ nhánh (p == c cũng ra đúng p) để trình biên dịch vector hoá vòng lặp trộn
static inline uint32_t blend(uint32_t p, uint32_t c, uint32_t wt) {
    uint32_t iw = 256 - wt;
    uint32_t rb = (((p & 0x00ff00ff) * iw + (c & 0x00ff00ff) * wt) >> 8) & 0x00ff00ff;
    uint32_t ag = (((p >> 8) & 0x00ff00ff) * iw + ((c >> 8) & 0x00ff00ff) * wt) & 0xff00ff00;
    return rb | ag;
}

// Lệch màu giữa 2 điểm ảnh: kênh lệch nhiều nhất
static inline int color_diff(uint32_t p, uint32_t c) {
    int r = abs((int)((p >> 16) & 0xff) - (int)((c >> 16) & 0xff));
    int g = abs((int)((p >> 8) & 0xff) - (int)((c >> 8) & 0xff));
    int b = abs((int)(p & 0xff) - (int)(c & 0xff));
    return r > g ? (r > b ? r : b) : (g > b ? g : b);
}

// Độ khớp của điểm (x, y) theo vector v ở thời điểm giữa 2 khung. Điểm dời ra ngoài màn hình ở 1 khung
// (mép đang cuộn) chỉ coi là khớp với vector của chính khối (own)
static inline int pixel_diff(int x, int y, Vec v, bool own) {
    int ax = v.x >> 1, ay = v.y >> 1;
    int px = x - ax, py = y - ay, qx = x + v.x - ax, qy = y + v.y - ay;
    bool p_in = px >= 0 && px < w && py >= 0 && py < h;
    bool q_in = qx >= 0 && qx < w && qy >= 0 && qy < h;
    if (p_in && q_in)
        return color_diff(prev[(size_t)py * w + px], cur[(size_t)qy * w + qx]);
    return own && (p_in || q_in) ? 0 : 255;
}

#define WS (BS + 2)     // khối và viền 1 điểm ảnh, để tính ô 3x3 quanh từng điểm

// Lệch màu của cả khối (và viền) theo vector v vào d. Trả về true nếu mọi điểm trong khối khớp tới mức exact.
static bool diff_map(int x0, int y0, int x1, int y1, Vec v, bool own, uint8_t d[WS][WS], int exact) {
    bool all = true;
    for (int y = y0 - 1; y <= y1; y++) {
        for (int x = x0 - 1; x <= x1; x++) {
            int yy = clampi(y, 0, h - 1), xx = clampi(x, 0, w - 1);
            int e = pixel_diff(xx, yy, v, own);
            d[y - y0 + 1][x - x0 + 1] = (uint8_t)e;
            if (e > exact && x >= x0 && x < x1 && y >= y0 && y < y1)
                all = false;
        }
    }
    return all;
}

static inline int win_sum(uint8_t d[WS][WS], int y, int x) {
    return d[y][x] + d[y][x + 1] + d[y][x + 2] + d[y + 1][x] + d[y + 1][x + 1] + d[y + 1][x + 2] + d[y + 2][x] +
           d[y + 2][x + 1] + d[y + 2][x + 2];
}

// Chọn vector cho từng điểm ảnh theo độ khớp của ô 3x3 quanh nó (vùng chọn liền theo vật, không lốm đốm).
// false = quá nhiều điểm không khớp (đổi cảnh), không nội suy.
static bool choose_pixels(void) {
    static uint8_t maps[10][WS][WS];
    size_t miss = 0;
    for (int by = 0; by < nby; by++) {
        for (int bx = 0; bx < nbx; bx++) {
            int i = by * nbx + bx;
            // Vector của khối, 8 khối quanh và 0 (vật đứng yên: chữ, thanh máu)
            Vec cand[10];
            int n = 0;
            cand[n++] = field[i];
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    int nx = bx + dx, ny = by + dy;
                    if ((!dx && !dy) || nx < 0 || nx >= nbx || ny < 0 || ny >= nby)
                        continue;
                    Vec v = field[ny * nbx + nx];
                    bool seen = false;
                    for (int c = 0; c < n && !seen; c++)
                        seen = cand[c].x == v.x && cand[c].y == v.y;
                    if (!seen)
                        cand[n++] = v;
                }
            }
            bool has_zero = false;
            for (int c = 0; c < n; c++)
                has_zero |= !cand[c].x && !cand[c].y;
            if (!has_zero)
                cand[n++] = (Vec){ 0, 0 };
            int x0 = bx * BS, y0 = by * BS;
            int x1 = x0 + BS < w ? x0 + BS : w, y1 = y0 + BS < h ? y0 + BS : h;
            // Cả khối khớp theo vector của nó (thường gặp): khỏi thử vector khác
            if (diff_map(x0, y0, x1, y1, cand[0], true, maps[0], PIX_EXACT))
                n = 1;
            for (int c = 1; c < n; c++)
                diff_map(x0, y0, x1, y1, cand[c], false, maps[c], 0);
            for (int y = y0; y < y1; y++) {
                for (int x = x0; x < x1; x++) {
                    size_t p = (size_t)y * w + x;
                    int my = y - y0, mx = x - x0;
                    int best = win_sum(maps[0], my, mx), bc = 0;
                    // Cùng khớp thì giữ vector của khối
                    for (int c = 1; c < n && best > 9 * PIX_EXACT; c++) {
                        int d = win_sum(maps[c], my, mx) + 9 * 2;
                        if (d < best) {
                            best = d;
                            bc = c;
                        }
                    }
                    pvec[p] = cand[bc];
                    pmiss[p] = best > 9 * WIN_T || maps[bc][my + 1][mx + 1] > PIX_T;
                    miss += pmiss[p];
                }
            }
        }
    }
    return miss < (size_t)(MAX_MISS * w * h);
}

// Khung ở thời điểm t (0 = khung trước, 1 = khung sau) vào dst, ảnh gấp k lần khung gốc (vector nhân k).
// Điểm không khớp lấy khung gần hơn nhưng vẫn dời theo vector, để không lệch với các điểm quanh nó.
// Làm theo từng đoạn điểm ảnh liền nhau trên 1 hàng có cùng vector và cùng kiểu (khớp / không khớp).
static void compose(uint32_t *dst, const uint32_t *p0, const uint32_t *p1, int k, double t) {
    int tq = clampi((int)(t * 256 + 0.5), 0, 256);
    uint32_t wt = (uint32_t)tq;
    bool near_prev = tq < 128;
    int iw = w * k, ih = h * k;
    for (int y = 0; y < h; y++) {
        const Vec *vrow = pvec + (size_t)y * w;
        const uint8_t *mrow = pmiss + (size_t)y * w;
        for (int x = 0; x < w;) {
            Vec v = vrow[x];
            bool miss = mrow[x];
            int xe = x + 1;
            while (xe < w && vrow[xe].x == v.x && vrow[xe].y == v.y && mrow[xe] == miss)
                xe++;
            int vx = v.x * k, vy = v.y * k;
            // Làm tròn v * t (dịch phải số âm = chia làm tròn xuống)
            int ax = (vx * tq + 128) >> 8, ay = (vy * tq + 128) >> 8;
            int cx = vx - ax, cy = vy - ay;
            int X0 = x * k, X1 = xe * k, Y0 = y * k;
            bool inside = X0 - ax >= 0 && X1 - ax <= iw && Y0 - ay >= 0 && Y0 + k - ay <= ih &&
                          X0 + cx >= 0 && X1 + cx <= iw && Y0 + cy >= 0 && Y0 + k + cy <= ih;
            for (int Y = Y0; Y < Y0 + k; Y++) {
                uint32_t *o = dst + (size_t)Y * iw;
                if (inside) {
                    const uint32_t *p = p0 + (size_t)(Y - ay) * iw - ax;
                    const uint32_t *c = p1 + (size_t)(Y + cy) * iw + cx;
                    if (miss)
                        memcpy(o + X0, (near_prev ? p : c) + X0, (size_t)(X1 - X0) * 4);
                    else
                        for (int X = X0; X < X1; X++)
                            o[X] = blend(p[X], c[X], wt);
                    continue;
                }
                // Mép màn hình: điểm dời ra ngoài ở 1 khung (nội dung cuộn vào / ra) thì lấy của khung kia
                int py = Y - ay, qy = Y + cy;
                for (int X = X0; X < X1; X++) {
                    int px = X - ax, qx = X + cx;
                    bool p_in = px >= 0 && px < iw && py >= 0 && py < ih;
                    bool c_in = qx >= 0 && qx < iw && qy >= 0 && qy < ih;
                    uint32_t a = p0[(size_t)clampi(py, 0, ih - 1) * iw + clampi(px, 0, iw - 1)];
                    uint32_t b = p1[(size_t)clampi(qy, 0, ih - 1) * iw + clampi(qx, 0, iw - 1)];
                    if (p_in != c_in)
                        o[X] = p_in ? a : b;
                    else
                        o[X] = miss ? (near_prev ? a : b) : blend(a, b, wt);
                }
            }
            x = xe;
        }
    }
}

const uint32_t *frameinterp_frame(double now, bool *hires) {
    *hires = hi_cur;
    if (!cur)
        return NULL;
    if (pending) {
        pending = false;
        estimate();
        active = choose_pixels();
        show_cur = !active;
    }
    if (active) {
        // Chuyển từ khung trước sang khung mới trong khoảng thời gian game thường vẽ 1 khung
        double t = (now - t_cur) / gap_avg;
        if (t < 1.0) {
            if (hi_cur)
                compose(hout, hprev, hcur, hk, t < 0 ? 0 : t);
            else
                compose(out, prev, cur, 1, t < 0 ? 0 : t);
            return hi_cur ? hout : out;
        }
        active = false;
        show_cur = true;
    }
    if (show_cur) {
        show_cur = false;
        return hi_cur ? hcur : cur;
    }
    return NULL;
}
