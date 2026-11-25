#ifndef GOOME_NO_DRAW
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "internal.h"
#include "unidata.h"

#if defined(_WIN32)
#include <io.h>
#include <wchar.h>
#elif defined(__unix__) || defined(__APPLE__)
#include <dirent.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#if !defined(__APPLE__)
#include <dlfcn.h>
#endif
#endif

#if defined(__clang__)
#pragma STDC FP_CONTRACT OFF
#elif defined(_MSC_VER)
#pragma fp_contract(off)
#endif

#define FT_MAXFILE (64u << 20)
#define FT_MAXPTS (1 << 17)
#define FT_MAXSEG (1 << 20)
#define FT_MAXDIM 2048
#define FT_MAXPX 1024
#define FT_ARENA (4u << 20)
#define FT_SLOTS 8192
#define FT_KC 2048
#define FT_MAXMAP (1u << 30)
#define FT_NFB 8
#define FT_CC 512
#define FT_NLIG 256
#define FT_REG 16

typedef struct {
    int32_t x, y;
    uint8_t t; // 0 on, 1 quad control, 2 cubic control
} ft_pt;

typedef struct {
    uint64_t key;
    uint32_t off;
    int16_t w, h, l, t;
} ft_ent;

typedef struct {
    uint32_t start, count, osz, data;
} ft_idx;

typedef struct {
    uint32_t g, px64, argb;
    int16_t w, h, l, t;
    uint32_t *px;
} ft_cent;

typedef struct {
    int g, lo, hi;
    int64_t adv;
    uint8_t fi, rtl, kind;
} ft_gl;

struct goome_font {
    const uint8_t *d;
    size_t n;
    uint8_t *own;
    uint32_t glyf, glyf_n, loca, loca_n, hmtx, hmtx_n, kern, gpos;
    int upem, nglyphs, nhm, locfmt, asc, desc, gap, xh;
    int cmfmt, sym;
    uint32_t cmoff;
    uint16_t low[256];
    int cff, cid;
    uint32_t cffbase;
    ft_idx gsubr, lsubr, chars, fdarr;
    uint32_t fdsel;
    uint32_t *ksub;
    int nksub;
    uint32_t kck[FT_KC];
    int16_t kcv[FT_KC];
    // glyph cache
    ft_ent *ents;
    int nents;
    int32_t *slots;
    uint8_t *arena;
    uint32_t used;
    // scratch
    ft_pt *pts;
    int npts, ptcap;
    int32_t *ends;
    int nends, endcap;
    int32_t *cover;
    int64_t *area;
    size_t cells;
    uint8_t *tmp;
    size_t tmpcap;
    int nseg, bad, ncomp;
    int rw, rh;
    uint32_t col;
    uint32_t tab[256];
    void *map;
    size_t maplen;
    int shared, color;
    goome_font *fb[FT_NFB];
    int nfb;
    uint32_t colr, cpal, cblc, cbdt, sbix, uvs;
    uint32_t lig[FT_NLIG];
    int nlig;
    ft_cent *cc;
    int ncc;
    size_t ccb;
    uint32_t *scp, *ssh;
    int *soff, *scs, *sord;
    uint8_t *scl, *swk, *slv;
    int scap;
    ft_gl *sgl;
    int sglcap;
};

static uint8_t ft_gamma[256];
static int ft_gamma_ok;

static uint32_t ft_u8(const goome_font *f, uint32_t o) { return o < f->n ? f->d[o] : 0; }
static uint32_t ft_u16(const goome_font *f, uint32_t o) { return o < f->n && f->n - o >= 2 ? (uint32_t)f->d[o] << 8 | f->d[o + 1] : 0; }
static int ft_s16(const goome_font *f, uint32_t o) { return (int)(ft_u16(f, o) ^ 0x8000) - 0x8000; }
static uint32_t ft_u32(const goome_font *f, uint32_t o)
{
    if (o >= f->n || f->n - o < 4) return 0;
    const uint8_t *p = f->d + o;
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}
static int ft_has(const goome_font *f, uint32_t o, uint32_t len) { return o <= f->n && f->n - o >= len; }

static int64_t ft_divr(int64_t a, int64_t b)
{
    if (b < 0) a = -a, b = -b;
    return a >= 0 ? (a + b / 2) / b : -((-a + b / 2) / b);
}
static int64_t ft_fdiv(int64_t a, int64_t b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }
static int64_t ft_abs(int64_t v) { return v < 0 ? -v : v; }
static int64_t ft_clamp(int64_t v, int64_t lim) { return v > lim ? lim : v < -lim ? -lim : v; }

static uint32_t ft_table(const goome_font *f, uint32_t dir, const char *tag, uint32_t *len)
{
    uint32_t nt = ft_u16(f, dir + 4), want = (uint32_t)tag[0] << 24 | (uint32_t)tag[1] << 16 | (uint32_t)tag[2] << 8 | (uint32_t)tag[3];
    for (uint32_t i = 0; i < nt && i < 4096; i++) {
        uint32_t r = dir + 12 + i * 16;
        if (ft_u32(f, r) != want) continue;
        uint32_t o = ft_u32(f, r + 8), l = ft_u32(f, r + 12);
        if (!ft_has(f, o, l)) return 0;
        if (len) *len = l;
        return o;
    }
    return 0;
}

static int ft_cmap_score(int plat, int enc, int fmt)
{
    if (fmt == 12 && ((plat == 3 && enc == 10) || plat == 0)) return 5;
    if (fmt == 4 && ((plat == 3 && enc == 1) || plat == 0)) return 4;
    if (fmt == 6 && ((plat == 3 && enc == 1) || plat == 0)) return 3;
    if (fmt == 4 && plat == 3 && enc == 0) return 2;
    return 0;
}

static int ft_cmap_find(goome_font *f, uint32_t cmap)
{
    int best = 0;
    uint32_t nt = ft_u16(f, cmap + 2);
    for (uint32_t i = 0; i < nt && i < 512; i++) {
        uint32_t r = cmap + 4 + i * 8, o = cmap + ft_u32(f, r + 4);
        int s = ft_cmap_score((int)ft_u16(f, r), (int)ft_u16(f, r + 2), (int)ft_u16(f, o));
        if (ft_u16(f, r) == 0 && ft_u16(f, r + 2) == 5 && ft_u16(f, o) == 14) f->uvs = o;
        if (s > best) best = s, f->cmoff = o, f->cmfmt = (int)ft_u16(f, o), f->sym = s == 2;
    }
    return best;
}

static int ft_cmap_raw(const goome_font *f, uint32_t cp)
{
    uint32_t o = f->cmoff;
    if (f->cmfmt == 4) {
        if (cp > 0xFFFF) return 0;
        uint32_t segs = ft_u16(f, o + 6) / 2, ends = o + 14, starts = ends + segs * 2 + 2;
        uint32_t lo = 0, hi = segs;
        while (lo < hi) {
            uint32_t m = (lo + hi) / 2;
            if (ft_u16(f, ends + m * 2) < cp) lo = m + 1;
            else hi = m;
        }
        if (lo >= segs) return 0;
        uint32_t st = ft_u16(f, starts + lo * 2);
        if (cp < st) return 0;
        uint32_t delta = ft_u16(f, starts + segs * 2 + lo * 2), ro_at = starts + segs * 4 + lo * 2, ro = ft_u16(f, ro_at);
        if (!ro) return (int)((cp + delta) & 0xFFFF);
        uint32_t g = ft_u16(f, ro_at + ro + (cp - st) * 2);
        return g ? (int)((g + delta) & 0xFFFF) : 0;
    }
    if (f->cmfmt == 12) {
        uint32_t ng = ft_u32(f, o + 12), lo = 0, hi = ng;
        if (ng > (f->n >> 3)) return 0;
        while (lo < hi) {
            uint32_t m = lo + (hi - lo) / 2, g = o + 16 + m * 12;
            if (ft_u32(f, g + 4) < cp) lo = m + 1;
            else hi = m;
        }
        if (lo >= ng) return 0;
        uint32_t g = o + 16 + lo * 12, st = ft_u32(f, g);
        if (cp < st) return 0;
        uint32_t v = ft_u32(f, g + 8) + (cp - st);
        return v < 65536 ? (int)v : 0;
    }
    if (f->cmfmt == 6) {
        uint32_t first = ft_u16(f, o + 6), cnt = ft_u16(f, o + 8);
        if (cp < first || cp - first >= cnt) return 0;
        return (int)ft_u16(f, o + 10 + (cp - first) * 2);
    }
    return 0;
}

static int ft_cmap(const goome_font *f, uint32_t cp)
{
    int g = cp < 256 ? f->low[cp] : ft_cmap_raw(f, cp);
    if (!g && f->sym && cp < 256) g = ft_cmap_raw(f, cp + 0xF000);
    return g < f->nglyphs ? g : 0;
}

static int ft_adv(const goome_font *f, int g)
{
    if (f->nhm <= 0) return 0;
    if (g >= f->nhm) g = f->nhm - 1;
    return (int)ft_u16(f, f->hmtx + (uint32_t)g * 4);
}

// outlines go into f->pts as font units * 64

static int ft_grow(goome_font *f, int pts, int ends)
{
    if (f->npts + pts > FT_MAXPTS || f->nends + ends > FT_MAXPTS) return 0;
    if (f->npts + pts > f->ptcap) {
        int c = f->ptcap ? f->ptcap : 256;
        while (c < f->npts + pts) c *= 2;
        ft_pt *p = realloc(f->pts, (size_t)c * sizeof(*p));
        if (!p) return 0;
        f->pts = p, f->ptcap = c;
    }
    if (f->nends + ends > f->endcap) {
        int c = f->endcap ? f->endcap : 64;
        while (c < f->nends + ends) c *= 2;
        int32_t *p = realloc(f->ends, (size_t)c * sizeof(*p));
        if (!p) return 0;
        f->ends = p, f->endcap = c;
    }
    return 1;
}

static int ft_glyf(goome_font *f, int g, int depth)
{
    if (g < 0 || g >= f->nglyphs || depth > 8 || ++f->ncomp > 4096) return 0;
    uint32_t a, b;
    if (f->locfmt) a = ft_u32(f, f->loca + (uint32_t)g * 4), b = ft_u32(f, f->loca + (uint32_t)g * 4 + 4);
    else a = ft_u16(f, f->loca + (uint32_t)g * 2) * 2, b = ft_u16(f, f->loca + (uint32_t)g * 2 + 2) * 2;
    if ((uint32_t)g * (f->locfmt ? 4u : 2u) + (f->locfmt ? 8u : 4u) > f->loca_n) return 0;
    if (b <= a || b > f->glyf_n || b - a < 10) return 1;
    uint32_t p = f->glyf + a, end = f->glyf + b;
    int nc = ft_s16(f, p);
    if (nc >= 0) {
        if (nc == 0) return 1;
        uint32_t ep = p + 10;
        int np = (int)ft_u16(f, ep + (uint32_t)(nc - 1) * 2) + 1, last = -1;
        if (!ft_grow(f, np, nc)) return 0;
        int base = f->npts;
        for (int i = 0; i < nc; i++) {
            int e = (int)ft_u16(f, ep + (uint32_t)i * 2);
            if (e <= last || e >= np) return 0;
            f->ends[f->nends + i] = base + e;
            last = e;
        }
        uint32_t q = ep + (uint32_t)nc * 2;
        q += 2 + ft_u16(f, q);
        ft_pt *pt = f->pts + base;
        for (int i = 0; i < np;) {
            if (q >= end) return 0;
            uint8_t fl = (uint8_t)ft_u8(f, q++);
            int rep = 1;
            if (fl & 8) {
                if (q >= end) return 0;
                rep += (int)ft_u8(f, q++);
            }
            while (rep-- && i < np) pt[i++].t = fl;
        }
        int64_t v = 0;
        for (int i = 0; i < np; i++) {
            uint8_t fl = pt[i].t;
            if (fl & 2) {
                if (q >= end) return 0;
                int d = (int)ft_u8(f, q++);
                v += fl & 16 ? d : -d;
            } else if (!(fl & 16)) {
                if (q + 2 > end) return 0;
                v += ft_s16(f, q), q += 2;
            }
            v = ft_clamp(v, 8388608);
            pt[i].x = (int32_t)(v * 64);
        }
        v = 0;
        for (int i = 0; i < np; i++) {
            uint8_t fl = pt[i].t;
            if (fl & 4) {
                if (q >= end) return 0;
                int d = (int)ft_u8(f, q++);
                v += fl & 32 ? d : -d;
            } else if (!(fl & 32)) {
                if (q + 2 > end) return 0;
                v += ft_s16(f, q), q += 2;
            }
            v = ft_clamp(v, 8388608);
            pt[i].y = (int32_t)(v * 64);
            pt[i].t = fl & 1 ? 0 : 1;
        }
        f->npts += np, f->nends += nc;
        return 1;
    }
    uint32_t q = p + 10;
    for (int k = 0; k < 256; k++) {
        if (q + 4 > end) return 0;
        uint32_t fl = ft_u16(f, q);
        int cg = (int)ft_u16(f, q + 2);
        int64_t dx = 0, dy = 0;
        q += 4;
        if (fl & 1) {
            if (fl & 2) dx = ft_s16(f, q), dy = ft_s16(f, q + 2);
            q += 4;
        } else {
            if (fl & 2) dx = (int)(ft_u8(f, q) ^ 0x80) - 0x80, dy = (int)(ft_u8(f, q + 1) ^ 0x80) - 0x80;
            q += 2;
        }
        int64_t ma = 16384, mb = 0, mc = 0, md = 16384;
        if (fl & 8) ma = md = ft_s16(f, q), q += 2;
        else if (fl & 0x40) ma = ft_s16(f, q), md = ft_s16(f, q + 2), q += 4;
        else if (fl & 0x80) ma = ft_s16(f, q), mb = ft_s16(f, q + 2), mc = ft_s16(f, q + 4), md = ft_s16(f, q + 6), q += 8;
        if (q > end) return 0;
        int s = f->npts;
        if (!ft_glyf(f, cg, depth + 1)) return 0;
        for (int i = s; i < f->npts; i++) {
            int64_t x = f->pts[i].x, y = f->pts[i].y;
            int64_t nx = ft_divr(ma * x + mc * y, 16384) + dx * 64, ny = ft_divr(mb * x + md * y, 16384) + dy * 64;
            f->pts[i].x = (int32_t)ft_clamp(nx, (int64_t)1 << 30), f->pts[i].y = (int32_t)ft_clamp(ny, (int64_t)1 << 30);
        }
        if (!(fl & 0x20)) break;
    }
    return 1;
}

// cff, type 2 charstrings

static int ft_index(const goome_font *f, uint32_t o, ft_idx *x)
{
    memset(x, 0, sizeof(*x));
    x->count = ft_u16(f, o);
    if (!x->count) {
        x->data = o + 2;
        return ft_has(f, o, 2);
    }
    x->osz = ft_u8(f, o + 2);
    if (x->osz < 1 || x->osz > 4) return 0;
    x->start = o + 3;
    uint32_t tab = x->count + 1;
    if (!ft_has(f, x->start, tab * x->osz)) return 0;
    x->data = x->start + tab * x->osz - 1;
    return 1;
}

static uint32_t ft_off(const goome_font *f, uint32_t o, uint32_t sz)
{
    uint32_t v = 0;
    for (uint32_t i = 0; i < sz; i++) v = v << 8 | ft_u8(f, o + i);
    return v;
}

static int ft_item(const goome_font *f, const ft_idx *x, uint32_t i, uint32_t *at, uint32_t *len)
{
    if (i >= x->count) return 0;
    uint32_t a = ft_off(f, x->start + i * x->osz, x->osz), b = ft_off(f, x->start + (i + 1) * x->osz, x->osz);
    if (a < 1 || b < a) return 0;
    if (x->data + a < x->data || !ft_has(f, x->data + a, b - a)) return 0;
    *at = x->data + a, *len = b - a;
    return 1;
}

static uint32_t ft_index_end(const goome_font *f, const ft_idx *x)
{
    if (!x->count) return x->data;
    return x->data + ft_off(f, x->start + x->count * x->osz, x->osz);
}

// reads a dict, gives back the operands of op (12 xx as 1200 + xx)
static int ft_dict(const goome_font *f, uint32_t o, uint32_t len, int op, int32_t *out, int max)
{
    int32_t st[48];
    int sp = 0;
    uint32_t end = o + len;
    if (!ft_has(f, o, len)) return 0;
    while (o < end) {
        uint32_t b = f->d[o++];
        if (b <= 21) {
            int k = (int)b;
            if (b == 12) k = 1200 + (int)ft_u8(f, o++);
            if (k == op) {
                for (int i = 0; i < sp && i < max; i++) out[i] = st[i];
                return sp;
            }
            sp = 0;
            continue;
        }
        int32_t v = 0;
        if (b == 28) v = ft_s16(f, o), o += 2;
        else if (b == 29) v = (int32_t)ft_u32(f, o), o += 4;
        else if (b == 30) {
            while (o < end) {
                uint32_t c = f->d[o++];
                if ((c & 15) == 15 || (c >> 4) == 15) break;
            }
        } else if (b >= 32 && b <= 246) v = (int32_t)b - 139;
        else if (b >= 247 && b <= 250) v = ((int32_t)b - 247) * 256 + (int32_t)ft_u8(f, o++) + 108;
        else if (b >= 251 && b <= 254) v = -((int32_t)b - 251) * 256 - (int32_t)ft_u8(f, o++) - 108;
        else return 0;
        if (sp < 48) st[sp++] = v;
    }
    return 0;
}

