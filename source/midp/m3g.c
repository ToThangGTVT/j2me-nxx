// Bộ dựng hình 3D phần mềm cho M3G (JSR-184): biến đổi đỉnh, chiếu sáng theo đỉnh,
// cắt mặt phẳng gần, Z-buffer, texture có hiệu chỉnh phối cảnh, fog, alpha test, blend.
// Phía Java (javax.microedition.m3g) duyệt cây cảnh rồi gọi renderSubmesh cho từng submesh.
#include "midp.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "../vm/vm.h"

// --- Bố cục mảng tham số (khớp với javax/microedition/m3g/Graphics3D.java)

// float[] f
enum {
    F_MODELVIEW = 0,        // 16, cột-chính (như OpenGL)
    F_PROJECTION = 16,      // 16
    F_POS_BIAS = 32,        // 3
    F_POS_SCALE = 35,
    F_TC_SCALE = 36,
    F_TC_BIAS = 37,         // 3
    F_TEX_MATRIX = 40,      // 16
    F_VIEWPORT = 56,        // x, y, w, h
    F_DEPTH_RANGE = 60,     // near, far
    F_MAT_AMBIENT = 62,     // rgba
    F_MAT_DIFFUSE = 66,
    F_MAT_EMISSIVE = 70,
    F_MAT_SPECULAR = 74,
    F_MAT_SHININESS = 78,
    F_ALPHA_FACTOR = 79,
    F_FOG_COLOR = 80,       // rgb
    F_FOG_DENSITY = 84,
    F_FOG_NEAR = 85,
    F_FOG_FAR = 86,
    F_TEX_BLEND_COLOR = 87, // rgb
    F_LIGHTS = 96,          // mỗi đèn 16 float
    F_COUNT = 96 + 16 * 8,
};

// Mỗi đèn: kiểu, màu*cường độ (3), vị trí/hướng trong không gian mắt (3), suy giảm (3),
// cos góc spot, mũ spot, hướng spot (3)
enum {
    L_TYPE = 0, L_COLOR = 1, L_POS = 4, L_ATTEN = 7, L_SPOT_COS = 10, L_SPOT_EXP = 11, L_SPOT_DIR = 12,
};

// int[] p
enum {
    P_POS_COMPS, P_POS_TYPE,        // TYPE: 1 byte, 2 short
    P_HAS_NORMALS, P_NORMAL_TYPE,
    P_COLOR_COMPS, P_DEFAULT_COLOR, // 0/3/4 thành phần; màu mặc định ARGB
    P_TC_COMPS, P_TC_TYPE,
    P_TEX_W, P_TEX_H, P_TEX_FUNC, P_TEX_WRAP_S, P_TEX_WRAP_T, P_TEX_HAS_ALPHA,
    P_BLENDING, P_ALPHA_THRESHOLD,  // ngưỡng 0..256
    P_DEPTH_TEST, P_DEPTH_WRITE, P_COLOR_WRITE, P_ALPHA_WRITE,
    P_CULLING, P_SHADING, P_WINDING, P_TWO_SIDED,
    P_LIGHTING, P_COLOR_TRACKING, P_LIGHT_COUNT,
    P_CLIP_X, P_CLIP_Y, P_CLIP_W, P_CLIP_H,
    P_FOG_MODE,                     // 0 tắt, 80 EXPONENTIAL, 81 LINEAR
    P_INDEX_COUNT,
    P_VERTEX_COUNT,
    P_TARGET_HAS_ALPHA,
    P_COUNT,
};

// Hằng số M3G
#define LIGHT_AMBIENT       128
#define LIGHT_DIRECTIONAL   129
#define LIGHT_OMNI          130
#define LIGHT_SPOT          131
#define BLEND_ALPHA         64
#define BLEND_ALPHA_ADD     65
#define BLEND_MODULATE      66
#define BLEND_MODULATE_X2   67
#define BLEND_REPLACE       68
#define CULL_BACK           160
#define CULL_FRONT          161
#define SHADE_FLAT          164
#define WINDING_CW          169
#define FUNC_ADD            224
#define FUNC_BLEND          225
#define FUNC_DECAL          226
#define FUNC_MODULATE       227
#define FUNC_REPLACE        228
#define WRAP_CLAMP          240
#define FOG_EXPONENTIAL     80
#define FOG_LINEAR          81

typedef struct {
    float x, y, z, w;       // clip space
    float r, g, b, a;       // màu sau chiếu sáng (0..1)
    float u, v;             // toạ độ texture (đơn vị texel chuẩn hoá)
    float fog;              // hệ số fog (1 = không fog)
} Vtx;

