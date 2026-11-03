#ifndef GOOME_NO_DRAW
#include <stdlib.h>
#include <string.h>

#include "internal.h"
#include "fonts.h"

// a fused multiply add changes the last bit and with it the pixel hashes
#if defined(__clang__)
#pragma STDC FP_CONTRACT OFF
#elif defined(_MSC_VER)
#pragma fp_contract(off)
#endif

typedef struct {
    double x, y, w, h;
} dr_rect;

static inline double dr_floor(double v)
{
    double t = (double)(long long)v;
    return t > v ? t - 1 : t;
}
static inline double dr_ceil(double v)
{
    double t = (double)(long long)v;
    return t < v ? t + 1 : t;
}
static inline double dr_min(double a, double b) { return a < b ? a : b; }
static inline double dr_max(double a, double b) { return a > b ? a : b; }
static inline int dr_ok(double v) { return v == v && v < 1e15 && v > -1e15; }
static inline long dr_round(double v) { return (long)(v < 0 ? v - 0.5 : v + 0.5); }
static double dr_sqrt(double v)
{
    if (v <= 0) return 0;
    double r = v > 1 ? v / 2 : 1;
    for (int i = 0; i < 40; i++) {
        double n = (r + v / r) / 2;
        if (n == r) break;
        r = n;
    }
    return r;
}

// premultiplied argb (what wayland takes)
static inline uint32_t dr_premul(uint32_t argb, double cover)
{
    double a = ((argb >> 24) & 255) / 255.0 * cover;
    if (a <= 0) return 0;
    uint32_t A = (uint32_t)(a * 255 + 0.5);
    uint32_t R = (uint32_t)(((argb >> 16) & 255) * a + 0.5);
    uint32_t G = (uint32_t)(((argb >> 8) & 255) * a + 0.5);
    uint32_t B = (uint32_t)((argb & 255) * a + 0.5);
    return A << 24 | R << 16 | G << 8 | B;
}

static inline void dr_blend(uint32_t *dst, uint32_t src)
{
    uint32_t sa = src >> 24;
    if (!sa) return;
    if (sa == 255) {
        *dst = src;
        return;
    }
    // t = x + 128, (t + (t >> 8)) >> 8 == (x + 127) / 255
    uint32_t d = *dst, inv = 255 - sa;
    uint32_t rb = (d & 0x00FF00FF) * inv + 0x00800080, ag = (d >> 8 & 0x00FF00FF) * inv + 0x00800080;
    rb = (rb + (rb >> 8 & 0x00FF00FF)) >> 8 & 0x00FF00FF;
    ag = (ag + (ag >> 8 & 0x00FF00FF)) >> 8 & 0x00FF00FF;
    *dst = src + (ag << 8 | rb);
}

static void dr_span(uint32_t *row, int xa, int xb, uint32_t s)
{
    if (!s) return;
    if (s >> 24 == 255) {
        for (int x = xa; x < xb; x++) row[x] = s;
        return;
    }
    uint32_t inv = 255 - (s >> 24);
    for (int x = xa; x < xb; x++) {
        uint32_t d = row[x], rb = (d & 0x00FF00FF) * inv + 0x00800080, ag = (d >> 8 & 0x00FF00FF) * inv + 0x00800080;
        rb = (rb + (rb >> 8 & 0x00FF00FF)) >> 8 & 0x00FF00FF;
        ag = (ag + (ag >> 8 & 0x00FF00FF)) >> 8 & 0x00FF00FF;
        row[x] = s + (ag << 8 | rb);
    }
}

static inline void dr_mark(goome_canvas *c, int x0, int y0, int x1, int y1)
{
    if (c->dx1 <= c->dx0) {
        c->dx0 = x0, c->dy0 = y0, c->dx1 = x1, c->dy1 = y1;
        return;
    }
    if (x0 < c->dx0) c->dx0 = x0;
    if (y0 < c->dy0) c->dy0 = y0;
    if (x1 > c->dx1) c->dx1 = x1;
    if (y1 > c->dy1) c->dy1 = y1;
}

// clip in real px. a zeroed clip means no clip
static inline void dr_kbox(const goome_canvas *c, int *x0, int *y0, int *x1, int *y1)
{
    int a = c->kx0, b = c->ky0, e = c->kx1, f = c->ky1;
    if (!c->nclip && !a && !b && !e && !f) e = c->w, f = c->h;
    a = a < 0 ? 0 : a > c->w ? c->w : a, b = b < 0 ? 0 : b > c->h ? c->h : b;
    e = e > c->w ? c->w : e < a ? a : e, f = f > c->h ? c->h : f < b ? b : f;
    *x0 = a, *y0 = b, *x1 = e, *y1 = f;
}

static inline void dr_plot(goome_canvas *c, int x, int y, uint32_t argb, double cover)
{
    int kx0, ky0, kx1, ky1;
    dr_kbox(c, &kx0, &ky0, &kx1, &ky1);
    if (x < kx0 || y < ky0 || x >= kx1 || y >= ky1 || cover <= 0) return;
    if (cover > 1) cover = 1;
    dr_mark(c, x, y, x + 1, y + 1);
    dr_blend(&c->px[(size_t)y * c->stride + x], dr_premul(argb, cover));
}

static void dr_fill_span(goome_canvas *c, int y, int xa, int xb, uint32_t src)
{
    int kx0, ky0, kx1, ky1;
    dr_kbox(c, &kx0, &ky0, &kx1, &ky1);
    if (y < ky0 || y >= ky1) return;
    if (xa < kx0) xa = kx0;
    if (xb > kx1) xb = kx1;
    if (xb <= xa) return;
    dr_mark(c, xa, y, xb, y + 1);
    dr_span(c->px + (size_t)y * c->stride, xa, xb, src);
}

void goome_canvas_blit(goome_canvas *dst, const goome_canvas *src, double alpha, double dx, double dy)
{
    int a = (int)(alpha * 256 + 0.5);
    if (!dst || !src || !dst->px || !src->px || a <= 0) return;
    if (a > 256) a = 256;
    double fx = (src->ox + dx - dst->ox) * dst->scale, fy = (src->oy + dy - dst->oy) * dst->scale;
    if (!dr_ok(fx) || !dr_ok(fy) || fx < -1e9 || fx > 1e9 || fy < -1e9 || fy > 1e9) return;
    int ox = (int)dr_round(fx), oy = (int)dr_round(fy);
    int h = src->h;
    if (src->dx1 <= src->dx0) return;
    int kx0, ky0, kx1, ky1;
    dr_kbox(dst, &kx0, &ky0, &kx1, &ky1);
    int mx0 = ox + src->dx0, my0 = oy + src->dy0, mx1 = ox + src->dx1, my1 = oy + (src->dy1 < h ? src->dy1 : h);
    if (mx0 < kx0) mx0 = kx0;
    if (my0 < ky0) my0 = ky0;
    if (mx1 > kx1) mx1 = kx1;
    if (my1 > ky1) my1 = ky1;
    if (mx1 > mx0 && my1 > my0) dr_mark(dst, mx0, my0, mx1, my1);
    int ya = ky0 - oy > 0 ? ky0 - oy : 0, yb = ky1 - oy < h ? ky1 - oy : h;
    int xa = kx0 - ox > 0 ? kx0 - ox : 0, xb = kx1 - ox < src->w ? kx1 - ox : src->w;
    for (int y = ya; y < yb; y++) {
        const uint32_t *row = src->px + (size_t)y * src->stride;
        uint32_t *out = dst->px + (size_t)(oy + y) * dst->stride + ox;
        for (int x = xa; x < xb; x++) {
            uint32_t v = row[x];
            if (!v) continue;
            if (a < 256)
                v = (((v >> 24) & 255) * a >> 8) << 24 | (((v >> 16) & 255) * a >> 8) << 16 | (((v >> 8) & 255) * a >> 8) << 8 |
                    ((v & 255) * a >> 8);
            dr_blend(&out[x], v);
        }
    }
}

static void dr_to_px(const goome_canvas *c, double lx, double ly, double *px, double *py)
{
    *px = (lx - c->ox) * c->scale;
    *py = (ly - c->oy) * c->scale;
}

