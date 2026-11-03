#include "internal.h"

#ifndef GOOME_NO_DRAW
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#define IM_MAXDIM 16384
#define IM_MAXFILE ((size_t)256 << 20)
#define IM_MAXRAW ((uint64_t)1 << 31)
#define IM_MAXCOEF ((uint64_t)1 << 28)

typedef unsigned char im_u8;

static const char *im_why = "";

static int im_fail(int code, const char *why)
{
    im_why = why;
    return code;
}

const char *goome_bitmap_error(void) { return im_why; }

static inline uint32_t im_be32(const im_u8 *p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }
static inline uint32_t im_le32(const im_u8 *p) { return (uint32_t)p[3] << 24 | (uint32_t)p[2] << 16 | (uint32_t)p[1] << 8 | p[0]; }
static inline uint32_t im_le16(const im_u8 *p) { return (uint32_t)p[1] << 8 | p[0]; }
static inline uint32_t im_be16(const im_u8 *p) { return (uint32_t)p[0] << 8 | p[1]; }

// round(c * a / 255), exact for 0..255
static inline uint32_t im_mul(uint32_t c, uint32_t a)
{
    uint32_t t = c * a + 128;
    return (t + (t >> 8)) >> 8;
}

static inline uint32_t im_px(uint32_t r, uint32_t g, uint32_t b, uint32_t a)
{
    if (a == 255) return 0xFF000000u | r << 16 | g << 8 | b;
    return a << 24 | im_mul(r, a) << 16 | im_mul(g, a) << 8 | im_mul(b, a);
}

static inline int im_clamp(int v) { return v < 0 ? 0 : v > 255 ? 255 : v; }

static int im_alloc(goome_bitmap *out, int w, int h)
{
    if (w <= 0 || h <= 0 || w > IM_MAXDIM || h > IM_MAXDIM) return im_fail(GOOME_EARG, "image size out of range");
    out->px = malloc((size_t)w * (size_t)h * 4);
    if (!out->px) return im_fail(GOOME_ENOMEM, "out of memory");
    out->w = w, out->h = h, out->stride = w, out->owned = 1;
    return GOOME_OK;
}

// inflate, the deflate half of zlib

#define IM_ZFAST 10

typedef struct {
    uint16_t fast[1 << IM_ZFAST]; // len << 9 | sym, 0 = look further
    uint16_t first[16], base[16], cnt[16];
    uint16_t sym[288];
} im_huff;

typedef struct {
    const im_u8 *p, *end;
    uint64_t bits;
    int nb, over;
    im_u8 *out;
    size_t n, cap, lim;
} im_zs;