typedef struct {
    float sx, sy, sz;       // toạ độ màn hình, độ sâu
    float iw;               // 1/w
    float r, g, b, a, u, v, fog;  // đã chia w (để nội suy phối cảnh)
} SVtx;

static float *zbuf;
static int zbuf_w, zbuf_h;

static void zbuf_ensure(int w, int h) {
    if (zbuf && zbuf_w == w && zbuf_h == h)
        return;
    free(zbuf);
    zbuf = malloc(sizeof(float) * (size_t)w * h);
    zbuf_w = w;
    zbuf_h = h;
    for (int i = 0; i < w * h; i++)
        zbuf[i] = 1.0f;
}

// --- Đọc dữ liệu đỉnh từ mảng Java (byte[] / short[])

// type: 1 = byte[], 2 = short[], 4 = float[]
static float read_comp(Object *arr, int type, int idx) {
    if (type == 1)
        return (float)ARRAY_DATA(arr, int8_t)[idx];
    if (type == 4)
        return ARRAY_DATA(arr, float)[idx];
    return (float)ARRAY_DATA(arr, int16_t)[idx];
}

static void mat_mul_vec(const float *m, float x, float y, float z, float w, float *out) {
    out[0] = m[0] * x + m[4] * y + m[8] * z + m[12] * w;
    out[1] = m[1] * x + m[5] * y + m[9] * z + m[13] * w;
    out[2] = m[2] * x + m[6] * y + m[10] * z + m[14] * w;
    out[3] = m[3] * x + m[7] * y + m[11] * z + m[15] * w;
}

// Ma trận pháp tuyến = nghịch đảo chuyển vị của phần 3x3 của modelview
static void normal_matrix(const float *m, float *n) {
    float a = m[0], b = m[4], c = m[8];
    float d = m[1], e = m[5], f = m[9];
    float g = m[2], h = m[6], i = m[10];
    float det = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
    if (fabsf(det) < 1e-12f)
        det = 1e-12f;
    float id = 1.0f / det;
    // inverse transpose, lưu theo hàng: n[row*3+col]
    n[0] = (e * i - f * h) * id;
    n[1] = -(d * i - f * g) * id;
    n[2] = (d * h - e * g) * id;
    n[3] = -(b * i - c * h) * id;
    n[4] = (a * i - c * g) * id;
    n[5] = -(a * h - b * g) * id;
    n[6] = (b * f - c * e) * id;
    n[7] = -(a * f - c * d) * id;
    n[8] = (a * e - b * d) * id;
}

static float clamp01(float v) {
    return v < 0 ? 0 : v > 1 ? 1 : v;
}