static void dr_px_box(const goome_canvas *c, dr_rect r, int *x0, int *y0, int *x1, int *y1)
{
    double a, b, e, f;
    dr_to_px(c, r.x, r.y, &a, &b);
    dr_to_px(c, r.x + r.w, r.y + r.h, &e, &f);
    if (!dr_ok(a) || !dr_ok(b) || !dr_ok(e) || !dr_ok(f)) {
        *x0 = *y0 = *x1 = *y1 = 0;
        return;
    }
    int kx0, ky0, kx1, ky1;
    dr_kbox(c, &kx0, &ky0, &kx1, &ky1);
    *x0 = a < kx0 ? kx0 : a > kx1 ? kx1 : (int)dr_floor(a), *y0 = b < ky0 ? ky0 : b > ky1 ? ky1 : (int)dr_floor(b);
    *x1 = e > kx1 ? kx1 : e < kx0 ? kx0 : (int)dr_ceil(e), *y1 = f > ky1 ? ky1 : f < ky0 ? ky0 : (int)dr_ceil(f);
}

// wipes what we drew in r
static void dr_erase(goome_canvas *c, dr_rect r)
{
    int x0, y0, x1, y1;
    dr_px_box(c, r, &x0, &y0, &x1, &y1);
    if (x1 <= x0 || y1 <= y0) return;
    dr_mark(c, x0, y0, x1, y1);
    for (int y = y0; y < y1; y++) memset(c->px + (size_t)y * c->stride + x0, 0, (size_t)(x1 - x0) * 4);
}

static void dr_image(goome_canvas *c, dr_rect r, const uint32_t *img, int w, int h)
{
    int x0, y0, x1, y1;
    dr_px_box(c, r, &x0, &y0, &x1, &y1);
    if (x1 <= x0 || y1 <= y0 || r.w <= 0 || r.h <= 0) return;
    dr_mark(c, x0, y0, x1, y1);
    for (int y = y0; y < y1; y++) {
        int sy = (int)((y / c->scale + c->oy - r.y) / r.h * h);
        if (sy < 0 || sy >= h) continue;
        for (int x = x0; x < x1; x++) {
            int sx = (int)((x / c->scale + c->ox - r.x) / r.w * w);
            if (sx >= 0 && sx < w) dr_blend(&c->px[(size_t)y * c->stride + x], dr_premul(img[sy * w + sx], 1));
        }
    }
}

static void dr_rect_fill(goome_canvas *c, dr_rect r, uint32_t argb)
{
    double x0, y0, x1, y1;
    dr_to_px(c, r.x, r.y, &x0, &y0);
    dr_to_px(c, r.x + r.w, r.y + r.h, &x1, &y1);
    if (!dr_ok(x0) || !dr_ok(y0) || !dr_ok(x1) || !dr_ok(y1)) return;
    x0 = dr_max(x0, -1e9), y0 = dr_max(y0, -1e9), x1 = dr_min(x1, 1e9), y1 = dr_min(y1, 1e9);
    int ix0 = (int)dr_floor(x0), iy0 = (int)dr_floor(y0), ix1 = (int)dr_ceil(x1), iy1 = (int)dr_ceil(y1);
    if (ix0 < 0) ix0 = 0;
    if (iy0 < 0) iy0 = 0;
    if (ix1 > c->w) ix1 = c->w;
    if (iy1 > c->h) iy1 = c->h;
    uint32_t solid = dr_premul(argb, 1);
    for (int y = iy0; y < iy1; y++) {
        double cy = dr_min(y + 1, y1) - dr_max(y, y0);
        int fa = (int)dr_ceil(x0), fb = (int)dr_floor(x1);
        if (fa < ix0) fa = ix0;
        if (fb > ix1) fb = ix1;
        if (cy >= 1 && fb > fa) {
            for (int x = ix0; x < fa; x++) dr_plot(c, x, y, argb, dr_min(x + 1, x1) - dr_max(x, x0));
            dr_fill_span(c, y, fa, fb, solid);
            for (int x = fb; x < ix1; x++) dr_plot(c, x, y, argb, dr_min(x + 1, x1) - dr_max(x, x0));
            continue;
        }
        for (int x = ix0; x < ix1; x++) {
            double cx = dr_min(x + 1, x1) - dr_max(x, x0);
            dr_plot(c, x, y, argb, cx * cy);
        }
    }
}

static void dr_frame(goome_canvas *c, dr_rect r, double w, uint32_t argb)
{
    dr_rect_fill(c, (dr_rect){r.x, r.y, r.w, w}, argb);
    dr_rect_fill(c, (dr_rect){r.x, r.y + r.h - w, r.w, w}, argb);
    dr_rect_fill(c, (dr_rect){r.x, r.y + w, w, r.h - 2 * w}, argb);
    dr_rect_fill(c, (dr_rect){r.x + r.w - w, r.y + w, w, r.h - 2 * w}, argb);
}

static void dr_round_rect(goome_canvas *c, dr_rect r, double rad, uint32_t argb)
{
    double x0, y0, x1, y1;
    dr_to_px(c, r.x, r.y, &x0, &y0);
    dr_to_px(c, r.x + r.w, r.y + r.h, &x1, &y1);
    rad *= c->scale;
    if (!dr_ok(x0) || !dr_ok(y0) || !dr_ok(x1) || !dr_ok(y1)) return; // nan turns into garbage ints below
    int ya = (int)dr_max(0, dr_floor(y0)), yb = (int)dr_min(c->h, dr_ceil(y1)), xa = (int)dr_max(0, dr_floor(x0)), xb = (int)dr_min(c->w, dr_ceil(x1));
    uint32_t solid = dr_premul(argb, 1);
    for (int y = ya; y < yb; y++) {
        int inner = y >= y0 + rad && y + 1 <= y1 - rad;
        int fa = (int)dr_ceil(inner ? x0 : x0 + rad), fb = (int)dr_floor(inner ? x1 : x1 - rad);
        if (y < y0 || y + 1 > y1) fa = fb = 0;
        if (fa < xa) fa = xa;
        if (fb > xb) fb = xb;
        for (int x = xa; x < xb; x++) {
            if (x == fa && fb > fa) {
                dr_fill_span(c, y, fa, fb, solid);
                x = fb - 1;
                continue;
            }
            double cov = 0; // 3x3
            for (int sy = 0; sy < 3; sy++)
                for (int sx = 0; sx < 3; sx++) {
                    double px = x + (sx + 0.5) / 3, py = y + (sy + 0.5) / 3;
                    if (px < x0 || px > x1 || py < y0 || py > y1) continue;
                    double qx = px < x0 + rad ? x0 + rad : (px > x1 - rad ? x1 - rad : px);
                    double qy = py < y0 + rad ? y0 + rad : (py > y1 - rad ? y1 - rad : py);
                    double dx = px - qx, dy = py - qy;
                    if (dx * dx + dy * dy <= rad * rad) cov += 1.0 / 9;
                }
            dr_plot(c, x, y, argb, cov);
        }
    }
}

static double dr_seg_dist(double px, double py, double x0, double y0, double x1, double y1)
{
    double dx = x1 - x0, dy = y1 - y0;
    double l2 = dx * dx + dy * dy;
    double t = l2 > 0 ? ((px - x0) * dx + (py - y0) * dy) / l2 : 0;
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    double ex = x0 + t * dx - px, ey = y0 + t * dy - py;
    return dr_sqrt(ex * ex + ey * ey);
}

void goome_draw_line(goome_canvas *c, double lx0, double ly0, double lx1, double ly1, double w, uint32_t argb)
{
    double x0, y0, x1, y1;
    dr_to_px(c, lx0, ly0, &x0, &y0);
    dr_to_px(c, lx1, ly1, &x1, &y1);
    double hw = w * c->scale / 2;
    if (!dr_ok(x0) || !dr_ok(y0) || !dr_ok(x1) || !dr_ok(y1) || !dr_ok(hw)) return;
    int bx0 = (int)dr_max(0, dr_floor(dr_min(x0, x1) - hw - 1)), bx1 = (int)dr_min(c->w, dr_ceil(dr_max(x0, x1) + hw + 1));
    int by0 = (int)dr_max(0, dr_floor(dr_min(y0, y1) - hw - 1)), by1 = (int)dr_min(c->h, dr_ceil(dr_max(y0, y1) + hw + 1));
    for (int y = by0; y < by1; y++)
        for (int x = bx0; x < bx1; x++) {
            double d = dr_seg_dist(x + 0.5, y + 0.5, x0, y0, x1, y1);
            double cov = hw + 0.5 - d; // ~1px soft edge
            dr_plot(c, x, y, argb, cov);
        }
}

static double dr_edge(double ax, double ay, double bx, double by, double px, double py)
{
    return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
}