static int ft_private(goome_font *f, uint32_t dict, uint32_t dlen, ft_idx *subs)
{
    int32_t pv[2], sv[1];
    memset(subs, 0, sizeof(*subs));
    if (ft_dict(f, dict, dlen, 18, pv, 2) < 2 || pv[0] < 0 || pv[1] < 0) return 1;
    uint32_t po = f->cffbase + (uint32_t)pv[1];
    if (ft_dict(f, po, (uint32_t)pv[0], 19, sv, 1) >= 1 && sv[0] > 0) return ft_index(f, po + (uint32_t)sv[0], subs);
    return 1;
}

static int ft_cff_init(goome_font *f, uint32_t base, uint32_t len)
{
    f->cffbase = base;
    if (len < 4 || ft_u8(f, base) != 1) return 0;
    ft_idx names, tops, strs;
    if (!ft_index(f, base + ft_u8(f, base + 2), &names)) return 0;
    if (!ft_index(f, ft_index_end(f, &names), &tops)) return 0;
    if (!ft_index(f, ft_index_end(f, &tops), &strs)) return 0;
    if (!ft_index(f, ft_index_end(f, &strs), &f->gsubr)) return 0;
    uint32_t td, tl;
    int32_t v[4];
    if (!ft_item(f, &tops, 0, &td, &tl)) return 0;
    if (ft_dict(f, td, tl, 1206, v, 1) >= 1 && v[0] != 2) return 0;
    if (ft_dict(f, td, tl, 17, v, 1) < 1 || v[0] <= 0) return 0;
    if (!ft_index(f, base + (uint32_t)v[0], &f->chars) || !f->chars.count) return 0;
    if (ft_dict(f, td, tl, 1230, v, 3) >= 3) {
        f->cid = 1;
        if (ft_dict(f, td, tl, 1236, v, 1) < 1 || v[0] <= 0 || !ft_index(f, base + (uint32_t)v[0], &f->fdarr)) return 0;
        if (ft_dict(f, td, tl, 1237, v, 1) < 1 || v[0] <= 0) return 0;
        f->fdsel = base + (uint32_t)v[0];
        return 1;
    }
    return ft_private(f, td, tl, &f->lsubr);
}

static int ft_cff_fd(const goome_font *f, int g)
{
    uint32_t o = f->fdsel;
    int fmt = (int)ft_u8(f, o);
    if (fmt == 0) return (int)ft_u8(f, o + 1 + (uint32_t)g);
    if (fmt == 3) {
        uint32_t nr = ft_u16(f, o + 1);
        for (uint32_t i = 0; i < nr && i < 65536; i++) {
            uint32_t r = o + 3 + i * 3;
            if ((uint32_t)g >= ft_u16(f, r) && (uint32_t)g < ft_u16(f, r + 3)) return (int)ft_u8(f, r + 2);
        }
    }
    return -1;
}

static int ft_bias(uint32_t n) { return n < 1240 ? 107 : n < 33900 ? 1131 : 32768; }

typedef struct {
    int64_t x, y;
    int open, nstems, ops;
} ft_cs;

static int ft_cs_pt(goome_font *f, ft_cs *s, int64_t x, int64_t y, int t)
{
    if (!ft_grow(f, 1, 0)) return 0;
    ft_pt *p = &f->pts[f->npts++];
    p->x = (int32_t)ft_clamp(ft_fdiv(x, 1024), (int64_t)1 << 30), p->y = (int32_t)ft_clamp(ft_fdiv(y, 1024), (int64_t)1 << 30), p->t = (uint8_t)t;
    return 1;
}

static void ft_cs_close(goome_font *f, ft_cs *s)
{
    if (s->open && f->npts > 0 && ft_grow(f, 0, 1)) f->ends[f->nends++] = f->npts - 1;
    s->open = 0;
}

static int ft_cs_move(goome_font *f, ft_cs *s, int64_t dx, int64_t dy)
{
    ft_cs_close(f, s);
    s->x = ft_clamp(s->x + dx, (int64_t)1 << 40), s->y = ft_clamp(s->y + dy, (int64_t)1 << 40);
    s->open = 1;
    return ft_cs_pt(f, s, s->x, s->y, 0);
}

static int ft_cs_line(goome_font *f, ft_cs *s, int64_t dx, int64_t dy)
{
    if (!s->open && !ft_cs_move(f, s, 0, 0)) return 0;
    s->x = ft_clamp(s->x + dx, (int64_t)1 << 40), s->y = ft_clamp(s->y + dy, (int64_t)1 << 40);
    return ft_cs_pt(f, s, s->x, s->y, 0);
}

static int ft_cs_curve(goome_font *f, ft_cs *s, int64_t a, int64_t b, int64_t c, int64_t d, int64_t e, int64_t g)
{
    if (!s->open && !ft_cs_move(f, s, 0, 0)) return 0;
    int64_t x1 = s->x + a, y1 = s->y + b, x2 = x1 + c, y2 = y1 + d;
    s->x = ft_clamp(x2 + e, (int64_t)1 << 40), s->y = ft_clamp(y2 + g, (int64_t)1 << 40);
    return ft_cs_pt(f, s, x1, y1, 2) && ft_cs_pt(f, s, x2, y2, 2) && ft_cs_pt(f, s, s->x, s->y, 0);
}

static int ft_cff_glyph(goome_font *f, int g)
{
    uint32_t at, len;
    if (!ft_item(f, &f->chars, (uint32_t)g, &at, &len)) return 0;
    ft_idx loc = f->lsubr;
    if (f->cid) {
        int fd = ft_cff_fd(f, g);
        uint32_t da, dl;
        if (fd < 0 || !ft_item(f, &f->fdarr, (uint32_t)fd, &da, &dl) || !ft_private(f, da, dl, &loc)) return 0;
    }
    int32_t st[48];
    int sp = 0, haswidth = 0, depth = 0;
    uint32_t rs[12], re[12], p = at, end = at + len;
    ft_cs s;
    memset(&s, 0, sizeof(s));
    for (;;) {
        if (p >= end) {
            if (!depth) break;
            depth--, p = rs[depth], end = re[depth];
            continue;
        }
        if (++s.ops > 100000) return 0;
        uint32_t b = f->d[p++];
        if (b >= 32 || b == 28) {
            int32_t v;
            if (b == 28) {
                if (p + 2 > end) return 0;
                v = ft_s16(f, p) * 65536, p += 2;
            } else if (b <= 246) v = ((int32_t)b - 139) * 65536;
            else if (b <= 250) {
                if (p >= end) return 0;
                v = (((int32_t)b - 247) * 256 + (int32_t)f->d[p++] + 108) * 65536;
            } else if (b <= 254) {
                if (p >= end) return 0;
                v = (-((int32_t)b - 251) * 256 - (int32_t)f->d[p++] - 108) * 65536;
            } else {
                if (p + 4 > end) return 0;
                v = (int32_t)((int64_t)ft_u32(f, p) - (ft_u32(f, p) >> 31 ? (int64_t)1 << 32 : 0)), p += 4;
            }
            if (sp >= 48) return 0;
            st[sp++] = v;
            continue;
        }
        int i = 0, ok = 1;
        switch (b) {
        case 1: case 3: case 18: case 23:
            if (!haswidth && (sp & 1)) i = 1;
            haswidth = 1;
            s.nstems += (sp - i) / 2;
            sp = 0;
            break;
        case 19: case 20:
            if (!haswidth && (sp & 1)) i = 1;
            haswidth = 1;
            s.nstems += (sp - i) / 2;
            p += (uint32_t)(s.nstems + 7) / 8;
            sp = 0;
            break;
        case 21:
            if (!haswidth && sp > 2) i = 1;
            haswidth = 1;
            if (sp - i < 2) return 0;
            ok = ft_cs_move(f, &s, st[i], st[i + 1]);
            sp = 0;
            break;
        case 22: case 4:
            if (!haswidth && sp > 1) i = 1;
            haswidth = 1;
            if (sp - i < 1) return 0;
            ok = b == 22 ? ft_cs_move(f, &s, st[i], 0) : ft_cs_move(f, &s, 0, st[i]);
            sp = 0;
            break;
        case 5:
            for (; ok && i + 1 < sp; i += 2) ok = ft_cs_line(f, &s, st[i], st[i + 1]);
            sp = 0;
            break;
        case 6: case 7: {
            int h = b == 6;
            for (; ok && i < sp; i++, h = !h) ok = h ? ft_cs_line(f, &s, st[i], 0) : ft_cs_line(f, &s, 0, st[i]);
            sp = 0;
            break;
        }
        case 8:
            for (; ok && i + 5 < sp; i += 6) ok = ft_cs_curve(f, &s, st[i], st[i + 1], st[i + 2], st[i + 3], st[i + 4], st[i + 5]);
            sp = 0;
            break;
        case 24:
            if (sp < 8) return 0;
            for (; ok && i + 5 < sp - 2; i += 6) ok = ft_cs_curve(f, &s, st[i], st[i + 1], st[i + 2], st[i + 3], st[i + 4], st[i + 5]);
            if (ok) ok = ft_cs_line(f, &s, st[i], st[i + 1]);
            sp = 0;
            break;
        case 25:
            if (sp < 8) return 0;
            for (; ok && i + 1 < sp - 6; i += 2) ok = ft_cs_line(f, &s, st[i], st[i + 1]);
            if (ok) ok = ft_cs_curve(f, &s, st[i], st[i + 1], st[i + 2], st[i + 3], st[i + 4], st[i + 5]);
            sp = 0;
            break;
        case 26: case 27: {
            int64_t first = 0;
            if (sp & 1) first = st[0], i = 1;
            for (; ok && i + 3 < sp; i += 4, first = 0)
                ok = b == 26 ? ft_cs_curve(f, &s, first, st[i], st[i + 1], st[i + 2], 0, st[i + 3])
                             : ft_cs_curve(f, &s, st[i], first, st[i + 1], st[i + 2], st[i + 3], 0);
            sp = 0;
            break;
        }
        case 30: case 31: {
            int h = b == 31;
            for (; ok && i + 3 < sp; i += 4, h = !h) {
                int64_t last = sp - i == 5 ? st[i + 4] : 0;
                ok = h ? ft_cs_curve(f, &s, st[i], 0, st[i + 1], st[i + 2], last, st[i + 3])
                       : ft_cs_curve(f, &s, 0, st[i], st[i + 1], st[i + 2], st[i + 3], last);
            }
            sp = 0;
            break;
        }
        case 10: case 29: {
            if (sp < 1 || depth >= 10) return 0;
            const ft_idx *x = b == 10 ? &loc : &f->gsubr;
            int64_t k = (int64_t)ft_fdiv(st[--sp], 65536) + ft_bias(x->count);
            uint32_t sa, sl;
            if (k < 0 || !ft_item(f, x, (uint32_t)k, &sa, &sl)) return 0;
            rs[depth] = p, re[depth] = end, depth++;
            p = sa, end = sa + sl;
            break;
        }
        case 11:
            if (!depth) return 0;
            depth--, p = rs[depth], end = re[depth];
            break;
        case 14:
            ft_cs_close(f, &s);
            return 1;
        case 12: {
            if (p >= end) return 0;
            uint32_t e = f->d[p++];
            int32_t *a = st;
            if (e == 35 && sp >= 12) {
                ok = ft_cs_curve(f, &s, a[0], a[1], a[2], a[3], a[4], a[5]) && ft_cs_curve(f, &s, a[6], a[7], a[8], a[9], a[10], a[11]);
            } else if (e == 34 && sp >= 7) {
                ok = ft_cs_curve(f, &s, a[0], 0, a[1], a[2], a[3], 0) && ft_cs_curve(f, &s, a[4], 0, a[5], -(int64_t)a[2], a[6], 0);
            } else if (e == 36 && sp >= 9) {
                ok = ft_cs_curve(f, &s, a[0], a[1], a[2], a[3], a[4], 0) &&
                     ft_cs_curve(f, &s, a[5], 0, a[6], a[7], a[8], -((int64_t)a[1] + a[3] + a[7]));
            } else if (e == 37 && sp >= 11) {
                int64_t dx = (int64_t)a[0] + a[2] + a[4] + a[6] + a[8], dy = (int64_t)a[1] + a[3] + a[5] + a[7] + a[9];
                int64_t ex = ft_abs(dx) > ft_abs(dy) ? a[10] : -dx, ey = ft_abs(dx) > ft_abs(dy) ? -dy : a[10];
                ok = ft_cs_curve(f, &s, a[0], a[1], a[2], a[3], a[4], a[5]) && ft_cs_curve(f, &s, a[6], a[7], a[8], a[9], ex, ey);
            } else if (e == 9 && sp >= 1) {
                if (a[sp - 1] < 0) a[sp - 1] = a[sp - 1] == INT32_MIN ? INT32_MAX : -a[sp - 1];
                break;
            } else if ((e == 10 || e == 11) && sp >= 2) {
                int64_t v = e == 10 ? (int64_t)a[sp - 2] + a[sp - 1] : (int64_t)a[sp - 2] - a[sp - 1];
                a[sp - 2] = (int32_t)ft_clamp(v, INT32_MAX), sp--;
                break;
            } else if (e == 14 && sp >= 1) {
                a[sp - 1] = a[sp - 1] == INT32_MIN ? INT32_MAX : -a[sp - 1];
                break;
            } else if (e == 18 && sp >= 1) {
                sp--;
                break;
            } else if (e == 27 && sp >= 1 && sp < 48) {
                a[sp] = a[sp - 1], sp++;
                break;
            } else if (e == 28 && sp >= 2) {
                int32_t t = a[sp - 1];
                a[sp - 1] = a[sp - 2], a[sp - 2] = t;
                break;
            } else return 0;
            sp = 0;
            break;
        }
        default:
            return 0;
        }
        if (!ok) return 0;
    }
    ft_cs_close(f, &s);
    return 1;
}

static int ft_outline(goome_font *f, int g)
{
    f->npts = f->nends = f->ncomp = 0;
    int ok = f->cff ? ft_cff_glyph(f, g) : ft_glyf(f, g, 0);
    if (!ok) f->npts = f->nends = 0;
    if (f->nends && f->ends[f->nends - 1] != f->npts - 1) f->npts = f->nends = 0;
    return ok;
}

// a hollow box for missing glyphs when the font has no .notdef of its own
static void ft_box(goome_font *f)
{
    int64_t u = f->upem, x0 = u / 10, x1 = u * 55 / 100, y0 = 0, y1 = u * 7 / 10, w = u / 16 + 1;
    int64_t r[2][4] = {{x0, y0, x1, y1}, {x0 + w, y0 + w, x1 - w, y1 - w}};
    f->npts = f->nends = 0;
    if (!ft_grow(f, 8, 2)) return;
    for (int k = 0; k < 2; k++) {
        int64_t a = r[k][0] * 64, b = r[k][1] * 64, c = r[k][2] * 64, d = r[k][3] * 64;
        int64_t xs[4] = {a, a, c, c}, ys[4] = {b, d, d, b};
        for (int i = 0; i < 4; i++) {
            int j = k ? 3 - i : i;
            f->pts[f->npts].x = (int32_t)xs[j], f->pts[f->npts].y = (int32_t)ys[j], f->pts[f->npts].t = 0, f->npts++;
        }
        f->ends[f->nends++] = f->npts - 1;
    }
}

// raster: cover + area cells like font-rs, all ints

static void ft_cell(goome_font *f, int row, int col, int fx0, int fx1, int dy)
{
    size_t i = (size_t)row * (size_t)(f->rw + 2) + (size_t)col;
    f->cover[i] += dy;
    f->area[i] += (int64_t)dy * (fx0 + fx1);
}

