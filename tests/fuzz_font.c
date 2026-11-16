// fonts + text shaping fuzz. cc -std=c99 -g -O1 -fsanitize=address,undefined -Iinclude -Isrc tests/fuzz_font.c src/*.c -ldl -o fuzz_font && ./fuzz_font 20000
// with clang -fsanitize=fuzzer add -DGOOME_LIBFUZZER and it becomes a libFuzzer target
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "internal.h"
#include "fontdata.h"
#include "colorfonts.h"

static uint32_t fz_seed = 2463534242u;
static uint32_t fz_rand(void)
{
    fz_seed ^= fz_seed << 13, fz_seed ^= fz_seed >> 17, fz_seed ^= fz_seed << 5;
    return fz_seed;
}

static const char *const fz_text[] = {
    "goome! AV \xE2\x98\x83",
    "a\xF0\x9F\x98\x80\xF0\x9F\x8F\xBB\xF0\x9F\x98\x80\xE2\x80\x8D\xF0\x9F\x98\x80\xF0\x9F\x90\xB1\xE2\x9D\xA4\xEF\xB8\x8F\xE2\x9D\xA4\xEF\xB8\x8E",
    "\xD8\xA8\xD9\x84\xD8\xA7\xD9\x85 \xD8\xA8\xD9\x8E\xD8\xA8\xD8\xA8 \xD9\x84\xD9\x85 (12) ab \xD7\x90\xD7\x91\xD7\x92 [x]\n\xD9\x84\xD8\xA3",
    "\xF0\x9F\x87\xA9\xF0\x9F\x87\xAA\xF0\x9F\x87\xA9 #\xEF\xB8\x8F\xE2\x83\xA3\t\xE2\x80\x8F" "1.5%\xE2\x80\xAE" "x\xE2\x81\xA7y\xE2\x81\xA9",
};

static void fz_text_api(goome_font *f, const char *s, int n)
{
    gm_tc *b;
    int nb = gm_text_layout(f, 17, s, n, &b);
    for (int i = 0; i < nb; i++)
        if (b[i].lo < 0 || b[i].hi > n || b[i].lo > b[i].hi) abort();
    free(b);
    gm_text_rtl(f, s, n);
    for (int i = 0, k = 0; i < n && k < 4096; k++) {
        int j = gm_text_next(s, i, n);
        if (j <= i || j > n) abort();
        if (gm_text_prev(s, j) > i) abort();
        i = j;
    }
}

static void fz_hit(const uint8_t *d, size_t n, goome_font *base)
{
    static uint32_t px[160 * 80];
    goome_font *f = goome_font_load_mem(d, n, 0);
    if (!f) {
        goome_font_free(base);
        return;
    }
    goome_canvas c;
    memset(&c, 0, sizeof(c));
    c.px = px, c.w = 160, c.h = 80, c.stride = 160, c.scale = 1;
    static const double sizes[] = {6, 13, 31, 97};
    goome_font_fallback(base, f);
    for (int k = 0; k < 4; k++) {
        const char *t = fz_text[k];
        goome_text_dir(k % 3);
        goome_text(&c, f, sizes[k], -3, 2, t, 0xFFFFFFFF);
        goome_text(&c, base, sizes[3 - k], 1, 1, t, 0x80FF00FF);
        goome_text_measure(f, sizes[k], t, NULL);
        fz_text_api(f, t, (int)strlen(t));
    }
    goome_text_dir(GOOME_DIR_AUTO);
    goome_font_free(base);
    goome_font_free(f);
}

static goome_font *fz_base(void) { return goome_font_load_mem(sub_ttf, sizeof(sub_ttf), 0); }