void goome_draw_triangle(goome_canvas *c, double ax, double ay, double bx, double by, double cx, double cy, uint32_t argb)
{
    dr_to_px(c, ax, ay, &ax, &ay);
    dr_to_px(c, bx, by, &bx, &by);
    dr_to_px(c, cx, cy, &cx, &cy);
    if (dr_edge(ax, ay, bx, by, cx, cy) < 0) {
        double tx = bx, ty = by;
        bx = cx;
        by = cy;
        cx = tx;
        cy = ty;
    }
    if (!dr_ok(ax + ay + bx + by + cx + cy)) return;
    int x0 = (int)dr_max(0, dr_floor(dr_min(ax, dr_min(bx, cx)))), x1 = (int)dr_min(c->w, dr_ceil(dr_max(ax, dr_max(bx, cx))));
    int y0 = (int)dr_max(0, dr_floor(dr_min(ay, dr_min(by, cy)))), y1 = (int)dr_min(c->h, dr_ceil(dr_max(ay, dr_max(by, cy))));
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) {
            int in = 0;
            for (int s = 0; s < 4; s++) {
                double px = x + (s & 1 ? 0.75 : 0.25), py = y + (s & 2 ? 0.75 : 0.25);
                if (dr_edge(ax, ay, bx, by, px, py) >= 0 && dr_edge(bx, by, cx, cy, px, py) >= 0 &&
                    dr_edge(cx, cy, ax, ay, px, py) >= 0)
                    in++;
            }
            dr_plot(c, x, y, argb, in / 4.0);
        }
}

static int dr_font_pick;
void goome_set_font(int k) { dr_font_pick = k >= 0 && k < GM_NFONTS ? k : 0; }
static const gm_face *dr_face(int big) { return gm_fonts[dr_font_pick][!!big]; }


static double dr_text_width(const char *s, int big)
{
    const gm_face *f = dr_face(big);
    double w = 0;
    for (; *s; s++) {
        int ch = (unsigned char)*s;
        if (ch < 32 || ch > 126) ch = '?';
        w += f->glyphs[ch - 32].adv;
    }
    return w;
}


static void dr_glyph(goome_canvas *c, const gm_face *f, const gm_glyph *g, int x, int y, int sc, uint32_t argb)
{
    int b = f->px * sc;
    for (int gy = 0; gy < g->h; gy++)
        for (int gx = 0; gx < g->w; gx++) {
            int i = g->off + gy * g->w + gx, a = (f->bits[i >> 1] >> (i & 1 ? 4 : 0)) & 15;
            if (!a) continue;
            for (int dy = 0; dy < b; dy++)
                for (int dx = 0; dx < b; dx++) dr_plot(c, x + g->x * sc + gx * b + dx, y + g->y * sc + gy * b + dy, argb, a / 15.0);
        }
}

double goome_draw_text(goome_canvas *c, double lx, double ly, const char *s, int big, uint32_t argb)
{
    const gm_face *f = dr_face(big);
    double x, y;
    dr_to_px(c, lx, ly, &x, &y);
    int sc = (c->scale >= 1.75 ? 2 : 1); // hidpi
    double start = lx;
    uint32_t sh = (argb & 0xFF000000) | (argb >> 2 & 0x3F3F3F);
    // dark text on a light button looks smudged with a shadow
    int lum = (argb >> 16 & 255) + (argb >> 8 & 255) + (argb & 255);
    for (; *s; s++) {
        int ch = (unsigned char)*s;
        if (ch < 32 || ch > 126) ch = '?';
        const gm_glyph *g = &f->glyphs[ch - 32];
        if (f->px > 1 && lum > 300) dr_glyph(c, f, g, (int)x + f->px * sc, (int)y + f->px * sc, sc, sh);
        dr_glyph(c, f, g, (int)x, (int)y, sc, argb);
        x += g->adv * sc;
        lx += g->adv * sc / c->scale;
    }
    return lx - start;
}

double goome_text_width(const goome_canvas *c, const char *s, int big)
{
    double k = c && c->scale >= 1.75 ? 2 : 1; // font doubles on hidpi, it doesnt scale
    return dr_text_width(s, big) * k / (c && c->scale > 0 ? c->scale : 1);
}

int goome_font_count(void) { return GM_NFONTS; }

void goome_clear(goome_canvas *c)
{
    if (c) gm_canvas_clear(c);
}

void goome_draw_rect(goome_canvas *c, double x, double y, double w, double h, uint32_t argb) { dr_rect_fill(c, (dr_rect){x, y, w, h}, argb); }

void goome_draw_round_rect(goome_canvas *c, double x, double y, double w, double h, double radius, uint32_t argb)
{
    dr_round_rect(c, (dr_rect){x, y, w, h}, radius, argb);
}

void goome_draw_frame(goome_canvas *c, double x, double y, double w, double h, double width, uint32_t argb)
{
    dr_frame(c, (dr_rect){x, y, w, h}, width, argb);
}

void goome_draw_image(goome_canvas *c, double x, double y, double w, double h, const uint32_t *img, int iw, int ih)
{
    if (img && iw > 0 && ih > 0) dr_image(c, (dr_rect){x, y, w, h}, img, iw, ih);
}

void goome_erase(goome_canvas *c, double x, double y, double w, double h) { dr_erase(c, (dr_rect){x, y, w, h}); }

static inline int dr_fl(double v) { return v < -1e9 ? -1000000000 : v > 1e9 ? 1000000000 : (int)dr_floor(v); }
static inline int dr_cl(double v) { return v < -1e9 ? -1000000000 : v > 1e9 ? 1000000000 : (int)dr_ceil(v); }

// 1/sqrt guess from the exponent bits, then newton without divides, ~1e-11 off
static double dr_rt(double v)
{
    if (!(v > 0) || v > 1e300) return 0;
    uint64_t b;
    double y, h = 0.5 * v;
    memcpy(&b, &v, 8);
    b = 0x5FE6EB50C7B537A9ull - (b >> 1);
    memcpy(&y, &b, 8);
    for (int i = 0; i < 3; i++) y = y * (1.5 - h * y * y);
    return v * y;
}

static double dr_sin(double x)
{
    if (!dr_ok(x)) return 0;
    x -= 6.283185307179586 * dr_floor(x / 6.283185307179586 + 0.5);
    if (x > 1.5707963267948966) x = 3.141592653589793 - x;
    else if (x < -1.5707963267948966) x = -3.141592653589793 - x;
    double q = x * x;
    return x * (1 + q * (-1.0 / 6 + q * (1.0 / 120 + q * (-1.0 / 5040 + q * (1.0 / 362880 + q * (-1.0 / 39916800 + q / 6227020800.0))))));
}
static double dr_cos(double x) { return dr_sin(x + 1.5707963267948966); }

// abramowitz stegun 7.1.27, 5e-4 off at worst
static double dr_erf(double x)
{
    double s = x < 0 ? -1 : 1, a = x * s;
    double t = 1 + a * (0.278393 + a * (0.230389 + a * (0.000972 + a * 0.078108)));
    t *= t;
    t *= t;
    return s * (1 - 1 / t);
}

static inline uint32_t dr_mul(uint32_t p, int k)
{
    uint32_t rb = (p & 0x00FF00FF) * (uint32_t)k + 0x00800080, ag = (p >> 8 & 0x00FF00FF) * (uint32_t)k + 0x00800080;
    rb = (rb + (rb >> 8 & 0x00FF00FF)) >> 8 & 0x00FF00FF;
    ag = (ag + (ag >> 8 & 0x00FF00FF)) >> 8 & 0x00FF00FF;
    return ag << 8 | rb;
}

// rounded rect in real px, corners tl tr br bl
typedef struct {
    double x0, y0, x1, y1, r[4];
} dr_rr;

static void dr_fit(dr_rr *q, const double *v)
{
    double w = q->x1 - q->x0, h = q->y1 - q->y0, f = 1;
    if (v[0] + v[1] > w) f = dr_min(f, w / (v[0] + v[1]));
    if (v[3] + v[2] > w) f = dr_min(f, w / (v[3] + v[2]));
    if (v[0] + v[3] > h) f = dr_min(f, h / (v[0] + v[3]));
    if (v[1] + v[2] > h) f = dr_min(f, h / (v[1] + v[2]));
    for (int i = 0; i < 4; i++) q->r[i] = v[i] * f;
}

static int dr_mkrr(const goome_canvas *c, double x, double y, double w, double h, goome_radii r, dr_rr *q)
{
    if (!c || !c->px || !(w > 0) || !(h > 0)) return 0;
    dr_to_px(c, x, y, &q->x0, &q->y0);
    dr_to_px(c, x + w, y + h, &q->x1, &q->y1);
    if (!dr_ok(q->x0) || !dr_ok(q->y0) || !dr_ok(q->x1) || !dr_ok(q->y1) || q->x1 <= q->x0 || q->y1 <= q->y0) return 0;
    double v[4] = {r.tl, r.tr, r.br, r.bl};
    for (int i = 0; i < 4; i++) v[i] = dr_ok(v[i]) && v[i] > 0 ? v[i] * c->scale : 0;
    dr_fit(q, v);
    return 1;
}