static void ft_row(goome_font *f, int row, int xa, int fa, int xb, int fb, int dir)
{
    int ca = xa >> 8, cb = xb >> 8;
    if (ca == cb) {
        ft_cell(f, row, ca, xa - ca * 256, xb - ca * 256, (fb - fa) * dir);
        return;
    }
    int x = xa, y = fa;
    if (xb > xa) {
        for (int c = ca; c < cb; c++) {
            int bx = (c + 1) * 256, by = fa + (int)ft_divr((int64_t)(fb - fa) * (bx - xa), xb - xa);
            ft_cell(f, row, c, x - c * 256, 256, (by - y) * dir);
            x = bx, y = by;
        }
    } else {
        for (int c = ca; c > cb; c--) {
            int bx = c * 256, by = fa + (int)ft_divr((int64_t)(fb - fa) * (xa - bx), xa - xb);
            ft_cell(f, row, c, x - c * 256, 0, (by - y) * dir);
            x = bx, y = by;
        }
    }
    ft_cell(f, row, cb, x - cb * 256, xb - cb * 256, (fb - y) * dir);
}

static void ft_line(goome_font *f, int x0, int y0, int x1, int y1)
{
    if (y0 == y1) return;
    if (++f->nseg > FT_MAXSEG) {
        f->bad = 1;
        return;
    }
    int dir = 1;
    if (y0 > y1) {
        int t = x0;
        x0 = x1, x1 = t, t = y0, y0 = y1, y1 = t, dir = -1;
    }
    int64_t dx = x1 - x0, dy = y1 - y0;
    int ya = y0, xa = x0;
    while (ya < y1) {
        int row = ya >> 8, yb = (row + 1) * 256;
        if (yb > y1) yb = y1;
        int xb = yb == y1 ? x1 : x0 + (int)ft_divr(dx * (yb - y0), dy);
        ft_row(f, row, xa, ya - row * 256, xb, yb - row * 256, dir);
        ya = yb, xa = xb;
    }
}

#define FT_TOL 16

static void ft_quad(goome_font *f, int x0, int y0, int x1, int y1, int x2, int y2)
{
    int64_t dd = ft_abs((int64_t)x0 - 2 * x1 + x2) + ft_abs((int64_t)y0 - 2 * y1 + y2);
    int64_t n = 1;
    while (n < 64 && n * n * 4 * FT_TOL < dd) n++;
    int px = x0, py = y0;
    for (int64_t i = 1; i <= n; i++) {
        int64_t j = n - i, nn = n * n;
        int qx = (int)((x0 * j * j + 2 * x1 * i * j + x2 * i * i + nn / 2) / nn), qy = (int)((y0 * j * j + 2 * y1 * i * j + y2 * i * i + nn / 2) / nn);
        ft_line(f, px, py, qx, qy);
        px = qx, py = qy;
    }
}

static void ft_cubic(goome_font *f, int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3)
{
    int64_t a = ft_abs((int64_t)x0 - 2 * x1 + x2) + ft_abs((int64_t)y0 - 2 * y1 + y2);
    int64_t b = ft_abs((int64_t)x1 - 2 * x2 + x3) + ft_abs((int64_t)y1 - 2 * y2 + y3);
    int64_t dd = (a > b ? a : b) * 3, n = 1;
    while (n < 64 && n * n * 4 * FT_TOL < dd) n++;
    int px = x0, py = y0;
    for (int64_t i = 1; i <= n; i++) {
        int64_t j = n - i, n3 = n * n * n;
        int qx = (int)((x0 * j * j * j + 3 * x1 * i * j * j + 3 * x2 * i * i * j + x3 * i * i * i + n3 / 2) / n3);
        int qy = (int)((y0 * j * j * j + 3 * y1 * i * j * j + 3 * y2 * i * i * j + y3 * i * i * i + n3 / 2) / n3);
        ft_line(f, px, py, qx, qy);
        px = qx, py = qy;
    }
}

static void ft_contour(goome_font *f, int s, int e)
{
    ft_pt *p = f->pts;
    int n = e - s + 1, k = -1;
    if (n < 2) return;
    for (int i = s; i <= e; i++)
        if (p[i].t == 0) {
            k = i;
            break;
        }
    int sx, sy, first;
    if (k < 0) {
        sx = (p[e].x + p[s].x) / 2, sy = (p[e].y + p[s].y) / 2, first = s;
    } else {
        sx = p[k].x, sy = p[k].y, first = k + 1;
    }
    int cx = sx, cy = sy, nc = 0, c[4] = {0, 0, 0, 0}, ctype = 0;
    for (int m = 0; m <= n; m++) {
        int i = s + (first - s + m) % n, ex, ey, t;
        if (m == n) {
            ex = sx, ey = sy, t = 0;
        } else {
            if (k >= 0 && m == n - 1 && i == k) {
                ex = sx, ey = sy, t = 0;
                m = n;
            } else ex = p[i].x, ey = p[i].y, t = p[i].t;
        }
        if (t == 0) {
            if (nc == 0) ft_line(f, cx, cy, ex, ey);
            else if (nc == 1) ft_quad(f, cx, cy, c[0], c[1], ex, ey);
            else ft_cubic(f, cx, cy, c[0], c[1], c[2], c[3], ex, ey);
            cx = ex, cy = ey, nc = 0;
        } else if (t == 1) {
            if (nc == 1 && ctype == 1) {
                int mx = (c[0] + ex) / 2, my = (c[1] + ey) / 2;
                ft_quad(f, cx, cy, c[0], c[1], mx, my);
                cx = mx, cy = my;
                c[0] = ex, c[1] = ey;
            } else {
                if (nc) ft_line(f, cx, cy, ex, ey), cx = ex, cy = ey, nc = 0;
                else c[0] = ex, c[1] = ey, nc = 1;
            }
            ctype = 1;
        } else {
            if (nc == 2 || ctype == 1) {
                if (nc) ft_line(f, cx, cy, c[2 * nc - 2], c[2 * nc - 1]), cx = c[2 * nc - 2], cy = c[2 * nc - 1];
                nc = 0;
            }
            c[nc * 2] = ex, c[nc * 2 + 1] = ey, nc++, ctype = 2;
        }
    }
}

static void ft_gamma_init(void)
{
    if (ft_gamma_ok) return;
    for (int i = 0; i < 256; i++) ft_gamma[i] = (uint8_t)(i + (i * (255 - i) * 3 + 1275) / 2550 > 255 ? 255 : i + (i * (255 - i) * 3 + 1275) / 2550);
    ft_gamma_ok = 1;
}

// renders glyph g into f->tmp, w x h at (l, t) from the pen, 0 when theres nothing to draw
static int ft_render(goome_font *f, int g, int64_t sx, int64_t sy, int ph, int *w, int *h, int *l, int *t)
{
    ft_outline(f, g);
    if (!f->npts && g == 0) ft_box(f);
    if (!f->npts) return 0;
    int64_t mnx = INT64_MAX, mny = INT64_MAX, mxx = INT64_MIN, mxy = INT64_MIN;
    for (int i = 0; i < f->npts; i++) {
        int64_t x = (f->pts[i].x * sx >> 24) + ph * 64, y = -(f->pts[i].y * sy >> 24);
        if (x < mnx) mnx = x;
        if (x > mxx) mxx = x;
        if (y < mny) mny = y;
        if (y > mxy) mxy = y;
        if (x < -268435456 || x > 268435456 || y < -268435456 || y > 268435456) return 0;
        f->pts[i].x = (int32_t)x, f->pts[i].y = (int32_t)y;
    }
    int64_t bx = ft_fdiv(mnx, 256), by = ft_fdiv(mny, 256), bw = -ft_fdiv(-mxx, 256) - bx, bh = -ft_fdiv(-mxy, 256) - by;
    if (bw <= 0 || bh <= 0 || bw > FT_MAXDIM || bh > FT_MAXDIM || bw * bh > 1048576) return 0;
    int W = (int)bw, H = (int)bh;
    size_t cells = (size_t)(W + 2) * (size_t)(H + 1);
    if (cells > f->cells) {
        free(f->cover), free(f->area);
        f->cover = calloc(cells, sizeof(int32_t)), f->area = calloc(cells, sizeof(int64_t));
        f->cells = f->cover && f->area ? cells : 0;
        if (!f->cells) return 0;
    }
    if ((size_t)W * H > f->tmpcap) {
        uint8_t *nt = realloc(f->tmp, (size_t)W * H);
        if (!nt) return 0;
        f->tmp = nt, f->tmpcap = (size_t)W * H;
    }
    for (int i = 0; i < f->npts; i++) f->pts[i].x -= (int32_t)(bx * 256), f->pts[i].y -= (int32_t)(by * 256);
    f->rw = W, f->rh = H, f->nseg = 0, f->bad = 0;
    int s = 0;
    for (int k = 0; k < f->nends; k++) {
        int e = f->ends[k];
        if (e >= s && e < f->npts) ft_contour(f, s, e);
        s = e + 1;
    }
    ft_gamma_init();
    int any = 0;
    for (int y = 0; y < H; y++) {
        int32_t *cv = f->cover + (size_t)y * (W + 2);
        int64_t *ar = f->area + (size_t)y * (W + 2), acc = 0;
        uint8_t *o = f->tmp + (size_t)y * W;
        for (int x = 0; x < W; x++) {
            int64_t v = ft_abs((acc + cv[x]) * 512 - ar[x]);
            acc += cv[x];
            int c = v >= 131072 ? 255 : (int)((v * 255 + 65536) >> 17);
            o[x] = ft_gamma[c];
            any |= c;
            cv[x] = 0, ar[x] = 0;
        }
        cv[W] = 0, ar[W] = 0, cv[W + 1] = 0, ar[W + 1] = 0;
    }
    if (f->bad || !any) return 0;
    *w = W, *h = H, *l = (int)bx, *t = (int)by;
    return 1;
}

static void ft_cache_clear(goome_font *f)
{
    if (f->slots) memset(f->slots, 0xFF, FT_SLOTS * sizeof(int32_t));
    f->nents = 0, f->used = 0;
}

static uint32_t ft_hash(uint64_t k)
{
    k *= 0x9E3779B97F4A7C15ull;
    return (uint32_t)(k >> 40) & (FT_SLOTS - 1);
}

// cached glyph, or NULL. *bits points at w*h coverage bytes
static const ft_ent *ft_glyph(goome_font *f, int g, int px64, int64_t sx, int64_t sy, int ph, const uint8_t **bits)
{
    static ft_ent big;
    uint64_t key = (uint64_t)g | (uint64_t)ph << 16 | (uint64_t)px64 << 18;
    if (!f->slots) {
        f->slots = malloc(FT_SLOTS * sizeof(int32_t));
        f->ents = malloc((FT_SLOTS * 3 / 4) * sizeof(ft_ent));
        f->arena = malloc(FT_ARENA);
        if (!f->slots || !f->ents || !f->arena) {
            free(f->slots), free(f->ents), free(f->arena);
            f->slots = NULL, f->ents = NULL, f->arena = NULL;
            return NULL;
        }
        ft_cache_clear(f);
    }
    uint32_t h = ft_hash(key);
    for (;;) {
        int32_t i = f->slots[h];
        if (i < 0) break;
        if (f->ents[i].key == key) {
            *bits = f->arena + f->ents[i].off;
            return f->ents[i].w ? &f->ents[i] : NULL;
        }
        h = (h + 1) & (FT_SLOTS - 1);
    }
    int w = 0, hh = 0, l = 0, t = 0;
    int ok = ft_render(f, g, sx, sy, ph, &w, &hh, &l, &t);
    size_t sz = ok ? (size_t)w * hh : 0;
    if (sz > FT_ARENA / 8) {
        big.key = key, big.w = (int16_t)w, big.h = (int16_t)hh, big.l = (int16_t)l, big.t = (int16_t)t, big.off = 0;
        *bits = f->tmp;
        return &big;
    }
    if (f->nents >= FT_SLOTS * 3 / 4 || f->used + sz > FT_ARENA) {
        ft_cache_clear(f);
        h = ft_hash(key);
    }
    ft_ent *e = &f->ents[f->nents];
    e->key = key, e->off = f->used, e->w = (int16_t)(ok ? w : 0), e->h = (int16_t)hh, e->l = (int16_t)l, e->t = (int16_t)t;
    if (sz) memcpy(f->arena + f->used, f->tmp, sz);
    f->used += (uint32_t)((sz + 7) & ~(size_t)7);
    while (f->slots[h] >= 0) h = (h + 1) & (FT_SLOTS - 1);
    f->slots[h] = f->nents++;
    *bits = f->arena + e->off;
    return ok ? e : NULL;
}

static int ft_coverage(const goome_font *f, uint32_t o, int g)
{
    uint32_t fmt = ft_u16(f, o), n = ft_u16(f, o + 2), lo = 0, hi = n;
    if (fmt == 1) {
        while (lo < hi) {
            uint32_t m = (lo + hi) / 2, v = ft_u16(f, o + 4 + m * 2);
            if (v == (uint32_t)g) return (int)m;
            if (v < (uint32_t)g) lo = m + 1;
            else hi = m;
        }
    } else if (fmt == 2) {
        while (lo < hi) {
            uint32_t m = (lo + hi) / 2, r = o + 4 + m * 6;
            if ((uint32_t)g < ft_u16(f, r)) hi = m;
            else if ((uint32_t)g > ft_u16(f, r + 2)) lo = m + 1;
            else return (int)(ft_u16(f, r + 4) + (uint32_t)g - ft_u16(f, r));
        }
    }
    return -1;
}

static int ft_class(const goome_font *f, uint32_t o, int g)
{
    uint32_t fmt = ft_u16(f, o);
    if (fmt == 1) {
        uint32_t st = ft_u16(f, o + 2), n = ft_u16(f, o + 4);
        return (uint32_t)g >= st && (uint32_t)g - st < n ? (int)ft_u16(f, o + 6 + ((uint32_t)g - st) * 2) : 0;
    }
    if (fmt == 2) {
        uint32_t n = ft_u16(f, o + 2), lo = 0, hi = n;
        while (lo < hi) {
            uint32_t m = (lo + hi) / 2, r = o + 4 + m * 6;
            if ((uint32_t)g < ft_u16(f, r)) hi = m;
            else if ((uint32_t)g > ft_u16(f, r + 2)) lo = m + 1;
            else return (int)ft_u16(f, r + 4);
        }
    }
    return 0;
}

static int ft_bits(uint32_t v)
{
    int n = 0;
    for (; v; v &= v - 1) n++;
    return n;
}

// 1 if subtable st applied, *v gets the x advance change
static int ft_pairpos(const goome_font *f, uint32_t st, int a, int b, int *v)
{
    uint32_t fmt = ft_u16(f, st);
    int ci = ft_coverage(f, st + ft_u16(f, st + 2), a);
    if (ci < 0) return 0;
    uint32_t vf1 = ft_u16(f, st + 4), vf2 = ft_u16(f, st + 6);
    uint32_t s1 = (uint32_t)ft_bits(vf1) * 2, s2 = (uint32_t)ft_bits(vf2) * 2, xo = (uint32_t)ft_bits(vf1 & 3) * 2;
    if (fmt == 1) {
        if ((uint32_t)ci >= ft_u16(f, st + 8)) return 0;
        uint32_t ps = st + ft_u16(f, st + 10 + (uint32_t)ci * 2), n = ft_u16(f, ps), rs = 2 + s1 + s2, lo = 0, hi = n;
        while (lo < hi) {
            uint32_t m = (lo + hi) / 2, r = ps + 2 + m * rs, sg = ft_u16(f, r);
            if (sg == (uint32_t)b) {
                *v = vf1 & 4 ? ft_s16(f, r + 2 + xo) : 0;
                return 1;
            }
            if (sg < (uint32_t)b) lo = m + 1;
            else hi = m;
        }
        return 0;
    }
    if (fmt == 2) {
        uint32_t c1 = (uint32_t)ft_class(f, st + ft_u16(f, st + 8), a), c2 = (uint32_t)ft_class(f, st + ft_u16(f, st + 10), b);
        uint32_t n1 = ft_u16(f, st + 12), n2 = ft_u16(f, st + 14);
        if (c1 >= n1 || c2 >= n2) return 0;
        uint32_t r = st + 16 + (c1 * n2 + c2) * (s1 + s2);
        *v = vf1 & 4 ? ft_s16(f, r + xo) : 0;
        return 1;
    }
    return 0;
}

static void ft_kern_init(goome_font *f)
{
    uint32_t g = f->gpos;
    if (!g) return;
    uint32_t fl = g + ft_u16(f, g + 6), ll = g + ft_u16(f, g + 8), nf = ft_u16(f, fl), nl = ft_u16(f, ll);
    uint8_t *use = calloc(nl + 1, 1);
    if (!use) return;
    for (uint32_t i = 0; i < nf; i++) {
        uint32_t r = fl + 2 + i * 6;
        if (ft_u32(f, r) != 0x6B65726Eu) continue;
        uint32_t ft = fl + ft_u16(f, r + 4), n = ft_u16(f, ft + 2);
        for (uint32_t k = 0; k < n; k++) {
            uint32_t li = ft_u16(f, ft + 4 + k * 2);
            if (li < nl) use[li] = 1;
        }
    }
    int cap = 0;
    for (uint32_t i = 0; i < nl && f->nksub < 512; i++) {
        if (!use[i]) continue;
        uint32_t lk = ll + ft_u16(f, ll + 2 + i * 2), type = ft_u16(f, lk), ns = ft_u16(f, lk + 4);
        for (uint32_t k = 0; k < ns && f->nksub < 512; k++) {
            uint32_t st = lk + ft_u16(f, lk + 6 + k * 2), ty = type;
            if (ty == 9 && ft_u16(f, st) == 1) ty = ft_u16(f, st + 2), st += ft_u32(f, st + 4);
            if (ty != 2 || !ft_has(f, st, 8)) continue;
            if (f->nksub >= cap) {
                cap = cap ? cap * 2 : 16;
                uint32_t *nk = realloc(f->ksub, (size_t)cap * sizeof(uint32_t));
                if (!nk) break;
                f->ksub = nk;
            }
            f->ksub[f->nksub++] = st;
        }
    }
    free(use);
}