static void fz_utf8(goome_font *f)
{
    static const uint32_t pick[] = {0x20, 0x61, 0x28, 0x29, 0x31, 0x2E, 0x5D0, 0x5D1, 0x5B4, 0x627, 0x644, 0x628, 0x64E, 0x660, 0x6F1, 0x200D, 0x200F,
                                    0x202B, 0x202C, 0x2067, 0x2069, 0xFE0F, 0xFE0E, 0x20E3, 0x1F600, 0x1F3FB, 0x1F1E6, 0x1F1E9, 0xE0067, 0x9, 0xA};
    char s[256];
    int n = 0, len = (int)(fz_rand() % 40);
    for (int i = 0; i < len && n < 240; i++) {
        uint32_t c = fz_rand() % 8 == 0 ? fz_rand() % 0x110000 : pick[fz_rand() % (sizeof(pick) / sizeof(pick[0]))];
        if (fz_rand() % 50 == 0) s[n++] = (char)(fz_rand() & 255);
        else if (c < 0x80) s[n++] = (char)(c ? c : 'z');
        else if (c < 0x800) s[n++] = (char)(0xC0 | c >> 6), s[n++] = (char)(0x80 | (c & 63));
        else if (c < 0x10000) s[n++] = (char)(0xE0 | c >> 12), s[n++] = (char)(0x80 | (c >> 6 & 63)), s[n++] = (char)(0x80 | (c & 63));
        else s[n++] = (char)(0xF0 | c >> 18), s[n++] = (char)(0x80 | (c >> 12 & 63)), s[n++] = (char)(0x80 | (c >> 6 & 63)), s[n++] = (char)(0x80 | (c & 63));
    }
    s[n] = 0;
    for (int i = 0; i < n; i++)
        if (!s[i]) s[i] = 'z';
    static uint32_t px[160 * 80];
    goome_canvas c;
    memset(&c, 0, sizeof(c));
    c.px = px, c.w = 160, c.h = 80, c.stride = 160, c.scale = 1.25;
    goome_text_dir((int)(fz_rand() % 3));
    goome_text(&c, f, 9 + fz_rand() % 30, 0, 0, s, 0xFFFFFFFF);
    goome_text_dir(GOOME_DIR_AUTO);
    fz_text_api(f, s, n);
}

#if defined(GOOME_LIBFUZZER)
int LLVMFuzzerTestOneInput(const uint8_t *d, size_t n)
{
    fz_hit(d, n, fz_base());
    return 0;
}
#else
int main(int argc, char **argv)
{
    static const struct {
        const unsigned char *d;
        size_t n;
    } seeds[] = {{colr_ttf, sizeof(colr_ttf)}, {cbdt_ttf, sizeof(cbdt_ttf)}, {sbix_ttf, sizeof(sbix_ttf)}, {sub_ttf, sizeof(sub_ttf)}, {sub_otf, sizeof(sub_otf)}};
    long runs = argc > 1 ? atol(argv[1]) : 20000;
    if (argc > 2) fz_seed = (uint32_t)atol(argv[2]) | 1;
    static uint8_t buf[1 << 14];
    goome_font *good = goome_font_load_mem(colr_ttf, sizeof(colr_ttf), 0), *cb = goome_font_load_mem(cbdt_ttf, sizeof(cbdt_ttf), 0);
    goome_font_fallback(good, cb);
    for (long r = 0; r < runs; r++) {
        int si = (int)(fz_rand() % 5);
        size_t n = seeds[si].n;
        memcpy(buf, seeds[si].d, n);
        uint32_t nt = (uint32_t)buf[4] << 8 | buf[5];
        int ops = 1 + (int)(fz_rand() % 6);
        for (int o = 0; o < ops; o++) {
            uint32_t at = fz_rand() % (uint32_t)n, kind = fz_rand() % 8;
            if (nt && nt < 64 && kind < 4) {
                uint32_t rec = 12 + (fz_rand() % nt) * 16, off = (uint32_t)buf[rec + 8] << 24 | (uint32_t)buf[rec + 9] << 16 | (uint32_t)buf[rec + 10] << 8 | buf[rec + 11];
                uint32_t len = (uint32_t)buf[rec + 12] << 24 | (uint32_t)buf[rec + 13] << 16 | (uint32_t)buf[rec + 14] << 8 | buf[rec + 15];
                if (rec + 16 <= n && off < n && len) at = off + fz_rand() % (len < n - off ? len : (uint32_t)(n - off));
            }
            if (kind == 0 || kind == 4) buf[at] ^= (uint8_t)(1u << (fz_rand() % 8));
            else if (kind == 1 || kind == 5) buf[at] = (uint8_t)fz_rand();
            else if (kind == 2 && at + 1 < n) buf[at] = buf[at + 1] = fz_rand() & 1 ? 0xFF : 0;
            else if (kind == 3 && at + 3 < n) buf[at] = 0x7F, buf[at + 1] = buf[at + 2] = buf[at + 3] = (uint8_t)fz_rand();
            else if (kind == 6) n = at > 12 ? at : n;
            else if (at + 4 < n) memcpy(buf + at, buf + fz_rand() % (uint32_t)(n - 4), 4);
        }
        fz_hit(buf, n, fz_base());
        if (r % 4 == 0) fz_utf8(good);
        if (r % 1000 == 0) fprintf(stderr, "%ld\n", r);
    }
    goome_font_free(cb);
    goome_font_free(good);
    printf("fuzz ok, %ld runs\n", runs);
    return 0;
}
#endif