static int dr_circ(const goome_canvas *c, double cx, double cy, double r, dr_rr *q)
{
    if (!c || !c->px || !(r > 0) || !dr_ok(r)) return 0;
    double x, y;
    dr_to_px(c, cx, cy, &x, &y);
    if (!dr_ok(x) || !dr_ok(y)) return 0;
    r *= c->scale;
    q->x0 = x - r, q->y0 = y - r, q->x1 = x + r, q->y1 = y + r;
    for (int i = 0; i < 4; i++) q->r[i] = r;
    return 1;
}

// d > 0 shrinks, inner corners lose d, outer ones gain it
static int dr_inset(const dr_rr *a, double d, dr_rr *o)
{
    o->x0 = a->x0 + d, o->y0 = a->y0 + d, o->x1 = a->x1 - d, o->y1 = a->y1 - d;
    for (int i = 0; i < 4; i++) o->r[i] = a->r[i] - d > 0 ? a->r[i] - d : 0;
    return o->x1 > o->x0 && o->y1 > o->y0;
}

// row y: nothing outside [lo, hi), full cover in [slo, shi)
static void dr_rr_row(const dr_rr *q, int y, int *lo, int *slo, int *shi, int *hi)
{
    *lo = *hi = *slo = *shi = 0;
    if (y + 1 <= q->y0 || y >= q->y1) return;
    double py = y + 0.5;
    int l = dr_fl(q->x0), r = dr_cl(q->x1), sl = dr_cl(q->x0), sr = dr_fl(q->x1);
    for (int k = 0; k < 4; k++) {
        double rad = q->r[k];
        if (rad <= 0) continue;
        double dy = k < 2 ? q->y0 + rad - py : py - (q->y1 - rad);
        if (dy <= 0) continue;
        double t = (rad + 0.5) * (rad + 0.5) - dy * dy, u = (rad - 0.5) * (rad - 0.5) - dy * dy;
        double e = t > 0 ? dr_rt(t) : 0, f = rad > 0.5 && u > 0 ? dr_rt(u) : 0;
        if (k == 0 || k == 3) {
            double cx = q->x0 + rad;
            int a = dr_fl(cx - e - 0.5), b = dr_cl(cx - f - 0.5);
            if (a > l) l = a;
            if (b > sl) sl = b;
        } else {
            double cx = q->x1 - rad;
            int a = dr_cl(cx + e + 0.5), b = dr_fl(cx + f + 0.5);
            if (a < r) r = a;
            if (b < sr) sr = b;
        }
    }
    *lo = l, *hi = r;
    if (y >= q->y0 && y + 1 <= q->y1 && sr > sl) *slo = sl, *shi = sr;
}

static double dr_rr_cov(const dr_rr *q, double px, double py)
{
    double a = dr_min(px + 0.5, q->x1) - dr_max(px - 0.5, q->x0), b = dr_min(py + 0.5, q->y1) - dr_max(py - 0.5, q->y0);
    if (a <= 0 || b <= 0) return 0;
    double v = (a > 1 ? 1 : a) * (b > 1 ? 1 : b);
    for (int k = 0; k < 4; k++) {
        double rad = q->r[k];
        if (rad <= 0) continue;
        double dx = k == 0 || k == 3 ? q->x0 + rad - px : px - (q->x1 - rad);
        double dy = k < 2 ? q->y0 + rad - py : py - (q->y1 - rad);
        if (dx <= 0 || dy <= 0) continue;
        double e = 0.5 + rad - dr_rt(dx * dx + dy * dy);
        if (e <= 0) return 0;
        if (e < v) v = e;
    }
    return v;
}

typedef struct {
    int n, *st, *cnt, *w; // taps per dest px, weights sum to 65536
    int *a, *b, *f;       // the same as two taps with an 8 bit weight
} dr_axis;

typedef struct {
    int kind; // 0 color, 1 gradient, 2 bitmap
    uint32_t col;
    double g0[4], gd[4], t0, tk;
    int side, gy, gx0, gw;
    uint32_t gc[4], *gt;
    const uint32_t *bp;
    int bs, ox, oy, fast, tint, tm[4];
    dr_axis ax, ay;
} dr_paint;

static const unsigned char dr_bayer[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};

static uint32_t dr_grad_at(const dr_paint *p, double t, int th)
{
    if (!(t > 0)) t = 0;
    if (t > 1) t = 1;
    double d = (th + 0.5) / 16;
    int v[4];
    for (int k = 0; k < 4; k++) {
        v[k] = (int)(p->g0[k] + p->gd[k] * t + d);
        if (v[k] > 255) v[k] = 255;
        if (v[k] < 0) v[k] = 0;
        if (k && v[k] > v[0]) v[k] = v[0];
    }
    return (uint32_t)v[0] << 24 | (uint32_t)v[1] << 16 | (uint32_t)v[2] << 8 | (uint32_t)v[3];
}

static inline uint32_t dr_grad_px(dr_paint *p, int x, int y)
{
    if (p->side) return p->gt ? p->gt[(y & 3) * p->gw + x - p->gx0] : dr_grad_at(p, (x + 0.5 - p->t0) * p->tk, dr_bayer[y & 3][x & 3]);
    if (p->gy != y) {
        double t = (y + 0.5 - p->t0) * p->tk;
        for (int i = 0; i < 4; i++) p->gc[i] = dr_grad_at(p, t, dr_bayer[y & 3][i]);
        p->gy = y;
    }
    return p->gc[x & 3];
}

static inline uint32_t dr_lerp(uint32_t a, uint32_t b, int w)
{
    uint32_t rb = ((a & 0x00FF00FF) * (uint32_t)(256 - w) + (b & 0x00FF00FF) * (uint32_t)w + 0x00800080) >> 8 & 0x00FF00FF;
    uint32_t ag = ((a >> 8 & 0x00FF00FF) * (uint32_t)(256 - w) + (b >> 8 & 0x00FF00FF) * (uint32_t)w + 0x00800080) >> 8 & 0x00FF00FF;
    return ag << 8 | rb;
}

static inline uint32_t dr_tint(const dr_paint *p, uint32_t v)
{
    uint32_t a = (v >> 24) * (uint32_t)p->tm[0] + 128, r = (v >> 16 & 255) * (uint32_t)p->tm[1] + 128;
    uint32_t g = (v >> 8 & 255) * (uint32_t)p->tm[2] + 128, b = (v & 255) * (uint32_t)p->tm[3] + 128;
    return ((a + (a >> 8)) >> 8) << 24 | ((r + (r >> 8)) >> 8) << 16 | ((g + (g >> 8)) >> 8) << 8 | ((b + (b >> 8)) >> 8);
}

static uint32_t dr_bm_px(const dr_paint *p, int x, int y)
{
    int i = x - p->ox, j = y - p->oy;
    uint32_t v;
    if (p->fast) {
        const uint32_t *r0 = p->bp + (size_t)p->ay.a[j] * p->bs, *r1 = p->bp + (size_t)p->ay.b[j] * p->bs;
        int xa = p->ax.a[i], xb = p->ax.b[i], wx = p->ax.f[i], wy = p->ay.f[j];
        uint32_t t = wx ? dr_lerp(r0[xa], r0[xb], wx) : r0[xa];
        v = wy ? dr_lerp(t, wx ? dr_lerp(r1[xa], r1[xb], wx) : r1[xa], wy) : t;
    } else {
        uint64_t s[4] = {0, 0, 0, 0};
        int ys = p->ay.st[j], yn = p->ay.cnt[j], xs = p->ax.st[i], xn = p->ax.cnt[i];
        const int *wx = p->ax.w + p->ax.st[p->ax.n + i], *wy = p->ay.w + p->ay.st[p->ay.n + j];
        for (int a = 0; a < yn; a++) {
            const uint32_t *row = p->bp + (size_t)(ys + a) * p->bs + xs;
            uint32_t r[4] = {0, 0, 0, 0};
            for (int b = 0; b < xn; b++) {
                uint32_t q = row[b], k = (uint32_t)wx[b];
                r[0] += (q >> 24) * k, r[1] += (q >> 16 & 255) * k, r[2] += (q >> 8 & 255) * k, r[3] += (q & 255) * k;
            }
            uint64_t k = (uint64_t)wy[a];
            for (int c = 0; c < 4; c++) s[c] += r[c] * k;
        }
        v = 0;
        for (int c = 0; c < 4; c++) {
            uint64_t o = (s[c] + (1ull << 31)) >> 32;
            v |= (uint32_t)(o > 255 ? 255 : o) << (24 - 8 * c);
        }
    }
    return p->tint ? dr_tint(p, v) : v;
}