static int ft_kern_raw(const goome_font *f, int a, int b)
{
    if (f->nksub) {
        int sum = 0, v;
        for (int i = 0; i < f->nksub; i++) {
            if (ft_pairpos(f, f->ksub[i], a, b, &v)) sum += v;
        }
        return sum;
    }
    uint32_t k = f->kern;
    if (!k || ft_u16(f, k) != 0) return 0;
    uint32_t nt = ft_u16(f, k + 2), o = k + 4, want = (uint32_t)a << 16 | (uint32_t)b;
    for (uint32_t t = 0; t < nt && t < 64; t++) {
        uint32_t len = ft_u16(f, o + 2), cov = ft_u16(f, o + 4);
        if ((cov >> 8) == 0 && (cov & 1) && !(cov & 4)) {
            uint32_t np = ft_u16(f, o + 6), lo = 0, hi = np;
            while (lo < hi) {
                uint32_t m = (lo + hi) / 2, r = o + 14 + m * 6, key = ft_u32(f, r);
                if (key == want) return ft_s16(f, r + 4);
                if (key < want) lo = m + 1;
                else hi = m;
            }
        }
        if (len < 6) break;
        o += len;
    }
    return 0;
}

static int ft_kern(goome_font *f, int a, int b)
{
    if (!f->nksub && !f->kern) return 0;
    uint32_t key = ((uint32_t)a << 16 | (uint32_t)b) + 1, h = (key * 2654435761u) >> 21 & (FT_KC - 1);
    if (f->kck[h] == key) return f->kcv[h];
    int v = ft_kern_raw(f, a, b);
    if (v < -32768) v = -32768;
    if (v > 32767) v = 32767;
    f->kck[h] = key, f->kcv[h] = (int16_t)v;
    return v;
}

static int ft_tag_is(uint32_t t, const char *const *tags)
{
    for (; *tags; tags++)
        if (t == ((uint32_t)(unsigned char)(*tags)[0] << 24 | (uint32_t)(unsigned char)(*tags)[1] << 16 | (uint32_t)(unsigned char)(*tags)[2] << 8 | (uint32_t)(unsigned char)(*tags)[3])) return 1;
    return 0;
}

static void ft_gsub_init(goome_font *f, uint32_t g)
{
    static const char *const tags[] = {"ccmp", "liga", "rlig", "clig", NULL};
    if (!g || !ft_has(f, g, 10)) return;
    uint32_t fl = g + ft_u16(f, g + 6), ll = g + ft_u16(f, g + 8), nf = ft_u16(f, fl), nl = ft_u16(f, ll);
    uint8_t *use = calloc(nl + 1, 1);
    if (!use) return;
    for (uint32_t i = 0; i < nf; i++) {
        uint32_t r = fl + 2 + i * 6;
        if (!ft_tag_is(ft_u32(f, r), tags)) continue;
        uint32_t ft = fl + ft_u16(f, r + 4), n = ft_u16(f, ft + 2);
        for (uint32_t k = 0; k < n; k++) {
            uint32_t li = ft_u16(f, ft + 4 + k * 2);
            if (li < nl) use[li] = 1;
        }
    }
    for (uint32_t i = 0; i < nl && f->nlig < FT_NLIG; i++) {
        if (!use[i]) continue;
        uint32_t lk = ll + ft_u16(f, ll + 2 + i * 2), type = ft_u16(f, lk), ns = ft_u16(f, lk + 4);
        for (uint32_t k = 0; k < ns && f->nlig < FT_NLIG; k++) {
            uint32_t st = lk + ft_u16(f, lk + 6 + k * 2), ty = type;
            if (ty == 7 && ft_u16(f, st) == 1) ty = ft_u16(f, st + 2), st += ft_u32(f, st + 4);
            if (ty == 4 && ft_u16(f, st) == 1 && ft_has(f, st, 6)) f->lig[f->nlig++] = st;
        }
    }
    free(use);
}

static int ft_parse(goome_font *f, int index)
{
    if (f->n < 12 || f->n > (f->map ? FT_MAXMAP : FT_MAXFILE)) return 0;
    uint32_t dir = 0, tag = ft_u32(f, 0);
    if (tag == 0x74746366u) {
        uint32_t nf = ft_u32(f, 8);
        if (index < 0 || (uint32_t)index >= nf) return 0;
        dir = ft_u32(f, 12 + (uint32_t)index * 4);
        tag = ft_u32(f, dir);
    } else if (index != 0) return 0;
    if (tag != 0x00010000u && tag != 0x74727565u && tag != 0x4F54544Fu) return 0;
    uint32_t head = ft_table(f, dir, "head", NULL), hhea = ft_table(f, dir, "hhea", NULL), maxp = ft_table(f, dir, "maxp", NULL);
    uint32_t cmap = ft_table(f, dir, "cmap", NULL), os2 = ft_table(f, dir, "OS/2", NULL), cff, cfflen = 0;
    if (!head || !hhea || !maxp || !cmap || !ft_has(f, head, 54) || !ft_has(f, hhea, 36)) return 0;
    f->hmtx = ft_table(f, dir, "hmtx", &f->hmtx_n);
    f->upem = (int)ft_u16(f, head + 18);
    f->locfmt = ft_s16(f, head + 50);
    f->nglyphs = (int)ft_u16(f, maxp + 4);
    f->nhm = (int)ft_u16(f, hhea + 34);
    if (f->upem < 16 || f->upem > 16384 || f->nglyphs < 1 || !f->hmtx) return 0;
    if ((uint32_t)f->nhm * 4 > f->hmtx_n) f->nhm = (int)(f->hmtx_n / 4);
    if (f->nhm < 1) return 0;
    f->glyf = ft_table(f, dir, "glyf", &f->glyf_n);
    f->loca = ft_table(f, dir, "loca", &f->loca_n);
    if (!f->glyf || !f->loca) {
        cff = ft_table(f, dir, "CFF ", &cfflen);
        if (cff && ft_cff_init(f, cff, cfflen)) f->cff = 1;
        else if (!ft_table(f, dir, "CBDT", NULL) && !ft_table(f, dir, "sbix", NULL)) return 0;
        else f->glyf = f->loca = f->glyf_n = f->loca_n = 0;
    }
    if (!ft_cmap_find(f, cmap)) return 0;
    for (int i = 0; i < 256; i++) {
        int g = ft_cmap_raw(f, (uint32_t)i);
        f->low[i] = (uint16_t)(g < f->nglyphs ? g : 0);
    }
    f->asc = ft_s16(f, hhea + 4), f->desc = ft_s16(f, hhea + 6), f->gap = ft_s16(f, hhea + 8);
    if (os2 && ft_has(f, os2, 78)) {
        int ta = ft_s16(f, os2 + 68), td = ft_s16(f, os2 + 70), tg = ft_s16(f, os2 + 72);
        if ((ft_u16(f, os2 + 62) & 128) || (f->asc == 0 && f->desc == 0)) f->asc = ta, f->desc = td, f->gap = tg;
        if (f->asc == 0 && f->desc == 0) f->asc = (int)ft_u16(f, os2 + 74), f->desc = -(int)ft_u16(f, os2 + 76), f->gap = 0;
        if (ft_u16(f, os2) >= 2 && ft_has(f, os2, 88)) f->xh = ft_s16(f, os2 + 86);
    }
    if (f->asc <= 0 || f->asc > f->upem * 4) f->asc = f->upem * 8 / 10;
    if (f->desc > 0) f->desc = -f->desc;
    if (f->desc < -f->upem * 4) f->desc = -f->upem / 5;
    if (f->gap < 0 || f->gap > f->upem * 4) f->gap = 0;
    if (f->xh <= 0 || f->xh > f->upem) {
        f->xh = 0;
        int g = ft_cmap(f, 'x');
        if (g && ft_outline(f, g)) {
            int64_t m = 0;
            for (int i = 0; i < f->npts; i++)
                if (f->pts[i].y > m) m = f->pts[i].y;
            f->xh = (int)(m / 64);
        }
        if (f->xh <= 0 || f->xh > f->upem) f->xh = 0;
    }
    f->kern = ft_table(f, dir, "kern", NULL);
    f->gpos = ft_table(f, dir, "GPOS", NULL);
    ft_kern_init(f);
    ft_gsub_init(f, ft_table(f, dir, "GSUB", NULL));
    f->colr = ft_table(f, dir, "COLR", NULL), f->cpal = ft_table(f, dir, "CPAL", NULL);
    f->cblc = ft_table(f, dir, "CBLC", NULL), f->cbdt = ft_table(f, dir, "CBDT", NULL);
    f->sbix = ft_table(f, dir, "sbix", NULL);
    if (!f->colr || !f->cpal || !ft_has(f, f->colr, 14) || !ft_has(f, f->cpal, 14)) f->colr = 0;
    if (!f->cblc || !f->cbdt || !ft_has(f, f->cblc, 8)) f->cblc = 0;
    if (!f->sbix || !ft_has(f, f->sbix, 8)) f->sbix = 0;
    f->color = f->colr || f->cblc || f->sbix;
    return 1;
}

static goome_font *ft_new(const uint8_t *d, size_t n, uint8_t *own, int index, void *map)
{
    goome_font *f = calloc(1, sizeof(*f));
    if (!f) {
        free(own);
#if !defined(_WIN32) && (defined(__unix__) || defined(__APPLE__))
        if (map) munmap(map, n);
#endif
        return NULL;
    }
    f->d = d, f->n = n, f->own = own, f->map = map, f->maplen = map ? n : 0;
    if (!ft_parse(f, index)) {
        goome_font_free(f);
        return NULL;
    }
    return f;
}

static FILE *ft_fopen(const char *path)
{
#if defined(_WIN32)
    wchar_t w[1024];
    size_t k = 0;
    const unsigned char *s = (const unsigned char *)path;
    while (*s && k < 1020) {
        uint32_t c = *s++;
        if (c >= 0xF0 && (s[0] & 0xC0) == 0x80 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80)
            c = (c & 7) << 18 | (uint32_t)(s[0] & 63) << 12 | (uint32_t)(s[1] & 63) << 6 | (s[2] & 63), s += 3;
        else if (c >= 0xE0 && (s[0] & 0xC0) == 0x80 && (s[1] & 0xC0) == 0x80)
            c = (c & 15) << 12 | (uint32_t)(s[0] & 63) << 6 | (s[1] & 63), s += 2;
        else if (c >= 0xC0 && (s[0] & 0xC0) == 0x80)
            c = (c & 31) << 6 | (s[0] & 63), s += 1;
        if (c >= 0x10000) {
            c -= 0x10000;
            w[k++] = (wchar_t)(0xD800 | c >> 10), w[k++] = (wchar_t)(0xDC00 | (c & 1023));
        } else w[k++] = (wchar_t)c;
    }
    w[k] = 0;
    return _wfopen(w, L"rb");
#else
    return fopen(path, "rb");
#endif
}

goome_font *goome_font_load_index(const char *path, int index)
{
    if (!path) return NULL;
    FILE *fp = ft_fopen(path);
    if (!fp) return NULL;
    size_t cap = 65536, n = 0;
    uint8_t *d = malloc(cap);
    while (d) {
        if (n == cap) {
            if (cap >= FT_MAXFILE) {
                free(d), d = NULL;
                break;
            }
            uint8_t *nd = realloc(d, cap * 2);
            if (!nd) {
                free(d), d = NULL;
                break;
            }
            d = nd, cap *= 2;
        }
        size_t got = fread(d + n, 1, cap - n, fp);
        if (!got) break;
        n += got;
    }
    fclose(fp);
    if (!d || n > FT_MAXFILE) {
        free(d);
        return NULL;
    }
    return ft_new(d, n, d, index, NULL);
}

goome_font *goome_font_load(const char *path) { return goome_font_load_index(path, 0); }

goome_font *goome_font_load_mem(const void *data, size_t n, int copy)
{
    if (!data || n < 12 || n > FT_MAXFILE) return NULL;
    if (!copy) return ft_new(data, n, NULL, 0, NULL);
    uint8_t *d = malloc(n);
    if (!d) return NULL;
    memcpy(d, data, n);
    return ft_new(d, n, d, 0, NULL);
}

static void ft_cc_clear(goome_font *f)
{
    for (int i = 0; f->cc && i < FT_CC; i++) free(f->cc[i].px), f->cc[i].px = NULL, f->cc[i].px64 = 0;
    f->ncc = 0, f->ccb = 0;
}

void goome_font_free(goome_font *f)
{
    if (!f || f->shared) return;
    ft_cc_clear(f);
    free(f->cc);
#if !defined(_WIN32) && (defined(__unix__) || defined(__APPLE__))
    if (f->map) munmap(f->map, f->maplen);
#endif
    free(f->own), free(f->ksub), free(f->ents), free(f->slots), free(f->arena);
    free(f->pts), free(f->ends), free(f->cover), free(f->area), free(f->tmp);
    free(f->scp), free(f->ssh), free(f->soff), free(f->scs), free(f->sord), free(f->scl), free(f->swk), free(f->slv), free(f->sgl);
    free(f);
}

static goome_font *ft_map_load(const char *path, int index)
{
#if !defined(_WIN32) && (defined(__unix__) || defined(__APPLE__))
    int fd = open(path, O_RDONLY);
    if (fd < 0) return NULL;
    struct stat st;
    if (fstat(fd, &st) || st.st_size < 12 || (uint64_t)st.st_size > FT_MAXMAP) {
        close(fd);
        return NULL;
    }
    size_t n = (size_t)st.st_size;
    void *m = mmap(NULL, n, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (m == MAP_FAILED) return NULL;
    return ft_new(m, n, NULL, index, m);
#else
    return goome_font_load_index(path, index);
#endif
}

static struct {
    char path[512];
    int index;
    goome_font *f;
} ft_reg[FT_REG];
static int ft_nreg;

static goome_font *ft_shared(const char *path, int index)
{
    if (!path || strlen(path) >= sizeof(ft_reg[0].path)) return NULL;
    for (int i = 0; i < ft_nreg; i++)
        if (ft_reg[i].index == index && !strcmp(ft_reg[i].path, path)) return ft_reg[i].f;
    if (ft_nreg >= FT_REG) return NULL;
    goome_font *f = ft_map_load(path, index);
    if (!f && index) f = ft_map_load(path, 0);
    strcpy(ft_reg[ft_nreg].path, path);
    ft_reg[ft_nreg].index = index, ft_reg[ft_nreg].f = f;
    ft_nreg++;
    if (f) f->shared = 1;
    return f;
}

void goome_font_fallback(goome_font *f, goome_font *fallback)
{
    if (!f || !fallback || f == fallback || f->nfb >= FT_NFB) return;
    for (int i = 0; i < f->nfb; i++)
        if (f->fb[i] == fallback) return;
    f->fb[f->nfb++] = fallback;
}

static int ft_norm(const char *s, char *out, int max)
{
    int k = 0;
    for (; *s && k < max - 1; s++) {
        int c = (unsigned char)*s;
        if (c >= 'A' && c <= 'Z') c += 32;
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) out[k++] = (char)c;
    }
    out[k] = 0;
    return k;
}

static int ft_font_ext(const char *name, char *base, int max)
{
    size_t n = strlen(name);
    if (n < 5) return 0;
    const char *e = name + n - 4;
    char x[5];
    for (int i = 0; i < 4; i++) x[i] = (char)(e[i] >= 'A' && e[i] <= 'Z' ? e[i] + 32 : e[i]);
    x[4] = 0;
    if (strcmp(x, ".ttf") && strcmp(x, ".otf") && strcmp(x, ".ttc")) return 0;
    char tmp[256];
    size_t m = n - 4 < sizeof(tmp) - 1 ? n - 4 : sizeof(tmp) - 1;
    memcpy(tmp, name, m);
    tmp[m] = 0;
    ft_norm(tmp, base, max);
    return 1;
}