// Chiếu sáng 1 đỉnh trong không gian mắt
static void light_vertex(const float *f, const int *p, const float *eye, const float *nrm, const float *vcol,
                         float *out) {
    const float *amb_m = f + F_MAT_AMBIENT, *dif_m = f + F_MAT_DIFFUSE;
    float amb[3] = { amb_m[0], amb_m[1], amb_m[2] };
    float dif[4] = { dif_m[0], dif_m[1], dif_m[2], dif_m[3] };
    if (p[P_COLOR_TRACKING] && vcol) {
        amb[0] = dif[0] = vcol[0];
        amb[1] = dif[1] = vcol[1];
        amb[2] = dif[2] = vcol[2];
        dif[3] = vcol[3];
    }
    const float *em = f + F_MAT_EMISSIVE, *sp = f + F_MAT_SPECULAR;
    float shin = f[F_MAT_SHININESS];
    float r = em[0], g = em[1], b = em[2];

    for (int li = 0; li < p[P_LIGHT_COUNT]; li++) {
        const float *L = f + F_LIGHTS + li * 16;
        int type = (int)L[L_TYPE];
        const float *lc = L + L_COLOR;
        if (type == LIGHT_AMBIENT) {
            r += amb[0] * lc[0];
            g += amb[1] * lc[1];
            b += amb[2] * lc[2];
            continue;
        }
        float lx, ly, lz, att = 1.0f;
        if (type == LIGHT_DIRECTIONAL) {
            lx = -L[L_POS];
            ly = -L[L_POS + 1];
            lz = -L[L_POS + 2];
        } else {
            lx = L[L_POS] - eye[0];
            ly = L[L_POS + 1] - eye[1];
            lz = L[L_POS + 2] - eye[2];
            float d = sqrtf(lx * lx + ly * ly + lz * lz);
            if (d > 1e-6f) {
                lx /= d;
                ly /= d;
                lz /= d;
            }
            float den = L[L_ATTEN] + L[L_ATTEN + 1] * d + L[L_ATTEN + 2] * d * d;
            att = den > 1e-6f ? 1.0f / den : 1.0f;
            if (type == LIGHT_SPOT) {
                float c = -(lx * L[L_SPOT_DIR] + ly * L[L_SPOT_DIR + 1] + lz * L[L_SPOT_DIR + 2]);
                if (c < L[L_SPOT_COS])
                    continue;
                att *= powf(c, L[L_SPOT_EXP]);
            }
        }
        float ndl = nrm[0] * lx + nrm[1] * ly + nrm[2] * lz;
        if (ndl <= 0)
            continue;
        r += att * ndl * dif[0] * lc[0];
        g += att * ndl * dif[1] * lc[1];
        b += att * ndl * dif[2] * lc[2];
        if (shin > 0 && (sp[0] > 0 || sp[1] > 0 || sp[2] > 0)) {
            // Half vector với mắt ở gốc (nhìn theo -z)
            float hx = lx, hy = ly, hz = lz + 1.0f;
            float hl = sqrtf(hx * hx + hy * hy + hz * hz);
            if (hl > 1e-6f) {
                float ndh = (nrm[0] * hx + nrm[1] * hy + nrm[2] * hz) / hl;
                if (ndh > 0) {
                    float s = att * powf(ndh, shin);
                    r += s * sp[0] * lc[0];
                    g += s * sp[1] * lc[1];
                    b += s * sp[2] * lc[2];
                }
            }
        }
    }
    out[0] = clamp01(r);
    out[1] = clamp01(g);
    out[2] = clamp01(b);
    out[3] = clamp01(dif[3]);
}