// fills [xa, xb) on row y, cov NULL = fully covered
static void dr_run(goome_canvas *c, dr_paint *p, int y, int xa, int xb, const uint8_t *cov)
{
    if (xb <= xa) return;
    dr_mark(c, xa, y, xb, y + 1);
    uint32_t *row = c->px + (size_t)y * c->stride;
    if (p->kind == 0) {
        uint32_t s = p->col;
        if (!cov)
            dr_span(row, xa, xb, s);
        else
            for (int x = xa; x < xb; x++) {
                int k = cov[x - xa];
                if (k) dr_blend(&row[x], k == 255 ? s : dr_mul(s, k));
            }
        return;
    }
    if (p->kind == 1 && (!p->side || p->gt)) {
        const uint32_t *t = p->gc;
        if (p->side)
            t = p->gt + (y & 3) * p->gw;
        else
            dr_grad_px(p, xa, y);
        for (int x = xa; x < xb; x++) {
            int k = cov ? cov[x - xa] : 255;
            uint32_t s = p->side ? t[x - p->gx0] : t[x & 3];
            if (k) dr_blend(&row[x], k == 255 ? s : dr_mul(s, k));
        }
        return;
    }
    if (p->kind == 2 && p->fast) {
        int j = y - p->oy, wy = p->ay.f[j];
        const uint32_t *r0 = p->bp + (size_t)p->ay.a[j] * p->bs, *r1 = p->bp + (size_t)p->ay.b[j] * p->bs;
        for (int x = xa; x < xb; x++) {
            int k = cov ? cov[x - xa] : 255, i = x - p->ox;
            if (!k) continue;
            int a = p->ax.a[i], b = p->ax.b[i], wx = p->ax.f[i];
            uint32_t s = wx ? dr_lerp(r0[a], r0[b], wx) : r0[a];
            if (wy) s = dr_lerp(s, wx ? dr_lerp(r1[a], r1[b], wx) : r1[a], wy);
            if (p->tint) s = dr_tint(p, s);
            dr_blend(&row[x], k == 255 ? s : dr_mul(s, k));
        }
        return;
    }
    for (int x = xa; x < xb; x++) {
        int k = cov ? cov[x - xa] : 255;
        if (!k) continue;
        uint32_t s = p->kind == 1 ? dr_grad_px(p, x, y) : dr_bm_px(p, x, y);
        dr_blend(&row[x], k == 255 ? s : dr_mul(s, k));
    }
}

typedef struct {
    int kind; // 0 shape, 1 shape minus hole, 3 arc
    dr_rr out, sol, hole, shape;
    int has_sol, has_hole;
    double k, cx, cy, r, hw, sx, sy, ex, ey;
    int big;
} dr_geo;

static double dr_arc_cov(const dr_geo *g, double px, double py)
{
    double dx = px - g->cx, dy = py - g->cy, d;
    double c1 = g->sx * dy - g->sy * dx, c2 = dx * g->ey - dy * g->ex;
    if (g->big ? c1 >= 0 || c2 >= 0 : c1 >= 0 && c2 >= 0) {
        d = dr_rt(dx * dx + dy * dy) - g->r;
        if (d < 0) d = -d;
    } else {
        double ax = dx - g->r * g->sx, ay = dy - g->r * g->sy, bx = dx - g->r * g->ex, by = dy - g->r * g->ey;
        d = dr_rt(dr_min(ax * ax + ay * ay, bx * bx + by * by));
    }
    return 0.5 + g->hw - d;
}

static double dr_geo_cov(const dr_geo *g, double px, double py)
{
    switch (g->kind) {
    case 0: return dr_rr_cov(&g->shape, px, py);
    case 1: return dr_rr_cov(&g->shape, px, py) - dr_rr_cov(&g->hole, px, py);
    default: return dr_arc_cov(g, px, py);
    }
}

static void dr_shape(goome_canvas *c, const dr_geo *g, dr_paint *p)
{
    int kx0, ky0, kx1, ky1;
    dr_kbox(c, &kx0, &ky0, &kx1, &ky1);
    int ya = dr_fl(g->out.y0), yb = dr_cl(g->out.y1);
    if (ya < ky0) ya = ky0;
    if (yb > ky1) yb = ky1;
    uint8_t cv[256];
    for (int y = ya; y < yb; y++) {
        int lo, hi, slo = 0, shi = 0, a, b, h[4] = {0, 0, 0, 0};
        dr_rr_row(&g->out, y, &lo, &a, &b, &hi);
        if (lo < kx0) lo = kx0;
        if (hi > kx1) hi = kx1;
        if (hi <= lo) continue;
        if (g->has_sol) dr_rr_row(&g->sol, y, &a, &slo, &shi, &b);
        if (g->has_hole) dr_rr_row(&g->hole, y, &h[0], &h[1], &h[2], &h[3]);
        int bp[8] = {lo, hi, slo, shi, h[0], h[1], h[2], h[3]}, pt[8], n = 0;
        for (int i = 0; i < 8; i++) {
            int v = bp[i] < lo ? lo : bp[i] > hi ? hi : bp[i], j = n;
            for (int k = 0; k < n; k++)
                if (pt[k] >= v) {
                    j = k;
                    break;
                }
            if (j < n && pt[j] == v) continue;
            for (int k = n; k > j; k--) pt[k] = pt[k - 1];
            pt[j] = v, n++;
        }
        for (int i = 0; i + 1 < n; i++) {
            int x0 = pt[i], x1 = pt[i + 1];
            if (h[1] < h[2] && x0 >= h[1] && x1 <= h[2]) continue;
            if (slo < shi && x0 >= slo && x1 <= shi && (h[3] <= h[0] || x1 <= h[0] || x0 >= h[3])) {
                dr_run(c, p, y, x0, x1, NULL);
                continue;
            }
            for (int x = x0; x < x1; x += 256) {
                int m = x1 - x < 256 ? x1 - x : 256, any = 0;
                for (int j = 0; j < m; j++) {
                    double v = dr_geo_cov(g, x + j + 0.5, y + 0.5);
                    cv[j] = v <= 0 ? 0 : v >= 1 ? 255 : (uint8_t)(v * 255 + 0.5);
                    any |= cv[j];
                }
                if (any) dr_run(c, p, y, x, x + m, cv);
            }
        }
    }
}

static void dr_solid(dr_paint *p, uint32_t argb)
{
    memset(p, 0, sizeof(*p));
    p->col = dr_premul(argb, 1);
}

static void dr_fill_rr(goome_canvas *c, const dr_rr *q, dr_paint *p)
{
    dr_geo g;
    memset(&g, 0, sizeof(g));
    g.out = g.sol = g.shape = *q;
    g.has_sol = 1;
    dr_shape(c, &g, p);
}

static void dr_minus(goome_canvas *c, const dr_rr *q, const dr_rr *hole, dr_paint *p)
{
    dr_geo g;
    memset(&g, 0, sizeof(g));
    g.kind = 1;
    g.out = g.sol = g.shape = *q;
    g.hole = *hole;
    g.has_sol = g.has_hole = 1;
    dr_shape(c, &g, p);
}

goome_radii goome_radius(double r) { return (goome_radii){r, r, r, r}; }

void goome_clip_push(goome_canvas *c, double x, double y, double w, double h)
{
    if (!c) return;
    if (c->nclip >= 16) {
        c->nclip++;
        return;
    }
    int x0, y0, x1, y1;
    dr_kbox(c, &x0, &y0, &x1, &y1);
    int *s = c->clips[c->nclip++];
    s[0] = x0, s[1] = y0, s[2] = x1, s[3] = y1;
    double a, b, e, f;
    dr_to_px(c, x, y, &a, &b);
    dr_to_px(c, x + w, y + h, &e, &f);
    if (!(w > 0) || !(h > 0) || !dr_ok(a) || !dr_ok(b) || !dr_ok(e) || !dr_ok(f)) {
        c->kx0 = c->kx1 = x0, c->ky0 = c->ky1 = y0;
        return;
    }
    int ia = dr_fl(a + 0.5), ib = dr_fl(b + 0.5), ie = dr_fl(e + 0.5), ig = dr_fl(f + 0.5);
    if (ia > x0) x0 = ia;
    if (ib > y0) y0 = ib;
    if (ie < x1) x1 = ie;
    if (ig < y1) y1 = ig;
    if (x1 < x0) x1 = x0;
    if (y1 < y0) y1 = y0;
    c->kx0 = x0, c->ky0 = y0, c->kx1 = x1, c->ky1 = y1;
}