static int ft_score(const char *base, const char *want)
{
    size_t n = strlen(want);
    if (!strcmp(base, want)) return 100;
    if (strncmp(base, want, n)) return 0;
    const char *r = base + n;
    if (!strcmp(r, "regular") || !strcmp(r, "r") || !strcmp(r, "book") || !strcmp(r, "roman")) return 90;
    if (!strcmp(r, "wdthwght") || !strcmp(r, "wght") || !strcmp(r, "vf")) return 80;
    return 50 - (int)(strlen(r) > 40 ? 40 : strlen(r));
}

typedef struct {
    char want[128];
    char best[1024];
    int score, seen;
} ft_hunt;

static int ft_join(char *out, size_t cap, const char *a, const char *b)
{
    size_t na = strlen(a), nb = strlen(b);
    if (na + nb + 2 > cap) return 0;
    memcpy(out, a, na);
    out[na] = '/';
    memcpy(out + na + 1, b, nb + 1);
    return 1;
}

static void ft_consider(ft_hunt *h, const char *dir, const char *name)
{
    char base[256];
    if (!ft_font_ext(name, base, sizeof(base))) return;
    int s = ft_score(base, h->want);
    if (s > h->score && ft_join(h->best, sizeof(h->best), dir, name)) h->score = s;
}

#if defined(_WIN32)
static void ft_scan(ft_hunt *h, const char *dir, int depth)
{
    wchar_t pat[600];
    size_t k = 0;
    for (const char *s = dir; *s && k < 580; s++) pat[k++] = (wchar_t)(unsigned char)*s;
    pat[k++] = L'\\', pat[k++] = L'*', pat[k] = 0;
    struct _wfinddata_t fd;
    intptr_t hd = _wfindfirst(pat, &fd);
    if (hd == -1) return;
    do {
        char nm[260];
        size_t j = 0;
        for (const wchar_t *w = fd.name; *w && j < sizeof(nm) - 1; w++) nm[j++] = *w < 128 ? (char)*w : '_';
        nm[j] = 0;
        if (!(fd.attrib & _A_SUBDIR)) ft_consider(h, dir, nm);
    } while (++h->seen < 20000 && _wfindnext(hd, &fd) == 0);
    _findclose(hd);
    (void)depth;
}
#elif defined(__unix__) || defined(__APPLE__)
static void ft_scan(ft_hunt *h, const char *dir, int depth)
{
    DIR *d = opendir(dir);
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d)) && ++h->seen < 20000) {
        if (e->d_name[0] == '.') continue;
        char base[256];
        if (ft_font_ext(e->d_name, base, sizeof(base))) {
            ft_consider(h, dir, e->d_name);
            continue;
        }
        char sub[1024];
        if (depth < 5 && ft_join(sub, sizeof(sub), dir, e->d_name)) ft_scan(h, sub, depth + 1);
    }
    closedir(d);
}
#else
static void ft_scan(ft_hunt *h, const char *dir, int depth) {}
#endif

static goome_font *ft_hunt_for(const char *want)
{
    ft_hunt h;
    memset(&h, 0, sizeof(h));
    if (!ft_norm(want, h.want, sizeof(h.want))) return NULL;
    char dirs[6][600];
    int nd = 0;
#if defined(_WIN32)
    const char *win = getenv("WINDIR"), *lad = getenv("LOCALAPPDATA");
    snprintf(dirs[nd++], 600, "%s\\Fonts", win ? win : "C:\\Windows");
    if (lad) snprintf(dirs[nd++], 600, "%s\\Microsoft\\Windows\\Fonts", lad);
#elif defined(__APPLE__)
    const char *home = getenv("HOME");
    snprintf(dirs[nd++], 600, "/System/Library/Fonts");
    snprintf(dirs[nd++], 600, "/Library/Fonts");
    if (home) snprintf(dirs[nd++], 600, "%s/Library/Fonts", home);
#else
    const char *home = getenv("HOME");
    snprintf(dirs[nd++], 600, "/usr/share/fonts");
    snprintf(dirs[nd++], 600, "/usr/local/share/fonts");
    if (home) snprintf(dirs[nd++], 600, "%s/.local/share/fonts", home);
    if (home) snprintf(dirs[nd++], 600, "%s/.fonts", home);
#endif
    for (int i = 0; i < nd; i++) ft_scan(&h, dirs[i], 0);
    return h.score ? goome_font_load(h.best) : NULL;
}

#if defined(__unix__) && !defined(__APPLE__)
static int ft_fc_file(const char *name, char *out, size_t cap, int *index)
{
    static void *lib;
    static void *cfg;
    static void *(*parse)(const unsigned char *);
    static int (*subst)(void *, void *, int);
    static void (*defs)(void *);
    static void *(*match)(void *, void *, int *);
    static int (*gets)(const void *, const char *, int, unsigned char **);
    static int (*geti)(const void *, const char *, int, int *);
    static void (*destroy)(void *);
    static int tried;
    if (!tried) {
        tried = 1;
        lib = dlopen("libfontconfig.so.1", RTLD_NOW | RTLD_LOCAL);
        if (!lib) return 0;
        void *(*init)(void) = NULL;
        *(void **)&init = dlsym(lib, "FcInitLoadConfigAndFonts");
        *(void **)&parse = dlsym(lib, "FcNameParse");
        *(void **)&subst = dlsym(lib, "FcConfigSubstitute");
        *(void **)&defs = dlsym(lib, "FcDefaultSubstitute");
        *(void **)&match = dlsym(lib, "FcFontMatch");
        *(void **)&gets = dlsym(lib, "FcPatternGetString");
        *(void **)&geti = dlsym(lib, "FcPatternGetInteger");
        *(void **)&destroy = dlsym(lib, "FcPatternDestroy");
        if (init && parse && subst && defs && match && gets && geti && destroy) cfg = init();
    }
    if (!cfg) return 0;
    void *pat = parse((const unsigned char *)name);
    if (!pat) return 0;
    subst(cfg, pat, 0);
    defs(pat);
    int res = 0, idx = 0, ok = 0;
    void *m = match(cfg, pat, &res);
    destroy(pat);
    if (!m) return 0;
    unsigned char *file = NULL;
    if (gets(m, "file", 0, &file) == 0 && file && strlen((const char *)file) < cap) {
        if (geti(m, "index", 0, &idx) != 0) idx = 0;
        strcpy(out, (const char *)file);
        *index = idx & 0xFFFF, ok = 1;
    }
    destroy(m);
    return ok;
}

static goome_font *ft_fontconfig(const char *name)
{
    char path[1024];
    int idx = 0;
    if (!ft_fc_file(name, path, sizeof(path), &idx)) return NULL;
    goome_font *f = goome_font_load_index(path, idx);
    if (!f && idx) f = goome_font_load(path);
    return f;
}
#endif

static goome_font *ft_find_shared(const char *const *fc, const char *const *paths, uint32_t cp, int color)
{
    goome_font *f;
#if defined(__unix__) && !defined(__APPLE__)
    for (int i = 0; fc && fc[i]; i++) {
        char path[1024];
        int idx = 0;
        if (!ft_fc_file(fc[i], path, sizeof(path), &idx)) continue;
        f = ft_shared(path, idx);
        if (f && ft_cmap(f, cp) && (!color || f->color)) return f;
    }
#else
    (void)fc;
#endif
    for (int i = 0; paths && paths[i]; i++) {
        char path[600];
#if defined(_WIN32)
        const char *win = getenv("WINDIR");
        snprintf(path, sizeof(path), "%s\\Fonts\\%s", win ? win : "C:\\Windows", paths[i]);
#else
        snprintf(path, sizeof(path), "%s", paths[i]);
#endif
        f = ft_shared(path, 0);
        if (f && ft_cmap(f, cp) && (!color || f->color)) return f;
    }
    return NULL;
}

static goome_font *ft_auto(goome_font *f)
{
#if defined(_WIN32)
    static const char *const emo[] = {"seguiemj.ttf", NULL}, *const ar[] = {"segoeui.ttf", "arial.ttf", "tahoma.ttf", NULL}, *const he[] = {"segoeui.ttf", "arial.ttf", NULL};
#elif defined(__APPLE__)
    static const char *const emo[] = {"/System/Library/Fonts/Apple Color Emoji.ttc", NULL};
    static const char *const ar[] = {"/System/Library/Fonts/GeezaPro.ttc", "/System/Library/Fonts/Supplemental/GeezaPro.ttc", "/System/Library/Fonts/Supplemental/Arial Unicode.ttf", NULL};
    static const char *const he[] = {"/System/Library/Fonts/ArialHB.ttc", "/System/Library/Fonts/Supplemental/Arial Unicode.ttf", NULL};
#else
    static const char *const emo[] = {"/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf", "/usr/share/fonts/noto/NotoColorEmoji.ttf", "/usr/share/fonts/google-noto-emoji/NotoColorEmoji.ttf",
                                      "/usr/share/fonts/noto-emoji/NotoColorEmoji.ttf", "/usr/share/fonts/TTF/NotoColorEmoji.ttf", NULL};
    static const char *const ar[] = {"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", "/usr/share/fonts/TTF/DejaVuSans.ttf", NULL}, *const he[] = {"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", "/usr/share/fonts/TTF/DejaVuSans.ttf", NULL};
#endif
    static const char *const fc_emo[] = {"emoji", "Noto Color Emoji", NULL}, *const fc_ar[] = {":lang=ar", NULL}, *const fc_he[] = {":lang=he", NULL};
    if (!f) return NULL;
    goome_font *e;
    if (!f->color && (e = ft_find_shared(fc_emo, emo, 0x1F600, 1))) goome_font_fallback(f, e);
    if (!ft_cmap(f, 0x627) && (e = ft_find_shared(fc_ar, ar, 0x627, 0))) goome_font_fallback(f, e);
    if (!ft_cmap(f, 0x5D0) && (e = ft_find_shared(fc_he, he, 0x5D0, 0))) goome_font_fallback(f, e);
    return f;
}

static goome_font *ft_system(const char *name);

goome_font *goome_font_system(const char *name) { return ft_auto(ft_system(name)); }

static goome_font *ft_system(const char *name)
{
    goome_font *f = NULL;
    if (name && (strchr(name, '/') || strchr(name, '\\'))) return goome_font_load(name);
#if defined(__unix__) && !defined(__APPLE__)
    f = ft_fontconfig(name ? name : "sans-serif");
    if (f) return f;
#endif
    if (name) return ft_hunt_for(name);
#if defined(_WIN32)
    {
        const char *win = getenv("WINDIR");
        char p[600];
        snprintf(p, sizeof(p), "%s\\Fonts\\segoeui.ttf", win ? win : "C:\\Windows");
        if ((f = goome_font_load(p))) return f;
    }
    static const char *const names[] = {"segoeui", "arial", "tahoma", "verdana", NULL};
#elif defined(__APPLE__)
    static const char *const paths[] = {"/System/Library/Fonts/SFNS.ttf", "/System/Library/Fonts/SFNSText.ttf", "/System/Library/Fonts/Helvetica.ttc",
                                        "/System/Library/Fonts/HelveticaNeue.ttc", "/Library/Fonts/Arial.ttf", NULL};
    for (int i = 0; paths[i]; i++)
        if ((f = goome_font_load(paths[i]))) return f;
    static const char *const names[] = {"sfns", "helvetica", "arial", NULL};
#else
    static const char *const names[] = {"cantarell", "dejavusans", "notosans", "ubuntu", "liberationsans", "freesans", "droidsans", NULL};
#endif
    for (int i = 0; names[i]; i++)
        if ((f = ft_hunt_for(names[i]))) return f;
    return NULL;
}

static inline void ft_mark(goome_canvas *c, int x0, int y0, int x1, int y1)
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

static void ft_colors(goome_font *f, uint32_t argb)
{
    if (f->col == argb && f->tab[255]) return;
    uint32_t A = argb >> 24, R = argb >> 16 & 255, G = argb >> 8 & 255, B = argb & 255;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t a = (A * i + 127) / 255;
        f->tab[i] = a << 24 | ((R * a + 127) / 255) << 16 | ((G * a + 127) / 255) << 8 | ((B * a + 127) / 255);
    }
    f->tab[0] = 0;
    if (!f->tab[255]) f->tab[255] = 1;
    f->col = argb;
}

static void ft_blit(goome_canvas *c, goome_font *f, const uint8_t *bits, int w, int h, int x, int y, const int *k)
{
    int x0 = x > k[0] ? x : k[0], y0 = y > k[1] ? y : k[1], x1 = x + w < k[2] ? x + w : k[2], y1 = y + h < k[3] ? y + h : k[3];
    if (x1 <= x0 || y1 <= y0) return;
    ft_mark(c, x0, y0, x1, y1);
    for (int yy = y0; yy < y1; yy++) {
        const uint8_t *s = bits + (size_t)(yy - y) * w + (x0 - x);
        uint32_t *d = c->px + (size_t)yy * c->stride + x0;
        for (int xx = x0; xx < x1; xx++, s++, d++) {
            uint32_t src = f->tab[*s], v = *d, inv = 255 - (src >> 24);
            uint32_t rb = (v & 0xFF00FF) * inv + 0x800080, ag = (v >> 8 & 0xFF00FF) * inv + 0x800080;
            rb = (rb + (rb >> 8 & 0xFF00FF)) >> 8 & 0xFF00FF;
            ag = (ag + (ag >> 8 & 0xFF00FF)) & 0xFF00FF00;
            *d = src + rb + ag;
        }
    }
}


static double ft_round(double v) { return (double)(int64_t)(v < 0 ? v - 0.5 : v + 0.5); }
static int ft_okd(double v) { return v == v && v < 1e9 && v > -1e9; }

static void ft_cblit(goome_canvas *c, const uint32_t *px, int w, int h, int x, int y, const int *k, uint32_t a)
{
    int x0 = x > k[0] ? x : k[0], y0 = y > k[1] ? y : k[1], x1 = x + w < k[2] ? x + w : k[2], y1 = y + h < k[3] ? y + h : k[3];
    if (x1 <= x0 || y1 <= y0) return;
    ft_mark(c, x0, y0, x1, y1);
    for (int yy = y0; yy < y1; yy++) {
        const uint32_t *s = px + (size_t)(yy - y) * w + (x0 - x);
        uint32_t *d = c->px + (size_t)yy * c->stride + x0;
        for (int xx = x0; xx < x1; xx++, s++, d++) {
            uint32_t src = *s;
            if (!src) continue;
            if (a < 255) {
                uint32_t rb = (src & 0xFF00FF) * a + 0x800080, ag = (src >> 8 & 0xFF00FF) * a + 0x800080;
                src = ((rb + (rb >> 8 & 0xFF00FF)) >> 8 & 0xFF00FF) | ((ag + (ag >> 8 & 0xFF00FF)) & 0xFF00FF00);
            }
            uint32_t v = *d, inv = 255 - (src >> 24);
            uint32_t rb = (v & 0xFF00FF) * inv + 0x800080, ag = (v >> 8 & 0xFF00FF) * inv + 0x800080;
            rb = (rb + (rb >> 8 & 0xFF00FF)) >> 8 & 0xFF00FF;
            ag = (ag + (ag >> 8 & 0xFF00FF)) & 0xFF00FF00;
            *d = src + rb + ag;
        }
    }
}

static void ft_scale(const uint32_t *s, int sw, int sh, int ss, uint32_t *d, int dw, int dh)
{
    if (dw <= sw && dh <= sh) {
        for (int y = 0; y < dh; y++) {
            int64_t ya = (int64_t)y * sh * 256 / dh, yb = (int64_t)(y + 1) * sh * 256 / dh;
            for (int x = 0; x < dw; x++) {
                int64_t xa = (int64_t)x * sw * 256 / dw, xb = (int64_t)(x + 1) * sw * 256 / dw;
                uint64_t acc[4] = {0, 0, 0, 0}, ws = 0;
                for (int64_t sy = ya >> 8; sy * 256 < yb && sy < sh; sy++) {
                    int64_t wy = (yb < (sy + 1) * 256 ? yb : (sy + 1) * 256) - (ya > sy * 256 ? ya : sy * 256);
                    for (int64_t sx = xa >> 8; sx * 256 < xb && sx < sw; sx++) {
                        int64_t wx = (xb < (sx + 1) * 256 ? xb : (sx + 1) * 256) - (xa > sx * 256 ? xa : sx * 256);
                        uint64_t wt = (uint64_t)(wx * wy);
                        uint32_t p = s[(size_t)sy * ss + (size_t)sx];
                        for (int q = 0; q < 4; q++) acc[q] += (uint64_t)(p >> (q * 8) & 255) * wt;
                        ws += wt;
                    }
                }
                uint32_t o = 0;
                for (int q = 0; q < 4 && ws; q++) o |= (uint32_t)((acc[q] + ws / 2) / ws) << (q * 8);
                d[(size_t)y * dw + x] = o;
            }
        }
        return;
    }
    for (int y = 0; y < dh; y++) {
        int64_t v = ((int64_t)(2 * y + 1) * sh * 65536 / (2 * dh)) - 32768;
        if (v < 0) v = 0;
        if (v > (int64_t)(sh - 1) << 16) v = (int64_t)(sh - 1) << 16;
        int y0 = (int)(v >> 16), y1 = y0 + 1 < sh ? y0 + 1 : y0;
        uint32_t fy = (uint32_t)(v >> 8 & 255);
        for (int x = 0; x < dw; x++) {
            int64_t h = ((int64_t)(2 * x + 1) * sw * 65536 / (2 * dw)) - 32768;
            if (h < 0) h = 0;
            if (h > (int64_t)(sw - 1) << 16) h = (int64_t)(sw - 1) << 16;
            int x0 = (int)(h >> 16), x1 = x0 + 1 < sw ? x0 + 1 : x0;
            uint32_t fx = (uint32_t)(h >> 8 & 255);
            uint32_t a = s[(size_t)y0 * ss + x0], b = s[(size_t)y0 * ss + x1], c = s[(size_t)y1 * ss + x0], e = s[(size_t)y1 * ss + x1], o = 0;
            for (int q = 0; q < 32; q += 8) {
                uint32_t t = ((a >> q & 255) * (256 - fx) + (b >> q & 255) * fx) * (256 - fy) + ((c >> q & 255) * (256 - fx) + (e >> q & 255) * fx) * fy;
                o |= ((t + 32768) >> 16) << q;
            }
            d[(size_t)y * dw + x] = o;
        }
    }
}