static const uint16_t im_lbase[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
static const im_u8 im_lext[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
static const uint16_t im_dbase[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
static const im_u8 im_dext[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

static inline uint32_t im_rev16(uint32_t v)
{
    v = (v & 0x5555) << 1 | (v >> 1 & 0x5555);
    v = (v & 0x3333) << 2 | (v >> 2 & 0x3333);
    v = (v & 0x0F0F) << 4 | (v >> 4 & 0x0F0F);
    return (v & 0x00FF) << 8 | (v >> 8 & 0x00FF);
}

static int im_hbuild(im_huff *h, const im_u8 *lens, int n)
{
    int cnt[16] = {0}, next[16], left = 1, code = 0, k = 0;
    memset(h->fast, 0, sizeof(h->fast));
    for (int i = 0; i < n; i++) cnt[lens[i]]++;
    cnt[0] = 0;
    for (int l = 1; l < 16; l++) {
        left = left * 2 - cnt[l];
        if (left < 0) return -1;
    }
    for (int l = 1; l < 16; l++) {
        next[l] = code;
        h->first[l] = (uint16_t)code, h->base[l] = (uint16_t)k, h->cnt[l] = (uint16_t)cnt[l];
        code = (code + cnt[l]) << 1;
        k += cnt[l];
    }
    for (int i = 0; i < n; i++) {
        int l = lens[i];
        if (!l) continue;
        int c = next[l]++;
        h->sym[h->base[l] + c - h->first[l]] = (uint16_t)i;
        if (l <= IM_ZFAST)
            for (uint32_t j = im_rev16((uint32_t)c) >> (16 - l); j < (1u << IM_ZFAST); j += 1u << l) h->fast[j] = (uint16_t)(l << 9 | i);
    }
    return 0;
}

static inline void im_zfill(im_zs *z)
{
    if (z->end - z->p >= 8) {
        const im_u8 *p = z->p;
        uint64_t v = (uint64_t)im_le32(p) | (uint64_t)im_le32(p + 4) << 32;
        z->bits |= v << z->nb;
        z->p += (63 - z->nb) >> 3;
        z->nb |= 56;
        return;
    }
    while (z->nb <= 56) {
        uint64_t b = 0;
        if (z->p < z->end) b = *z->p++;
        else z->over++;
        z->bits |= b << z->nb;
        z->nb += 8;
    }
}

static inline uint32_t im_zbits(im_zs *z, int n)
{
    if (z->nb < n) im_zfill(z);
    uint32_t v = (uint32_t)(z->bits & ((1u << n) - 1));
    z->bits >>= n;
    z->nb -= n;
    return v;
}

static inline int im_zsym(im_zs *z, const im_huff *h)
{
    if (z->nb < 16) im_zfill(z);
    uint32_t e = h->fast[z->bits & ((1u << IM_ZFAST) - 1)];
    if (e) {
        z->bits >>= e >> 9;
        z->nb -= (int)(e >> 9);
        return (int)(e & 511);
    }
    uint32_t r = im_rev16((uint32_t)(z->bits & 0xFFFF));
    for (int l = IM_ZFAST + 1; l < 16; l++) {
        uint32_t c = (r >> (16 - l)) - h->first[l];
        if (c < h->cnt[l]) {
            z->bits >>= l;
            z->nb -= l;
            return h->sym[h->base[l] + c];
        }
    }
    return -1;
}

static int im_zroom(im_zs *z, size_t need)
{
    if (z->cap - z->n >= need) return 0;
    size_t c = z->cap ? z->cap : 65536;
    while (c - z->n < need && c < z->lim) c *= 2;
    if (c > z->lim) c = z->lim;
    if (c - z->n < need) return -1;
    im_u8 *o = realloc(z->out, c + 16);
    if (!o) return -1;
    z->out = o, z->cap = c;
    return 0;
}

static int im_zblock(im_zs *z, const im_huff *lit, const im_huff *dst)
{
    for (;;) {
        if (z->over > 8) return -1;
        if (z->cap - z->n < 258 && im_zroom(z, z->lim - z->n < 258 ? z->lim - z->n : 258)) return -1;
        int s = im_zsym(z, lit);
        if (s < 0) return -1;
        if (s < 256) {
            z->out[z->n++] = (im_u8)s;
            if (z->n == z->lim) return 1;
            continue;
        }
        if (s == 256) return 0;
        s -= 257;
        if (s >= 29) return -1;
        size_t len = im_lbase[s] + im_zbits(z, im_lext[s]);
        int ds = im_zsym(z, dst);
        if (ds < 0 || ds >= 30) return -1;
        size_t dist = im_dbase[ds] + im_zbits(z, im_dext[ds]);
        if (dist > z->n) return -1;
        int done = 0;
        if (len >= z->lim - z->n) len = z->lim - z->n, done = 1;
        im_u8 *d = z->out + z->n, *src = d - dist;
        if (dist >= 8)
            for (size_t i = 0; i < len; i += 8) memcpy(d + i, src + i, 8);
        else if (dist == 1)
            memset(d, *src, len);
        else
            for (size_t i = 0; i < len; i++) d[i] = src[i];
        z->n += len;
        if (done) return 1;
    }
}

// fills z->out up to z->lim, returns how much it got. stops early on bad data
static size_t im_inflate(im_zs *z)
{
    im_huff *t = malloc(sizeof(im_huff) * 3);
    if (!t) return 0;
    im_huff *lit = t, *dst = t + 1, *cl = t + 2;
    static const im_u8 order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
    int last = 0;
    while (!last && z->n < z->lim) {
        last = (int)im_zbits(z, 1);
        int type = (int)im_zbits(z, 2), r;
        if (z->over > 8) break;
        if (type == 0) {
            int drop = z->nb & 7, k;
            z->bits >>= drop, z->nb -= drop;
            k = z->nb >> 3;
            if (z->over > k) break;
            z->p -= k - z->over;
            z->over = 0, z->bits = 0, z->nb = 0;
            if (z->end - z->p < 4) break;
            uint32_t len = im_le16(z->p), nlen = im_le16(z->p + 2);
            z->p += 4;
            if ((len ^ 0xFFFF) != nlen) break;
            size_t k2 = len;
            if (k2 > (size_t)(z->end - z->p)) k2 = (size_t)(z->end - z->p);
            if (k2 > z->lim - z->n) k2 = z->lim - z->n;
            if (im_zroom(z, k2)) break;
            if (k2) memcpy(z->out + z->n, z->p, k2);
            z->n += k2, z->p += k2;
            if (k2 < len && z->n < z->lim) break;
            continue;
        }
        if (type == 1) {
            im_u8 l[320];
            memset(l, 8, 144), memset(l + 144, 9, 112), memset(l + 256, 7, 24), memset(l + 280, 8, 8);
            im_hbuild(lit, l, 288);
            memset(l, 5, 30);
            im_hbuild(dst, l, 30);
        } else if (type == 2) {
            int hlit = (int)im_zbits(z, 5) + 257, hdist = (int)im_zbits(z, 5) + 1, hclen = (int)im_zbits(z, 4) + 4;
            if (hlit > 286 || hdist > 30) break;
            im_u8 cll[19] = {0}, l[320];
            for (int i = 0; i < hclen; i++) cll[order[i]] = (im_u8)im_zbits(z, 3);
            if (im_hbuild(cl, cll, 19)) break;
            int i = 0, n = hlit + hdist;
            while (i < n) {
                int s = im_zsym(z, cl), rep = 0;
                im_u8 v = 0;
                if (s < 0 || z->over > 8) goto out;
                if (s < 16) {
                    l[i++] = (im_u8)s;
                    continue;
                }
                if (s == 16) {
                    if (!i) goto out;
                    v = l[i - 1], rep = 3 + (int)im_zbits(z, 2);
                } else if (s == 17) rep = 3 + (int)im_zbits(z, 3);
                else rep = 11 + (int)im_zbits(z, 7);
                if (rep > n - i) goto out;
                memset(l + i, v, (size_t)rep);
                i += rep;
            }
            if (!l[256] || im_hbuild(lit, l, hlit) || im_hbuild(dst, l + hlit, hdist)) break;
        } else
            break;
        r = im_zblock(z, lit, dst);
        if (r < 0) break;
        if (r > 0) break;
    }
out:
    free(t);
    return z->n;
}

typedef struct {
    int w, h, depth, ct, ch, bpp;
    int key[3], haskey;
    uint32_t pal[256];
} im_png;

static size_t im_rowbytes(const im_png *p, int w) { return ((size_t)w * (size_t)p->ch * (size_t)p->depth + 7) / 8; }

static int im_unfilter(im_u8 *raw, int rows, size_t rb, int bpp, const im_u8 *zero)
{
    const im_u8 *prev = zero;
    for (int y = 0; y < rows; y++) {
        im_u8 *row = raw + (size_t)y * (rb + 1), *x = row + 1;
        size_t i;
        switch (row[0]) {
        case 0:
            break;
        case 1:
            for (i = (size_t)bpp; i < rb; i++) x[i] = (im_u8)(x[i] + x[i - bpp]);
            break;
        case 2:
            for (i = 0; i < rb; i++) x[i] = (im_u8)(x[i] + prev[i]);
            break;
        case 3:
            for (i = 0; i < (size_t)bpp && i < rb; i++) x[i] = (im_u8)(x[i] + (prev[i] >> 1));
            for (; i < rb; i++) x[i] = (im_u8)(x[i] + ((x[i - bpp] + prev[i]) >> 1));
            break;
        case 4:
            for (i = 0; i < (size_t)bpp && i < rb; i++) x[i] = (im_u8)(x[i] + prev[i]);
            for (; i < rb; i++) {
                int a = x[i - bpp], b = prev[i], c = prev[i - bpp];
                int pa = b - c, pb = a - c, pc;
                pa = pa < 0 ? -pa : pa, pb = pb < 0 ? -pb : pb;
                pc = a + b - 2 * c, pc = pc < 0 ? -pc : pc;
                x[i] = (im_u8)(x[i] + (pa <= pb && pa <= pc ? a : pb <= pc ? b : c));
            }
            break;
        default:
            return -1;
        }
        prev = x;
    }
    return 0;
}

static void im_png_row(const im_png *p, const im_u8 *s, int n, uint32_t *d, int step)
{
    int dp = p->depth;
    if (dp < 8) {
        int mask = (1 << dp) - 1, sc = dp == 1 ? 255 : dp == 2 ? 85 : 17;
        for (int x = 0; x < n; x++) {
            int bit = x * dp;
            int v = s[bit >> 3] >> (8 - dp - (bit & 7)) & mask;
            uint32_t c;
            if (p->ct == 3) c = p->pal[v];
            else if (p->haskey && v == p->key[0]) c = 0;
            else c = im_px((uint32_t)(v * sc), (uint32_t)(v * sc), (uint32_t)(v * sc), 255);
            d[(size_t)x * step] = c;
        }
        return;
    }
    if (dp == 8 && step == 1 && !p->haskey && (p->ct == 2 || p->ct == 6)) {
        if (p->ct == 2)
            for (int x = 0; x < n; x++, s += 3) d[x] = 0xFF000000u | (uint32_t)s[0] << 16 | (uint32_t)s[1] << 8 | s[2];
        else
            for (int x = 0; x < n; x++, s += 4) d[x] = im_px(s[0], s[1], s[2], s[3]);
        return;
    }
    int b = dp / 8;
    for (int x = 0; x < n; x++, s += (size_t)p->ch * b) {
        uint32_t c;
        switch (p->ct) {
        case 0: {
            int v = b == 2 ? (int)im_be16(s) : s[0];
            c = p->haskey && v == p->key[0] ? 0 : im_px(s[0], s[0], s[0], 255);
            break;
        }
        case 2:
            if (p->haskey) {
                int r = b == 2 ? (int)im_be16(s) : s[0], g = b == 2 ? (int)im_be16(s + 2) : s[1], bl = b == 2 ? (int)im_be16(s + 4) : s[2];
                if (r == p->key[0] && g == p->key[1] && bl == p->key[2]) {
                    c = 0;
                    break;
                }
            }
            c = im_px(s[0], s[b], s[2 * b], 255);
            break;
        case 3:
            c = p->pal[s[0]];
            break;
        case 4:
            c = im_px(s[0], s[0], s[0], s[b]);
            break;
        default:
            c = im_px(s[0], s[b], s[2 * b], s[3 * b]);
            break;
        }
        d[(size_t)x * step] = c;
    }
}

static int im_png_load(const im_u8 *d, size_t n, goome_bitmap *out)
{
    im_png p;
    memset(&p, 0, sizeof(p));
    int ilace = 0, gothdr = 0, npal = 0, ntrns = 0, rc;
    im_u8 trns[256];
    const im_u8 *pal = NULL;
    im_u8 *idat = NULL, *raw = NULL, *zero = NULL;
    size_t nidat = 0, cidat = 0;
    size_t pos = 8;
    while (pos + 8 <= n) {
        size_t len = im_be32(d + pos);
        const im_u8 *t = d + pos + 4, *c = d + pos + 8;
        int last = 0;
        if (len > n - pos - 8) len = n - pos - 8, last = 1;
        if (!gothdr) {
            if (memcmp(t, "IHDR", 4) || len < 13) {
                rc = im_fail(GOOME_EARG, "png has no header");
                goto fail;
            }
            uint32_t w = im_be32(c), h = im_be32(c + 4);
            p.depth = c[8], p.ct = c[9], ilace = c[12];
            if (w == 0 || h == 0 || w > IM_MAXDIM || h > IM_MAXDIM) {
                rc = im_fail(GOOME_EARG, "png size out of range");
                goto fail;
            }
            p.w = (int)w, p.h = (int)h;
            int dp = p.depth, ok;
            switch (p.ct) {
            case 0: ok = dp == 1 || dp == 2 || dp == 4 || dp == 8 || dp == 16, p.ch = 1; break;
            case 2: ok = dp == 8 || dp == 16, p.ch = 3; break;
            case 3: ok = dp == 1 || dp == 2 || dp == 4 || dp == 8, p.ch = 1; break;
            case 4: ok = dp == 8 || dp == 16, p.ch = 2; break;
            case 6: ok = dp == 8 || dp == 16, p.ch = 4; break;
            default: ok = 0;
            }
            if (!ok || c[10] || c[11] || ilace > 1) {
                rc = im_fail(GOOME_EARG, "png header has a bad color type or depth");
                goto fail;
            }
            gothdr = 1;
        } else if (!memcmp(t, "PLTE", 4)) {
            pal = c, npal = (int)(len / 3 > 256 ? 256 : len / 3);
        } else if (!memcmp(t, "tRNS", 4)) {
            ntrns = (int)(len > 256 ? 256 : len);
            memcpy(trns, c, (size_t)ntrns);
        } else if (!memcmp(t, "IDAT", 4) && len) {
            if (len > cidat - nidat) {
                size_t nc = cidat ? cidat : 65536;
                while (nc - nidat < len) nc *= 2;
                im_u8 *q = realloc(idat, nc);
                if (!q) {
                    rc = im_fail(GOOME_ENOMEM, "out of memory");
                    goto fail;
                }
                idat = q, cidat = nc;
            }
            memcpy(idat + nidat, c, len);
            nidat += len;
        } else if (!memcmp(t, "IEND", 4))
            break;
        if (last) break;
        pos += 12 + len;
    }
    if (!gothdr) {
        rc = im_fail(GOOME_EARG, "png has no header");
        goto fail;
    }
    if (p.ct == 3 && !pal) {
        rc = im_fail(GOOME_EARG, "png palette missing");
        goto fail;
    }
    if (nidat < 2) {
        rc = im_fail(GOOME_EARG, "png has no image data");
        goto fail;
    }
    for (int i = 0; i < 256; i++) {
        uint32_t a = p.ct == 3 && i < ntrns ? trns[i] : 255;
        p.pal[i] = i < npal ? im_px(pal[i * 3], pal[i * 3 + 1], pal[i * 3 + 2], a) : 0xFF000000u;
    }
    if (p.ct == 0 && ntrns >= 2) p.haskey = 1, p.key[0] = (int)im_be16(trns);
    if (p.ct == 2 && ntrns >= 6) p.haskey = 1, p.key[0] = (int)im_be16(trns), p.key[1] = (int)im_be16(trns + 2), p.key[2] = (int)im_be16(trns + 4);
    p.bpp = p.ch * p.depth / 8;
    if (p.bpp < 1) p.bpp = 1;

    static const int ax0[7] = {0, 4, 0, 2, 0, 1, 0}, ay0[7] = {0, 0, 4, 0, 2, 0, 1}, adx[7] = {8, 8, 4, 4, 2, 2, 1}, ady[7] = {8, 8, 8, 4, 4, 2, 2};
    int passes = ilace ? 7 : 1, pw[7], ph[7];
    uint64_t total = 0;
    for (int i = 0; i < passes; i++) {
        pw[i] = ilace ? (p.w - ax0[i] + adx[i] - 1) / adx[i] : p.w;
        ph[i] = ilace ? (p.h - ay0[i] + ady[i] - 1) / ady[i] : p.h;
        if (pw[i] > 0 && ph[i] > 0) total += (uint64_t)ph[i] * (im_rowbytes(&p, pw[i]) + 1);
    }
    if (total > IM_MAXRAW || total > (uint64_t)(size_t)-1) {
        rc = im_fail(GOOME_ENOMEM, "png too big to decode");
        goto fail;
    }
    if ((idat[0] & 15) != 8 || (idat[0] >> 4) > 7 || (idat[1] & 32)) {
        rc = im_fail(GOOME_EARG, "png data is not zlib");
        goto fail;
    }
    im_zs z;
    memset(&z, 0, sizeof(z));
    z.p = idat + 2, z.end = idat + nidat, z.lim = (size_t)total;
    if (im_inflate(&z) < z.lim) {
        free(z.out);
        rc = im_fail(GOOME_EARG, "png data is corrupt or cut off");
        goto fail;
    }
    raw = z.out;
    free(idat);
    idat = NULL;
    zero = calloc(im_rowbytes(&p, p.w) + 8, 1);
    if (!zero) {
        rc = im_fail(GOOME_ENOMEM, "out of memory");
        goto fail;
    }
    im_u8 *r = raw;
    for (int i = 0; i < passes; i++) {
        if (pw[i] <= 0 || ph[i] <= 0) continue;
        size_t rb = im_rowbytes(&p, pw[i]);
        if (im_unfilter(r, ph[i], rb, p.bpp, zero)) {
            rc = im_fail(GOOME_EARG, "png has a bad filter type");
            goto fail;
        }
        r += (size_t)ph[i] * (rb + 1);
    }
    if ((rc = im_alloc(out, p.w, p.h))) goto fail;
    r = raw;
    for (int i = 0; i < passes; i++) {
        if (pw[i] <= 0 || ph[i] <= 0) continue;
        size_t rb = im_rowbytes(&p, pw[i]);
        int x0 = ilace ? ax0[i] : 0, y0 = ilace ? ay0[i] : 0, dx = ilace ? adx[i] : 1, dy = ilace ? ady[i] : 1;
        for (int y = 0; y < ph[i]; y++, r += rb + 1) im_png_row(&p, r + 1, pw[i], out->px + (size_t)(y0 + y * dy) * p.w + x0, dx);
    }
    free(raw);
    free(zero);
    return GOOME_OK;
fail:
    free(idat);
    free(raw);
    free(zero);
    return rc;
}

typedef struct {
    uint16_t fast[512]; // len << 8 | sym, 0 = look further
    uint32_t maxc[18];
    int delta[17];
    im_u8 sym[256];
    int ok;
} im_jh;

typedef struct {
    int id, h, v, tq, cw, ch, stride, rows, pred, dc, ac, hasq;
    uint16_t q[64];
    im_u8 *plane;
    int16_t *coef;
} im_jc;

typedef struct {
    const im_u8 *p, *end;
    uint32_t bits;
    int nb, marker, pad;
} im_jr;

typedef struct {
    const im_u8 *d;
    size_t n;
    int w, h, nc, hmax, vmax, mcux, mcuy, ri, adobe, transform, orient, frame, scans, prog, ss, se, ah, al, eob;
    uint16_t q[4][64];
    int hasq[4];
    im_jh dc[4], ac[4];
    im_jc c[4];
    im_jr r;
} im_jpg;

static const im_u8 im_zz[64] = {0, 1, 8, 16, 9, 2, 3, 10, 17, 24, 32, 25, 18, 11, 4, 5, 12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6, 7, 14, 21, 28,
                                35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51, 58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63};

static int im_jh_build(im_jh *h, const im_u8 *cnt, const im_u8 *syms, int total)
{
    uint32_t code = 0;
    int k = 0;
    memset(h, 0, sizeof(*h));
    memcpy(h->sym, syms, (size_t)total);
    for (int l = 1; l <= 16; l++) {
        h->delta[l] = k - (int)code;
        for (int i = 0; i < cnt[l - 1]; i++, code++, k++) {
            if (code >= (1u << l)) return -1;
            if (l <= 9)
                for (uint32_t j = 0; j < (1u << (9 - l)); j++) h->fast[code << (9 - l) | j] = (uint16_t)(l << 8 | syms[k]);
        }
        h->maxc[l] = code << (16 - l);
        code <<= 1;
    }
    h->maxc[17] = 0xFFFFFFFFu;
    h->ok = 1;
    return 0;
}

static void im_jfill(im_jr *r)
{
    while (r->nb <= 24) {
        uint32_t b = 0;
        if (!r->marker && r->p < r->end) {
            b = *r->p;
            if (b == 0xFF) {
                const im_u8 *q = r->p + 1;
                while (q < r->end && *q == 0xFF) q++;
                if (q < r->end && *q == 0) r->p = q + 1;
                else r->marker = 1, b = 0, r->pad++;
            } else
                r->p++;
        } else
            r->pad++;
        r->bits |= b << (24 - r->nb);
        r->nb += 8;
    }
}

static inline int im_jsym(im_jr *r, const im_jh *h)
{
    if (r->nb < 16) im_jfill(r);
    uint32_t e = h->fast[r->bits >> 23];
    if (e) {
        int l = (int)(e >> 8);
        r->bits <<= l, r->nb -= l;
        return (int)(e & 255);
    }
    uint32_t p = r->bits >> 16;
    int l = 10;
    while (p >= h->maxc[l]) l++;
    if (l > 16) return -1;
    int i = (int)(p >> (16 - l)) + h->delta[l];
    if (i < 0 || i > 255) return -1;
    r->bits <<= l, r->nb -= l;
    return h->sym[i];
}

static inline int im_jget(im_jr *r, int s)
{
    if (!s) return 0;
    if (r->nb < s) im_jfill(r);
    int v = (int)(r->bits >> (32 - s));
    r->bits <<= s, r->nb -= s;
    if (v < (1 << (s - 1))) v -= (1 << s) - 1;
    return v;
}

static inline int im_jdq(int v, int q)
{
    v *= q;
    return v > 32767 ? 32767 : v < -32768 ? -32768 : v;
}

static void im_idct(const int *in, im_u8 *out, int stride)
{
    int ws[64];
    for (int c = 0; c < 8; c++) {
        const int *s = in + c;
        if (!(s[8] | s[16] | s[24] | s[32] | s[40] | s[48] | s[56])) {
            int v = s[0] * 4;
            for (int k = 0; k < 8; k++) ws[c + 8 * k] = v;
            continue;
        }
        int64_t z1, z2, z3, z4, z5, t0, t1, t2, t3, t10, t11, t12, t13;
        z2 = s[16], z3 = s[48];
        z1 = (z2 + z3) * 4433;
        t2 = z1 + z3 * -15137;
        t3 = z1 + z2 * 6270;
        t0 = ((int64_t)s[0] + s[32]) * 8192;
        t1 = ((int64_t)s[0] - s[32]) * 8192;
        t10 = t0 + t3, t13 = t0 - t3, t11 = t1 + t2, t12 = t1 - t2;
        t0 = s[56], t1 = s[40], t2 = s[24], t3 = s[8];
        z1 = t0 + t3, z2 = t1 + t2, z3 = t0 + t2, z4 = t1 + t3;
        z5 = (z3 + z4) * 9633;
        t0 *= 2446, t1 *= 16819, t2 *= 25172, t3 *= 12299;
        z1 *= -7373, z2 *= -20995, z3 *= -16069, z4 *= -3196;
        z3 += z5, z4 += z5;
        t0 += z1 + z3, t1 += z2 + z4, t2 += z2 + z3, t3 += z1 + z4;
        ws[c] = (int)((t10 + t3 + 1024) >> 11);
        ws[c + 56] = (int)((t10 - t3 + 1024) >> 11);
        ws[c + 8] = (int)((t11 + t2 + 1024) >> 11);
        ws[c + 48] = (int)((t11 - t2 + 1024) >> 11);
        ws[c + 16] = (int)((t12 + t1 + 1024) >> 11);
        ws[c + 40] = (int)((t12 - t1 + 1024) >> 11);
        ws[c + 24] = (int)((t13 + t0 + 1024) >> 11);
        ws[c + 32] = (int)((t13 - t0 + 1024) >> 11);
    }
    for (int y = 0; y < 8; y++) {
        const int *w = ws + y * 8;
        im_u8 *o = out + (size_t)y * stride;
        if (!(w[1] | w[2] | w[3] | w[4] | w[5] | w[6] | w[7])) {
            im_u8 v = (im_u8)im_clamp(((w[0] + 16) >> 5) + 128);
            memset(o, v, 8);
            continue;
        }
        int64_t z1, z2, z3, z4, z5, t0, t1, t2, t3, t10, t11, t12, t13;
        z2 = w[2], z3 = w[6];
        z1 = (z2 + z3) * 4433;
        t2 = z1 + z3 * -15137;
        t3 = z1 + z2 * 6270;
        t0 = ((int64_t)w[0] + w[4]) * 8192;
        t1 = ((int64_t)w[0] - w[4]) * 8192;
        t10 = t0 + t3, t13 = t0 - t3, t11 = t1 + t2, t12 = t1 - t2;
        t0 = w[7], t1 = w[5], t2 = w[3], t3 = w[1];
        z1 = t0 + t3, z2 = t1 + t2, z3 = t0 + t2, z4 = t1 + t3;
        z5 = (z3 + z4) * 9633;
        t0 *= 2446, t1 *= 16819, t2 *= 25172, t3 *= 12299;
        z1 *= -7373, z2 *= -20995, z3 *= -16069, z4 *= -3196;
        z3 += z5, z4 += z5;
        t0 += z1 + z3, t1 += z2 + z4, t2 += z2 + z3, t3 += z1 + z4;
        o[0] = (im_u8)im_clamp((int)((t10 + t3 + (1 << 17)) >> 18) + 128);
        o[7] = (im_u8)im_clamp((int)((t10 - t3 + (1 << 17)) >> 18) + 128);
        o[1] = (im_u8)im_clamp((int)((t11 + t2 + (1 << 17)) >> 18) + 128);
        o[6] = (im_u8)im_clamp((int)((t11 - t2 + (1 << 17)) >> 18) + 128);
        o[2] = (im_u8)im_clamp((int)((t12 + t1 + (1 << 17)) >> 18) + 128);
        o[5] = (im_u8)im_clamp((int)((t12 - t1 + (1 << 17)) >> 18) + 128);
        o[3] = (im_u8)im_clamp((int)((t13 + t0 + (1 << 17)) >> 18) + 128);
        o[4] = (im_u8)im_clamp((int)((t13 - t0 + (1 << 17)) >> 18) + 128);
    }
}

static int im_jblock(im_jpg *j, im_jc *c, int bx, int by)
{
    im_jr *r = &j->r;
    int blk[64];
    const uint16_t *q = j->q[c->tq];
    memset(blk, 0, sizeof(blk));
    int t = im_jsym(r, &j->dc[c->dc]);
    if (t < 0 || t > 15) return -1;
    c->pred = (int16_t)(uint16_t)((c->pred + im_jget(r, t)) & 0xFFFF);
    blk[0] = im_jdq(c->pred, q[0]);
    const im_jh *ac = &j->ac[c->ac];
    for (int k = 1; k < 64;) {
        int rs = im_jsym(r, ac);
        if (rs < 0) return -1;
        int run = rs >> 4, s = rs & 15;
        if (!s) {
            if (run != 15) break;
            k += 16;
            continue;
        }
        k += run;
        if (k > 63) return -1;
        blk[im_zz[k]] = im_jdq(im_jget(r, s), q[im_zz[k]]);
        k++;
    }
    im_idct(blk, c->plane + (size_t)by * 8 * c->stride + (size_t)bx * 8, c->stride);
    return 0;
}

static int im_jrestart(im_jpg *j)
{
    im_jr *r = &j->r;
    r->bits = 0, r->nb = 0, r->pad = 0;
    for (int i = 0; i < j->nc; i++) j->c[i].pred = 0;
    j->eob = 0;
    if (!r->marker) {
        while (r->p + 1 < r->end && !(r->p[0] == 0xFF && r->p[1] != 0 && r->p[1] != 0xFF)) r->p++;
        if (r->p + 1 >= r->end) {
            r->p = r->end;
            return 0;
        }
    }
    const im_u8 *q = r->p + 1;
    while (q < r->end && *q == 0xFF) q++;
    if (q < r->end && *q >= 0xD0 && *q <= 0xD7) {
        r->p = q + 1, r->marker = 0;
        return 1;
    }
    r->marker = 1;
    return 0;
}

// true once a block needed bits past the end of the scan data
static inline int im_jdry(const im_jr *r) { return r->pad * 8 > r->nb; }

static inline int16_t im_s16(int v) { return (int16_t)(uint16_t)((unsigned)v & 0xFFFF); }

static inline int im_jbits(im_jr *r, int n)
{
    if (r->nb < n) im_jfill(r);
    int v = (int)(r->bits >> (32 - n));
    r->bits <<= n, r->nb -= n;
    return v;
}

// one block of a progressive scan, same steps as libjpeg
static int im_jprog(im_jpg *j, im_jc *c, int bx, int by)
{
    im_jr *r = &j->r;
    int16_t *b = c->coef + ((size_t)by * (size_t)(c->stride / 8) + (size_t)bx) * 64;
    if (!j->ss) {
        if (!j->ah) {
            int t = im_jsym(r, &j->dc[c->dc]);
            if (t < 0 || t > 15) return -1;
            c->pred = im_s16(c->pred + im_jget(r, t));
            b[0] = im_s16(c->pred * (1 << j->al));
        } else if (im_jbits(r, 1))
            b[0] = im_s16(b[0] | (1 << j->al));
        return 0;
    }
    const im_jh *h = &j->ac[c->ac];
    int k = j->ss;
    if (!j->ah) {
        if (j->eob) {
            j->eob--;
            return 0;
        }
        for (; k <= j->se; k++) {
            int rs = im_jsym(r, h);
            if (rs < 0) return -1;
            int run = rs >> 4, sz = rs & 15;
            if (!sz) {
                if (run < 15) {
                    j->eob = (1 << run) - 1;
                    if (run) j->eob += im_jbits(r, run);
                    break;
                }
                k += 15;
                continue;
            }
            k += run;
            b[k > 63 ? 63 : im_zz[k]] = im_s16(im_jget(r, sz) * (1 << j->al));
        }
        return 0;
    }
    int p1 = 1 << j->al, m1 = -p1;
    if (!j->eob) {
        for (; k <= j->se; k++) {
            int rs = im_jsym(r, h);
            if (rs < 0) return -1;
            int run = rs >> 4, sz = rs & 15;
            if (sz) sz = im_jbits(r, 1) ? p1 : m1;
            else if (run != 15) {
                j->eob = 1 << run;
                if (run) j->eob += im_jbits(r, run);
                break;
            }
            do {
                int16_t *cf = &b[im_zz[k]];
                if (*cf) {
                    if (im_jbits(r, 1) && !(*cf & p1)) *cf = im_s16(*cf + (*cf >= 0 ? p1 : m1));
                } else if (--run < 0)
                    break;
                k++;
            } while (k <= j->se);
            if (sz) b[k > 63 ? 63 : im_zz[k]] = im_s16(sz);
        }
    }
    if (j->eob) {
        for (; k <= j->se; k++) {
            int16_t *cf = &b[im_zz[k]];
            if (*cf && im_jbits(r, 1) && !(*cf & p1)) *cf = im_s16(*cf + (*cf >= 0 ? p1 : m1));
        }
        j->eob--;
    }
    return 0;
}

static int im_jdo(im_jpg *j, im_jc *c, int bx, int by) { return j->prog ? im_jprog(j, c, bx, by) : im_jblock(j, c, bx, by); }

static int im_jplanes(im_jpg *j)
{
    for (int i = 0; i < j->nc; i++) {
        im_jc *c = &j->c[i];
        c->plane = malloc((size_t)c->stride * (size_t)c->rows);
        if (!c->plane) return im_fail(GOOME_ENOMEM, "out of memory");
        memset(c->plane, 128, (size_t)c->stride * (size_t)c->rows);
    }
    return 0;
}

// progressive: coefficients only once a scan shows up, every block needs at least a bit of dc
static int im_jcoefs(im_jpg *j, size_t rest)
{
    uint64_t blocks = 0, total = 0;
    for (int i = 0; i < j->nc; i++) {
        im_jc *c = &j->c[i];
        blocks += (uint64_t)((c->cw + 7) / 8) * (uint64_t)((c->ch + 7) / 8);
        total += (uint64_t)c->stride * (uint64_t)c->rows;
    }
    if (blocks > (uint64_t)rest * 8 + 64) return im_fail(GOOME_EARG, "jpeg is too short for its size");
    if (total > IM_MAXCOEF || total * 2 > (uint64_t)(size_t)-1) return im_fail(GOOME_ENOMEM, "progressive jpeg too big to decode");
    for (int i = 0; i < j->nc; i++) {
        im_jc *c = &j->c[i];
        c->coef = calloc((size_t)c->stride * (size_t)c->rows, 2);
        if (!c->coef) return im_fail(GOOME_ENOMEM, "out of memory");
    }
    return 0;
}

static int im_jfinish(im_jpg *j)
{
    int rc = im_jplanes(j);
    if (rc) return rc;
    for (int i = 0; i < j->nc; i++) {
        im_jc *c = &j->c[i];
        const uint16_t *q = c->hasq ? c->q : j->q[c->tq];
        int bw = c->stride / 8, bh = c->rows / 8, blk[64];
        for (int by = 0; by < bh; by++)
            for (int bx = 0; bx < bw; bx++) {
                const int16_t *cf = c->coef + ((size_t)by * bw + bx) * 64;
                for (int k = 0; k < 64; k++) blk[k] = im_jdq(cf[k], q[k]);
                im_idct(blk, c->plane + (size_t)by * 8 * c->stride + (size_t)bx * 8, c->stride);
            }
    }
    return 0;
}

static int im_jscan(im_jpg *j, const im_u8 *s, size_t len, size_t start)
{
    if (!j->frame) return im_fail(GOOME_EARG, "jpeg scan before frame");
    if (len < 1) return im_fail(GOOME_EARG, "jpeg scan header cut off");
    int ns = s[0];
    if (ns < 1 || ns > 4 || len < (size_t)(1 + ns * 2 + 3)) return im_fail(GOOME_EARG, "jpeg scan header is bad");
    im_jc *sc[4];
    for (int i = 0; i < ns; i++) {
        int id = s[1 + i * 2], tt = s[2 + i * 2], k;
        for (k = 0; k < j->nc && j->c[k].id != id; k++) {}
        if (k == j->nc) return im_fail(GOOME_EARG, "jpeg scan uses a missing component");
        for (int m = 0; m < i; m++)
            if (sc[m] == &j->c[k]) return im_fail(GOOME_EARG, "jpeg scan repeats a component");
        sc[i] = &j->c[k];
        sc[i]->dc = tt >> 4, sc[i]->ac = tt & 15;
        sc[i]->pred = 0;
    }
    const im_u8 *sp = s + 1 + ns * 2;
    j->ss = sp[0], j->se = sp[1], j->ah = sp[2] >> 4, j->al = sp[2] & 15, j->eob = 0;
    if (j->prog) {
        if (j->ss > 63 || j->se > 63 || j->al > 13 || (j->ah && j->al != j->ah - 1) || (j->ss == 0 && j->se != 0) || (j->ss > 0 && (j->se < j->ss || ns != 1)))
            return im_fail(GOOME_EARG, "jpeg progressive scan is bad");
        if (!j->c[0].coef) {
            int rc = im_jcoefs(j, j->n - start);
            if (rc) return rc;
        }
    }
    for (int i = 0; i < ns; i++) {
        int needdc = !j->prog || (j->ss == 0 && j->ah == 0), needac = !j->prog || j->ss > 0;
        if (sc[i]->dc > 3 || sc[i]->ac > 3 || (needdc && !j->dc[sc[i]->dc].ok) || (needac && !j->ac[sc[i]->ac].ok)) return im_fail(GOOME_EARG, "jpeg huffman table missing");
        if (!j->hasq[sc[i]->tq]) return im_fail(GOOME_EARG, "jpeg quant table missing");
        if (j->prog && !sc[i]->hasq) memcpy(sc[i]->q, j->q[sc[i]->tq], sizeof(sc[i]->q)), sc[i]->hasq = 1;
    }
    if (ns > 1) {
        int sum = 0;
        for (int i = 0; i < ns; i++) sum += sc[i]->h * sc[i]->v;
        if (sum > 10) return im_fail(GOOME_EARG, "jpeg mcu too big");
    }
    im_jr *r = &j->r;
    r->p = j->d + start, r->end = j->d + j->n, r->bits = 0, r->nb = 0, r->marker = 0, r->pad = 0;
    int left = j->ri, dead = 0, one = ns == 1;
    int bw = one ? (sc[0]->cw + 7) / 8 : j->mcux, bh = one ? (sc[0]->ch + 7) / 8 : j->mcuy;
    for (int my = 0; my < bh; my++)
        for (int mx = 0; mx < bw; mx++) {
            if (j->ri && !left--) {
                if (!im_jrestart(j)) return 0;
                left = j->ri - 1, dead = 0;
            }
            if (dead) continue;
            if (one) {
                if (im_jdo(j, sc[0], mx, my)) return 1;
            } else
                for (int i = 0; i < ns; i++) {
                    im_jc *c = sc[i];
                    for (int yy = 0; yy < c->v; yy++)
                        for (int xx = 0; xx < c->h; xx++)
                            if (im_jdo(j, c, mx * c->h + xx, my * c->v + yy)) return 1;
                }
            if (im_jdry(r)) {
                if (!j->ri) return 0;
                dead = 1;
            }
        }
    return 0;
}

static int im_jframe(im_jpg *j, const im_u8 *s, size_t len, size_t rest, int prog)
{
    if (j->frame) return im_fail(GOOME_EARG, "jpeg has two frames");
    if (len < 6) return im_fail(GOOME_EARG, "jpeg frame header cut off");
    if (s[0] != 8) return im_fail(GOOME_ENOTSUP, "only 8 bit jpeg is supported");
    j->h = (int)im_be16(s + 1), j->w = (int)im_be16(s + 3), j->nc = s[5];
    if (!j->h) return im_fail(GOOME_ENOTSUP, "jpeg with the height in a dnl marker is not supported");
    if (!j->w || j->w > IM_MAXDIM || j->h > IM_MAXDIM) return im_fail(GOOME_EARG, "jpeg size out of range");
    if (j->nc < 1 || j->nc > 4 || len < (size_t)(6 + j->nc * 3)) return im_fail(GOOME_EARG, "jpeg frame header is bad");
    j->hmax = j->vmax = 1;
    for (int i = 0; i < j->nc; i++) {
        im_jc *c = &j->c[i];
        c->id = s[6 + i * 3], c->h = s[7 + i * 3] >> 4, c->v = s[7 + i * 3] & 15, c->tq = s[8 + i * 3];
        if (c->h < 1 || c->h > 4 || c->v < 1 || c->v > 4 || c->tq > 3) return im_fail(GOOME_EARG, "jpeg frame header is bad");
        if (c->h > j->hmax) j->hmax = c->h;
        if (c->v > j->vmax) j->vmax = c->v;
    }
    j->mcux = (j->w + 8 * j->hmax - 1) / (8 * j->hmax);
    j->mcuy = (j->h + 8 * j->vmax - 1) / (8 * j->vmax);
    uint64_t blocks = 0, bytes = 0;
    for (int i = 0; i < j->nc; i++) {
        im_jc *c = &j->c[i];
        c->cw = (j->w * c->h + j->hmax - 1) / j->hmax;
        c->ch = (j->h * c->v + j->vmax - 1) / j->vmax;
        c->stride = j->mcux * c->h * 8, c->rows = j->mcuy * c->v * 8;
        blocks += (uint64_t)((c->cw + 7) / 8) * (uint64_t)((c->ch + 7) / 8);
        bytes += (uint64_t)c->stride * (uint64_t)c->rows;
    }
    if (!prog && blocks > (uint64_t)rest * 4 + 64) return im_fail(GOOME_EARG, "jpeg is too short for its size");
    if (bytes > IM_MAXRAW || bytes > (uint64_t)(size_t)-1) return im_fail(GOOME_ENOMEM, "jpeg too big to decode");
    j->prog = prog;
    if (!prog) {
        int rc = im_jplanes(j);
        if (rc) return rc;
    }
    j->frame = 1;
    return 0;
}

static void im_jexif(im_jpg *j, const im_u8 *s, size_t len)
{
    if (len < 14 || memcmp(s, "Exif\0\0", 6)) return;
    const im_u8 *t = s + 6;
    size_t n = len - 6;
    int le = t[0] == 'I' && t[1] == 'I';
    if (!le && !(t[0] == 'M' && t[1] == 'M')) return;
    uint32_t off = le ? im_le32(t + 4) : im_be32(t + 4);
    if (off > n || n - off < 2) return;
    uint32_t cnt = le ? im_le16(t + off) : im_be16(t + off);
    for (uint32_t i = 0; i < cnt; i++) {
        size_t e = (size_t)off + 2 + (size_t)i * 12;
        if (e + 12 > n) return;
        uint32_t tag = le ? im_le16(t + e) : im_be16(t + e);
        if (tag == 0x112) {
            uint32_t v = le ? im_le16(t + e + 8) : im_be16(t + e + 8);
            if (v >= 1 && v <= 8) j->orient = (int)v;
            return;
        }
    }
}

static const im_u8 *im_up_row(const im_jpg *j, const im_jc *c, int y, im_u8 *o)
{
    int hs = j->hmax / c->h, vs = j->vmax / c->v, w = c->cw;
    int exact = hs * c->h == j->hmax && vs * c->v == j->vmax;
    if (exact && hs == 1 && vs == 1) return c->plane + (size_t)y * c->stride;
    if (exact && hs <= 2 && vs <= 2) {
        int cy = y / vs;
        const im_u8 *a = c->plane + (size_t)cy * c->stride;
        if (vs == 2) {
            int oy = y & 1 ? cy + 1 : cy - 1;
            if (oy < 0) oy = 0;
            if (oy > c->ch - 1) oy = c->ch - 1;
            const im_u8 *b = c->plane + (size_t)oy * c->stride;
            if (hs == 1) {
                int bias = y & 1 ? 2 : 1;
                for (int x = 0; x < j->w; x++) o[x] = (im_u8)((a[x] * 3 + b[x] + bias) >> 2);
                return o;
            }
            int t = a[0] * 3 + b[0], nx = a[1] * 3 + b[1], l;
            o[0] = (im_u8)((t * 4 + 8) >> 4);
            o[1] = (im_u8)((t * 3 + nx + 7) >> 4);
            l = t, t = nx;
            int k = 2;
            for (int x = 2; x < w; x++, k += 2) {
                nx = a[x] * 3 + b[x];
                o[k] = (im_u8)((t * 3 + l + 8) >> 4);
                o[k + 1] = (im_u8)((t * 3 + nx + 7) >> 4);
                l = t, t = nx;
            }
            o[k] = (im_u8)((t * 3 + l + 8) >> 4);
            o[k + 1] = (im_u8)((t * 4 + 7) >> 4);
            return o;
        }
        if (w == 1) {
            o[0] = o[1] = a[0];
            return o;
        }
        o[0] = a[0];
        o[1] = (im_u8)((a[0] * 3 + a[1] + 2) >> 2);
        int k = 2;
        for (int x = 1; x < w - 1; x++, k += 2) {
            o[k] = (im_u8)((a[x] * 3 + a[x - 1] + 1) >> 2);
            o[k + 1] = (im_u8)((a[x] * 3 + a[x + 1] + 2) >> 2);
        }
        o[k] = (im_u8)((a[w - 1] * 3 + a[w - 2] + 1) >> 2);
        o[k + 1] = a[w - 1];
        return o;
    }
    int cy = (int)((int64_t)y * c->v / j->vmax);
    if (cy > c->rows - 1) cy = c->rows - 1;
    const im_u8 *a = c->plane + (size_t)cy * c->stride;
    for (int x = 0; x < j->w; x++) o[x] = a[(int64_t)x * c->h / j->hmax];
    return o;
}

static int im_jconvert(im_jpg *j, goome_bitmap *out)
{
    int rc = im_alloc(out, j->w, j->h);
    if (rc) return rc;
    size_t rw = (size_t)j->w + 64;
    im_u8 *rows = malloc(rw * (size_t)j->nc);
    if (!rows) {
        goome_bitmap_free(out);
        return im_fail(GOOME_ENOMEM, "out of memory");
    }
    int crr[256], cbb[256], crg[256], cbg[256];
    im_u8 lim[1024];
    for (int i = 0; i < 1024; i++) lim[i] = (im_u8)im_clamp(i - 384);
    for (int i = 0; i < 256; i++) {
        int x = i - 128;
        crr[i] = (int)(((int64_t)91881 * x + 32768) >> 16);
        cbb[i] = (int)(((int64_t)116130 * x + 32768) >> 16);
        crg[i] = -46802 * x;
        cbg[i] = -22554 * x + 32768;
    }
    int rgb = j->nc == 3 && ((j->adobe && j->transform == 0) || (j->c[0].id == 'R' && j->c[1].id == 'G' && j->c[2].id == 'B'));
    int ycck = j->nc == 4 && j->adobe && j->transform == 2;
    for (int y = 0; y < j->h; y++) {
        const im_u8 *pl[4];
        for (int i = 0; i < j->nc; i++) pl[i] = im_up_row(j, &j->c[i], y, rows + rw * i);
        const im_u8 *a = pl[0], *b = pl[j->nc > 1], *c = pl[j->nc > 2 ? 2 : 0], *k = pl[j->nc > 3 ? 3 : 0];
        uint32_t *o = out->px + (size_t)y * j->w;
        if (j->nc < 3)
            for (int x = 0; x < j->w; x++) o[x] = 0xFF000000u | (uint32_t)a[x] * 0x010101u;
        else if (rgb)
            for (int x = 0; x < j->w; x++) o[x] = 0xFF000000u | (uint32_t)a[x] << 16 | (uint32_t)b[x] << 8 | c[x];
        else if (j->nc == 3 || ycck) {
            for (int x = 0; x < j->w; x++) {
                int Y = a[x] + 384, cb = b[x], cr = c[x];
                uint32_t R = lim[Y + crr[cr]], G = lim[Y + ((cbg[cb] + crg[cr]) >> 16)], B = lim[Y + cbb[cb]];
                if (ycck) R = im_mul(255 - R, k[x]), G = im_mul(255 - G, k[x]), B = im_mul(255 - B, k[x]);
                o[x] = 0xFF000000u | R << 16 | G << 8 | B;
            }
        } else
            for (int x = 0; x < j->w; x++) {
                uint32_t C = a[x], M = b[x], Y = c[x], K = k[x];
                if (!j->adobe) C = 255 - C, M = 255 - M, Y = 255 - Y, K = 255 - K;
                o[x] = 0xFF000000u | im_mul(C, K) << 16 | im_mul(M, K) << 8 | im_mul(Y, K);
            }
    }
    free(rows);
    return GOOME_OK;
}

static int im_orient(goome_bitmap *b, int o)
{
    if (o <= 1 || o > 8) return GOOME_OK;
    int w = b->w, h = b->h, sw = o >= 5;
    int nw = sw ? h : w, nh = sw ? w : h;
    uint32_t *n = malloc((size_t)w * h * 4);
    if (!n) return im_fail(GOOME_ENOMEM, "out of memory");
    for (int y = 0; y < nh; y++)
        for (int x = 0; x < nw; x++) {
            int sx, sy;
            switch (o) {
            case 2: sx = w - 1 - x, sy = y; break;
            case 3: sx = w - 1 - x, sy = h - 1 - y; break;
            case 4: sx = x, sy = h - 1 - y; break;
            case 5: sx = y, sy = x; break;
            case 6: sx = y, sy = h - 1 - x; break;
            case 7: sx = w - 1 - y, sy = h - 1 - x; break;
            default: sx = w - 1 - y, sy = x; break;
            }
            n[(size_t)y * nw + x] = b->px[(size_t)sy * w + sx];
        }
    free(b->px);
    b->px = n, b->w = nw, b->h = nh, b->stride = nw;
    return GOOME_OK;
}

static int im_jpg_load(const im_u8 *d, size_t n, goome_bitmap *out)
{
    im_jpg *j = calloc(1, sizeof(im_jpg));
    if (!j) return im_fail(GOOME_ENOMEM, "out of memory");
    j->d = d, j->n = n;
    int rc = 0;
    size_t pos = 2;
    for (;;) {
        while (pos < n && d[pos] != 0xFF) pos++;
        while (pos < n && d[pos] == 0xFF) pos++;
        if (pos >= n) break;
        int m = d[pos++];
        if (m == 0xD9) break;
        if (m == 0 || m == 1 || (m >= 0xD0 && m <= 0xD8)) continue;
        if (n - pos < 2) break;
        size_t len = im_be16(d + pos);
        if (len < 2 || len > n - pos) {
            if (j->scans) break;
            rc = im_fail(GOOME_EARG, "jpeg segment cut off");
            goto done;
        }
        const im_u8 *s = d + pos + 2;
        len -= 2;
        pos += 2 + len;
        if (m == 0xC0 || m == 0xC1 || m == 0xC2) {
            if ((rc = im_jframe(j, s, len, n - pos, m == 0xC2))) goto done;
        } else if (m == 0xC6 || m == 0xCA || m == 0xCE) {
            rc = im_fail(GOOME_ENOTSUP, m == 0xC6 ? "hierarchical jpeg is not supported" : "arithmetic coded jpeg is not supported");
            goto done;
        } else if (m == 0xC3 || m == 0xC7 || m == 0xCB || m == 0xCF) {
            rc = im_fail(GOOME_ENOTSUP, "lossless jpeg is not supported");
            goto done;
        } else if (m == 0xC5 || m == 0xC9 || m == 0xCD || m == 0xCC) {
            rc = im_fail(GOOME_ENOTSUP, m == 0xC5 ? "hierarchical jpeg is not supported" : "arithmetic coded jpeg is not supported");
            goto done;
        } else if (m == 0xC4) {
            size_t i = 0;
            while (i + 17 <= len) {
                int tc = s[i] >> 4, th = s[i] & 15, total = 0;
                for (int k = 0; k < 16; k++) total += s[i + 1 + k];
                if (tc > 1 || th > 3 || total > 256 || i + 17 + (size_t)total > len || im_jh_build(tc ? &j->ac[th] : &j->dc[th], s + i + 1, s + i + 17, total)) {
                    rc = im_fail(GOOME_EARG, "jpeg huffman table is bad");
                    goto done;
                }
                i += 17 + (size_t)total;
            }
        } else if (m == 0xDB) {
            size_t i = 0;
            while (i < len) {
                int pq = s[i] >> 4, tq = s[i] & 15;
                size_t sz = pq ? 128 : 64;
                if (pq > 1 || tq > 3 || i + 1 + sz > len) {
                    rc = im_fail(GOOME_EARG, "jpeg quant table is bad");
                    goto done;
                }
                for (int k = 0; k < 64; k++) j->q[tq][im_zz[k]] = (uint16_t)(pq ? im_be16(s + i + 1 + k * 2) : s[i + 1 + k]);
                j->hasq[tq] = 1;
                i += 1 + sz;
            }
        } else if (m == 0xDD) {
            if (len >= 2) j->ri = (int)im_be16(s);
        } else if (m == 0xDA) {
            if (j->scans >= 256) {
                rc = im_fail(GOOME_EARG, "jpeg has too many scans");
                goto done;
            }
            int r = im_jscan(j, s, len, pos);
            if (r < 0) {
                rc = r;
                goto done;
            }
            j->scans++;
            while (pos + 1 < n && !(d[pos] == 0xFF && d[pos + 1] != 0 && d[pos + 1] != 0xFF && !(d[pos + 1] >= 0xD0 && d[pos + 1] <= 0xD7))) pos++;
        } else if (m == 0xE1) {
            if (!j->orient) im_jexif(j, s, len);
        } else if (m == 0xEE) {
            if (len >= 12 && !memcmp(s, "Adobe", 5)) j->adobe = 1, j->transform = s[11];
        }
    }
    if (!j->frame || !j->scans) {
        rc = im_fail(GOOME_EARG, "jpeg has no image data");
        goto done;
    }
    if (j->prog && (rc = im_jfinish(j))) goto done;
    if ((rc = im_jconvert(j, out))) goto done;
    if ((rc = im_orient(out, j->orient))) goome_bitmap_free(out);
done:
    for (int i = 0; i < 4; i++) free(j->c[i].plane), free(j->c[i].coef);
    free(j);
    return rc;
}

static void im_mask(uint32_t m, int *sh, uint32_t *max)
{
    *sh = 0, *max = 0;
    if (!m) return;
    while (!(m & 1)) m >>= 1, (*sh)++;
    *max = m;
}

static inline uint32_t im_chan(uint32_t p, int sh, uint32_t max)
{
    if (!max) return 0;
    uint32_t v = (p >> sh) & max;
    return max == 255 ? v : (uint32_t)((uint64_t)v * 255 / max);
}

static int im_bmp_load(const im_u8 *d, size_t n, goome_bitmap *out)
{
    if (n < 26) return im_fail(GOOME_EARG, "bmp cut off");
    uint32_t off = im_le32(d + 10), hs = im_le32(d + 14);
    int64_t w, h;
    int bpp, comp = 0, top = 0;
    uint32_t used = 0, mk[4] = {0, 0, 0, 0};
    int hasmask = 0;
    if (hs == 12) {
        w = im_le16(d + 18), h = im_le16(d + 20), bpp = (int)im_le16(d + 24);
    } else {
        if (hs < 40 || hs > n - 14) return im_fail(GOOME_EARG, "bmp header is bad");
        w = (int32_t)im_le32(d + 18), h = (int32_t)im_le32(d + 22);
        bpp = (int)im_le16(d + 28), comp = (int)im_le32(d + 30), used = im_le32(d + 46);
        if (comp == 3 || comp == 6) {
            size_t mo = 54, nm = comp == 6 || hs >= 56 ? 4 : 3;
            if (mo + nm * 4 > n) return im_fail(GOOME_EARG, "bmp cut off");
            for (size_t i = 0; i < nm; i++) mk[i] = im_le32(d + mo + i * 4);
            hasmask = 1;
        }
    }
    if (h < 0) h = -h, top = 1;
    if (w <= 0 || h <= 0 || w > IM_MAXDIM || h > IM_MAXDIM) return im_fail(GOOME_EARG, "bmp size out of range");
    if (comp == 1 || comp == 2) return im_fail(GOOME_ENOTSUP, "rle bmp is not supported");
    if (comp == 4 || comp == 5) return im_fail(GOOME_ENOTSUP, "bmp with jpeg or png inside is not supported");
    if (comp != 0 && comp != 3 && comp != 6) return im_fail(GOOME_EARG, "bmp compression is unknown");
    if (bpp != 1 && bpp != 4 && bpp != 8 && bpp != 16 && bpp != 24 && bpp != 32) return im_fail(GOOME_EARG, "bmp bit depth is bad");
    if (hasmask && bpp != 16 && bpp != 32) return im_fail(GOOME_EARG, "bmp bitfields need 16 or 32 bit");
    size_t stride = (((size_t)w * bpp + 31) / 32) * 4, used_row = ((size_t)w * bpp + 7) / 8;
    if (off > n || (size_t)(h - 1) * stride + used_row > n - off) return im_fail(GOOME_EARG, "bmp cut off");
    uint32_t pal[256];
    if (bpp <= 8) {
        size_t ps = hs == 12 ? 3 : 4, po = 14 + hs, np = used && used < (1u << bpp) ? used : 1u << bpp;
        for (size_t i = 0; i < 256; i++) {
            size_t e = po + i * ps;
            pal[i] = i < np && e + 3 <= n && e + 3 <= off ? 0xFF000000u | (uint32_t)d[e + 2] << 16 | (uint32_t)d[e + 1] << 8 | d[e] : 0xFF000000u;
        }
    }
    if (!hasmask) {
        if (bpp == 16) mk[0] = 0x7C00, mk[1] = 0x3E0, mk[2] = 0x1F, mk[3] = 0;
        if (bpp == 32) mk[0] = 0xFF0000, mk[1] = 0xFF00, mk[2] = 0xFF, mk[3] = 0;
    }
    int sh[4];
    uint32_t mx[4];
    for (int i = 0; i < 4; i++) im_mask(mk[i], &sh[i], &mx[i]);
    int alpha = mx[3] != 0;
    if (bpp == 32 && !hasmask) {
        for (int64_t y = 0; y < h && !alpha; y++)
            for (int64_t x = 0; x < w; x++)
                if (d[off + (size_t)y * stride + (size_t)x * 4 + 3]) {
                    alpha = 1, mk[3] = 0xFF000000u;
                    im_mask(mk[3], &sh[3], &mx[3]);
                    break;
                }
    }
    int rc = im_alloc(out, (int)w, (int)h);
    if (rc) return rc;
    for (int64_t y = 0; y < h; y++) {
        const im_u8 *s = d + off + (size_t)(top ? y : h - 1 - y) * stride;
        uint32_t *o = out->px + (size_t)y * (size_t)w;
        for (int64_t x = 0; x < w; x++) {
            uint32_t c;
            if (bpp == 1) c = pal[s[x >> 3] >> (7 - (x & 7)) & 1];
            else if (bpp == 4) c = pal[s[x >> 1] >> (x & 1 ? 0 : 4) & 15];
            else if (bpp == 8) c = pal[s[x]];
            else if (bpp == 24) c = 0xFF000000u | (uint32_t)s[x * 3 + 2] << 16 | (uint32_t)s[x * 3 + 1] << 8 | s[x * 3];
            else {
                uint32_t p = bpp == 16 ? im_le16(s + x * 2) : im_le32(s + x * 4);
                uint32_t a = alpha ? im_chan(p, sh[3], mx[3]) : 255;
                c = im_px(im_chan(p, sh[0], mx[0]), im_chan(p, sh[1], mx[1]), im_chan(p, sh[2], mx[2]), a);
            }
            o[x] = c;
        }
    }
    return GOOME_OK;
}

// ppm / pgm / pbm, binary only

static int im_pnum(const im_u8 *d, size_t n, size_t *pos, uint32_t *v)
{
    size_t p = *pos;
    for (;;) {
        while (p < n && (d[p] == ' ' || d[p] == '\t' || d[p] == '\r' || d[p] == '\n' || d[p] == '\v' || d[p] == '\f')) p++;
        if (p < n && d[p] == '#') {
            while (p < n && d[p] != '\n' && d[p] != '\r') p++;
            continue;
        }
        break;
    }
    if (p >= n || d[p] < '0' || d[p] > '9') return -1;
    uint32_t r = 0;
    while (p < n && d[p] >= '0' && d[p] <= '9') {
        r = r * 10 + (uint32_t)(d[p++] - '0');
        if (r > 1000000) return -1;
    }
    *pos = p, *v = r;
    return 0;
}

static int im_ppm_load(const im_u8 *d, size_t n, goome_bitmap *out)
{
    int t = d[1] - '0';
    size_t pos = 2;
    uint32_t w, h, mv = 1;
    if (t < 4 || t > 6) return im_fail(GOOME_ENOTSUP, "only binary ppm, pgm and pbm are supported");
    if (im_pnum(d, n, &pos, &w) || im_pnum(d, n, &pos, &h) || (t != 4 && im_pnum(d, n, &pos, &mv))) return im_fail(GOOME_EARG, "ppm header is bad");
    if (pos >= n) return im_fail(GOOME_EARG, "ppm cut off");
    pos++;
    if (!w || !h || w > IM_MAXDIM || h > IM_MAXDIM) return im_fail(GOOME_EARG, "ppm size out of range");
    if (!mv || mv > 65535) return im_fail(GOOME_EARG, "ppm maxval is bad");
    int ch = t == 6 ? 3 : 1, bs = mv > 255 ? 2 : 1;
    size_t rb = t == 4 ? (w + 7) / 8 : (size_t)w * ch * bs;
    if ((uint64_t)rb * h > n - pos) return im_fail(GOOME_EARG, "ppm cut off");
    int rc = im_alloc(out, (int)w, (int)h);
    if (rc) return rc;
    for (uint32_t y = 0; y < h; y++) {
        const im_u8 *s = d + pos + (size_t)y * rb;
        uint32_t *o = out->px + (size_t)y * w;
        for (uint32_t x = 0; x < w; x++) {
            if (t == 4) {
                o[x] = s[x >> 3] >> (7 - (x & 7)) & 1 ? 0xFF000000u : 0xFFFFFFFFu;
                continue;
            }
            uint32_t c[3];
            for (int k = 0; k < ch; k++) {
                uint32_t v = bs == 2 ? im_be16(s + ((size_t)x * ch + k) * 2) : s[(size_t)x * ch + k];
                if (v > mv) v = mv;
                uint32_t q = v * 255 / mv, r = v * 255 % mv;
                c[k] = q + (2 * r > mv || (2 * r == mv && (q & 1)));
            }
            if (ch == 1) c[1] = c[2] = c[0];
            o[x] = 0xFF000000u | c[0] << 16 | c[1] << 8 | c[2];
        }
    }
    return GOOME_OK;
}

static int im_qoi_load(const im_u8 *d, size_t n, goome_bitmap *out)
{
    if (n < 14) return im_fail(GOOME_EARG, "qoi cut off");
    uint32_t w = im_be32(d + 4), h = im_be32(d + 8);
    int chans = d[12];
    if (!w || !h || w > IM_MAXDIM || h > IM_MAXDIM) return im_fail(GOOME_EARG, "qoi size out of range");
    if (chans != 3 && chans != 4) return im_fail(GOOME_EARG, "qoi header is bad");
    if ((uint64_t)w * h > (uint64_t)(n - 14) * 62) return im_fail(GOOME_EARG, "qoi is too short for its size");
    int rc = im_alloc(out, (int)w, (int)h);
    if (rc) return rc;
    im_u8 idx[64][4], px[4] = {0, 0, 0, 255};
    memset(idx, 0, sizeof(idx));
    size_t p = 14, total = (size_t)w * h;
    int run = 0;
    for (size_t i = 0; i < total; i++) {
        if (run) run--;
        else {
            if (p >= n) {
                goome_bitmap_free(out);
                return im_fail(GOOME_EARG, "qoi cut off");
            }
            int b = d[p++];
            if (b == 0xFE || b == 0xFF) {
                size_t k = b == 0xFE ? 3 : 4;
                if (n - p < k) {
                    goome_bitmap_free(out);
                    return im_fail(GOOME_EARG, "qoi cut off");
                }
                memcpy(px, d + p, k);
                p += k;
            } else if ((b >> 6) == 0)
                memcpy(px, idx[b], 4);
            else if ((b >> 6) == 1) {
                px[0] = (im_u8)(px[0] + ((b >> 4) & 3) - 2);
                px[1] = (im_u8)(px[1] + ((b >> 2) & 3) - 2);
                px[2] = (im_u8)(px[2] + (b & 3) - 2);
            } else if ((b >> 6) == 2) {
                if (p >= n) {
                    goome_bitmap_free(out);
                    return im_fail(GOOME_EARG, "qoi cut off");
                }
                int b2 = d[p++], dg = (b & 63) - 32;
                px[0] = (im_u8)(px[0] + dg - 8 + ((b2 >> 4) & 15));
                px[1] = (im_u8)(px[1] + dg);
                px[2] = (im_u8)(px[2] + dg - 8 + (b2 & 15));
            } else
                run = b & 63;
            memcpy(idx[(px[0] * 3 + px[1] * 5 + px[2] * 7 + px[3] * 11) & 63], px, 4);
        }
        out->px[i] = im_px(px[0], px[1], px[2], chans == 3 ? 255 : px[3]);
    }
    return GOOME_OK;
}

int goome_bitmap_load_mem(const void *data, size_t n, goome_bitmap *out)
{
    if (!out) return im_fail(GOOME_EARG, "no bitmap given");
    memset(out, 0, sizeof(*out));
    if (!data) return im_fail(GOOME_EARG, "no data given");
    const im_u8 *d = data;
    im_why = "";
    if (n >= 8 && !memcmp(d, "\x89PNG\r\n\x1a\n", 8)) return im_png_load(d, n, out);
    if (n >= 3 && d[0] == 0xFF && d[1] == 0xD8 && d[2] == 0xFF) return im_jpg_load(d, n, out);
    if (n >= 2 && d[0] == 'B' && d[1] == 'M') return im_bmp_load(d, n, out);
    if (n >= 4 && !memcmp(d, "qoif", 4)) return im_qoi_load(d, n, out);
    if (n >= 3 && d[0] == 'P' && d[1] >= '1' && d[1] <= '7' && (d[2] == ' ' || d[2] == '\n' || d[2] == '\r' || d[2] == '\t')) return im_ppm_load(d, n, out);
    if (n >= 6 && (!memcmp(d, "GIF87a", 6) || !memcmp(d, "GIF89a", 6))) return im_fail(GOOME_ENOTSUP, "gif is not supported");
    if (n >= 12 && !memcmp(d, "RIFF", 4) && !memcmp(d + 8, "WEBP", 4)) return im_fail(GOOME_ENOTSUP, "webp is not supported");
    return im_fail(GOOME_ENOTSUP, "unknown image format");
}

static FILE *im_open(const char *path)
{
#if defined(_WIN32)
    int n = MultiByteToWideChar(CP_UTF8, 0, path, -1, NULL, 0);
    if (n <= 0) return NULL;
    wchar_t *w = malloc((size_t)n * sizeof(wchar_t));
    if (!w) return NULL;
    MultiByteToWideChar(CP_UTF8, 0, path, -1, w, n);
    FILE *f = _wfopen(w, L"rb");
    free(w);
    return f;
#else
    return fopen(path, "rb");
#endif
}

int goome_bitmap_load(const char *path, goome_bitmap *out)
{
    if (!out) return im_fail(GOOME_EARG, "no bitmap given");
    memset(out, 0, sizeof(*out));
    if (!path) return im_fail(GOOME_EARG, "no path given");
    FILE *f = im_open(path);
    if (!f) return im_fail(GOOME_EARG, "cant open the file");
    size_t n = 0, cap = 0;
    im_u8 *buf = NULL;
    for (;;) {
        if (n == cap) {
            if (cap > IM_MAXFILE) {
                free(buf);
                fclose(f);
                return im_fail(GOOME_EARG, "file is bigger than 256 MB");
            }
            size_t nc = cap ? cap * 2 : 65536;
            im_u8 *b = realloc(buf, nc);
            if (!b) {
                free(buf);
                fclose(f);
                return im_fail(GOOME_ENOMEM, "out of memory");
            }
            buf = b, cap = nc;
        }
        size_t got = fread(buf + n, 1, cap - n, f);
        if (!got) break;
        n += got;
    }
    int bad = ferror(f);
    fclose(f);
    if (bad) {
        free(buf);
        return im_fail(GOOME_EARG, "cant read the file");
    }
    int rc = goome_bitmap_load_mem(buf, n, out);
    free(buf);
    return rc;
}

int goome_bitmap_from_argb(const uint32_t *argb, int w, int h, goome_bitmap *out)
{
    if (!out) return GOOME_EARG;
    memset(out, 0, sizeof(*out));
    if (!argb || w <= 0 || h <= 0) return GOOME_EARG;
    if ((size_t)w > ((size_t)-1) / 4 / (size_t)h) return GOOME_ENOMEM;
    out->px = malloc((size_t)w * h * 4);
    if (!out->px) return GOOME_ENOMEM;
    for (size_t i = 0; i < (size_t)w * h; i++) {
        uint32_t v = argb[i], a = v >> 24;
        out->px[i] = im_px(v >> 16 & 255, v >> 8 & 255, v & 255, a);
    }
    out->w = w, out->h = h, out->stride = w, out->owned = 1;
    return GOOME_OK;
}

void goome_bitmap_free(goome_bitmap *b)
{
    if (b && b->owned) free(b->px);
    if (b) memset(b, 0, sizeof(*b));
}

#undef IM_MAXDIM
#undef IM_MAXFILE
#undef IM_MAXRAW
#undef IM_MAXCOEF
#undef IM_ZFAST
#endif