void goome_clip_pop(goome_canvas *c)
{
    if (!c || c->nclip <= 0) return;
    if (--c->nclip < 16) {
        int *s = c->clips[c->nclip];
        c->kx0 = s[0], c->ky0 = s[1], c->kx1 = s[2], c->ky1 = s[3];
    }
}

void goome_draw_rrect(goome_canvas *c, double x, double y, double w, double h, goome_radii r, uint32_t argb)
{
    dr_rr q;
    dr_paint p;
    if (!dr_mkrr(c, x, y, w, h, r, &q)) return;
    dr_solid(&p, argb);
    dr_fill_rr(c, &q, &p);
}

void goome_draw_rrect_line(goome_canvas *c, double x, double y, double w, double h, goome_radii r, double width, uint32_t argb)
{
    dr_rr q, in;
    dr_paint p;
    if (!(width > 0) || !dr_mkrr(c, x, y, w, h, r, &q)) return;
    dr_solid(&p, argb);
    if (dr_inset(&q, width * c->scale, &in))
        dr_minus(c, &q, &in, &p);
    else
        dr_fill_rr(c, &q, &p);
}

void goome_draw_rrect_grad(goome_canvas *c, double x, double y, double w, double h, goome_radii r, uint32_t from, uint32_t to, int sideways)
{
    dr_rr q;
    dr_paint p;
    if (!dr_mkrr(c, x, y, w, h, r, &q)) return;
    memset(&p, 0, sizeof(p));
    p.kind = 1, p.side = !!sideways, p.gy = -1;
    double a = (from >> 24) / 255.0, b = (to >> 24) / 255.0;
    for (int k = 0; k < 4; k++) {
        int sh = 24 - 8 * k;
        double u = (from >> sh & 255) * (k ? a : 1), v = (to >> sh & 255) * (k ? b : 1);
        p.g0[k] = u, p.gd[k] = v - u;
    }
    p.t0 = sideways ? q.x0 : q.y0;
    p.tk = 1 / (sideways ? q.x1 - q.x0 : q.y1 - q.y0);
    if (sideways) {
        int kx0, ky0, kx1, ky1;
        dr_kbox(c, &kx0, &ky0, &kx1, &ky1);
        p.gx0 = dr_fl(q.x0) < kx0 ? kx0 : dr_fl(q.x0);
        p.gw = (dr_cl(q.x1) > kx1 ? kx1 : dr_cl(q.x1)) - p.gx0;
        p.gt = p.gw > 0 ? malloc((size_t)p.gw * 4 * sizeof(uint32_t)) : NULL;
        for (int i = 0; p.gt && i < p.gw; i++) {
            double u = (p.gx0 + i + 0.5 - p.t0) * p.tk;
            for (int j = 0; j < 4; j++) p.gt[j * p.gw + i] = dr_grad_at(&p, u, dr_bayer[j][(p.gx0 + i) & 3]);
        }
    }
    dr_fill_rr(c, &q, &p);
    free(p.gt);
}

static double dr_sh_d(const dr_geo *g, double px, double py)
{
    double qx = px - g->cx, qy = py - g->cy;
    qx = (qx < 0 ? -qx : qx) - g->sx, qy = (qy < 0 ? -qy : qy) - g->sy;
    double d = dr_rt(dr_max(qx, 0) * dr_max(qx, 0) + dr_max(qy, 0) * dr_max(qy, 0)) - g->r;
    // 4-norm-ish min, a hard min left creases on the diagonals
    if (qx < 0 && qy < 0) {
        double a = qx * qx, b = qy * qy;
        d -= qx * qy / dr_rt(dr_rt(a * a + b * b));
    }
    return d;
}

static inline uint32_t dr_sh_col(const dr_geo *g, uint32_t col, double d)
{
    double v = 0.5 - 0.5 * dr_erf(d * g->k);
    int k = v <= 0 ? 0 : v >= 1 ? 255 : (int)(v * 255 + 0.5);
    return k == 255 ? col : k ? dr_mul(col, k) : 0;
}

// pixels with |center - cx| < t
static void dr_sh_in(double cx, double t, int *a, int *b)
{
    if (t > 0) {
        *a = dr_fl(cx - t - 0.5) + 1, *b = dr_cl(cx + t - 0.5);
        return;
    }
    *a = *b = dr_fl(cx + 0.5);
}