static int ft_png_ok(const goome_font *f, uint32_t at, uint32_t len)
{
    if (len < 33 || !ft_has(f, at, len) || memcmp(f->d + at, "\x89PNG\r\n\x1a\n", 8)) return 0;
    uint32_t w = ft_u32(f, at + 16), h = ft_u32(f, at + 20);
    return w >= 1 && h >= 1 && w <= 1024 && h <= 1024;
}

static int ft_cblc_glyph(const goome_font *f, uint32_t sz, int g, uint32_t *at, uint32_t *len, int *fmt, int *mw, int *mh, int *bx, int *by)
{
    uint32_t arr = f->cblc + ft_u32(f, sz), n = ft_u32(f, sz + 8);
    if ((uint32_t)g < ft_u16(f, sz + 40) || (uint32_t)g > ft_u16(f, sz + 42)) return 0;
    for (uint32_t i = 0; i < n && i < 4096; i++) {
        uint32_t r = arr + i * 8, first = ft_u16(f, r), last = ft_u16(f, r + 2);
        if ((uint32_t)g < first || (uint32_t)g > last) continue;
        uint32_t st = arr + ft_u32(f, r + 4), ifmt = ft_u16(f, st), base = f->cbdt + ft_u32(f, st + 4), k = (uint32_t)g - first, a = 0, b = 0;
        *fmt = (int)ft_u16(f, st + 2);
        uint32_t big = 0;
        if (ifmt == 1) a = ft_u32(f, st + 8 + k * 4), b = ft_u32(f, st + 12 + k * 4);
        else if (ifmt == 3) a = ft_u16(f, st + 8 + k * 2), b = ft_u16(f, st + 10 + k * 2);
        else if (ifmt == 2) {
            uint32_t isz = ft_u32(f, st + 8);
            a = isz * k, b = a + isz, big = st + 12;
            if (isz && k > 0xFFFFFFFFu / isz - 1) return 0;
        } else if (ifmt == 4) {
            uint32_t ng = ft_u32(f, st + 8);
            for (uint32_t j = 0; j < ng && j < 65536; j++) {
                uint32_t e = st + 12 + j * 4;
                if (ft_u16(f, e) == (uint32_t)g) {
                    a = ft_u16(f, e + 2), b = ft_u16(f, e + 6);
                    break;
                }
            }
        } else if (ifmt == 5) {
            uint32_t isz = ft_u32(f, st + 8), ng = ft_u32(f, st + 20);
            big = st + 12;
            for (uint32_t j = 0; j < ng && j < 65536; j++)
                if (ft_u16(f, st + 24 + j * 2) == (uint32_t)g) {
                    if (isz && j > 0xFFFFFFFFu / isz - 1) return 0;
                    a = isz * j, b = a + isz;
                    break;
                }
        } else
            return 0;
        if (b <= a || base + a < base) return 0;
        uint32_t p = base + a, e = b - a;
        if (*fmt == 17) {
            *mh = (int)ft_u8(f, p), *mw = (int)ft_u8(f, p + 1), *bx = (int)(ft_u8(f, p + 2) ^ 0x80) - 0x80, *by = (int)(ft_u8(f, p + 3) ^ 0x80) - 0x80;
            *len = ft_u32(f, p + 5), *at = p + 9;
            if (e < 9 || *len > e - 9) return 0;
        } else if (*fmt == 18) {
            *mh = (int)ft_u8(f, p), *mw = (int)ft_u8(f, p + 1), *bx = (int)(ft_u8(f, p + 2) ^ 0x80) - 0x80, *by = (int)(ft_u8(f, p + 3) ^ 0x80) - 0x80;
            *len = ft_u32(f, p + 8), *at = p + 12;
            if (e < 12 || *len > e - 12) return 0;
        } else if (*fmt == 19 && big) {
            *mh = (int)ft_u8(f, big), *mw = (int)ft_u8(f, big + 1), *bx = (int)(ft_u8(f, big + 2) ^ 0x80) - 0x80, *by = (int)(ft_u8(f, big + 3) ^ 0x80) - 0x80;
            *len = ft_u32(f, p), *at = p + 4;
            if (e < 4 || *len > e - 4) return 0;
        } else
            return 0;
        return ft_png_ok(f, *at, *len);
    }
    return 0;
}

static int ft_better(int ppem, int best, int want)
{
    if (best <= 0) return 1;
    if (ppem >= want) return best < want || ppem < best;
    return best < want && ppem > best;
}

static int ft_bitmap_find(const goome_font *f, int g, int want, uint32_t *at, uint32_t *len, int *ppem, int *bx, int *by)
{
    int best = 0;
    if (f->cblc) {
        uint32_t ns = ft_u32(f, f->cblc + 4);
        for (uint32_t i = 0; i < ns && i < 64; i++) {
            uint32_t sz = f->cblc + 8 + i * 48;
            int pp = (int)ft_u8(f, sz + 45), fmt, mw, mh, x, y;
            uint32_t a, l;
            if (pp < 1 || !ft_better(pp, best, want) || !ft_cblc_glyph(f, sz, g, &a, &l, &fmt, &mw, &mh, &x, &y)) continue;
            best = pp, *at = a, *len = l, *bx = x, *by = y;
        }
    }
    if (f->sbix && !best) {
        uint32_t ns = ft_u32(f, f->sbix + 4);
        for (uint32_t i = 0; i < ns && i < 64; i++) {
            uint32_t st = f->sbix + ft_u32(f, f->sbix + 8 + i * 4);
            int pp = (int)ft_u16(f, st), gg = g;
            if (pp < 1 || !ft_better(pp, best, want)) continue;
            for (int hop = 0; hop < 2; hop++) {
                uint32_t a = ft_u32(f, st + 4 + (uint32_t)gg * 4), b = ft_u32(f, st + 8 + (uint32_t)gg * 4);
                if ((uint32_t)gg >= (uint32_t)f->nglyphs || b <= a + 8 || st + a < st) break;
                uint32_t p = st + a, tag = ft_u32(f, p + 4);
                if (tag == 0x64757065u && hop == 0) {
                    gg = (int)ft_u16(f, p + 8);
                    continue;
                }
                if (tag != 0x706E6720u || !ft_png_ok(f, p + 8, b - a - 8)) break;
                int h = (int)ft_u32(f, p + 8 + 20);
                best = pp, *at = p + 8, *len = b - a - 8, *bx = ft_s16(f, p), *by = ft_s16(f, p + 2) + h;
                break;
            }
        }
    }
    *ppem = best;
    return best > 0;
}

static int64_t ft_sc(int64_t v, int px64, int ppem) { return ft_divr(v * px64, (int64_t)ppem * 64); }

static int ft_bitmap_glyph(goome_font *f, int g, int px64, ft_cent *e)
{
    uint32_t at, len;
    int pp, bx, by;
    if (!ft_bitmap_find(f, g, (px64 + 63) / 64, &at, &len, &pp, &bx, &by)) return 0;
    goome_bitmap bm;
    if (goome_bitmap_load_mem(f->d + at, len, &bm) != GOOME_OK) return 0;
    int64_t w = ft_sc(bm.w, px64, pp), h = ft_sc(bm.h, px64, pp);
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    int ok = 0;
    if (w <= FT_MAXDIM && h <= FT_MAXDIM && w * h <= 1048576) {
        e->px = malloc((size_t)(w * h) * 4);
        if (e->px) {
            ft_scale(bm.px, bm.w, bm.h, bm.stride, e->px, (int)w, (int)h);
            e->w = (int16_t)w, e->h = (int16_t)h, e->l = (int16_t)ft_sc(bx, px64, pp), e->t = (int16_t)-ft_sc(by, px64, pp);
            ok = 1;
        }
    }
    goome_bitmap_free(&bm);
    return ok;
}

static uint32_t ft_pal(const goome_font *f, uint32_t idx, uint32_t argb)
{
    uint32_t p = f->cpal;
    if (idx == 0xFFFF || idx >= ft_u16(f, p + 2)) return argb | 0xFF000000u;
    uint32_t r = p + ft_u32(f, p + 8) + (ft_u16(f, p + 12) + idx) * 4;
    if (!ft_has(f, r, 4)) return argb | 0xFF000000u;
    return ft_u8(f, r + 3) << 24 | ft_u8(f, r + 2) << 16 | ft_u8(f, r + 1) << 8 | ft_u8(f, r);
}

static int ft_colr_layers(const goome_font *f, int g, uint32_t *first, uint32_t *num)
{
    uint32_t o = f->colr, nb = ft_u16(f, o + 2), bo = o + ft_u32(f, o + 4), lo = 0, hi = nb;
    while (lo < hi) {
        uint32_t m = (lo + hi) / 2, r = bo + m * 6, v = ft_u16(f, r);
        if (v == (uint32_t)g) {
            *first = ft_u16(f, r + 2), *num = ft_u16(f, r + 4);
            if (*num > 256) *num = 256;
            return *num > 0 && ft_has(f, o + ft_u32(f, o + 8), (*first + *num) * 4);
        }
        if (v < (uint32_t)g) lo = m + 1;
        else hi = m;
    }
    return 0;
}

static int ft_colr_glyph(goome_font *f, int g, int64_t sx, int64_t sy, uint32_t argb, ft_cent *e)
{
    uint32_t first, num;
    if (!ft_colr_layers(f, g, &first, &num)) return 0;
    uint32_t lay = f->colr + ft_u32(f, f->colr + 8) + first * 4;
    int x0 = 1 << 30, y0 = 1 << 30, x1 = -(1 << 30), y1 = -(1 << 30);
    for (uint32_t i = 0; i < num; i++) {
        int w, h, l, t, lg = (int)ft_u16(f, lay + i * 4);
        if (lg >= f->nglyphs || !ft_render(f, lg, sx, sy, 0, &w, &h, &l, &t)) continue;
        if (l < x0) x0 = l;
        if (t < y0) y0 = t;
        if (l + w > x1) x1 = l + w;
        if (t + h > y1) y1 = t + h;
    }
    if (x1 <= x0 || y1 <= y0 || (int64_t)(x1 - x0) * (y1 - y0) > 1048576) return 0;
    int W = x1 - x0, H = y1 - y0;
    uint32_t *out = calloc((size_t)W * H, 4);
    if (!out) return 0;
    for (uint32_t i = 0; i < num; i++) {
        int w, h, l, t, lg = (int)ft_u16(f, lay + i * 4);
        if (lg >= f->nglyphs || !ft_render(f, lg, sx, sy, 0, &w, &h, &l, &t)) continue;
        uint32_t col = ft_pal(f, ft_u16(f, lay + i * 4 + 2), argb), A = col >> 24, R = col >> 16 & 255, G = col >> 8 & 255, B = col & 255;
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) {
                uint32_t cov = f->tmp[(size_t)y * w + x];
                if (!cov) continue;
                uint32_t a = (A * cov + 127) / 255, *d = &out[(size_t)(y + t - y0) * W + (x + l - x0)], v = *d, inv = 255 - a;
                uint32_t s = a << 24 | ((R * a + 127) / 255) << 16 | ((G * a + 127) / 255) << 8 | ((B * a + 127) / 255);
                uint32_t rb = (v & 0xFF00FF) * inv + 0x800080, ag = (v >> 8 & 0xFF00FF) * inv + 0x800080;
                *d = s + ((rb + (rb >> 8 & 0xFF00FF)) >> 8 & 0xFF00FF) + ((ag + (ag >> 8 & 0xFF00FF)) & 0xFF00FF00);
            }
    }
    e->px = out, e->w = (int16_t)W, e->h = (int16_t)H, e->l = (int16_t)x0, e->t = (int16_t)y0;
    return 1;
}

static const ft_cent *ft_cglyph(goome_font *f, int g, int px64, int64_t sx, int64_t sy, uint32_t argb)
{
    if (!f->color || px64 <= 0) return NULL;
    if (!f->cc && !(f->cc = calloc(FT_CC, sizeof(ft_cent)))) return NULL;
    uint32_t key = f->colr ? argb | 0xFF000000u : 0, h = (((uint32_t)g * 2654435761u) ^ ((uint32_t)px64 * 40503u) ^ key) & (FT_CC - 1);
    for (;;) {
        ft_cent *e = &f->cc[h];
        if (!e->px64) break;
        if (e->g == (uint32_t)g && e->px64 == (uint32_t)px64 && e->argb == key) return e->px ? e : NULL;
        h = (h + 1) & (FT_CC - 1);
    }
    if (f->ncc >= FT_CC * 3 / 4 || f->ccb > (16u << 20)) {
        ft_cc_clear(f);
        h = (((uint32_t)g * 2654435761u) ^ ((uint32_t)px64 * 40503u) ^ key) & (FT_CC - 1);
    }
    ft_cent *e = &f->cc[h];
    memset(e, 0, sizeof(*e));
    e->g = (uint32_t)g, e->px64 = (uint32_t)px64, e->argb = key;
    f->ncc++;
    if (!(f->colr && ft_colr_glyph(f, g, sx, sy, argb, e)) && !((f->cblc || f->sbix) && ft_bitmap_glyph(f, g, px64, e))) return NULL;
    f->ccb += (size_t)e->w * e->h * 4;
    return e;
}

static int ft_ucls(uint32_t cp)
{
    int lo = 0, hi = (int)(sizeof(ud_bidi) / sizeof(ud_bidi[0])) - 1;
    while (lo < hi) {
        int m = (lo + hi + 1) / 2;
        if ((ud_bidi[m] >> 5) <= cp) lo = m;
        else hi = m - 1;
    }
    return (int)(ud_bidi[lo] & 31);
}

static uint32_t ft_mirror(uint32_t cp)
{
    int lo = 0, hi = (int)(sizeof(ud_mirror) / sizeof(ud_mirror[0]) / 2);
    while (lo < hi) {
        int m = (lo + hi) / 2;
        if (ud_mirror[m * 2] == cp) return ud_mirror[m * 2 + 1];
        if (ud_mirror[m * 2] < cp) lo = m + 1;
        else hi = m;
    }
    return 0;
}

static const uint16_t *ft_forms(uint32_t cp)
{
    int lo = 0, hi = (int)(sizeof(ud_forms) / sizeof(ud_forms[0]) / 5);
    if (cp < 0x600 || cp > 0x7FF) return NULL;
    while (lo < hi) {
        int m = (lo + hi) / 2;
        if (ud_forms[m * 5] == cp) return ud_forms + m * 5 + 1;
        if (ud_forms[m * 5] < cp) lo = m + 1;
        else hi = m;
    }
    return NULL;
}

static int ft_emo_pres(uint32_t cp)
{
    if (cp >= 0x1F000 && cp <= 0x1FAFF) return cp < 0x1F100 || cp > 0x1F1E5 || (cp >= 0x1F18E && cp <= 0x1F19A);
    for (size_t i = 0; i < sizeof(ud_emoji) / sizeof(ud_emoji[0]); i += 2)
        if (cp >= ud_emoji[i] && cp <= ud_emoji[i + 1]) return 1;
    return 0;
}

static int ft_ri(uint32_t cp) { return cp >= 0x1F1E6 && cp <= 0x1F1FF; }
static int ft_vs(uint32_t cp) { return (cp >= 0xFE00 && cp <= 0xFE0F) || (cp >= 0xE0100 && cp <= 0xE01EF); }

static int ft_ignorable(uint32_t cp)
{
    return cp == 0xAD || cp == 0x34F || cp == 0x61C || (cp >= 0x180B && cp <= 0x180F) || (cp >= 0x200B && cp <= 0x200F) || (cp >= 0x202A && cp <= 0x202E) ||
           (cp >= 0x2060 && cp <= 0x206F) || cp == 0xFEFF || ft_vs(cp) || (cp >= 0xE0000 && cp <= 0xE0FFF);
}