static void process_vertex(const float *f, const int *p, Object *pos, Object *nrm, Object *col, Object *tc,
                           const float *nm, int vi, Vtx *v) {
    int pc = p[P_POS_COMPS];
    float s = f[F_POS_SCALE];
    float px = read_comp(pos, p[P_POS_TYPE], vi * pc) * s + f[F_POS_BIAS];
    float py = read_comp(pos, p[P_POS_TYPE], vi * pc + 1) * s + f[F_POS_BIAS + 1];
    float pz = pc > 2 ? read_comp(pos, p[P_POS_TYPE], vi * pc + 2) * s + f[F_POS_BIAS + 2] : f[F_POS_BIAS + 2];
    float eye[4], clip[4];
    mat_mul_vec(f + F_MODELVIEW, px, py, pz, 1, eye);
    mat_mul_vec(f + F_PROJECTION, eye[0], eye[1], eye[2], eye[3], clip);
    v->x = clip[0];
    v->y = clip[1];
    v->z = clip[2];
    v->w = clip[3];

    // Màu đỉnh
    float vc[4];
    const float *vcp = NULL;
    if (p[P_COLOR_COMPS] && col) {
        int cc = p[P_COLOR_COMPS];
        const uint8_t *c = ARRAY_DATA(col, uint8_t) + vi * cc;
        vc[0] = c[0] / 255.0f;
        vc[1] = c[1] / 255.0f;
        vc[2] = c[2] / 255.0f;
        vc[3] = cc == 4 ? c[3] / 255.0f : 1.0f;
        vcp = vc;
    }

    float out[4];
    if (p[P_LIGHTING]) {
        float n[3] = { 0, 0, 1 };
        if (p[P_HAS_NORMALS] && nrm) {
            float nx = read_comp(nrm, p[P_NORMAL_TYPE], vi * 3);
            float ny = read_comp(nrm, p[P_NORMAL_TYPE], vi * 3 + 1);
            float nz = read_comp(nrm, p[P_NORMAL_TYPE], vi * 3 + 2);
            n[0] = nm[0] * nx + nm[1] * ny + nm[2] * nz;
            n[1] = nm[3] * nx + nm[4] * ny + nm[5] * nz;
            n[2] = nm[6] * nx + nm[7] * ny + nm[8] * nz;
            float l = sqrtf(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
            if (l > 1e-6f) {
                n[0] /= l;
                n[1] /= l;
                n[2] /= l;
            }
        }
        light_vertex(f, p, eye, n, vcp, out);
    } else if (vcp) {
        memcpy(out, vc, sizeof(out));
    } else {
        uint32_t dc = (uint32_t)p[P_DEFAULT_COLOR];
        out[0] = ((dc >> 16) & 255) / 255.0f;
        out[1] = ((dc >> 8) & 255) / 255.0f;
        out[2] = (dc & 255) / 255.0f;
        out[3] = (dc >> 24) / 255.0f;
    }
    v->r = out[0];
    v->g = out[1];
    v->b = out[2];
    v->a = out[3] * f[F_ALPHA_FACTOR];

    // Toạ độ texture
    v->u = v->v = 0;
    if (p[P_TC_COMPS] && tc) {
        int tcn = p[P_TC_COMPS];
        float ts = f[F_TC_SCALE];
        float s0 = read_comp(tc, p[P_TC_TYPE], vi * tcn) * ts + f[F_TC_BIAS];
        float t0 = read_comp(tc, p[P_TC_TYPE], vi * tcn + 1) * ts + f[F_TC_BIAS + 1];
        float r0 = tcn > 2 ? read_comp(tc, p[P_TC_TYPE], vi * tcn + 2) * ts + f[F_TC_BIAS + 2] : 0;
        float o[4];
        mat_mul_vec(f + F_TEX_MATRIX, s0, t0, r0, 1, o);
        float q = fabsf(o[3]) > 1e-9f ? o[3] : 1.0f;
        v->u = o[0] / q;
        v->v = o[1] / q;
    }

    // Fog theo khoảng cách mắt
    v->fog = 1.0f;
    if (p[P_FOG_MODE]) {
        float d = -eye[2];
        if (p[P_FOG_MODE] == FOG_LINEAR) {
            float n = f[F_FOG_NEAR], fa = f[F_FOG_FAR];
            v->fog = fa != n ? clamp01((fa - d) / (fa - n)) : 1.0f;
        } else {
            v->fog = clamp01(expf(-f[F_FOG_DENSITY] * d));
        }
    }
}

// --- Raster

typedef struct {
    uint32_t *px;
    int w, h;
    int x0, y0, x1, y1;         // vùng vẽ (clip ∩ viewport)
    const int *p;
    const float *f;
    const uint32_t *tex;
    int tw, th;
    float near, far;
    bool flat;
    float flat_rgba[4];
} Raster;

static inline float wrap(float t, int clamp) {
    if (clamp)
        return t < 0 ? 0 : t > 0.99999f ? 0.99999f : t;
    return t - floorf(t);
}

static inline void shade_pixel(Raster *R, int x, int y, float z, float r, float g, float b, float a, float u, float v,
                               float fog) {
    const int *p = R->p;
    const float *f = R->f;
    int idx = y * R->w + x;
    if (p[P_DEPTH_TEST] && z > zbuf[idx])
        return;

    if (R->flat) {
        r = R->flat_rgba[0];
        g = R->flat_rgba[1];
        b = R->flat_rgba[2];
        a = R->flat_rgba[3];
    }

    if (R->tex) {
        int tx = (int)(wrap(u, p[P_TEX_WRAP_S] == WRAP_CLAMP) * R->tw);
        int ty = (int)(wrap(v, p[P_TEX_WRAP_T] == WRAP_CLAMP) * R->th);
        uint32_t t = R->tex[ty * R->tw + tx];
        float tr = ((t >> 16) & 255) / 255.0f, tg = ((t >> 8) & 255) / 255.0f, tb = (t & 255) / 255.0f;
        float ta = p[P_TEX_HAS_ALPHA] ? (t >> 24) / 255.0f : 1.0f;
        switch (p[P_TEX_FUNC]) {
        case FUNC_REPLACE:
            r = tr;
            g = tg;
            b = tb;
            if (p[P_TEX_HAS_ALPHA])
                a = ta;
            break;
        case FUNC_DECAL:
            r = r * (1 - ta) + tr * ta;
            g = g * (1 - ta) + tg * ta;
            b = b * (1 - ta) + tb * ta;
            break;
        case FUNC_BLEND: {
            const float *bc = f + F_TEX_BLEND_COLOR;
            r = r * (1 - tr) + bc[0] * tr;
            g = g * (1 - tg) + bc[1] * tg;
            b = b * (1 - tb) + bc[2] * tb;
            a *= ta;
            break;
        }
        case FUNC_ADD:
            r = clamp01(r + tr);
            g = clamp01(g + tg);
            b = clamp01(b + tb);
            a *= ta;
            break;
        default:    // MODULATE
            r *= tr;
            g *= tg;
            b *= tb;
            a *= ta;
            break;
        }
    }

    if (p[P_FOG_MODE]) {
        const float *fc = f + F_FOG_COLOR;
        r = r * fog + fc[0] * (1 - fog);
        g = g * fog + fc[1] * (1 - fog);
        b = b * fog + fc[2] * (1 - fog);
    }

    if (p[P_ALPHA_THRESHOLD] > 0 && a * 256.0f < p[P_ALPHA_THRESHOLD])
        return;

    uint32_t dst = R->px[idx];
    float dr = ((dst >> 16) & 255) / 255.0f, dg = ((dst >> 8) & 255) / 255.0f, db = (dst & 255) / 255.0f;
    float da = (dst >> 24) / 255.0f;
    float orr, og, ob, oa;
    switch (p[P_BLENDING]) {
    case BLEND_ALPHA:
        orr = r * a + dr * (1 - a);
        og = g * a + dg * (1 - a);
        ob = b * a + db * (1 - a);
        oa = a + da * (1 - a);
        break;
    case BLEND_ALPHA_ADD:
        orr = dr + r * a;
        og = dg + g * a;
        ob = db + b * a;
        oa = da + a;
        break;
    case BLEND_MODULATE:
        orr = r * dr;
        og = g * dg;
        ob = b * db;
        oa = a * da;
        break;
    case BLEND_MODULATE_X2:
        orr = 2 * r * dr;
        og = 2 * g * dg;
        ob = 2 * b * db;
        oa = 2 * a * da;
        break;
    default:
        orr = r;
        og = g;
        ob = b;
        oa = a;
        break;
    }
    uint32_t out = dst;
    if (p[P_COLOR_WRITE]) {
        out = (out & 0xff000000u) | ((uint32_t)(clamp01(orr) * 255.0f + 0.5f) << 16) |
              ((uint32_t)(clamp01(og) * 255.0f + 0.5f) << 8) | (uint32_t)(clamp01(ob) * 255.0f + 0.5f);
    }
    if (!p[P_TARGET_HAS_ALPHA])
        out |= 0xff000000u;
    else if (p[P_ALPHA_WRITE])
        out = (out & 0x00ffffffu) | ((uint32_t)(clamp01(oa) * 255.0f + 0.5f) << 24);
    R->px[idx] = out;
    if (p[P_DEPTH_WRITE])
        zbuf[idx] = z;
}

static void to_screen(const Raster *R, const Vtx *v, SVtx *s) {
    const float *vp = R->f + F_VIEWPORT;
    float iw = 1.0f / v->w;
    float nx = v->x * iw, ny = v->y * iw, nz = v->z * iw;
    s->sx = vp[0] + (nx + 1) * 0.5f * vp[2];
    s->sy = vp[1] + (1 - ny) * 0.5f * vp[3];
    float z01 = (nz + 1) * 0.5f;
    s->sz = R->near + z01 * (R->far - R->near);
    s->iw = iw;
    s->r = v->r * iw;
    s->g = v->g * iw;
    s->b = v->b * iw;
    s->a = v->a * iw;
    s->u = v->u * iw;
    s->v = v->v * iw;
    s->fog = v->fog * iw;
}

static void raster_triangle(Raster *R, const SVtx *a, const SVtx *b, const SVtx *c) {
    float area = (b->sx - a->sx) * (c->sy - a->sy) - (c->sx - a->sx) * (b->sy - a->sy);
    if (fabsf(area) < 1e-6f)
        return;
    int minx = (int)floorf(fminf(a->sx, fminf(b->sx, c->sx)));
    int maxx = (int)ceilf(fmaxf(a->sx, fmaxf(b->sx, c->sx)));
    int miny = (int)floorf(fminf(a->sy, fminf(b->sy, c->sy)));
    int maxy = (int)ceilf(fmaxf(a->sy, fmaxf(b->sy, c->sy)));
    if (minx < R->x0) minx = R->x0;
    if (miny < R->y0) miny = R->y0;
    if (maxx > R->x1) maxx = R->x1;
    if (maxy > R->y1) maxy = R->y1;
    if (minx >= maxx || miny >= maxy)
        return;

    float inv = 1.0f / area;
    for (int y = miny; y < maxy; y++) {
        float py = y + 0.5f;
        for (int x = minx; x < maxx; x++) {
            float px = x + 0.5f;
            float w0 = ((b->sx - px) * (c->sy - py) - (c->sx - px) * (b->sy - py)) * inv;
            float w1 = ((c->sx - px) * (a->sy - py) - (a->sx - px) * (c->sy - py)) * inv;
            float w2 = 1.0f - w0 - w1;
            if (w0 < 0 || w1 < 0 || w2 < 0)
                continue;
            float z = w0 * a->sz + w1 * b->sz + w2 * c->sz;
            float iw = w0 * a->iw + w1 * b->iw + w2 * c->iw;
            float pw = 1.0f / iw;
            shade_pixel(R, x, y, z, (w0 * a->r + w1 * b->r + w2 * c->r) * pw, (w0 * a->g + w1 * b->g + w2 * c->g) * pw,
                        (w0 * a->b + w1 * b->b + w2 * c->b) * pw, (w0 * a->a + w1 * b->a + w2 * c->a) * pw,
                        (w0 * a->u + w1 * b->u + w2 * c->u) * pw, (w0 * a->v + w1 * b->v + w2 * c->v) * pw,
                        (w0 * a->fog + w1 * b->fog + w2 * c->fog) * pw);
        }
    }
}

static Vtx lerp_vtx(const Vtx *a, const Vtx *b, float t) {
    Vtx r;
    r.x = a->x + (b->x - a->x) * t;
    r.y = a->y + (b->y - a->y) * t;
    r.z = a->z + (b->z - a->z) * t;
    r.w = a->w + (b->w - a->w) * t;
    r.r = a->r + (b->r - a->r) * t;
    r.g = a->g + (b->g - a->g) * t;
    r.b = a->b + (b->b - a->b) * t;
    r.a = a->a + (b->a - a->a) * t;
    r.u = a->u + (b->u - a->u) * t;
    r.v = a->v + (b->v - a->v) * t;
    r.fog = a->fog + (b->fog - a->fog) * t;
    return r;
}

// Cắt tam giác theo mặt phẳng gần (z >= -w), vẽ đa giác còn lại dạng quạt
static void draw_triangle(Raster *R, const Vtx *v0, const Vtx *v1, const Vtx *v2) {
    const Vtx *in[3] = { v0, v1, v2 };
    Vtx poly[4];
    int n = 0;
    for (int i = 0; i < 3; i++) {
        const Vtx *a = in[i], *b = in[(i + 1) % 3];
        float da = a->z + a->w, db = b->z + b->w;
        if (da >= 0)
            poly[n++] = *a;
        if ((da >= 0) != (db >= 0))
            poly[n++] = lerp_vtx(a, b, da / (da - db));
    }
    if (n < 3)
        return;

    SVtx s[4];
    for (int i = 0; i < n; i++) {
        if (poly[i].w <= 1e-6f)
            return;
        to_screen(R, &poly[i], &s[i]);
    }

    // Loại mặt: diện tích có dấu trên màn hình (trục y hướng xuống nên đảo dấu so với OpenGL)
    float area = (s[1].sx - s[0].sx) * (s[2].sy - s[0].sy) - (s[2].sx - s[0].sx) * (s[1].sy - s[0].sy);
    bool ccw = area < 0;
    bool front = R->p[P_WINDING] == WINDING_CW ? !ccw : ccw;
    int cull = R->p[P_CULLING];
    if ((cull == CULL_BACK && !front) || (cull == CULL_FRONT && front))
        return;

    for (int i = 1; i + 1 < n; i++)
        raster_triangle(R, &s[0], &s[i], &s[i + 1]);
}

// renderSubmesh(int[] target, int w, int h, float[] f, int[] p, Object pos, Object nrm, Object col,
//               Object tc, int[] tex, int[] indices)
static NativeResult M3G_renderSubmesh(VMThread *t, Value *args, Value *ret) {
    (void)ret;
    Object *target = args[0].l, *fa = args[3].l, *pa = args[4].l;
    Object *pos = args[5].l, *nrm = args[6].l, *col = args[7].l, *tc = args[8].l;
    Object *tex = args[9].l, *ind = args[10].l;
    int w = args[1].i, h = args[2].i;
    if (!target || !fa || !pa || !pos || !ind) {
        throw_null(t);
        return NATIVE_EXCEPTION;
    }
    if (ARRAY_LEN(fa) < F_COUNT || ARRAY_LEN(pa) < P_COUNT || ARRAY_LEN(target) < w * h) {
        throw_new(t, "java/lang/IllegalArgumentException", "m3g params");
        return NATIVE_EXCEPTION;
    }
    const float *f = ARRAY_DATA(fa, float);
    const int *p = ARRAY_DATA(pa, jint);
    int nverts = p[P_VERTEX_COUNT];
    int nind = p[P_INDEX_COUNT];
    const jint *idx = ARRAY_DATA(ind, jint);
    if (nind > ARRAY_LEN(ind) || nverts <= 0)
        return NATIVE_OK;
    if (ARRAY_LEN(pos) < nverts * p[P_POS_COMPS])
        return NATIVE_OK;

    zbuf_ensure(w, h);

    Raster R;
    memset(&R, 0, sizeof(R));
    R.px = ARRAY_DATA(target, uint32_t);
    R.w = w;
    R.h = h;
    R.p = p;
    R.f = f;
    const float *vp = f + F_VIEWPORT;
    R.x0 = (int)vp[0];
    R.y0 = (int)vp[1];
    R.x1 = (int)(vp[0] + vp[2]);
    R.y1 = (int)(vp[1] + vp[3]);
    if (R.x0 < p[P_CLIP_X]) R.x0 = p[P_CLIP_X];
    if (R.y0 < p[P_CLIP_Y]) R.y0 = p[P_CLIP_Y];
    if (R.x1 > p[P_CLIP_X] + p[P_CLIP_W]) R.x1 = p[P_CLIP_X] + p[P_CLIP_W];
    if (R.y1 > p[P_CLIP_Y] + p[P_CLIP_H]) R.y1 = p[P_CLIP_Y] + p[P_CLIP_H];
    if (R.x0 < 0) R.x0 = 0;
    if (R.y0 < 0) R.y0 = 0;
    if (R.x1 > w) R.x1 = w;
    if (R.y1 > h) R.y1 = h;
    if (R.x0 >= R.x1 || R.y0 >= R.y1)
        return NATIVE_OK;
    R.near = f[F_DEPTH_RANGE];
    R.far = f[F_DEPTH_RANGE + 1];
    R.flat = p[P_SHADING] == SHADE_FLAT;
    if (tex && p[P_TC_COMPS] && p[P_TEX_W] > 0 && p[P_TEX_H] > 0 && ARRAY_LEN(tex) >= p[P_TEX_W] * p[P_TEX_H]) {
        R.tex = ARRAY_DATA(tex, uint32_t);
        R.tw = p[P_TEX_W];
        R.th = p[P_TEX_H];
    }

    float nm[9];
    normal_matrix(f + F_MODELVIEW, nm);

    // Xử lý đỉnh (chỉ những đỉnh được dùng, đánh dấu bằng mảng cờ)
    Vtx *verts = malloc(sizeof(Vtx) * (size_t)nverts);
    uint8_t *done = calloc((size_t)nverts, 1);
    if (!verts || !done) {
        free(verts);
        free(done);
        return NATIVE_OK;
    }
    for (int i = 0; i + 2 < nind; i += 3) {
        int tri[3] = { idx[i], idx[i + 1], idx[i + 2] };
        bool ok = true;
        for (int k = 0; k < 3; k++) {
            if (tri[k] < 0 || tri[k] >= nverts) {
                ok = false;
                break;
            }
            if (!done[tri[k]]) {
                process_vertex(f, p, pos, nrm, col, tc, nm, tri[k], &verts[tri[k]]);
                done[tri[k]] = 1;
            }
        }
        if (!ok)
            continue;
        if (R.flat) {
            // Màu của đỉnh cuối (như OpenGL)
            const Vtx *pv = &verts[tri[2]];
            R.flat_rgba[0] = pv->r;
            R.flat_rgba[1] = pv->g;
            R.flat_rgba[2] = pv->b;
            R.flat_rgba[3] = pv->a;
        }
        draw_triangle(&R, &verts[tri[0]], &verts[tri[1]], &verts[tri[2]]);
    }
    free(verts);
    free(done);
    return NATIVE_OK;
}

// clear(int[] target, int w, int h, int x, int y, int cw, int ch, int argb, boolean color, boolean depth)
static NativeResult M3G_clear(VMThread *t, Value *args, Value *ret) {
    (void)ret;
    Object *target = args[0].l;
    int w = args[1].i, h = args[2].i;
    int x0 = args[3].i, y0 = args[4].i, x1 = x0 + args[5].i, y1 = y0 + args[6].i;
    uint32_t color = (uint32_t)args[7].i;
    bool clear_color = args[8].i != 0, clear_depth = args[9].i != 0;
    if (!target) {
        throw_null(t);
        return NATIVE_EXCEPTION;
    }
    if (ARRAY_LEN(target) < w * h)
        return NATIVE_OK;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > w) x1 = w;
    if (y1 > h) y1 = h;
    zbuf_ensure(w, h);
    uint32_t *px = ARRAY_DATA(target, uint32_t);
    for (int y = y0; y < y1; y++) {
        for (int x = x0; x < x1; x++) {
            if (clear_color)
                px[y * w + x] = color;
            if (clear_depth)
                zbuf[y * w + x] = 1.0f;
        }
    }
    return NATIVE_OK;
}