// sigma = blur / 2 like css
void goome_draw_shadow(goome_canvas *c, double x, double y, double w, double h, double radius, double blur, uint32_t argb)
{
    dr_geo g;
    memset(&g, 0, sizeof(g));
    if (!dr_mkrr(c, x, y, w, h, goome_radius(radius), &g.shape) || !(argb >> 24)) return;
    double sg = dr_ok(blur) && blur > 0 ? blur * c->scale / 2 : 0;
    if (sg < 0.4) sg = 0.4;
    if (sg > 1e6) return;
    double t = 1, al = argb >> 24;
    while (t < 4 && al * (0.5 - 0.5 * dr_erf(t * 0.7071067811865476)) >= 0.5) t += 0.25; // past this it rounds to nothing
    double ext = t * sg;
    g.k = 1 / (sg * 1.4142135623730951);
    g.cx = (g.shape.x0 + g.shape.x1) / 2, g.cy = (g.shape.y0 + g.shape.y1) / 2;
    g.r = dr_rt(g.shape.r[0] * g.shape.r[0] + 4 * sg * sg);
    g.r = dr_min(g.r, dr_min(g.shape.x1 - g.shape.x0, g.shape.y1 - g.shape.y0) / 2);
    g.sx = (g.shape.x1 - g.shape.x0) / 2 - g.r, g.sy = (g.shape.y1 - g.shape.y0) / 2 - g.r;
    dr_inset(&g.shape, -ext - 1, &g.out);
    int kx0, ky0, kx1, ky1;
    dr_kbox(c, &kx0, &ky0, &kx1, &ky1);
    int bx0 = dr_fl(g.out.x0), bx1 = dr_cl(g.out.x1), ya = dr_fl(g.out.y0), yb = dr_cl(g.out.y1);
    if (bx0 < kx0) bx0 = kx0;
    if (bx1 > kx1) bx1 = kx1;
    if (ya < ky0) ya = ky0;
    if (yb > ky1) yb = ky1;
    if (bx1 <= bx0 || yb <= ya) return;
    // one color per column where only x matters
    uint32_t col = dr_premul(argb, 1), *tab = malloc((size_t)(bx1 - bx0 + 1) * (sizeof(uint32_t) + sizeof(double)));
    if (!tab) return;
    double *qx2 = (double *)(void *)(tab + (bx1 - bx0 + 1) / 2 * 2);
    for (int i = bx0; i < bx1; i++) {
        double u = i + 0.5 - g.cx, qx = (u < 0 ? -u : u) - g.sx;
        tab[i - bx0] = dr_sh_col(&g, col, qx - g.r);
        qx2[i - bx0] = qx > 0 ? qx * qx : 0;
    }
    // corners only depend on distance to the corner center, 1/8 px steps
    double rmax = g.r + ext + 2;
    int nrad = (int)(rmax * 8) + 2;
    uint32_t *rad = nrad < (1 << 22) ? malloc((size_t)nrad * sizeof(uint32_t)) : NULL;
    for (int i = 0; rad && i < nrad; i++) rad[i] = dr_sh_col(&g, col, i / 8.0 - g.r);
    // box symmetric on the pixel grid -> do one corner and mirror it
    int sym = g.cx > -1e8 && g.cx < 1e8 && g.cy > -1e8 && g.cy < 1e8 && 2 * g.cx == dr_floor(2 * g.cx) && 2 * g.cy == dr_floor(2 * g.cy), cw = (int)rmax + 3, ch = cw;
    int mx2 = sym ? (int)(2 * g.cx) - 1 : 0, my2 = sym ? (int)(2 * g.cy) - 1 : 0, cx0 = dr_fl(g.out.x0), cy0 = dr_fl(g.out.y0);
    uint32_t *cc = sym && rad && cw < 1024 ? malloc((size_t)cw * ch * sizeof(uint32_t)) : NULL;
    for (int i = 0; cc && i < cw * ch; i++) cc[i] = 0x00FFFFFF; // never a premultiplied color
    dr_paint p;
    dr_solid(&p, 0);
    double deep = (ext - g.r) / 0.84 + 1;
    if (deep < 0) deep = 0;
    for (int y = ya; y < yb; y++) {
        int lo, hi, a, b, s[8];
        dr_rr_row(&g.out, y, &lo, &a, &b, &hi);
        if (lo < bx0) lo = bx0;
        if (hi > bx1) hi = bx1;
        if (hi <= lo) continue;
        double py = y + 0.5, v = py - g.cy, qy = (v < 0 ? -v : v) - g.sy;
        p.col = dr_sh_col(&g, col, qy - g.r);
        int md[7];
        s[0] = lo, s[7] = hi;
        dr_sh_in(g.cx, g.sx, &s[1], &s[6]);
        if (qy >= 0) {
            s[2] = s[3] = s[1], s[4] = s[5] = s[6];
            md[0] = 2, md[1] = md[2] = 1, md[3] = 1, md[4] = md[5] = 1, md[6] = 2;
        } else {
            dr_sh_in(g.cx, g.sx + qy / 4, &s[2], &s[5]);
            double m = -4 * qy;
            if (-qy >= deep && deep < m) m = dr_max(deep, -qy / 4);
            dr_sh_in(g.cx, g.sx - m, &s[3], &s[4]);
            md[0] = md[1] = 0, md[2] = 2, md[3] = 1, md[4] = 2, md[5] = md[6] = 0;
        }
        uint32_t *row = c->px + (size_t)y * c->stride;
        for (int i = 0; i < 7; i++) {
            int xa = s[i] < lo ? lo : s[i] > hi ? hi : s[i], xb = s[i + 1] < lo ? lo : s[i + 1] > hi ? hi : s[i + 1];
            if (xb <= xa) continue;
            if (md[i] == 1) {
                if (p.col) dr_run(c, &p, y, xa, xb, NULL);
                continue;
            }
            dr_mark(c, xa, y, xb, y + 1);
            if (md[i] == 2 && qy >= 0 && rad) {
                if (qy >= rmax) continue;
                int da, db;
                dr_sh_in(g.cx, g.sx + dr_rt(rmax * rmax - qy * qy), &da, &db); // nothing past the outer circle
                if (xa < da) xa = da;
                if (xb > db) xb = db;
                double q = qy * qy;
                int ry = y < g.cy ? y : my2 - y;
                uint32_t *crow = cc && ry - cy0 >= 0 && ry - cy0 < ch ? cc + (size_t)(ry - cy0) * cw : NULL;
                for (int k = xa; k < xb; k++) {
                    int rx = (k < g.cx ? k : mx2 - k) - cx0;
                    uint32_t *slot = crow && rx >= 0 && rx < cw ? crow + rx : NULL;
                    if (slot && *slot != 0x00FFFFFF) {
                        if (*slot) dr_blend(&row[k], *slot);
                        continue;
                    }
                    double l2 = qx2[k - bx0] + q, h = 0.5 * l2, y2;
                    uint64_t bits;
                    memcpy(&bits, &l2, 8);
                    bits = 0x5FE6EB50C7B537A9ull - (bits >> 1);
                    memcpy(&y2, &bits, 8);
                    y2 = y2 * (1.5 - h * y2 * y2);
                    y2 = y2 * (1.5 - h * y2 * y2);
                    y2 = y2 * (1.5 - h * y2 * y2);
                    int ri = l2 > 0 ? (int)(l2 * y2 * 8 + 0.5) : 0;
                    uint32_t v2 = rad[ri < nrad ? ri : nrad - 1];
                    if (slot) *slot = v2;
                    if (v2) dr_blend(&row[k], v2);
                }
                continue;
            }
            if (!md[i]) {
                for (int k = xa; k < xb;) {
                    uint32_t v2 = tab[k - bx0];
                    if (v2 == col) {
                        int e = k + 1;
                        while (e < xb && tab[e - bx0] == col) e++;
                        dr_span(row, k, e, col);
                        k = e;
                        continue;
                    }
                    if (v2) dr_blend(&row[k], v2);
                    k++;
                }
                continue;
            }
            for (int k = xa; k < xb; k++) {
                uint32_t v2 = md[i] ? dr_sh_col(&g, col, dr_sh_d(&g, k + 0.5, py)) : tab[k - bx0];
                if (v2) dr_blend(&row[k], v2);
            }
        }
    }
    free(cc);
    free(rad);
    free(tab);
}

void goome_draw_circle(goome_canvas *c, double cx, double cy, double r, uint32_t argb)
{
    dr_rr q;
    dr_paint p;
    if (!dr_circ(c, cx, cy, r, &q)) return;
    dr_solid(&p, argb);
    dr_fill_rr(c, &q, &p);
}

// r is the middle of the stroke
void goome_draw_ring(goome_canvas *c, double cx, double cy, double r, double width, uint32_t argb)
{
    dr_rr q, in;
    dr_paint p;
    if (!(width > 0) || !dr_circ(c, cx, cy, r + width / 2, &q)) return;
    dr_solid(&p, argb);
    if (dr_circ(c, cx, cy, r - width / 2, &in))
        dr_minus(c, &q, &in, &p);
    else
        dr_fill_rr(c, &q, &p);
}

// a0 is 3 o'clock, it goes clockwise. round caps
void goome_draw_arc(goome_canvas *c, double cx, double cy, double r, double width, double a0, double a1, uint32_t argb)
{
    if (!dr_ok(a0) || !dr_ok(a1) || !(a1 > a0) || !dr_ok(r) || r < 0) return;
    if (a1 - a0 >= 6.283185307179586) {
        goome_draw_ring(c, cx, cy, r, width, argb);
        return;
    }
    dr_geo g;
    dr_paint p;
    memset(&g, 0, sizeof(g));
    if (!(width > 0) || !dr_circ(c, cx, cy, r + width / 2, &g.out)) return;
    g.has_hole = dr_circ(c, cx, cy, r - width / 2, &g.hole);
    g.kind = 3;
    dr_to_px(c, cx, cy, &g.cx, &g.cy);
    g.r = r * c->scale, g.hw = width * c->scale / 2;
    g.sx = dr_cos(a0), g.sy = dr_sin(a0), g.ex = dr_cos(a1), g.ey = dr_sin(a1);
    g.big = a1 - a0 > 3.141592653589793;
    dr_inset(&g.out, -1, &g.out);
    dr_solid(&p, argb);
    dr_shape(c, &g, &p);
}

// signed area per cell, one row at a time, x local to the row buffer
static void dr_acc(double *a, int w, double xa, double xb, double d)
{
    if (xa <= 0 && xb <= 0) {
        a[0] += d;
        return;
    }
    if (xa >= w && xb >= w) return;
    double x0 = dr_min(xa, xb), x1 = dr_max(xa, xb);
    if (x0 < 0 || x1 > w) {
        double e = x0 < 0 ? 0 : w, t = (e - xa) / (xb - xa);
        dr_acc(a, w, xa, e, d * t);
        dr_acc(a, w, e, xb, d * (1 - t));
        return;
    }
    double fl = dr_floor(x0);
    int i0 = (int)fl, i1 = (int)dr_ceil(x1);
    if (i1 <= i0 + 1) {
        double m = 0.5 * (xa + xb) - fl;
        a[i0] += d - d * m;
        a[i0 + 1] += d * m;
        return;
    }
    double s = 1 / (x1 - x0), f0 = x0 - fl, a0 = 0.5 * s * (1 - f0) * (1 - f0), f1 = x1 - i1 + 1, am = 0.5 * s * f1 * f1;
    a[i0] += d * a0;
    if (i1 == i0 + 2)
        a[i0 + 1] += d * (1 - a0 - am);
    else {
        double a1 = s * (1.5 - f0);
        a[i0 + 1] += d * (a1 - a0);
        for (int i = i0 + 2; i < i1 - 1; i++) a[i] += d * s;
        double a2 = a1 + (i1 - i0 - 3) * s;
        a[i1 - 1] += d * (1 - a2 - am);
    }
    a[i1] += d * am;
}