static int ft_extend(uint32_t cp)
{
    if (cp < 0x300) return 0;
    return ft_vs(cp) || cp == 0x200D || cp == 0x20E3 || (cp >= 0x1F3FB && cp <= 0x1F3FF) || (cp >= 0xE0020 && cp <= 0xE007F) || ft_ucls(cp) == UD_NSM;
}

static int ft_alef(uint32_t cp) { return cp == 0x622 || cp == 0x623 || cp == 0x625 || cp == 0x627; }

static int ft_brk(uint32_t prev, uint32_t cp, int nri, int len)
{
    if (len >= 32 || cp < 32 || prev < 32 || cp == 127 || prev == 127) return 1;
    if (ft_extend(cp) || prev == 0x200D) return 0;
    if (ft_ri(cp) && ft_ri(prev) && nri == 1) return 0;
    if (prev == 0x644 && ft_alef(cp)) return 0;
    return 1;
}

static int ft_dir_mode;

void goome_text_dir(int dir) { ft_dir_mode = dir == GOOME_DIR_LTR || dir == GOOME_DIR_RTL ? dir : GOOME_DIR_AUTO; }

static uint32_t ft_utf8n(const char **ps, const char *end)
{
    const unsigned char *s = (const unsigned char *)*ps;
    size_t left = (size_t)(end - *ps);
    uint32_t c = s[0];
    int n = 0;
    uint32_t min = 0;
    if (c < 0x80) {
        *ps += 1;
        return c;
    }
    if (c >= 0xC2 && c <= 0xDF) n = 1, c &= 31, min = 0x80;
    else if (c >= 0xE0 && c <= 0xEF) n = 2, c &= 15, min = 0x800;
    else if (c >= 0xF0 && c <= 0xF4) n = 3, c &= 7, min = 0x10000;
    else {
        *ps += 1;
        return 0xFFFD;
    }
    for (int i = 1; i <= n; i++) {
        if ((size_t)i >= left || (s[i] & 0xC0) != 0x80) {
            *ps += i;
            return 0xFFFD;
        }
        c = c << 6 | (s[i] & 63);
    }
    *ps += n + 1;
    if (c < min || c > 0x10FFFF || (c >= 0xD800 && c <= 0xDFFF)) return 0xFFFD;
    return c;
}

int gm_text_next(const char *s, int i, int n)
{
    if (!s || i >= n) return n < 0 ? 0 : n;
    const char *p = s + i, *e = s + n;
    uint32_t prev = ft_utf8n(&p, e);
    int nri = ft_ri(prev), len = 1;
    while (p < e) {
        const char *q = p;
        uint32_t cp = ft_utf8n(&q, e);
        if (ft_brk(prev, cp, nri, len)) break;
        nri += ft_ri(cp), len++, prev = cp, p = q;
    }
    return (int)(p - s);
}

int gm_text_prev(const char *s, int i)
{
    if (!s || i <= 0) return 0;
    int st = i;
    for (int hop = 0; st > 0 && hop < 64;) {
        st--;
        while (st > 0 && ((unsigned char)s[st] & 0xC0) == 0x80) st--;
        const char *p = s + st;
        uint32_t cp = ft_utf8n(&p, s + i);
        if (ft_extend(cp) || ft_ri(cp) || ft_alef(cp) || cp == 0x200D) {
            hop++;
            continue;
        }
        if (st > 0) {
            int b = st - 1;
            while (b > 0 && ((unsigned char)s[b] & 0xC0) == 0x80) b--;
            p = s + b;
            if (ft_utf8n(&p, s + st) == 0x200D) {
                hop++;
                continue;
            }
        }
        break;
    }
    int last = st;
    for (int k = st; k < i;) {
        last = k;
        k = gm_text_next(s, k, i);
    }
    return last;
}

static int ft_scratch(goome_font *f, int m)
{
    if (m <= f->scap) return 1;
    int c = f->scap ? f->scap : 256;
    while (c < m) {
        if (c > (1 << 24)) return 0;
        c *= 2;
    }
    uint32_t *a = realloc(f->scp, (size_t)c * 4);
    if (a) f->scp = a;
    uint32_t *b = realloc(f->ssh, (size_t)c * 4);
    if (b) f->ssh = b;
    int *o = realloc(f->soff, (size_t)(c + 1) * sizeof(int));
    if (o) f->soff = o;
    int *cs = realloc(f->scs, (size_t)(c + 1) * sizeof(int));
    if (cs) f->scs = cs;
    int *od = realloc(f->sord, (size_t)(c + 1) * sizeof(int));
    if (od) f->sord = od;
    uint8_t *k1 = realloc(f->scl, (size_t)c);
    if (k1) f->scl = k1;
    uint8_t *k2 = realloc(f->swk, (size_t)c);
    if (k2) f->swk = k2;
    uint8_t *k3 = realloc(f->slv, (size_t)c);
    if (k3) f->slv = k3;
    if (!a || !b || !o || !cs || !od || !k1 || !k2 || !k3) return 0;
    f->scap = c;
    return 1;
}

static int ft_strong(int t) { return t == UD_L ? UD_L : (t == UD_R || t == UD_AL || t == UD_EN || t == UD_AN) ? UD_R : -1; }

static int ft_bracket(uint32_t cp)
{
    static const uint32_t pairs[] = {'(', ')', '[', ']', '{', '}', 0x2045, 0x2046, 0x207D, 0x207E, 0x208D, 0x208E, 0x2329, 0x232A, 0x3008, 0x3009, 0x300A, 0x300B,
                                     0x300C, 0x300D, 0x300E, 0x300F, 0x3010, 0x3011, 0xFF08, 0xFF09, 0xFF3B, 0xFF3D, 0xFF5B, 0xFF5D};
    for (int i = 0; i < (int)(sizeof(pairs) / sizeof(pairs[0])); i++)
        if (pairs[i] == cp) return i & 1 ? -(i / 2 + 1) : i / 2 + 1;
    return 0;
}

static void ft_pairs(const uint32_t *cp, uint8_t *t, int m, int e, int sos)
{
    int st[63][2], ns = 0, pr[64][2], np = 0;
    for (int i = 0; i < m && np < 64; i++) {
        if (t[i] != UD_ON) continue;
        int b = ft_bracket(cp[i]);
        if (b > 0) {
            if (ns == 63) break;
            st[ns][0] = b, st[ns][1] = i, ns++;
        } else if (b < 0) {
            for (int k = ns - 1; k >= 0; k--)
                if (st[k][0] == -b) {
                    int j = np++;
                    while (j > 0 && pr[j - 1][0] > st[k][1]) pr[j][0] = pr[j - 1][0], pr[j][1] = pr[j - 1][1], j--;
                    pr[j][0] = st[k][1], pr[j][1] = i, ns = k;
                    break;
                }
        }
    }
    int dir = e & 1 ? UD_R : UD_L;
    for (int p = 0; p < np; p++) {
        int a = pr[p][0], z = pr[p][1], in = -1, set = -1;
        for (int i = a + 1; i < z; i++) {
            int s = ft_strong(t[i]);
            if (s == dir) {
                in = dir;
                break;
            }
            if (s >= 0) in = s;
        }
        if (in == dir) set = dir;
        else if (in >= 0) {
            int before = sos;
            for (int i = a - 1; i >= 0; i--)
                if (ft_strong(t[i]) >= 0) {
                    before = ft_strong(t[i]);
                    break;
                }
            set = before == in ? in : dir;
        }
        if (set < 0) continue;
        t[a] = t[z] = (uint8_t)set;
    }
}

static void ft_levels(const uint32_t *cp, const uint8_t *cls, uint8_t *t, uint8_t *lv, int m, int e)
{
    int sos = e & 1 ? UD_R : UD_L;
    for (int i = 0; i < m; i++) {
        int c = cls[i];
        if (c >= UD_LRE && c <= UD_PDF) c = UD_BN;
        else if (c >= UD_LRI) c = UD_ON;
        t[i] = (uint8_t)c;
    }
    for (int i = 0; i < m; i++)
        if (t[i] == UD_NSM || t[i] == UD_BN) t[i] = (uint8_t)(i ? t[i - 1] : sos);
    int last = sos;
    for (int i = 0; i < m; i++) {
        if (t[i] == UD_L || t[i] == UD_R || t[i] == UD_AL) last = t[i];
        else if (t[i] == UD_EN && last == UD_AL) t[i] = UD_AN;
    }
    for (int i = 0; i < m; i++)
        if (t[i] == UD_AL) t[i] = UD_R;
    for (int i = 1; i + 1 < m; i++) {
        if (t[i] == UD_ES && t[i - 1] == UD_EN && t[i + 1] == UD_EN) t[i] = UD_EN;
        else if (t[i] == UD_CS && t[i - 1] == t[i + 1] && (t[i - 1] == UD_EN || t[i - 1] == UD_AN)) t[i] = t[i - 1];
    }
    for (int i = 0; i < m;) {
        if (t[i] != UD_ET) {
            i++;
            continue;
        }
        int k = i;
        while (k < m && t[k] == UD_ET) k++;
        if ((i > 0 && t[i - 1] == UD_EN) || (k < m && t[k] == UD_EN))
            for (int j = i; j < k; j++) t[j] = UD_EN;
        i = k;
    }
    for (int i = 0; i < m; i++)
        if (t[i] == UD_ES || t[i] == UD_ET || t[i] == UD_CS) t[i] = UD_ON;
    last = sos;
    for (int i = 0; i < m; i++) {
        if (t[i] == UD_L || t[i] == UD_R) last = t[i];
        else if (t[i] == UD_EN && last == UD_L) t[i] = UD_L;
    }
    ft_pairs(cp, t, m, e, sos);
    for (int i = 0; i < m;) {
        if (ft_strong(t[i]) >= 0) {
            i++;
            continue;
        }
        int k = i;
        while (k < m && ft_strong(t[k]) < 0) k++;
        int a = i ? ft_strong(t[i - 1]) : sos, b = k < m ? ft_strong(t[k]) : sos, d = a == b ? a : (e & 1 ? UD_R : UD_L);
        for (int j = i; j < k; j++) t[j] = (uint8_t)d;
        i = k;
    }
    for (int i = 0; i < m; i++) {
        int l = e;
        if (!(e & 1)) l += t[i] == UD_R ? 1 : (t[i] == UD_AN || t[i] == UD_EN) ? 2 : 0;
        else l += (t[i] == UD_L || t[i] == UD_EN || t[i] == UD_AN) ? 1 : 0;
        lv[i] = (uint8_t)l;
    }
    for (int i = m - 1, tail = 1; i >= 0; i--) {
        int c = cls[i];
        if (c == UD_S || c == UD_B) lv[i] = (uint8_t)e, tail = 1;
        else if (tail && (c == UD_WS || c == UD_BN || (c >= UD_LRE && c <= UD_PDI))) lv[i] = (uint8_t)e;
        else tail = 0;
    }
}

static int ft_has_cp(goome_font **ch, int nch, uint32_t cp)
{
    for (int i = 0; i < nch; i++)
        if (ft_cmap(ch[i], cp)) return 1;
    return 0;
}

static int ft_jtype(uint32_t cp)
{
    if (cp == 0x640 || cp == 0x200D) return 'C';
    const uint16_t *fm = ft_forms(cp);
    if (fm) return fm[2] || fm[3] ? 'D' : fm[1] ? 'R' : 'U';
    return cp >= 0x300 && ft_ucls(cp) == UD_NSM ? 'T' : 'U';
}

static void ft_arabic(goome_font **ch, int nch, const uint32_t *cp, uint32_t *out, int m)
{
    for (int i = 0; i < m; i++) {
        int ti = ft_jtype(cp[i]);
        const uint16_t *fm = ft_forms(cp[i]);
        if (!fm || (ti != 'D' && ti != 'R')) continue;
        int p = i - 1, n = i + 1;
        while (p >= 0 && ft_jtype(cp[p]) == 'T') p--;
        while (n < m && ft_jtype(cp[n]) == 'T') n++;
        int tp = p >= 0 ? ft_jtype(cp[p]) : 'U', tn = n < m ? ft_jtype(cp[n]) : 'U';
        int jp = tp == 'D' || tp == 'C', jn = ti == 'D' && (tn == 'D' || tn == 'R' || tn == 'C');
        if (cp[i] == 0x644 && i + 1 < m && ft_alef(cp[i + 1])) {
            for (size_t k = 0; k < sizeof(ud_lamalef) / sizeof(ud_lamalef[0]); k += 3)
                if (ud_lamalef[k] == cp[i + 1]) {
                    uint32_t lg = ud_lamalef[k + 1 + (jp ? 1 : 0)];
                    if (ft_has_cp(ch, nch, lg)) out[i] = lg, out[i + 1] = 0;
                    break;
                }
            if (!out[i + 1]) {
                i++;
                continue;
            }
        }
        int want = jp && jn ? 3 : jp ? 1 : jn ? 2 : 0;
        if (want == 3 && !fm[3]) want = 1;
        if (want == 2 && !fm[2]) want = 0;
        if (want == 1 && !fm[1]) want = 0;
        uint32_t c = fm[want];
        if (c && ft_has_cp(ch, nch, c)) out[i] = c;
    }
}

static int ft_pick(goome_font **ch, int nch, uint32_t cp, int pref, int *fi)
{
    for (int pass = 0; pass < (pref ? 2 : 1); pass++)
        for (int i = 0; i < nch; i++) {
            int col = ch[i]->color != 0;
            if (pref == 1 && (pass == 0) != col) continue;
            if (pref == 2 && (pass == 0) == col) continue;
            int g = ft_cmap(ch[i], cp);
            if (g) {
                *fi = i;
                return g;
            }
        }
    *fi = 0;
    return 0;
}

static int ft_uvs(const goome_font *f, uint32_t base, uint32_t vs)
{
    uint32_t o = f->uvs, n = ft_u32(f, o + 6);
    if (!o) return -1;
    for (uint32_t i = 0; i < n && i < 256; i++) {
        uint32_t r = o + 10 + i * 11;
        if ((ft_u32(f, r) >> 8) != vs) continue;
        uint32_t nd = ft_u32(f, r + 7);
        if (!nd) return -1;
        uint32_t t = o + nd, cnt = ft_u32(f, t), lo = 0, hi = cnt < 1000000 ? cnt : 1000000;
        while (lo < hi) {
            uint32_t m = (lo + hi) / 2, u = ft_u32(f, t + 4 + m * 5) >> 8;
            if (u == base) {
                int g = (int)ft_u16(f, t + 4 + m * 5 + 3);
                return g < f->nglyphs ? g : -1;
            }
            if (u < base) lo = m + 1;
            else hi = m;
        }
        return -1;
    }
    return -1;
}

static int ft_liga(const goome_font *f, int *g, uint32_t *src, int n)
{
    for (int li = 0; li < f->nlig && n > 1; li++) {
        uint32_t st = f->lig[li], cov = st + ft_u16(f, st + 2), nset = ft_u16(f, st + 4);
        for (int i = 0; i < n; i++) {
            int ci = ft_coverage(f, cov, g[i]);
            if (ci < 0 || (uint32_t)ci >= nset) continue;
            uint32_t set = st + ft_u16(f, st + 6 + (uint32_t)ci * 2), nl = ft_u16(f, set);
            for (uint32_t k = 0; k < nl && k < 4096; k++) {
                uint32_t lg = set + ft_u16(f, set + 2 + k * 2), cc = ft_u16(f, lg + 2), out = ft_u16(f, lg);
                if (cc < 1 || cc > 32 || i + (int)cc > n || out >= (uint32_t)f->nglyphs) continue;
                int ok = 1;
                for (uint32_t q = 1; q < cc && ok; q++) ok = ft_u16(f, lg + 4 + (q - 1) * 2) == (uint32_t)g[i + (int)q];
                if (!ok) continue;
                g[i] = (int)out;
                if (cc > 1) {
                    memmove(g + i + 1, g + i + cc, sizeof(int) * (size_t)(n - i - (int)cc));
                    memmove(src + i + 1, src + i + cc, sizeof(uint32_t) * (size_t)(n - i - (int)cc));
                    n -= (int)cc - 1;
                }
                break;
            }
        }
    }
    return n;
}

static int ft_emit(goome_font *f, int64_t adv, int fi, int g, int lo, int hi, int rtl, int kind, int *ng)
{
    if (*ng >= f->sglcap) {
        int c = f->sglcap ? f->sglcap * 2 : 256;
        ft_gl *n = realloc(f->sgl, sizeof(ft_gl) * (size_t)c);
        if (!n) return 0;
        f->sgl = n, f->sglcap = c;
    }
    ft_gl *e = &f->sgl[(*ng)++];
    e->g = g, e->lo = lo, e->hi = hi, e->adv = adv, e->fi = (uint8_t)fi, e->rtl = (uint8_t)rtl, e->kind = (uint8_t)kind;
    return 1;
}