// blitBackground(int[] target, int w, int h, int vx, int vy, int vw, int vh,
//                int[] img, int iw, int ih, int cropX, int cropY, int cropW, int cropH, int modeX, int modeY, int bg)
static NativeResult M3G_blitBackground(VMThread *t, Value *args, Value *ret) {
    (void)ret;
    Object *target = args[0].l, *img = args[7].l;
    int w = args[1].i, h = args[2].i;
    int vx = args[3].i, vy = args[4].i, vw = args[5].i, vh = args[6].i;
    int iw = args[8].i, ih = args[9].i;
    int cx = args[10].i, cy = args[11].i, cw = args[12].i, ch = args[13].i;
    bool rep_x = args[14].i == 33, rep_y = args[15].i == 33;
    uint32_t bg = (uint32_t)args[16].i;
    if (!target || !img) {
        throw_null(t);
        return NATIVE_EXCEPTION;
    }
    if (ARRAY_LEN(target) < w * h || ARRAY_LEN(img) < iw * ih || cw <= 0 || ch <= 0)
        return NATIVE_OK;
    uint32_t *dst = ARRAY_DATA(target, uint32_t);
    const uint32_t *src = ARRAY_DATA(img, uint32_t);
    for (int y = vy; y < vy + vh; y++) {
        if (y < 0 || y >= h)
            continue;
        // Ảnh crop co giãn cho vừa viewport
        int sy = cy + (int)((int64_t)(y - vy) * ch / vh);
        if (rep_y)
            sy = ((sy % ih) + ih) % ih;
        for (int x = vx; x < vx + vw; x++) {
            if (x < 0 || x >= w)
                continue;
            int sx = cx + (int)((int64_t)(x - vx) * cw / vw);
            if (rep_x)
                sx = ((sx % iw) + iw) % iw;
            uint32_t c = (sx < 0 || sy < 0 || sx >= iw || sy >= ih) ? bg : src[sy * iw + sx];
            dst[y * w + x] = c | 0xff000000u;
        }
    }
    return NATIVE_OK;
}