void goome_draw_poly(goome_canvas *c, const double *xy, int n, uint32_t argb)
{
    if (!c || !c->px || !xy || n < 3 || n > 1 << 20) return;
    double *pt = malloc((size_t)n * 2 * sizeof(double));
    if (!pt) return;
    double mx0 = 1e300, my0 = 1e300, mx1 = -1e300, my1 = -1e300;
    for (int i = 0; i < n; i++) {
        dr_to_px(c, xy[2 * i], xy[2 * i + 1], &pt[2 * i], &pt[2 * i + 1]);
        if (!dr_ok(pt[2 * i]) || !dr_ok(pt[2 * i + 1])) {
            free(pt);
            return;
        }
        mx0 = dr_min(mx0, pt[2 * i]), mx1 = dr_max(mx1, pt[2 * i]);
        my0 = dr_min(my0, pt[2 * i + 1]), my1 = dr_max(my1, pt[2 * i + 1]);
    }
    int kx0, ky0, kx1, ky1;
    dr_kbox(c, &kx0, &ky0, &kx1, &ky1);
    int x0 = dr_fl(mx0), x1 = dr_cl(mx1) + 1, ya = dr_fl(my0), yb = dr_cl(my1);
    if (x0 < kx0) x0 = kx0;
    if (x1 > kx1) x1 = kx1;
    if (ya < ky0) ya = ky0;
    if (yb > ky1) yb = ky1;
    int w = x1 - x0;
    double *acc = w > 0 && yb > ya ? calloc((size_t)w + 2, sizeof(double)) : NULL;
    uint8_t *cv = acc ? malloc((size_t)w) : NULL;
    dr_paint p;
    dr_solid(&p, argb);
    for (int y = ya; cv && y < yb; y++) {
        for (int i = 0; i < n; i++) {
            const double *a = pt + 2 * i, *b = pt + 2 * ((i + 1) % n);
            if (a[1] == b[1]) continue;
            double d = b[1] > a[1] ? 1 : -1, lo = dr_min(a[1], b[1]), hi = dr_max(a[1], b[1]);
            double ta = dr_max(lo, y), tb = dr_min(hi, y + 1.0);
            if (tb <= ta) continue;
            double k = (b[0] - a[0]) / (b[1] - a[1]);
            dr_acc(acc, w, a[0] + (ta - a[1]) * k - x0, a[0] + (tb - a[1]) * k - x0, d * (tb - ta));
        }
        double s = 0;
        for (int i = 0; i < w; i++) {
            s += acc[i];
            acc[i] = 0;
            double v = s < 0 ? -s : s;
            cv[i] = v >= 1 ? 255 : (uint8_t)(v * 255 + 0.5);
        }
        acc[w] = acc[w + 1] = 0;
        for (int i = 0; i < w;) {
            if (!cv[i]) {
                i++;
                continue;
            }
            int j = i;
            if (cv[i] == 255) {
                while (j < w && cv[j] == 255) j++;
                dr_run(c, &p, y, x0 + i, x0 + j, NULL);
            } else {
                while (j < w && cv[j] && cv[j] != 255) j++;
                dr_run(c, &p, y, x0 + i, x0 + j, cv + i);
            }
            i = j;
        }
    }
    free(cv);
    free(acc);
    free(pt);
}

// taps for one axis over dest px d0 .. d0+n, source span s0 .. s0+sn
static int dr_axis_make(dr_axis *t, int n, int d0, double org, double k, int s0, int sn, int mode)
{
    int cap = mode == 2 ? (k + 3 < sn + 1 ? (int)k + 3 : sn + 1) : 2;
    t->n = n;
    t->st = malloc(sizeof(int) * ((size_t)n * 6 + (size_t)n * cap));
    if (!t->st) return 0;
    t->cnt = t->st + 2 * n, t->a = t->cnt + n, t->b = t->a + n, t->f = t->b + n, t->w = t->f + n;
    int used = 0;
    for (int i = 0; i < n; i++) {
        double px = d0 + i;
        int *w = t->w + used, m = 0, st;
        if (mode == 0) {
            int u = dr_fl(px + 0.5 - org);
            st = u < 0 ? 0 : u >= sn ? sn - 1 : u;
            w[m++] = 65536;
        } else if (mode == 1) {
            double u = (px + 0.5 - org) * k - 0.5, fu = dr_floor(u);
            int iu = dr_fl(u), wb = (int)((u - fu) * 65536 + 0.5);
            int a = iu < 0 ? 0 : iu >= sn ? sn - 1 : iu, b = iu + 1 < 0 ? 0 : iu + 1 >= sn ? sn - 1 : iu + 1;
            st = a;
            if (b == a || wb == 0)
                w[m++] = 65536;
            else
                w[m++] = 65536 - wb, w[m++] = wb;
        } else {
            double u0 = (px - org) * k, u1 = u0 + k;
            if (u0 < 0) u0 = 0;
            if (u1 > sn) u1 = sn;
            if (!(u1 - u0 > 1e-9)) {
                int u = dr_fl(u0);
                st = u < 0 ? 0 : u >= sn ? sn - 1 : u;
                w[m++] = 65536;
            } else {
                int a = dr_fl(u0), b = dr_cl(u1), sum = 0;
                if (b > sn) b = sn;
                if (b - a > cap) b = a + cap;
                st = a;
                for (int j = a; j < b; j++) {
                    double o = (dr_min(j + 1, u1) - dr_max(j, u0)) / (u1 - u0);
                    w[m] = (int)(o * 65536 + 0.5), sum += w[m++];
                }
                w[m - 1] += 65536 - sum;
            }
        }
        t->st[i] = s0 + st, t->cnt[i] = m, t->st[n + i] = used;
        t->a[i] = s0 + st, t->b[i] = s0 + st + (m > 1), t->f[i] = m > 1 ? (w[1] + 128) >> 8 : 0;
        used += m;
    }
    return 1;
}

static int dr_axis_fast(const dr_axis *t)
{
    for (int i = 0; i < t->n; i++)
        if (t->cnt[i] > 2) return 0;
    return 1;
}

static void dr_bitmap(goome_canvas *c, const goome_bitmap *bm, int sx, int sy, int sw, int sh, double x, double y, double w, double h, goome_radii r, uint32_t tint)
{
    if (!bm || !bm->px || bm->w <= 0 || bm->h <= 0 || bm->w > 65536 || bm->h > 65536) return;
    int bs = bm->stride > 0 ? bm->stride : bm->w;
    if (bs < bm->w || !(tint >> 24)) return;
    if (sx < 0) sw += sx, sx = 0;
    if (sy < 0) sh += sy, sy = 0;
    if (sw > bm->w - sx) sw = bm->w - sx;
    if (sh > bm->h - sy) sh = bm->h - sy;
    if (sw <= 0 || sh <= 0) return;
    dr_rr q;
    if (!dr_mkrr(c, x, y, w, h, r, &q)) return;
    int kx0, ky0, kx1, ky1;
    dr_kbox(c, &kx0, &ky0, &kx1, &ky1);
    int bx0 = dr_fl(q.x0), by0 = dr_fl(q.y0), bx1 = dr_cl(q.x1), by1 = dr_cl(q.y1);
    if (bx0 < kx0) bx0 = kx0;
    if (by0 < ky0) by0 = ky0;
    if (bx1 > kx1) bx1 = kx1;
    if (by1 > ky1) by1 = ky1;
    if (bx1 <= bx0 || by1 <= by0) return;
    double kx = sw / (q.x1 - q.x0), ky = sh / (q.y1 - q.y0);
    int one = q.x1 - q.x0 == sw && q.y1 - q.y0 == sh;
    dr_paint p;
    memset(&p, 0, sizeof(p));
    p.kind = 2, p.bp = bm->px, p.bs = bs, p.ox = bx0, p.oy = by0;
    if (!dr_axis_make(&p.ax, bx1 - bx0, bx0, q.x0, kx, sx, sw, one ? 0 : kx > 2 ? 2 : 1)) return;
    if (!dr_axis_make(&p.ay, by1 - by0, by0, q.y0, ky, sy, sh, one ? 0 : ky > 2 ? 2 : 1)) {
        free(p.ax.st);
        return;
    }
    p.fast = dr_axis_fast(&p.ax) && dr_axis_fast(&p.ay);
    if (tint != 0xFFFFFFFFu) {
        int ta = tint >> 24;
        p.tint = 1, p.tm[0] = ta;
        for (int k = 1; k < 4; k++) p.tm[k] = ((tint >> (24 - 8 * k) & 255) * ta + 127) / 255;
    }
    dr_fill_rr(c, &q, &p);
    free(p.ax.st);
    free(p.ay.st);
}

void goome_draw_bitmap(goome_canvas *c, const goome_bitmap *b, double x, double y, double w, double h, goome_radii r, uint32_t tint)
{
    if (b) dr_bitmap(c, b, 0, 0, b->w, b->h, x, y, w, h, r, tint);
}

void goome_draw_bitmap_part(goome_canvas *c, const goome_bitmap *b, int sx, int sy, int sw, int sh, double x, double y, double w, double h, uint32_t tint)
{
    dr_bitmap(c, b, sx, sy, sw, sh, x, y, w, h, goome_radius(0), tint);
}

#endif