static int64_t ft_adv_in(goome_font *f, goome_font *o, int g)
{
    int64_t a = ft_adv(o, g);
    return o == f ? a : ft_divr(a * f->upem, o->upem);
}

static int ft_cluster(goome_font *f, goome_font **ch, int nch, int a, int b, int rtl, int *ng)
{
    const uint32_t *ocp = f->scp, *cp = f->ssh;
    int lo = f->soff[a], hi = f->soff[b];
    uint32_t c0 = ocp[a];
    if (c0 == '\t') return ft_emit(f, 0, 0, 0, lo, hi, rtl, 1, ng);
    if (c0 < 32 || c0 == 127) return ft_emit(f, 0, 0, 0, lo, hi, rtl, 2, ng);
    int pref = ft_emo_pres(c0) || ft_ri(c0) ? 1 : 0, vs = 0;
    for (int k = a + 1; k < b; k++) {
        uint32_t c = ocp[k];
        if (c == 0xFE0E) pref = 2;
        else if (c == 0xFE0F || c == 0x20E3 || c == 0x200D || (c >= 0x1F3FB && c <= 0x1F3FF) || (c >= 0xE0020 && c <= 0xE007F)) pref = pref == 2 ? 2 : 1;
        if (ft_vs(c) && !vs) vs = (int)c;
    }
    int fi = 0, k0 = a;
    while (k0 < b && !cp[k0]) k0++;
    if (k0 >= b) return ft_emit(f, 0, 0, 0, lo, hi, rtl, 2, ng);
    int g0 = ft_pick(ch, nch, cp[k0], pref, &fi);
    if (!g0 && ft_ignorable(cp[k0]) && b - a == 1) return ft_emit(f, 0, 0, 0, lo, hi, rtl, 2, ng);
    goome_font *F = ch[fi];
    if (vs) {
        int u = ft_uvs(F, cp[k0], (uint32_t)vs);
        if (u > 0) g0 = u;
    }
    int gs[32], fs[32], n = 0;
    uint32_t src[32];
    for (int k = k0; k < b && n < 32; k++) {
        uint32_t c = cp[k];
        if (!c) continue;
        int g = k == k0 ? g0 : ft_cmap(F, c), gf = fi;
        if (!g && k > k0) {
            if (ft_ignorable(c)) continue;
            g = ft_pick(ch, nch, c, 0, &gf);
            if (!g) gf = fi;
        }
        gs[n] = g, fs[n] = gf, src[n] = c, n++;
    }
    if (n > 1 && F->nlig) {
        int same = 1;
        for (int k = 1; k < n; k++) same &= fs[k] == fi;
        if (same) n = ft_liga(F, gs, src, n);
    }
    int w = 0;
    for (int k = 0; k < n; k++)
        if (!(n > 1 && ft_ignorable(src[k]))) gs[w] = gs[k], fs[w] = fs[k], src[w] = src[k], w++;
    n = w;
    if (!n) return ft_emit(f, 0, 0, 0, lo, hi, rtl, 2, ng);
    for (int q = 0; q < n; q++) {
        int k = rtl ? n - 1 - q : q;
        if (!ft_emit(f, ft_adv_in(f, ch[fs[k]], gs[k]), fs[k], gs[k], lo, hi, rtl, 0, ng)) return 0;
    }
    return 1;
}

static int ft_chain(goome_font *f, goome_font **ch)
{
    int n = 1;
    ch[0] = f;
    for (int i = 0; i < n && n < 16; i++)
        for (int k = 0; k < ch[i]->nfb && n < 16; k++) {
            goome_font *c = ch[i]->fb[k];
            int dup = 0;
            for (int j = 0; j < n; j++) dup |= ch[j] == c;
            if (!dup) ch[n++] = c;
        }
    return n;
}

static int ft_line_dir(goome_font *f, int m)
{
    if (ft_dir_mode == GOOME_DIR_RTL) return 1;
    if (ft_dir_mode == GOOME_DIR_LTR) return 0;
    for (int i = 0; i < m; i++) {
        int c = f->scl[i];
        if (c == UD_L) return 0;
        if (c == UD_R || c == UD_AL) return 1;
    }
    return 0;
}

static int ft_shape(goome_font *f, goome_font **ch, int nch, const char *s, int n, int *para)
{
    int m = 0, ng = 0, bidi = ft_dir_mode == GOOME_DIR_RTL, arab = 0;
    if (para) *para = 0;
    if (n <= 0) return 0;
    if (!ft_scratch(f, n + 1)) return -1;
    const char *p = s, *e = s + n;
    while (p < e) {
        uint32_t c;
        f->soff[m] = (int)(p - s);
        f->scp[m] = c = ft_utf8n(&p, e);
        f->ssh[m] = c;
        if (c >= 0x590) {
            int k = ft_ucls(c);
            bidi |= k == UD_R || k == UD_AL || k == UD_AN || k == UD_RLE || k == UD_RLO || k == UD_RLI;
            arab |= (c >= 0x600 && c <= 0x6FF) || (c >= 0x750 && c <= 0x77F) || (c >= 0x8A0 && c <= 0x8FF);
        }
        m++;
    }
    f->soff[m] = n;
    int base = 0;
    if (bidi) {
        for (int i = 0; i < m; i++) f->scl[i] = (uint8_t)ft_ucls(f->scp[i]);
        base = ft_line_dir(f, m);
        ft_levels(f->scp, f->scl, f->swk, f->slv, m, base);
        for (int i = 0; i < m; i++)
            if (f->slv[i] & 1) {
                uint32_t mc = ft_mirror(f->scp[i]);
                if (mc && ft_has_cp(ch, nch, mc)) f->ssh[i] = mc;
            }
    } else
        memset(f->slv, 0, (size_t)m);
    if (para) *para = base + (bidi ? 1 : 0);
    if (arab) ft_arabic(ch, nch, f->scp, f->ssh, m);
    int nc = 0, nri = 0, len = 0;
    for (int i = 0; i < m; i++) {
        if (!i || ft_brk(f->scp[i - 1], f->scp[i], nri, len)) f->scs[nc++] = i, nri = 0, len = 0;
        nri += ft_ri(f->scp[i]), len++;
    }
    f->scs[nc] = m;
    int top = 0, low = 255;
    for (int k = 0; k < nc; k++) {
        int l = f->slv[f->scs[k]];
        f->sord[k] = k;
        if (l > top) top = l;
        if ((l & 1) && l < low) low = l;
    }
    for (int l = top; l >= low && l > 0; l--)
        for (int k = 0; k < nc;) {
            if (f->slv[f->scs[f->sord[k]]] < l) {
                k++;
                continue;
            }
            int j = k;
            while (j < nc && f->slv[f->scs[f->sord[j]]] >= l) j++;
            for (int x = k, y = j - 1; x < y; x++, y--) {
                int t = f->sord[x];
                f->sord[x] = f->sord[y], f->sord[y] = t;
            }
            k = j;
        }
    for (int q = 0; q < nc; q++) {
        int k = f->sord[q], a = f->scs[k], b = f->scs[k + 1];
        if (!ft_cluster(f, ch, nch, a, b, f->slv[a] & 1, &ng)) return -1;
    }
    return ng;
}

int gm_text_rtl(goome_font *f, const char *s, int n)
{
    if (!f || !s || n <= 0) return 0;
    goome_font *ch[16];
    int para = 0;
    if (ft_dir_mode == GOOME_DIR_RTL) return 2;
    int any = 0;
    for (int i = 0; i < n && !any; i++) any = (unsigned char)s[i] >= 0xD6;
    if (!any) return 0;
    if (ft_shape(f, ch, ft_chain(f, ch), s, n, &para) < 0) return 0;
    return para;
}

int gm_text_layout(goome_font *f, double size, const char *s, int n, gm_tc **out)
{
    *out = NULL;
    if (!f || !s || n <= 0 || !ft_okd(size) || size <= 0) return 0;
    goome_font *ch[16];
    int nch = ft_chain(f, ch), ng = ft_shape(f, ch, nch, s, n, NULL), sp = ft_cmap(f, ' '), prev = -1, nb = 0;
    if (ng <= 0) return 0;
    gm_tc *b = malloc(sizeof(gm_tc) * (size_t)ng);
    if (!b) return 0;
    int64_t pen = 0;
    for (int i = 0; i < ng; i++) {
        ft_gl *e = &f->sgl[i];
        int64_t x0 = pen;
        if (e->kind == 1) pen += 4 * (int64_t)ft_adv(f, sp), prev = -1;
        else if (e->kind == 0) {
            if (e->fi == 0 && !e->rtl) {
                if (prev >= 0) pen += ft_kern(f, prev, e->g), x0 = pen;
                prev = e->g;
            } else
                prev = -1;
            pen += e->adv;
        }
        double a = (double)x0 * size / f->upem, z = (double)pen * size / f->upem;
        if (nb && b[nb - 1].lo == e->lo) {
            if (a < b[nb - 1].x0) b[nb - 1].x0 = a;
            if (z > b[nb - 1].x1) b[nb - 1].x1 = z;
            continue;
        }
        b[nb].lo = e->lo, b[nb].hi = e->hi, b[nb].rtl = e->rtl, b[nb].x0 = a, b[nb].x1 = z;
        nb++;
    }
    for (int i = 1; i < nb; i++) {
        gm_tc t = b[i];
        int j = i - 1;
        while (j >= 0 && b[j].lo > t.lo) b[j + 1] = b[j], j--;
        b[j + 1] = t;
    }
    *out = b;
    return nb;
}

static void ft_scales(const goome_font *f, int px64, int64_t *sx, int64_t *sy)
{
    *sx = ((int64_t)px64 * 1048576 + f->upem / 2) / f->upem;
    *sy = *sx;
    if (px64 < 32 * 64 && f->xh > 0) {
        int64_t xp = (int64_t)f->xh * 64 * *sx >> 24, want = (xp + 128) / 256 * 256;
        if (want < 256) want = 256;
        if (xp > 0) *sy = *sx * want / xp;
    }
}

// draws (c != NULL) or measures. returns the widest line in logical px
static double ft_run(goome_canvas *c, goome_font *f, double size, double x, double y, const char *s, uint32_t argb, int *lines)
{
    *lines = 1;
    if (!s || !ft_okd(size) || size <= 0 || size > 100000) return 0;
    int k[4] = {0, 0, 0, 0}, draw = 0, px64 = 0;
    int64_t sx = 0, sy = 0, ox = 0, base = 0, lh = 0;
    double lhl = (double)(f->asc - f->desc + f->gap) * size / f->upem;
    if (c && c->px && ft_okd(x) && ft_okd(y) && c->scale > 0 && (argb >> 24)) {
        double px = size * c->scale;
        draw = px * 64 >= 1 && px <= FT_MAXPX;
        px64 = (int)ft_round(px * 64);
        k[0] = 0, k[1] = 0, k[2] = c->w, k[3] = c->h;
        if (c->nclip || c->kx1 > c->kx0 || c->ky1 > c->ky0) {
            if (c->kx0 > k[0]) k[0] = c->kx0;
            if (c->ky0 > k[1]) k[1] = c->ky0;
            if (c->kx1 < k[2]) k[2] = c->kx1;
            if (c->ky1 < k[3]) k[3] = c->ky1;
        }
        if (k[2] <= k[0] || k[3] <= k[1]) draw = 0;
    }
    if (draw) {
        ft_scales(f, px64, &sx, &sy);
        ft_colors(f, argb);
        ox = (int64_t)ft_round((x - c->ox) * c->scale * 256);
        double top = (y - c->oy) * c->scale;
        double asc = (double)(f->asc + f->gap / 2) * size * c->scale / f->upem;
        base = (int64_t)ft_round(top + asc);
        lh = (int64_t)ft_round(lhl * c->scale * 256);
    }
    goome_font *ch[16];
    int64_t fsx[16], fsy[16];
    int nch = ft_chain(f, ch);
    fsx[0] = sx, fsy[0] = sy;
    for (int i = 1; i < nch; i++) fsx[i] = fsy[i] = 0;
    int64_t pen = 0, best = 0, line0 = base * 256;
    int prev = -1, sp = ft_cmap(f, ' ');
    for (;;) {
        const char *le = s;
        while (*le && *le != '\n') le++;
        int ng = ft_shape(f, ch, nch, s, (int)(le - s), NULL);
        for (int i = 0; i < ng; i++) {
            const ft_gl *e = &f->sgl[i];
            if (e->kind == 1) {
                pen += 4 * (int64_t)ft_adv(f, sp), prev = -1;
                continue;
            }
            if (e->kind) continue;
            int g = e->g, fi = e->fi;
            if (fi == 0 && !e->rtl) {
                if (prev >= 0) pen += ft_kern(f, prev, g);
                prev = g;
            } else
                prev = -1;
            if (draw && pen * 64 < ((int64_t)1 << 62) / (sx + 1) && pen * 64 > -(((int64_t)1 << 62) / (sx + 1))) {
                int64_t gx = ox + (pen * 64 * sx >> 24), ix = ft_fdiv(gx, 256), ph = ((gx - ix * 256) + 32) >> 6;
                if (ph == 4) ph = 0, ix++;
                int64_t by = ft_fdiv(line0 + 128, 256);
                if (ix > -100000 && ix < 100000 && by > -100000 && by < 100000) {
                    goome_font *F = ch[fi];
                    if (!fsx[fi]) ft_scales(F, px64, &fsx[fi], &fsy[fi]);
                    const ft_cent *ce = F->color ? ft_cglyph(F, g, px64, fsx[fi], fsy[fi], argb) : NULL;
                    if (ce) ft_cblit(c, ce->px, ce->w, ce->h, (int)ix + ce->l, (int)by + ce->t, k, argb >> 24);
                    else {
                        const uint8_t *bits;
                        if (F != f) ft_colors(F, argb);
                        const ft_ent *ge = ft_glyph(F, g, px64, fsx[fi], fsy[fi], (int)ph, &bits);
                        if (ge) ft_blit(c, F, bits, ge->w, ge->h, (int)ix + ge->l, (int)by + ge->t, k);
                    }
                }
            }
            pen += e->adv;
        }
        if (*le != '\n') break;
        if (pen > best) best = pen;
        pen = 0, prev = -1, (*lines)++;
        line0 += lh;
        s = le + 1;
    }
    if (pen > best) best = pen;
    return (double)best * size / f->upem;
}

static double ft_builtin(goome_canvas *c, double size, double x, double y, const char *s, uint32_t argb)
{
    char buf[512];
    double best = 0, lh = size >= 17 ? 24 : 18;
    for (int line = 0; s;) {
        const char *nl = strchr(s, '\n');
        size_t n = nl ? (size_t)(nl - s) : strlen(s);
        size_t m = n < sizeof(buf) - 1 ? n : sizeof(buf) - 1;
        memcpy(buf, s, m);
        buf[m] = 0;
        double w;
        if (c) w = goome_draw_text(c, x, y + line * lh, buf, size >= 17, argb);
        else {
            goome_canvas t;
            memset(&t, 0, sizeof(t));
            t.scale = 1;
            w = goome_text_width(&t, buf, size >= 17);
        }
        if (w > best) best = w;
        s = nl ? nl + 1 : NULL, line++;
    }
    return best;
}

double goome_text(goome_canvas *c, goome_font *f, double size, double x, double y, const char *utf8, uint32_t argb)
{
    int lines;
    if (!c || !utf8) return 0;
    if (!f) return ft_builtin(c, size, x, y, utf8, argb);
    return ft_run(c, f, size, x, y, utf8, argb, &lines);
}

double goome_text_measure(goome_font *f, double size, const char *utf8, double *height)
{
    int lines = 1;
    double w;
    if (!f) {
        w = utf8 ? ft_builtin(NULL, size, 0, 0, utf8, 0) : 0;
        for (const char *s = utf8; s && *s; s++) lines += *s == '\n';
    } else w = ft_run(NULL, f, size, 0, 0, utf8, 0, &lines);
    if (height) *height = goome_text_line_height(f, size) * lines;
    return w;
}

double goome_text_line_height(goome_font *f, double size)
{
    if (!f) return size >= 17 ? 24 : 18;
    if (!ft_okd(size) || size <= 0) return 0;
    return (double)(f->asc - f->desc + f->gap) * size / f->upem;
}

#if defined(__clang__)
#pragma STDC FP_CONTRACT DEFAULT
#endif

#undef FT_MAXFILE
#undef FT_MAXPTS
#undef FT_MAXSEG
#undef FT_MAXDIM
#undef FT_MAXPX
#undef FT_ARENA
#undef FT_SLOTS
#undef FT_KC
#undef FT_MAXMAP
#undef FT_NFB
#undef FT_CC
#undef FT_NLIG
#undef FT_REG
#undef FT_TOL
#endif