// Loader.inflate0(byte[] data, int off, int len, int outLen) -> byte[] (zlib)
static NativeResult M3G_inflate0(VMThread *t, Value *args, Value *ret) {
    Object *data = args[0].l;
    jint off = args[1].i, len = args[2].i, out_len = args[3].i;
    if (!data) {
        throw_null(t);
        return NATIVE_EXCEPTION;
    }
    if (off < 0 || len < 0 || off + len > ARRAY_LEN(data) || out_len < 0 || out_len > 64 * 1024 * 1024) {
        throw_new(t, "java/io/IOException", "M3G: section hong");
        return NATIVE_EXCEPTION;
    }
    Object *out = heap_new_prim_array(t, 'B', out_len);
    if (!out)
        return NATIVE_EXCEPTION;
    uLongf dlen = (uLongf)out_len;
    if (uncompress(ARRAY_DATA(out, Bytef), &dlen, ARRAY_DATA(data, Bytef) + off, (uLong)len) != Z_OK ||
        dlen != (uLongf)out_len) {
        throw_new(t, "java/io/IOException", "M3G: giai nen zlib loi");
        return NATIVE_EXCEPTION;
    }
    ret->l = out;
    return NATIVE_OK;
}

void midp_m3g_register(void) {
    native_register("javax/microedition/m3g/Loader", "inflate0", "([BIII)[B", M3G_inflate0);
    const char *G = "javax/microedition/m3g/Graphics3D";
    native_register(G, "renderSubmesh",
                    "([III[F[ILjava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;[I[I)V",
                    M3G_renderSubmesh);
    native_register(G, "clear0", "([IIIIIIIIZZ)V", M3G_clear);
    native_register(G, "blitBackground", "([IIIIIII[IIIIIIIIII)V", M3G_blitBackground);
}

void midp_m3g_shutdown(void) {
    free(zbuf);
    zbuf = NULL;
    zbuf_w = zbuf_h = 0;
}
