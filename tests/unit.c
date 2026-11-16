// goome's own checks, no screen needed. ./build.sh test runs it
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "goome.h"
#include "fontdata.h"
#include "images.h"
#include "colorfonts.h"

static int fails;
static const char *gm_dump_dir;
#define CHECK(x)                                                                    \
    do {                                                                            \
        if (!(x)) printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #x), fails++;     \
    } while (0)

static uint32_t fnv(const uint32_t *px, int w, int h, int stride)
{
    uint32_t v = 2166136261u;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            uint32_t p = px[(size_t)y * stride + x];
            for (int k = 0; k < 4; k++) v = (v ^ ((p >> (k * 8)) & 255)) * 16777619u;
        }
    return v;
}

static void combos(void)
{
    int k, m;
    CHECK(goome_parse_combo("alt+h", &k, &m) == GOOME_OK && k == 'h' && m == GOOME_ALT);
    CHECK(goome_parse_combo("Ctrl+Shift+F5", &k, &m) == GOOME_OK && k == GOOME_K_F1 + 4 && m == (GOOME_CTRL | GOOME_SHIFT));
    CHECK(goome_parse_combo("alt+?", &k, &m) == GOOME_OK && k == '/' && m == (GOOME_ALT | GOOME_SHIFT));
    CHECK(goome_parse_combo("super+space", &k, &m) == GOOME_OK && k == ' ' && m == GOOME_SUPER);
    CHECK(goome_parse_combo("cmd+left", &k, &m) == GOOME_OK && k == GOOME_K_LEFT && m == GOOME_SUPER);
    CHECK(goome_parse_combo("H", &k, &m) == GOOME_OK && k == 'h' && m == GOOME_SHIFT);
    CHECK(goome_parse_combo("alt+", &k, &m) != GOOME_OK);
    CHECK(goome_parse_combo("alt+f13", &k, &m) != GOOME_OK);
    CHECK(goome_parse_combo("", &k, &m) != GOOME_OK);
}

static void pacer(void)
{
    goome_pacer p = {0};
    CHECK(goome_pace(&p, 50) == 0);
    int w = goome_pace(&p, 50);
    CHECK(w > 0 && w <= 20);
}

static void drawing(void)
{
    static uint32_t px[200 * 100];
    goome_canvas c;
    memset(&c, 0, sizeof(c));
    memset(px, 0, sizeof(px));
    c.px = px, c.w = 160, c.h = 100, c.stride = 200, c.scale = 1.5; // stride wider than w on purpose
    goome_draw_round_rect(&c, 5, 5, 60, 40, 8, 0xC0203040);
    goome_draw_line(&c, 0, 0, 100, 60, 3, 0xFFFF0000);
    goome_draw_triangle(&c, 10, 50, 60, 50, 35, 10, 0x8000FF00);
    goome_draw_text(&c, 8, 8, "goome!", 1, 0xFFFFFFFF);
    goome_draw_frame(&c, 70, 10, 30, 30, 2, 0xFF00FFFF);
    for (int y = 0; y < 100; y++) CHECK(px[y * 200 + 170] == 0); // nothing past w
    CHECK(c.dx1 > c.dx0 && c.dy1 > c.dy0);
    printf("draw hash %08x\n", fnv(px, 200, 100, 200));
    CHECK(fnv(px, 200, 100, 200) == 0xb93b60cfu);
    goome_clear(&c);
    CHECK(fnv(px, 200, 100, 200) == fnv((uint32_t[200 * 100]){0}, 200, 100, 200));
    CHECK(goome_text_width(&c, "abc", 0) > 0);
}

static void shape(goome_canvas *c, int k, const goome_bitmap *b)
{
    static const double star[] = {80, 5, 98, 85, 40, 35, 120, 35, 62, 85};
    switch (k) {
    case 0: goome_draw_rrect(c, 10.3, 8.6, 120, 70.2, (goome_radii){4, 20, 0, 33}, 0xE02060C0); break;
    case 1: goome_draw_rrect_line(c, 10.3, 8.6, 120, 70.2, goome_radius(18), 2.5, 0xFF20A060); break;
    case 2: goome_draw_rrect_grad(c, 10, 10, 130, 70, goome_radius(12), 0xFF102040, 0x8080C0FF, 0); break;
    case 3: goome_draw_rrect_grad(c, 10, 10, 130, 70, goome_radius(35), 0xFFFF4080, 0x004080FF, 1); break;
    case 4: goome_draw_shadow(c, 30, 20, 90, 50, 10, 16, 0x80000000); break;
    case 5: goome_draw_circle(c, 50.5, 50.2, 30.7, 0xFFFFC020), goome_draw_circle(c, 120, 30, 1.3, 0xFF000000); break;
    case 6: goome_draw_ring(c, 80, 50, 30, 5, 0xC0208080); break;
    case 7: goome_draw_arc(c, 50, 50, 30, 6, 0.3, 4.5, 0xFF3060E0), goome_draw_arc(c, 120, 50, 20, 30, 2, 2.5, 0xFF905020); break;
    case 8: goome_draw_poly(c, star, 5, 0xFF2040A0); break;
    case 9: goome_draw_bitmap(c, b, 10, 10, 100.5, 75.3, goome_radius(20), 0xFFFFFFFF); break;
    case 10: goome_draw_bitmap(c, b, 10, 10, 24, 16, goome_radius(0), 0x80FF8080); break;
    case 11: goome_draw_bitmap(c, b, 10, 10, 9, 5, goome_radius(2), 0xFFFFFFFF); break;
    case 12: goome_draw_bitmap_part(c, b, 4, 2, 8, 8, 100, 10, 40, 40, 0xFFFFFFFF); break;
    default:
        goome_clip_push(c, 20, 15, 80, 50);
        goome_draw_rrect(c, 0, 0, 160, 100, goome_radius(30), 0xFF4080FF);
        goome_clip_push(c, 50, 0, 100, 40);
        goome_draw_circle(c, 70, 40, 30, 0xFFFF4040);
        goome_draw_text(c, 30, 30, "clip", 1, 0xFF000000);
        goome_draw_shadow(c, 40, 20, 60, 30, 8, 10, 0xFF000000);
        goome_draw_bitmap(c, b, 40, 10, 48, 32, goome_radius(6), 0xFFFFFFFF);
        goome_clip_pop(c);
        goome_draw_line(c, 0, 0, 160, 100, 3, 0xFF00A000);
        goome_draw_poly(c, star, 5, 0x80000000);
        goome_clip_pop(c);
        goome_draw_frame(c, 20, 15, 80, 50, 1, 0x80000000);
        break;
    }
}

static void shapes(void)
{
    static const uint32_t want[3][14] = {
        {0xa7b3b4c5, 0x0fb29a82, 0x1a98cf16, 0x1e7b729f, 0x21fd3b05, 0x241ce7fd, 0x6910b0d5, 0x7180b28e, 0x6f33ac30, 0xb48adfa3, 0x796b9105, 0x2d93dc92, 0x1f908fad, 0x23a6cf0c},
        {0x73429a18, 0xd9bd97ab, 0xd031f56c, 0xd2d9fa9a, 0xd639a1a7, 0xa77c3e9e, 0x7bd3fead, 0xac76561e, 0x5bd0648d, 0xd0ac70e1, 0x0bb5dda0, 0xe7c304c7, 0xccef8860, 0xefc1f212},
        {0x9175788f, 0x6e99ea87, 0x935cc8e1, 0x0b73f68d, 0x23891be5, 0xa28476c1, 0xea64f655, 0x87dc9430, 0x909d11b5, 0x0550bb7a, 0x6455ee86, 0xd8c08e2b, 0xfd0dd1fd, 0x28c27977},
    };
    static const double sc[3] = {1, 1.25, 2};
    static uint32_t px[323 * 200], bp[27 * 16];
    goome_bitmap b = {bp, 24, 16, 27, 0};
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 24; x++) {
            uint32_t a = (uint32_t)(x * 255 / 23), v = (x / 4 + y / 4) & 1 ? 0xFFFFFFu : 0x2050C0u;
            bp[y * 27 + x] = a << 24 | ((v >> 16 & 255) * a / 255) << 16 | ((v >> 8 & 255) * a / 255) << 8 | (v & 255) * a / 255;
        }
    for (int s = 0; s < 3; s++) {
        uint32_t got[14];
        for (int k = 0; k < 14; k++) {
            goome_canvas c;
            memset(&c, 0, sizeof(c));
            memset(px, 0, sizeof(px));
            c.px = px, c.scale = sc[s], c.w = (int)(160 * sc[s]), c.h = (int)(100 * sc[s]), c.stride = c.w + 3; // odd stride
            shape(&c, k, &b);
            got[k] = fnv(px, c.stride, c.h, c.stride);
            CHECK(c.dx1 > c.dx0);
            for (int y = 0; y < c.h; y++)
                for (int x = c.w; x < c.stride; x++) CHECK(px[y * c.stride + x] == 0);
            for (int i = 0; i < c.stride * c.h; i++) {
                uint32_t p = px[i], a = p >> 24;
                if ((p >> 16 & 255) > a || (p >> 8 & 255) > a || (p & 255) > a) {
                    CHECK(!"premultiplied");
                    break;
                }
            }
            if (k == 13) { // nothing outside the outer clip, damage inside it
                int x0 = (int)(20 * sc[s] + 0.5), y0 = (int)(15 * sc[s] + 0.5), x1 = (int)(100 * sc[s] + 0.5), y1 = (int)(65 * sc[s] + 0.5);
                memset(px, 0, sizeof(px));
                memset(&c, 0, sizeof(c));
                c.px = px, c.scale = sc[s], c.w = (int)(160 * sc[s]), c.h = (int)(100 * sc[s]), c.stride = c.w + 3;
                goome_clip_push(&c, 20, 15, 80, 50);
                shape(&c, 8, &b);
                shape(&c, 4, &b);
                shape(&c, 9, &b);
                goome_draw_text(&c, 0, 20, "outside", 1, 0xFFFFFFFF);
                CHECK(c.dx0 >= x0 && c.dy0 >= y0 && c.dx1 <= x1 && c.dy1 <= y1 && c.dx1 > c.dx0);
                for (int y = 0; y < c.h; y++)
                    for (int x = 0; x < c.w; x++)
                        if ((x < x0 || x >= x1 || y < y0 || y >= y1) && px[y * c.stride + x]) {
                            CHECK(!"drew outside the clip");
                            y = c.h;
                            break;
                        }
                goome_clip_pop(&c);
                CHECK(c.nclip == 0);
            }
        }
        printf("shape hashes x%g:", sc[s]);
        for (int k = 0; k < 14; k++) printf(" 0x%08x,", got[k]);
        printf("\n");
        for (int k = 0; k < 14; k++) CHECK(got[k] == want[s][k]);
    }
}

static void fonts(void)
{
    static uint32_t px[240 * 120], px2[240 * 120];
    CHECK(goome_font_load_mem("nope, not a font", 16, 1) == NULL);
    CHECK(goome_font_load_mem(sub_ttf, 40, 0) == NULL);
    goome_font *f = goome_font_load_mem(sub_ttf, sizeof(sub_ttf), 0), *o = goome_font_load_mem(sub_otf, sizeof(sub_otf), 1);
    CHECK(f && o);
    if (!f || !o) return;
    double h, w = goome_text_measure(f, 20, "goome!", &h);
    CHECK(w > 76.97 && w < 76.98 && h > 23.2 && h < 23.3); // g o o m e ! = 7882 units at 20 / 2048
    CHECK(goome_text_measure(f, 20, "AV", NULL) < goome_text_measure(f, 20, "A", NULL) + goome_text_measure(f, 20, "V", NULL) - 1);
    CHECK(goome_text_measure(o, 20, "AV", NULL) < goome_text_measure(o, 20, "A", NULL) + goome_text_measure(o, 20, "V", NULL) - 1);
    goome_text_measure(f, 20, "a\nbb\n", &h);
    CHECK(h > 3 * 23.2 && h < 3 * 23.3);
    CHECK(goome_text_measure(f, 20, "\xff\xc3(\xed\xa0\x80\xf4\x90\x80\x80", NULL) > 0); // junk utf-8 turns into U+FFFD boxes
    uint32_t want[2] = {0x412c299fu, 0xbcd42095u};
    goome_font *both[2] = {f, o};
    uint32_t *bufs[2] = {px, px2};
    for (int k = 0; k < 2; k++) {
        goome_canvas c;
        memset(&c, 0, sizeof(c));
        memset(bufs[k], 0, sizeof(px));
        c.px = bufs[k], c.w = 200, c.h = 120, c.stride = 240, c.scale = 1.5, c.kx1 = 200, c.ky1 = 120;
        goome_text(&c, both[k], 13, 4, 2, "goome! Hi\xc3\xa9\nAVAWAY Ty.@ \xe2\x98\x83", 0xFFFFFFFF);
        goome_text(&c, both[k], 30, 2.3, 24, "Hig", 0xC0FF8040);
        c.kx0 = 30, c.ky0 = 60, c.kx1 = 90, c.ky1 = 100, c.nclip = 1;
        goome_text(&c, both[k], 40, 10, 50, "WoW", 0xFF2080FF);
        for (int y = 0; y < 120; y++)
            for (int x = 200; x < 240; x++) CHECK(bufs[k][y * 240 + x] == 0);
        for (int y = 100; y < 120; y++)
            for (int x = 0; x < 200; x++) CHECK(bufs[k][y * 240 + x] == 0);
        CHECK(c.dx0 >= 0 && c.dx1 <= 200 && c.dy1 <= 100 && c.dx1 > c.dx0);
        printf("font hash %d %08x\n", k, fnv(bufs[k], 240, 120, 240));
        CHECK(fnv(bufs[k], 240, 120, 240) == want[k]);
    }
    for (int k = 0; k < 2; k++) {
        goome_canvas c;
        memset(&c, 0, sizeof(c));
        memset(bufs[k], 0, sizeof(px));
        c.px = bufs[k], c.w = 240, c.h = 120, c.stride = 240, c.scale = 1, c.kx1 = 240, c.ky1 = 120;
        goome_text(&c, both[k], 31, 3, 3, "goome Hi\nmoge", 0xFFFFFFFF);
    }
    long diff = 0, ink = 0;
    for (int i = 0; i < 240 * 120; i++) {
        ink += (long)(px[i] >> 24);
        int a = (int)(px[i] >> 24), b = (int)(px2[i] >> 24);
        diff += a > b ? a - b : b - a;
    }
    CHECK(ink > 0 && diff * 50 < ink); // glyf and cff outlines of the same font come out the same
    for (size_t cut = 0; cut < sizeof(sub_otf); cut += 7) { // cut short, flip bytes, nothing may crash
        static unsigned char bad[sizeof(sub_otf)];
        memcpy(bad, sub_otf, sizeof(bad));
        bad[cut] ^= 0x5A;
        goome_font *b = goome_font_load_mem(bad, (cut & 1) ? sizeof(bad) : cut + 12, 0);
        goome_canvas c;
        memset(&c, 0, sizeof(c));
        c.px = px, c.w = 200, c.h = 120, c.stride = 240, c.scale = 1, c.kx1 = 200, c.ky1 = 120;
        goome_text(&c, b, 20, 0, 0, "goome! AV", 0xFFFFFFFF);
        goome_font_free(b);
    }
    goome_canvas c;
    memset(&c, 0, sizeof(c));
    c.px = px, c.w = 200, c.h = 120, c.stride = 240, c.scale = 1;
    CHECK(goome_text(&c, NULL, 12, 0, 0, "ab\nabc", 0xFFFFFFFF) == goome_text_measure(NULL, 12, "abc", NULL));
    goome_font_free(f);
    goome_font_free(o);
}

static void bitmaps(void)
{
    for (size_t i = 0; i < sizeof(img_cases) / sizeof(img_cases[0]); i++) {
        const img_case *t = &img_cases[i];
        goome_bitmap b;
        int rc = goome_bitmap_load_mem(t->data, t->n, &b);
        if (rc) {
            printf("FAIL %s: %s\n", t->name, goome_bitmap_error());
            fails++;
            continue;
        }
        uint32_t hv = fnv(b.px, b.w, b.h, b.stride);
        if (b.w != t->w || b.h != t->h || hv != t->hash || !b.owned) printf("FAIL %s: %dx%d %08x\n", t->name, b.w, b.h, hv), fails++;
        goome_bitmap_free(&b);
        CHECK(!b.px && !b.w);
        rc = goome_bitmap_load_mem(t->data, t->n / 2, &b);
        CHECK(rc != GOOME_OK || b.px);
        goome_bitmap_free(&b);
    }
    goome_bitmap b;
    CHECK(goome_bitmap_load_mem(img_arith, sizeof(img_arith), &b) == GOOME_ENOTSUP && !b.px);
    CHECK(strstr(goome_bitmap_error(), "arithmetic") != NULL);
    CHECK(goome_bitmap_load_mem("GIF89a......", 12, &b) == GOOME_ENOTSUP);
    CHECK(goome_bitmap_load_mem("\x89PNG\r\n\x1a\n", 8, &b) != GOOME_OK && !b.px);
    CHECK(goome_bitmap_load_mem(NULL, 4, &b) == GOOME_EARG);
    CHECK(goome_bitmap_load("no/such/file.png", &b) != GOOME_OK && !b.px);
    uint32_t argb[4] = {0xFFFF0000u, 0x80FFFFFFu, 0x00123456u, 0x40804020u};
    CHECK(goome_bitmap_from_argb(argb, 2, 2, &b) == GOOME_OK);
    CHECK(b.px[0] == 0xFFFF0000u && b.px[1] == 0x80808080u && b.px[2] == 0 && b.px[3] == 0x40201008u);
    goome_bitmap_free(&b);
}

static void scripted(void)
{
    int err;
    goome *g = goome_open(&(goome_opts){.backend = GOOME_TEST, .test_script = "screen 640x360 2; 1 click 20,20; 1 click 300,300; 2 hotkey alt+j; "
                                                                              "3 key Hi\\b; 4 send ping; 5 scroll 30,30 -1; 6 hold 25,25 8; end 10"},
                          &err);
    CHECK(g != NULL);
    if (!g) return;
    goome_monitor m;
    CHECK(goome_monitors(g, &m, 1) == 1 && m.area.w == 640 && m.scale == 2 && m.pw == 1280);
    CHECK(goome_hotkey(g, "alt+j", 7) == GOOME_OK);
    CHECK(goome_keyboard(g, 1) == GOOME_OK);
    int downs = 0, ups = 0, hot = 0, keys = 0, ctl = 0, scroll = 0, quit = 0, held = 0, frames = 0;
    char text[16] = "";
    while (!quit && frames < 100) {
        goome_canvas *c = goome_begin(g, 0);
        CHECK(c && c->scale == 2 && c->w == 1280);
        goome_draw_rect(c, 10, 10, 40, 40, 0xFF336699);
        goome_rect hit = {10, 10, 40, 40};
        goome_end(g, c, &hit, 1);
        frames++;
        double x, y;
        int b;
        if (goome_pointer(g, &x, &y, &b) && b) held++;
        goome_event ev;
        while (goome_poll(g, &ev, 1) > 0) {
            if (ev.type == GOOME_DOWN) downs++;
            if (ev.type == GOOME_UP) ups++;
            if (ev.type == GOOME_HOTKEY && ev.id == 7) hot++;
            if (ev.type == GOOME_KEY) keys++, strcat(text, ev.text);
            if (ev.type == GOOME_CONTROL && !strcmp(ev.msg, "ping")) ctl++;
            if (ev.type == GOOME_SCROLL && ev.dy == -1) scroll++;
            if (ev.type == GOOME_QUIT) quit = 1;
        }
    }
    CHECK(downs == 2 && ups == 2); // the click outside the box went to whatever was under it
    CHECK(hot == 1 && keys == 3 && !strcmp(text, "Hi") && ctl == 1 && scroll == 1 && quit && held >= 1);
    goome_image img;
    goome_rect r = {10, 10, 40, 40};
    CHECK(goome_capture(g, 0, &r, &img) == GOOME_OK && img.w == 80 && img.h == 80 && img.has_self);
    CHECK(img.px[0] == 0xFF336699u);
    CHECK(goome_alpha_at(g, 20, 20) == 255 && goome_alpha_at(g, 200, 200) == 0);
    goome_close(g);
}

static void ui(void)
{
    int err;
    goome *g = goome_open(&(goome_opts){.backend = GOOME_TEST,
                                        .test_script = "screen 400x360 1; 1 click 35,71; 2 click 29,106; 3 hold 40,157 6; 4 hover 120,157; "
                                                       "7 click 100,213; 8 key ab; 9 key \\n; 10 key zz; 11 click 120,273; 13 click 120,206; "
                                                       "15 click 300,20; 16 hold 100,20 20; 18 hover 150,40; end 22"},
                          &err);
    CHECK(g != NULL);
    if (!g) return;
    goome_ui *u = goome_ui_new(g);
    CHECK(u != NULL);
    if (u) goome_ui_theme(u)->font = NULL, goome_ui_theme(u)->font_size = 14; // the system font differs per machine
    double x = 10, y = 10, w = 220, h = 0, val = 0;
    int quit = 0, frames = 0, clicks = 0, check = 0, pick = 0, downs = 0, keys_off = 0, kb_seen = 0;
    char text[16] = "";
    static const char *const items[] = {"one", "two", "three"};
    uint32_t hs[3] = {0, 0, 0};
    while (!quit && frames < 100 && u) {
        goome_event ev;
        while (goome_poll(g, &ev, 0) > 0) {
            if (ev.type == GOOME_QUIT) quit = 1;
            if (ev.type == GOOME_DOWN) downs++;
            if (ev.type == GOOME_KEY && frames > 10) keys_off++;
            goome_ui_feed(u, &ev);
        }
        goome_canvas *c = goome_ui_begin(u, 0);
        CHECK(c != NULL);
        if (!c) break;
        if (goome_ui_window(u, "test", &x, &y, &w, &h, GOOME_WIN_MOVE, NULL)) {
            if (goome_ui_button(u, "Go")) clicks++;
            goome_ui_checkbox(u, "Check", &check);
            goome_ui_slider(u, "Val", &val, 0, 1, NULL);
            goome_ui_input(u, "Text", text, sizeof(text));
            goome_ui_combo(u, "Pick", &pick, items, 3);
        }
        goome_ui_window_end(u);
        goome_ui_end(u);
        if (frames == 8) kb_seen = text[0] != 0;
        if (frames == 0 || frames == 12 || frames == 21) hs[frames == 0 ? 0 : frames == 12 ? 1 : 2] = fnv(c->px, c->w, c->h, c->stride);
        frames++;
    }
    printf("ui hashes %08x %08x %08x  val %.3f x %.0f y %.0f text '%s' pick %d downs %d\n", hs[0], hs[1], hs[2], val, x, y, text, pick, downs);
    CHECK(clicks == 1 && check == 1);
    CHECK(val > 0.45 && val < 0.55);
    CHECK(kb_seen && !strcmp(text, "ab") && keys_off == 0); // enter dropped the keyboard, zz never came
    CHECK(pick == 1);
    CHECK(x == 60 && y == 30);
    CHECK(downs == 7); // the click at 300,20 missed every window, it went through
    CHECK(hs[0] == 0x51713294u && hs[1] == 0xb52f16c5u && hs[2] == 0xea01692cu);
    goome_ui_free(u);
    goome_close(g);
}

static void offscreen(void)
{
    int err;
    goome *g = goome_open(&(goome_opts){.mode = GOOME_MODE_OFFSCREEN, .title = "engine", .width = 200, .height = 100, .scale = 2}, &err);
    CHECK(g != NULL && err == GOOME_OK);
    if (!g) return;
    CHECK(!strcmp(goome_backend_name(g), "offscreen") && goome_mode(g) == GOOME_MODE_OFFSCREEN);
    CHECK((goome_caps(g) & (GOOME_CAP_CLIPBOARD | GOOME_CAP_CURSOR | GOOME_CAP_KEYBOARD)) == (GOOME_CAP_CLIPBOARD | GOOME_CAP_CURSOR | GOOME_CAP_KEYBOARD));
    CHECK(!(goome_caps(g) & (GOOME_CAP_CAPTURE | GOOME_CAP_WINDOW | GOOME_CAP_GLOBAL_POINTER)));
    goome_monitor m;
    CHECK(goome_monitors(g, &m, 1) == 1 && m.area.w == 200 && m.area.h == 100 && m.scale == 2 && m.pw == 400 && m.ph == 200);
    CHECK(!strcmp(m.name, "engine"));
    goome_image f;
    CHECK(goome_frame(g, &f) == GOOME_EAGAIN);
    goome_canvas *c = goome_begin(g, 0);
    CHECK(c && c->w == 400 && c->h == 200 && c->scale == 2);
    if (!c) return;
    goome_draw_rect(c, 10, 10, 20, 20, 0x80FF0000);
    goome_rect hit = {10, 10, 20, 20};
    CHECK(goome_end(g, c, &hit, 1) == GOOME_OK);
    CHECK(goome_frame(g, &f) == GOOME_OK && f.w == 400 && f.h == 200 && f.stride == 400 && f.area.w == 200);
    CHECK(f.px[25 * 400 + 25] == 0x80800000u && f.px[0] == 0);
    CHECK(goome_hit(g, 15, 15) == 1 && goome_hit(g, 30, 30) == 0 && goome_hit(g, 9.5, 15) == 0);
    CHECK(goome_alpha_at(g, 15, 15) == 0x80);

    goome_event ev, in;
    memset(&in, 0, sizeof(in));
    in.type = GOOME_DOWN, in.x = 12, in.y = 13, in.button = 1;
    CHECK(goome_push(g, &in) == GOOME_OK);
    in.type = GOOME_KEY, in.key = 'q', in.text[0] = 'q';
    CHECK(goome_push(g, &in) == GOOME_OK);
    CHECK(goome_poll(g, &ev, 0) == 1 && ev.type == GOOME_DOWN && ev.x == 12 && ev.y == 13);
    CHECK(goome_poll(g, &ev, 0) == 1 && ev.type == GOOME_KEY && ev.key == 'q');
    long t0 = goome_now_ms();
    CHECK(goome_poll(g, &ev, 200) == 0);
    CHECK(goome_now_ms() - t0 < 100);
    int pushed = 0;
    memset(&in, 0, sizeof(in));
    in.type = GOOME_KEY;
    while (pushed < 1000 && (in.id = pushed, goome_push(g, &in)) == GOOME_OK) pushed++;
    CHECK(pushed > 10 && pushed < 1000);
    while (goome_poll(g, &ev, 0) > 0) {}

    CHECK(goome_resize(g, 300, 150, 0) == GOOME_OK);
    CHECK(goome_poll(g, &ev, 0) == 1 && ev.type == GOOME_MONITORS);
    CHECK(goome_monitors(g, &m, 1) == 1 && m.area.w == 300 && m.pw == 600 && m.ph == 300);
    c = goome_begin(g, 0);
    CHECK(c && c->w == 600 && c->h == 300 && c->px[25 * 600 + 25] == 0);
    if (c) goome_end(g, c, NULL, 0);
    CHECK(goome_hit(g, 15, 15) == 0);
    CHECK(goome_resize(g, 300, 150, 1) == GOOME_OK && goome_monitors(g, &m, 1) == 1 && m.pw == 300 && m.scale == 1);
    CHECK(goome_resize(g, 0, 10, 1) == GOOME_EARG);
    CHECK(goome_window_size(g, 320, 160) == GOOME_OK && goome_monitors(g, &m, 1) == 1 && m.area.w == 320);
    CHECK(goome_window_title(g, "renamed") == GOOME_OK && goome_monitors(g, &m, 1) == 1 && !strcmp(m.name, "renamed"));
    CHECK(goome_window_pos(g, NULL, NULL, 0) == GOOME_ENOTSUP);

    char buf[8];
    CHECK(goome_clipboard_get(g, buf, sizeof(buf)) == 0 && buf[0] == 0);
    CHECK(goome_clipboard_set(g, "hello clipboard") == GOOME_OK);
    CHECK(goome_clipboard_get(g, buf, sizeof(buf)) == 15 && !strcmp(buf, "hello c"));
    CHECK(goome_clipboard_get(g, NULL, 0) == 15);
    CHECK(goome_cursor_shape(g) == GOOME_CURSOR_ARROW);
    CHECK(goome_cursor(g, GOOME_CURSOR_IBEAM) == GOOME_OK && goome_cursor_shape(g) == GOOME_CURSOR_IBEAM);
    CHECK(goome_cursor(g, 99) == GOOME_EARG && goome_cursor_shape(g) == GOOME_CURSOR_IBEAM);
    CHECK(goome_hotkey(g, "alt+h", 1) == GOOME_ENOTSUP);
    goome_rect r = {0, 0, 10, 10};
    goome_image img;
    CHECK(goome_capture(g, 0, &r, &img) == GOOME_ENOTSUP);

    goome *g2 = goome_open(&(goome_opts){.backend = GOOME_OFFSCREEN}, &err);
    CHECK(g2 != NULL && goome_mode(g2) == GOOME_MODE_OFFSCREEN);
    if (g2) {
        CHECK(goome_monitors(g2, &m, 1) == 1 && m.area.w == 960 && m.area.h == 640 && m.scale == 1);
        goome_canvas *c2 = goome_begin(g2, 0);
        CHECK(c2 && c2->px != goome_begin(g, 0)->px);
        CHECK(goome_clipboard_get(g2, buf, sizeof(buf)) == 0);
        goome_close(g2);
    }
    goome_close(g);
    CHECK(goome_open(&(goome_opts){.mode = 7}, &err) == NULL && err == GOOME_EARG);
    CHECK(goome_open(&(goome_opts){.mode = GOOME_MODE_OFFSCREEN, .width = 100000}, &err) == NULL);
}

static void frame_copy(void)
{
    int err;
    goome *g = goome_open(&(goome_opts){.mode = GOOME_MODE_OFFSCREEN, .width = 100, .height = 50}, &err);
    CHECK(g != NULL);
    if (!g) return;
    goome_rect r = {0, 0, 0, 0};
    unsigned char px[4 * 100 * 50];
    CHECK(goome_frame_dirty(g, &r) == 0);
    CHECK(goome_frame_copy(g, px, 400, GOOME_FMT_RGBA, NULL) == GOOME_EAGAIN);
    goome_canvas *c = goome_begin(g, 0);
    goome_draw_rect(c, 10, 10, 20, 20, 0x80FF0000);
    goome_end(g, c, NULL, 0);
    CHECK(goome_frame_dirty(g, &r) == 1 && r.x == 0 && r.y == 0 && r.w == 100 && r.h == 50);
    CHECK(goome_frame_dirty(g, &r) == 0);
    c = goome_begin(g, 0);
    goome_draw_rect(c, 40, 5, 10, 10, 0xFF00FF00);
    goome_end(g, c, NULL, 0);
    CHECK(goome_frame_dirty(g, &r) == 1 && r.x <= 10 && r.y <= 5 && r.x + r.w >= 50 && r.y + r.h >= 30 && r.w < 50 && r.h < 35);
    c = goome_begin(g, 0);
    goome_end(g, c, NULL, 0);
    c = goome_begin(g, 0);
    goome_draw_rect(c, 70, 30, 5, 5, 0xFF0000FF);
    goome_end(g, c, NULL, 0);
    CHECK(goome_frame_dirty(g, &r) == 1 && r.x <= 40 && r.y <= 5 && r.x + r.w >= 75 && r.y + r.h >= 35);
    c = goome_begin(g, 0);
    goome_draw_rect(c, 70, 30, 5, 5, 0xFF0000FF);
    goome_end(g, c, NULL, 0);
    CHECK(goome_frame_dirty(g, &r) == 1 && r.x >= 60 && r.y >= 20);
    c = goome_begin(g, 0);
    goome_draw_rect(c, 10, 10, 20, 20, 0x80FF0000);
    goome_end(g, c, NULL, 0);
    goome_frame_dirty(g, &r);
    CHECK(goome_frame_copy(g, px, 400, GOOME_FMT_BGRA_PREMUL, NULL) == GOOME_OK);
    unsigned char *q = px + (20 * 100 + 20) * 4;
    CHECK(q[0] == 0 && q[1] == 0 && q[2] == 0x80 && q[3] == 0x80 && px[0] == 0 && px[3] == 0);
    CHECK(goome_frame_copy(g, px, 400, GOOME_FMT_RGBA_PREMUL, NULL) == GOOME_OK && q[0] == 0x80 && q[2] == 0 && q[3] == 0x80);
    CHECK(goome_frame_copy(g, px, 400, GOOME_FMT_RGBA, NULL) == GOOME_OK && q[0] == 0xFF && q[1] == 0 && q[2] == 0 && q[3] == 0x80);
    CHECK(goome_frame_copy(g, px, 400, GOOME_FMT_BGRA, NULL) == GOOME_OK && q[0] == 0 && q[2] == 0xFF && q[3] == 0x80);
    memset(px, 0xAA, sizeof(px));
    r = (goome_rect){15, 18, 4, 3};
    CHECK(goome_frame_copy(g, px, 20, GOOME_FMT_RGBA, &r) == GOOME_OK);
    CHECK(px[0] == 0xFF && px[3] == 0x80 && px[20 * 2 + 12] == 0xFF && px[16] == 0xAA && px[20 * 3] == 0xAA);
    r = (goome_rect){0, 0, 99, 10};
    CHECK(goome_frame_copy(g, px, 396, GOOME_FMT_RGBA, &r) == GOOME_OK);
    CHECK(goome_frame_copy(g, px, 395, GOOME_FMT_RGBA, &r) == GOOME_EARG);
    r = (goome_rect){90, 0, 11, 10};
    CHECK(goome_frame_copy(g, px, 400, GOOME_FMT_RGBA, &r) == GOOME_EARG);
    CHECK(goome_frame_copy(g, px, 400, 4, NULL) == GOOME_EARG && goome_frame_copy(g, NULL, 400, 0, NULL) == GOOME_EARG);
    CHECK(goome_resize(g, 100, 50, 1) == GOOME_OK);
    c = goome_begin(g, 0);
    goome_end(g, c, NULL, 0);
    CHECK(goome_frame_dirty(g, &r) == 1 && r.w == 100 && r.h == 50);
    CHECK(goome_resize(g, 60, 40, 2) == GOOME_OK);
    c = goome_begin(g, 0);
    goome_end(g, c, NULL, 0);
    CHECK(goome_frame_dirty(g, &r) == 1 && r.x == 0 && r.y == 0 && r.w == 120 && r.h == 80);
    CHECK(goome_frame_dirty(NULL, &r) == 0 && goome_frame_dirty(g, NULL) == 0);
    goome_close(g);

    g = goome_open(&(goome_opts){.backend = GOOME_TEST, .mode = GOOME_MODE_WINDOW, .width = 80, .height = 60, .test_script = "screen 1x1 1; end 9"}, &err);
    CHECK(g != NULL);
    if (!g) return;
    CHECK(goome_frame_dirty(g, &r) == 0);
    c = goome_begin(g, 0);
    goome_draw_rect(c, 0, 0, 80, 60, 0xFF202830);
    goome_end(g, c, NULL, 0);
    CHECK(goome_frame_dirty(g, &r) == 1 && r.w == 80 && r.h == 60);
    c = goome_begin(g, 0);
    goome_draw_rect(c, 0, 0, 80, 60, 0xFF202830);
    goome_end(g, c, NULL, 0);
    CHECK(goome_frame_dirty(g, &r) == 1 && r.w == 80);
    CHECK(goome_frame_copy(g, px, 320, GOOME_FMT_RGBA, NULL) == GOOME_OK && px[0] == 0x20 && px[1] == 0x28 && px[2] == 0x30 && px[3] == 0xFF);
    goome_close(g);
}

static void window_mode(void)
{
    int err;
    goome *g = goome_open(&(goome_opts){.backend = GOOME_TEST, .mode = GOOME_MODE_WINDOW, .title = "notes", .width = 300, .height = 200,
                                        .min_width = 250, .min_height = 120,
                                        .test_script = "screen 1x1 2; 1 click 250,150; 1 click 350,150; 2 resize 400x300; 3 close; 4 focus 0; "
                                                       "5 key a; 6 focus 1; 7 key b; 8 drop 10,20 /home/someone/a/really/long/folder/name/that/goes/past/63/bytes/file.txt; "
                                                       "9 resize 100x100; end 11"},
                          &err);
    CHECK(g != NULL);
    if (!g) return;
    CHECK(goome_mode(g) == GOOME_MODE_WINDOW && (goome_caps(g) & GOOME_CAP_WINDOW));
    goome_monitor m;
    CHECK(goome_monitors(g, &m, 1) == 1 && m.area.w == 300 && m.area.h == 200 && m.scale == 2 && m.pw == 600 && !strcmp(m.name, "notes"));
    int downs = 0, mons = 0, closes = 0, quit = 0, frames = 0, focus = 0, drops = 0, cw = 0;
    char keys[8] = "", drop[64] = "", full[256] = "";
    while (!quit && frames < 100) {
        goome_canvas *c = goome_begin(g, 0);
        CHECK(c != NULL);
        if (!c) break;
        if (frames == 3) cw = c->w;
        goome_draw_rect(c, 0, 0, 300, 200, 0xFF202830);
        goome_end(g, c, NULL, 0);
        frames++;
        goome_event ev;
        while (goome_poll(g, &ev, 1) > 0) {
            if (ev.type == GOOME_DOWN) downs++;
            if (ev.type == GOOME_MONITORS) mons++;
            if (ev.type == GOOME_CLOSE) closes++;
            if (ev.type == GOOME_FOCUS) focus = focus * 10 + 1 + ev.id;
            if (ev.type == GOOME_KEY) strcat(keys, ev.text);
            if (ev.type == GOOME_DROP) {
                drops++;
                snprintf(drop, sizeof(drop), "%s", ev.msg);
                snprintf(full, sizeof(full), "%s", goome_drop_path(g) ? goome_drop_path(g) : "");
                CHECK(ev.x == 10 && ev.y == 20);
            }
            if (ev.type == GOOME_QUIT) quit = 1;
        }
    }
    CHECK(downs == 1 && mons == 2 && closes == 1 && focus == 12 && !strcmp(keys, "b") && drops == 1 && cw == 800);
    CHECK(strlen(drop) == 63 && !strncmp(full, drop, 63) && strlen(full) > 63 && !strcmp(full + strlen(full) - 8, "file.txt"));
    CHECK(goome_monitors(g, &m, 1) == 1 && m.area.w == 250 && m.area.h == 120);
    goome_image f;
    CHECK(goome_frame(g, &f) == GOOME_OK && f.w == 500 && f.px[0] == 0xFF202830u);
    CHECK(goome_hit(g, 5, 5) == 0);
    CHECK(goome_window_title(g, "renamed") == GOOME_OK && goome_monitors(g, &m, 1) == 1 && !strcmp(m.name, "renamed"));
    CHECK(goome_window_size(g, 500, 400) == GOOME_OK && goome_monitors(g, &m, 1) == 1 && m.area.w == 500 && m.pw == 1000);
    int x = 30, y = 40;
    CHECK(goome_window_pos(g, &x, &y, 1) == GOOME_OK);
    x = y = 0;
    CHECK(goome_window_pos(g, &x, &y, 0) == GOOME_OK && x == 30 && y == 40);
    CHECK(goome_window_state(g, GOOME_STATE_MAXIMIZED) == GOOME_OK && goome_window_get_state(g) == GOOME_STATE_MAXIMIZED);
    CHECK(goome_window_state(g, 9) == GOOME_EARG);
    CHECK(goome_window_drag(g, GOOME_EDGE_LEFT) == GOOME_OK && goome_window_show(g, 1) == GOOME_OK);
    char buf[32];
    CHECK(goome_clipboard_set(g, "copied") == GOOME_OK && goome_clipboard_get(g, buf, sizeof(buf)) == 6 && !strcmp(buf, "copied"));
    CHECK(goome_cursor(g, GOOME_CURSOR_RESIZE_EW) == GOOME_OK && goome_cursor_shape(g) == GOOME_CURSOR_RESIZE_EW);
    CHECK(goome_resize(g, 10, 10, 1) == GOOME_ENOTSUP);
    goome_close(g);

#ifdef _WIN32
    _putenv("GOOME_BACKEND=test");
#else
    setenv("GOOME_BACKEND", "test", 1);
#endif
    g = goome_open_app("app", 640, 480, &err);
    CHECK(g != NULL && err == GOOME_OK);
    if (g) {
        CHECK(goome_mode(g) == GOOME_MODE_WINDOW && (goome_caps(g) & GOOME_CAP_WINDOW));
        CHECK(goome_monitors(g, &m, 1) == 1 && m.area.w == 640 && m.area.h == 480 && !strcmp(m.name, "app"));
        goome_close(g);
    }
    g = goome_open_app(NULL, 0, 0, &err);
    CHECK(g != NULL && goome_monitors(g, &m, 1) == 1 && m.area.w == 960 && m.area.h == 640 && !strcmp(m.name, "goome"));
    goome_close(g);
#ifdef _WIN32
    _putenv("GOOME_BACKEND=");
#else
    unsetenv("GOOME_BACKEND");
#endif

    g = goome_open(&(goome_opts){.backend = GOOME_TEST, .test_script = "end 1"}, &err);
    CHECK(g != NULL);
    if (!g) return;
    CHECK(goome_mode(g) == GOOME_MODE_OVERLAY && !(goome_caps(g) & GOOME_CAP_WINDOW));
    CHECK(goome_window_title(g, "x") == GOOME_ENOTSUP && goome_window_get_state(g) == GOOME_ENOTSUP);
    CHECK(goome_drop_path(g) == NULL);
    goome_close(g);
#ifdef __linux__
    CHECK(goome_open(&(goome_opts){.backend = GOOME_GNOME, .mode = GOOME_MODE_WINDOW}, &err) == NULL && err == GOOME_ENOTSUP);
#endif
}


typedef struct {
    goome *g;
    goome_ui *u;
    int opens, deep, chk, mopen, ok, a, b, cb, ctx, closes, focus_a, focus_in, focus_b;
    char name[32];
    const char *toast;
    int toast_ms;
} kit_t;

static void kit_ev(kit_t *k, int type, double x, double y, int button, int key, int mods, const char *text)
{
    goome_event e;
    memset(&e, 0, sizeof(e));
    e.type = type, e.x = x, e.y = y, e.button = button, e.key = key, e.mods = mods;
    if (text) snprintf(e.text, sizeof(e.text), "%s", text);
    goome_push(k->g, &e);
}
static void kit_click(kit_t *k, double x, double y, int button)
{
    kit_ev(k, GOOME_MOTION, x, y, 0, 0, 0, NULL);
    kit_ev(k, GOOME_DOWN, x, y, button, 0, 0, NULL);
    kit_ev(k, GOOME_UP, x, y, button, 0, 0, NULL);
}
static void kit_key(kit_t *k, int key, int mods)
{
    char t[2] = {0, 0};
    if (key >= 32 && key < 127 && !(mods & (GOOME_CTRL | GOOME_ALT | GOOME_SUPER))) t[0] = (char)key;
    kit_ev(k, GOOME_KEY, 0, 0, 0, key, mods, t);
}
static void kit_type(kit_t *k, const char *s)
{
    for (; *s; s++) kit_key(k, (unsigned char)*s, 0);
}

static uint32_t kit_frame(kit_t *k)
{
    goome_event ev;
    while (goome_poll(k->g, &ev, 0) > 0) {
        if (ev.type == GOOME_CLOSE) k->closes++;
        goome_ui_feed(k->u, &ev);
    }
    goome_ui *u = k->u;
    goome_canvas *c = goome_ui_begin(u, 0);
    if (!c) return 0;
    if (k->toast) goome_ui_toast(u, k->toast, k->toast_ms), k->toast = NULL;
    goome_ui_app(u, "Kit", GOOME_APP_TITLEBAR | GOOME_APP_MENUBAR | GOOME_APP_STATUSBAR);
    if (goome_ui_menubar_begin(u)) {
        if (goome_ui_menu_begin(u, "File")) {
            if (goome_ui_menu_item(u, "Open", "Ctrl+O", NULL)) k->opens++;
            goome_ui_menu_item(u, "Check", NULL, &k->chk);
            goome_ui_menu_separator(u);
            if (goome_ui_menu_begin(u, "More")) {
                if (goome_ui_menu_item(u, "Deep", NULL, NULL)) k->deep++;
                goome_ui_menu_end(u);
            }
            goome_ui_menu_end(u);
        }
        if (goome_ui_menu_begin(u, "Edit")) {
            goome_ui_menu_item(u, "Copy", "Ctrl+C", NULL);
            goome_ui_menu_end(u);
        }
        goome_ui_menubar_end(u);
    }
    if (goome_ui_button(u, "A")) k->a++;
    k->focus_a = goome_ui_focused(u);
    goome_ui_input(u, "Name", k->name, sizeof(k->name));
    k->focus_in = goome_ui_focused(u);
    if (goome_ui_button(u, "B")) k->b++;
    k->focus_b = goome_ui_focused(u);
    if (goome_ui_context_begin(u, "bctx")) {
        if (goome_ui_menu_item(u, "Ctx", NULL, NULL)) k->ctx++;
        goome_ui_context_end(u);
    }
    goome_ui_checkbox(u, "C", &k->cb);
    if (goome_ui_modal_begin(u, "Dialog", &k->mopen)) {
        goome_ui_label(u, "sure?");
        if (goome_ui_button(u, "OK")) k->ok++, goome_ui_popup_close(u);
        goome_ui_modal_end(u);
    }
    goome_ui_statusbar(u, "ready");
    goome_ui_app_end(u);
    goome_ui_end(u);
    return fnv(c->px, c->w, c->h, c->stride);
}

static void kit_frames(kit_t *k, int n)
{
    for (int i = 0; i < n; i++) kit_frame(k);
}

static void kit_dump(kit_t *k, const char *name)
{
    goome_image f;
    if (!gm_dump_dir || goome_frame(k->g, &f) != GOOME_OK) return;
    char path[512];
    snprintf(path, sizeof(path), "%s/%s.ppm", gm_dump_dir, name);
    goome_save_ppm(path, f.px, f.w, f.h, f.stride);
}

static double kit_ink(kit_t *k, int x0, int y0, int x1, int y1)
{
    goome_image f;
    if (goome_frame(k->g, &f) != GOOME_OK) return -1;
    double s = 0;
    for (int y = y0; y < y1 && y < f.h; y++)
        for (int x = x0; x < x1 && x < f.w; x++) s += (f.px[(size_t)y * f.stride + x] >> 8) & 255;
    return s;
}

static void app_kit(void)
{
    int err;
    goome *g = goome_open(&(goome_opts){.backend = GOOME_TEST, .mode = GOOME_MODE_WINDOW, .title = "kit", .width = 420, .height = 300,
                                        .window_flags = GOOME_WINDOW_BORDERLESS | GOOME_WINDOW_RESIZABLE, .test_script = "end 100000"},
                          &err);
    CHECK(g != NULL);
    if (!g) return;
    kit_t k;
    memset(&k, 0, sizeof(k));
    k.g = g, k.u = goome_ui_new(g);
    CHECK(k.u != NULL);
    if (!k.u) {
        goome_close(g);
        return;
    }
    goome_ui_theme(k.u)->font = NULL, goome_ui_theme(k.u)->font_size = 14;
    uint32_t hs[6];
    kit_frames(&k, 3);
    hs[0] = kit_frame(&k);
    kit_dump(&k, "kit0");

    kit_click(&k, 30, 53, 1);
    kit_frames(&k, 11);
    hs[1] = kit_frame(&k);
    kit_dump(&k, "kit1");
    CHECK(goome_hit(g, 100, 89));
    kit_click(&k, 60, 89, 1);
    kit_frames(&k, 2);
    CHECK(k.opens == 1);
    kit_click(&k, 30, 53, 1);
    kit_frames(&k, 3);
    kit_key(&k, GOOME_K_ESC, 0);
    kit_frames(&k, 2);
    kit_click(&k, 60, 89, 1);
    kit_frames(&k, 2);
    CHECK(k.opens == 1);

    kit_click(&k, 30, 53, 1);
    kit_frames(&k, 3);
    kit_key(&k, GOOME_K_DOWN, 0), kit_key(&k, GOOME_K_DOWN, 0), kit_key(&k, GOOME_K_ENTER, 0);
    kit_frames(&k, 2);
    CHECK(k.chk == 1 && k.opens == 1);

    kit_click(&k, 30, 53, 1);
    kit_frames(&k, 3);
    kit_ev(&k, GOOME_MOTION, 60, 152, 0, 0, 0, NULL);
    kit_frames(&k, 12);
    hs[2] = kit_frame(&k);
    kit_dump(&k, "kit2");
    kit_click(&k, 240, 152, 1);
    kit_frames(&k, 2);
    CHECK(k.deep == 1);
    kit_click(&k, 30, 53, 1);
    kit_frames(&k, 3);
    kit_key(&k, GOOME_K_UP, 0), kit_key(&k, GOOME_K_RIGHT, 0);
    kit_frames(&k, 3);
    kit_key(&k, GOOME_K_ESC, 0);
    kit_frames(&k, 2);
    kit_key(&k, GOOME_K_RIGHT, 0);
    kit_frames(&k, 3);
    kit_key(&k, GOOME_K_ENTER, 0);
    kit_frames(&k, 2);
    CHECK(k.deep == 2 && k.opens == 1 && k.chk == 1);
    kit_click(&k, 30, 53, 1);
    kit_frames(&k, 2);
    kit_key(&k, GOOME_K_RIGHT, 0);
    kit_frames(&k, 3);
    kit_key(&k, GOOME_K_LEFT, 0);
    kit_frames(&k, 3);
    kit_key(&k, GOOME_K_ENTER, 0);
    kit_frames(&k, 2);
    CHECK(k.opens == 1);
    kit_key(&k, GOOME_K_DOWN, 0), kit_key(&k, GOOME_K_ENTER, 0);
    kit_frames(&k, 2);
    CHECK(k.opens == 2);

    k.mopen = 1;
    kit_ev(&k, GOOME_MOTION, 300, 250, 0, 0, 0, NULL);
    kit_frames(&k, 13);
    hs[3] = kit_frame(&k);
    kit_dump(&k, "kit3");
    kit_click(&k, 32, 98, 1);
    kit_frames(&k, 2);
    CHECK(k.a == 0 && k.mopen == 1);
    kit_key(&k, GOOME_K_ESC, 0);
    kit_frames(&k, 2);
    CHECK(k.mopen == 0);
    k.mopen = 1;
    kit_frames(&k, 12);
    kit_click(&k, 64, 167, 1);
    kit_frames(&k, 2);
    CHECK(k.ok == 1 && k.mopen == 0);

    kit_key(&k, GOOME_K_TAB, 0);
    kit_frames(&k, 2);
    CHECK(k.focus_a && !k.focus_in);
    kit_key(&k, GOOME_K_TAB, 0);
    kit_frames(&k, 2);
    CHECK(k.focus_in && !k.focus_a);
    kit_key(&k, GOOME_K_TAB, 0);
    kit_frames(&k, 2);
    CHECK(k.focus_b);
    kit_key(&k, GOOME_K_TAB, GOOME_SHIFT);
    kit_frames(&k, 2);
    CHECK(k.focus_in && !k.focus_b);
    kit_key(&k, GOOME_K_TAB, 0);
    kit_frames(&k, 2);
    kit_key(&k, GOOME_K_SPACE, 0);
    kit_frames(&k, 1);
    hs[4] = kit_frame(&k);
    kit_dump(&k, "kit4");
    CHECK(k.b == 1 && k.focus_b);
    kit_key(&k, GOOME_K_TAB, 0), kit_key(&k, GOOME_K_ENTER, 0);
    kit_frames(&k, 2);
    CHECK(k.cb == 0 && k.b == 2);
    kit_key(&k, GOOME_K_SPACE, 0);
    kit_frames(&k, 2);
    CHECK(k.cb == 1);
    kit_key(&k, GOOME_K_TAB, 0);
    kit_frames(&k, 2);
    CHECK(k.focus_a);
    kit_key(&k, GOOME_K_ESC, 0);
    kit_frames(&k, 2);
    CHECK(!k.focus_a);

    kit_click(&k, 210, 159, 1);
    kit_frames(&k, 1);
    CHECK(goome_cursor_shape(g) == GOOME_CURSOR_IBEAM);
    kit_type(&k, "hello world");
    kit_frames(&k, 1);
    kit_key(&k, 'a', GOOME_CTRL), kit_key(&k, 'c', GOOME_CTRL);
    kit_frames(&k, 1);
    char clip[64];
    CHECK(goome_clipboard_get(g, clip, sizeof(clip)) == 11 && !strcmp(clip, "hello world"));
    kit_key(&k, GOOME_K_END, 0);
    for (int i = 0; i < 5; i++) kit_key(&k, GOOME_K_LEFT, GOOME_SHIFT);
    kit_frames(&k, 1);
    hs[5] = kit_frame(&k);
    kit_dump(&k, "kit5");
    kit_key(&k, 'x', GOOME_CTRL);
    kit_frames(&k, 1);
    CHECK(!strcmp(k.name, "hello ") && goome_clipboard_get(g, clip, sizeof(clip)) == 5 && !strcmp(clip, "world"));
    kit_key(&k, 'v', GOOME_CTRL), kit_key(&k, 'v', GOOME_CTRL);
    kit_frames(&k, 1);
    CHECK(!strcmp(k.name, "hello worldworld"));
    kit_key(&k, 'z', GOOME_CTRL);
    kit_frames(&k, 1);
    CHECK(!strcmp(k.name, "hello world"));
    goome_clipboard_set(g, "a\nb\tc");
    kit_key(&k, GOOME_K_HOME, 0), kit_key(&k, 'v', GOOME_CTRL);
    kit_frames(&k, 1);
    CHECK(!strcmp(k.name, "a b chello world"));
    kit_key(&k, GOOME_K_BACKSPACE, GOOME_CTRL), kit_key(&k, GOOME_K_DELETE, 0);
    kit_frames(&k, 1);
    CHECK(!strcmp(k.name, "a b ello world"));
    kit_ev(&k, GOOME_DOWN, 70, 159, 1, 0, 0, NULL), kit_ev(&k, GOOME_UP, 70, 159, 1, 0, 0, NULL);
    kit_ev(&k, GOOME_DOWN, 70, 159, 1, 0, 0, NULL), kit_ev(&k, GOOME_UP, 70, 159, 1, 0, 0, NULL);
    kit_frames(&k, 1);
    kit_type(&k, "hi");
    kit_frames(&k, 1);
    CHECK(!strcmp(k.name, "a b hi world"));
    kit_key(&k, GOOME_K_ENTER, 0);
    kit_frames(&k, 1);

    kit_ev(&k, GOOME_MOTION, 418, 150, 0, 0, 0, NULL);
    kit_frames(&k, 1);
    CHECK(goome_cursor_shape(g) == GOOME_CURSOR_RESIZE_EW);
    kit_ev(&k, GOOME_MOTION, 418, 298, 0, 0, 0, NULL);
    kit_frames(&k, 1);
    CHECK(goome_cursor_shape(g) == GOOME_CURSOR_RESIZE_NWSE);
    kit_ev(&k, GOOME_MOTION, 100, 250, 0, 0, 0, NULL);
    kit_frames(&k, 1);
    CHECK(goome_cursor_shape(g) == GOOME_CURSOR_ARROW);

    kit_click(&k, 32, 196, 3);
    kit_frames(&k, 12);
    kit_dump(&k, "kit6");
    kit_click(&k, 60, 216, 1);
    kit_frames(&k, 2);
    CHECK(k.ctx == 1 && k.b == 2);

    double base = kit_ink(&k, 220, 200, 420, 272), ink[4];
    k.toast = "saved", k.toast_ms = 400;
    int at[4] = {2, 10, 22, 30}, f = 0;
    for (int i = 0; i <= 30; i++) {
        kit_frame(&k);
        if (i == at[f]) ink[f++] = kit_ink(&k, 220, 200, 420, 272) - base;
    }
    printf("toast ink %.0f %.0f %.0f %.0f\n", ink[0], ink[1], ink[2], ink[3]);
    CHECK(ink[0] > 0 && ink[1] > ink[0] && ink[2] < ink[1] && ink[2] > 0 && ink[3] == 0);

    kit_ev(&k, GOOME_DOWN, 150, 18, 1, 0, 0, NULL), kit_ev(&k, GOOME_UP, 150, 18, 1, 0, 0, NULL);
    kit_ev(&k, GOOME_DOWN, 150, 18, 1, 0, 0, NULL), kit_ev(&k, GOOME_UP, 150, 18, 1, 0, 0, NULL);
    kit_frames(&k, 2);
    CHECK(goome_window_get_state(g) == GOOME_STATE_MAXIMIZED);
    kit_click(&k, 397, 18, 1);
    kit_frames(&k, 2);
    CHECK(k.closes == 1);

    static const uint32_t want[6] = {0xaef05fe5u, 0x26bd1b58u, 0xbcba3bddu, 0xbbbab1e5u, 0x863c70c5u, 0x3f3f63e1u};
    printf("kit hashes %08x %08x %08x %08x %08x %08x\n", hs[0], hs[1], hs[2], hs[3], hs[4], hs[5]);
    for (int i = 0; i < 6; i++) CHECK(hs[i] == want[i]);
    goome_ui_free(k.u);
    goome_close(g);
}


typedef struct {
    goome *g;
    goome_ui *u;
    int page, flags, ro_flags, tree_open, sort_col, sort_desc, sort_changes, lc_first, lc_last, links, ival, dropped;
    char ed[256], ro[64], small[12], got[32];
    double fv, nv, frac;
    uint32_t hash;
} w2_t;

static void w2_ev(w2_t *k, int type, double x, double y, int key, int mods, const char *text)
{
    goome_event e;
    memset(&e, 0, sizeof(e));
    e.type = type, e.x = x, e.y = y, e.button = 1, e.key = key, e.mods = mods;
    if (text) snprintf(e.text, sizeof(e.text), "%s", text);
    goome_push(k->g, &e);
}
static void w2_click(w2_t *k, double x, double y)
{
    w2_ev(k, GOOME_MOTION, x, y, 0, 0, NULL);
    w2_ev(k, GOOME_DOWN, x, y, 0, 0, NULL);
    w2_ev(k, GOOME_UP, x, y, 0, 0, NULL);
}
static void w2_key(w2_t *k, int key, int mods)
{
    char t[2] = {0, 0};
    if (key >= 32 && key < 127 && !(mods & (GOOME_CTRL | GOOME_ALT | GOOME_SUPER))) t[0] = (char)key;
    w2_ev(k, GOOME_KEY, 0, 0, key, mods, t);
}
static void w2_scroll(w2_t *k, double x, double y, int dy)
{
    goome_event e;
    memset(&e, 0, sizeof(e));
    e.type = GOOME_SCROLL, e.x = x, e.y = y, e.dy = dy;
    goome_push(k->g, &e);
}
static void w2_type(w2_t *k, const char *s)
{
    for (; *s; s++) w2_key(k, (unsigned char)*s, 0);
}

static void w2_frame(w2_t *k)
{
    static const char *const names[] = {"pear", "apple", "fig", "kiwi", "plum"};
    static const int sizes[] = {3, 9, 1, 7, 5};
    static const float line[] = {2, 5, 3, 8, 6, 9, 4, 7, 5, 10, 6, 3};
    goome_event ev;
    while (goome_poll(k->g, &ev, 0) > 0) goome_ui_feed(k->u, &ev);
    goome_ui *u = k->u;
    goome_canvas *c = goome_ui_begin(u, 0);
    if (!c) return;
    goome_ui_app(u, "W", 0);
    if (k->page == 0) {
        goome_ui_text_edit(u, "ed", k->ed, sizeof(k->ed), 200, k->flags);
        goome_ui_text_edit(u, "ro", k->ro, sizeof(k->ro), 60, GOOME_EDIT_READONLY);
        goome_ui_text_edit(u, "small", k->small, sizeof(k->small), 50, 0);
    } else if (k->page == 1) {
        int order[5] = {0, 1, 2, 3, 4};
        for (int i = 0; i < 5; i++)
            for (int j = i + 1; j < 5; j++) {
                int a = order[i], b = order[j], sw = k->sort_col == 1 ? sizes[a] > sizes[b] : strcmp(names[a], names[b]) > 0;
                if (k->sort_desc) sw = !sw;
                if (sw) order[i] = b, order[j] = a;
            }
        if (goome_ui_table_begin(u, "tab", 2, 0, GOOME_TABLE_SORTABLE | GOOME_TABLE_STRIPES | GOOME_TABLE_BORDERS | GOOME_TABLE_RESIZABLE)) {
            goome_ui_table_column(u, "Name", 0);
            goome_ui_table_column(u, "Size", 120);
            if (goome_ui_table_sort(u, &k->sort_col, &k->sort_desc)) k->sort_changes++;
            for (int i = 0; i < 5; i++) {
                goome_ui_table_row(u);
                goome_ui_label(u, "%s", names[order[i]]);
                goome_ui_table_next(u);
                goome_ui_label(u, "%d", sizes[order[i]]);
            }
        }
        goome_ui_table_end(u);
        if (goome_ui_tree(u, "root", &k->tree_open)) {
            goome_ui_label(u, "child a");
            goome_ui_label(u, "child b");
            goome_ui_tree_end(u);
        }
        goome_ui_scroll_begin(u, "lc", 100);
        goome_ui_list_clip(u, 1000, 0, &k->lc_first, &k->lc_last);
        for (int i = k->lc_first; i < k->lc_last; i++) {
            char t[16];
            snprintf(t, sizeof(t), "row %d", i);
            goome_ui_selectable(u, t, 0);
        }
        goome_ui_scroll_end(u);
    } else if (k->page == 2) {
        goome_ui_row(u, 2);
        goome_ui_drag_float(u, "Float", &k->fv, 0.1, 0, 100, "%.1f");
        goome_ui_drag_int(u, "Int", &k->ival, 0.5, -10, 10);
        goome_ui_number(u, "Num", &k->nv, 2, "%.1f");
        if (goome_ui_link(u, "a link")) k->links++;
        goome_ui_same_line(u);
        goome_ui_badge(u, "new", 0);
        goome_ui_same_line(u);
        goome_ui_badge(u, "light", 0xFFF2C94C);
        goome_ui_same_line(u);
        goome_ui_spinner(u, 18);
        goome_ui_row(u, 2);
        goome_ui_plot_lines(u, "Lines", line, 12, 0, 0, 60);
        goome_ui_plot_bars(u, "Bars", line, 12, 0, 12, 60);
    } else {
        goome_ui_split_begin(u, "sp", 0, &k->frac);
        goome_ui_selectable(u, "drag me", 0);
        goome_ui_drag_source(u, "thing", "payload", 8);
        goome_ui_split_next(u);
        goome_ui_button(u, "drop here");
        int n = goome_ui_drop_target(u, "thing", k->got, sizeof(k->got));
        if (n) k->dropped = n;
        goome_ui_button(u, "wrong type");
        goome_ui_drop_target(u, "other", NULL, 0);
        goome_ui_split_end(u);
    }
    goome_ui_app_end(u);
    goome_ui_end(u);
    k->hash = fnv(c->px, c->w, c->h, c->stride);
}

static void w2_frames(w2_t *k, int n)
{
    for (int i = 0; i < n; i++) w2_frame(k);
}

static void w2_dump(w2_t *k, const char *name)
{
    goome_image f;
    if (!gm_dump_dir || goome_frame(k->g, &f) != GOOME_OK) return;
    char path[512];
    snprintf(path, sizeof(path), "%s/%s.ppm", gm_dump_dir, name);
    goome_save_ppm(path, f.px, f.w, f.h, f.stride);
}

static void w2_fuzz(w2_t *k, int steps, unsigned seed)
{
    static const int keys[] = {GOOME_K_LEFT, GOOME_K_RIGHT, GOOME_K_UP, GOOME_K_DOWN, GOOME_K_HOME, GOOME_K_END, GOOME_K_PGUP, GOOME_K_PGDN,
                               GOOME_K_BACKSPACE, GOOME_K_DELETE, GOOME_K_ENTER, GOOME_K_TAB, 'z', 'y', 'x', 'c', 'v', 'a'};
    static const char *const texts[] = {"a", " ", "\xc3\xa9", "\xe2\x82\xac", "\xf0\x9f\x98\x80", "Z", "\n", "wq"};
    k->page = 0;
    for (int i = 0; i < steps; i++) {
        seed = seed * 1103515245u + 12345u;
        unsigned r = seed >> 8;
        int what = (int)(r % 10);
        double x = 14 + (r >> 4) % 460, y = 14 + (r >> 12) % 210;
        if (what < 2) w2_ev(k, GOOME_DOWN, x, y, 0, (r & 64) ? GOOME_SHIFT : 0, NULL), w2_ev(k, GOOME_MOTION, x + 30, y + 10, 0, 0, NULL), w2_ev(k, GOOME_UP, x + 30, y + 10, 0, 0, NULL);
        else if (what < 3) w2_scroll(k, x, y, (r & 1) ? 1 : -2);
        else if (what < 7) {
            int key = keys[(r >> 5) % (sizeof(keys) / sizeof(keys[0]))], mods = 0;
            if (key < 128 && key >= 'a') mods = GOOME_CTRL | ((r & 1024) ? GOOME_SHIFT : 0);
            else mods = (r & 2048 ? GOOME_SHIFT : 0) | (r & 4096 ? GOOME_CTRL : 0);
            w2_ev(k, GOOME_KEY, 0, 0, key, mods, NULL);
        } else {
            const char *t = texts[(r >> 6) % 8];
            if (t[0] == '\n') w2_ev(k, GOOME_KEY, 0, 0, GOOME_K_ENTER, 0, NULL);
            else w2_ev(k, GOOME_KEY, 0, 0, (unsigned char)t[0], 0, t);
        }
        if ((r >> 3) % 3 == 0) k->flags ^= GOOME_EDIT_WRAP;
        if ((r >> 9) % 41 == 0) goome_clipboard_set(k->g, (r & 1) ? "two\nlines\r\nand\ttab" : "\xe2\x82\xac\xe2\x82\xac\xe2\x82\xac\xe2\x82\xac");
        w2_frame(k);
        CHECK(memchr(k->ed, 0, sizeof(k->ed)) != NULL && memchr(k->small, 0, sizeof(k->small)) != NULL);
        int n = (int)strlen(k->ed);
        for (int j = 0; j < n; j++) {
            unsigned char ch = (unsigned char)k->ed[j];
            int need = ch >= 0xF0 ? 3 : ch >= 0xE0 ? 2 : ch >= 0xC0 ? 1 : 0;
            if ((ch & 0xC0) == 0x80) {
                CHECK(!"stray utf8 continuation");
                break;
            }
            for (int q = 1; q <= need; q++)
                if (j + q >= n || (k->ed[j + q] & 0xC0) != 0x80) {
                    CHECK(!"cut utf8 char");
                    q = need;
                }
            j += need;
        }
        if (fails) break;
    }
}

static void widgets2(void)
{
    int err;
    goome *g = goome_open(&(goome_opts){.backend = GOOME_TEST, .mode = GOOME_MODE_WINDOW, .title = "w2", .width = 480, .height = 360, .test_script = "end 1000000"}, &err);
    CHECK(g != NULL);
    if (!g) return;
    static w2_t k;
    memset(&k, 0, sizeof(k));
    k.g = g, k.u = goome_ui_new(g), k.flags = GOOME_EDIT_LINE_NUMBERS, k.tree_open = 1, k.frac = 0.4;
    CHECK(k.u != NULL);
    if (!k.u) {
        goome_close(g);
        return;
    }
    goome_ui_theme(k.u)->font = NULL, goome_ui_theme(k.u)->font_size = 14;
    snprintf(k.ro, sizeof(k.ro), "read only text");
    uint32_t hs[8];
    char clip[64];

    w2_frames(&k, 2);
    w2_click(&k, 100, 25);
    w2_frames(&k, 1);
    w2_type(&k, "hello world");
    w2_key(&k, GOOME_K_ENTER, 0);
    w2_type(&k, "  foo");
    w2_key(&k, GOOME_K_ENTER, 0);
    w2_type(&k, "bar");
    w2_frames(&k, 2);
    CHECK(!strcmp(k.ed, "hello world\n  foo\n  bar"));
    w2_key(&k, 'z', GOOME_CTRL);
    w2_frames(&k, 1);
    CHECK(!strcmp(k.ed, "hello world\n  foo\n  "));
    w2_key(&k, 'z', GOOME_CTRL);
    w2_frames(&k, 1);
    CHECK(!strcmp(k.ed, "hello world\n  foo"));
    w2_key(&k, 'y', GOOME_CTRL);
    w2_frames(&k, 1);
    CHECK(!strcmp(k.ed, "hello world\n  foo\n  "));
    w2_key(&k, 'z', GOOME_CTRL | GOOME_SHIFT);
    w2_frames(&k, 1);
    CHECK(!strcmp(k.ed, "hello world\n  foo\n  bar"));
    w2_key(&k, GOOME_K_HOME, GOOME_CTRL), w2_key(&k, GOOME_K_END, GOOME_SHIFT), w2_key(&k, 'c', GOOME_CTRL);
    w2_frames(&k, 1);
    CHECK(goome_clipboard_get(g, clip, sizeof(clip)) == 11 && !strcmp(clip, "hello world"));
    w2_key(&k, GOOME_K_END, GOOME_CTRL), w2_key(&k, GOOME_K_ENTER, 0), w2_key(&k, 'v', GOOME_CTRL);
    w2_frames(&k, 1);
    CHECK(!strcmp(k.ed, "hello world\n  foo\n  bar\n  hello world"));
    w2_key(&k, GOOME_K_UP, GOOME_SHIFT), w2_key(&k, GOOME_K_LEFT, GOOME_SHIFT | GOOME_CTRL);
    w2_frames(&k, 3);
    hs[0] = k.hash;
    w2_dump(&k, "w2_edit");
    w2_key(&k, 'x', GOOME_CTRL);
    w2_frames(&k, 1);
    CHECK(!strcmp(k.ed, "hello world\n  foo\n  ") && goome_clipboard_get(g, clip, sizeof(clip)) > 0 && !strcmp(clip, "bar\n  hello world"));
    w2_key(&k, 'z', GOOME_CTRL);
    w2_frames(&k, 1);
    CHECK(!strcmp(k.ed, "hello world\n  foo\n  bar\n  hello world"));

    w2_ev(&k, GOOME_DOWN, 75, 46, 0, 0, NULL), w2_ev(&k, GOOME_UP, 75, 46, 0, 0, NULL);
    w2_ev(&k, GOOME_DOWN, 75, 46, 0, 0, NULL), w2_ev(&k, GOOME_UP, 75, 46, 0, 0, NULL);
    w2_frames(&k, 1);
    w2_type(&k, "baz");
    w2_frames(&k, 1);
    CHECK(!strcmp(k.ed, "hello world\n  baz\n  bar\n  hello world"));
    for (int i = 0; i < 3; i++) w2_ev(&k, GOOME_DOWN, 90, 25, 0, 0, NULL), w2_ev(&k, GOOME_UP, 90, 25, 0, 0, NULL), w2_frames(&k, 1);
    w2_key(&k, GOOME_K_DELETE, 0);
    w2_frames(&k, 1);
    CHECK(!strcmp(k.ed, "  baz\n  bar\n  hello world"));
    w2_key(&k, 'a', GOOME_CTRL), w2_key(&k, GOOME_K_TAB, 0);
    w2_frames(&k, 1);
    CHECK(!strcmp(k.ed, "      baz\n      bar\n      hello world"));
    w2_key(&k, GOOME_K_TAB, GOOME_SHIFT), w2_key(&k, GOOME_K_TAB, GOOME_SHIFT);
    w2_frames(&k, 1);
    CHECK(!strcmp(k.ed, "baz\nbar\nhello world"));
    w2_key(&k, GOOME_K_HOME, GOOME_CTRL), w2_key(&k, GOOME_K_TAB, 0), w2_key(&k, GOOME_K_END, 0);
    w2_key(&k, GOOME_K_HOME, 0), w2_key(&k, GOOME_K_DELETE, 0), w2_key(&k, GOOME_K_HOME, 0), w2_key(&k, 'q', 0);
    w2_frames(&k, 1);
    CHECK(!strcmp(k.ed, "q    az\nbar\nhello world"));
    k.ed[0] = 0;
    w2_frames(&k, 1);
    for (int i = 0; i < 40; i++) {
        char t[8];
        snprintf(t, sizeof(t), "%d", i);
        w2_type(&k, t);
        if (i < 39) w2_key(&k, GOOME_K_ENTER, 0);
    }
    w2_frames(&k, 2);
    w2_key(&k, GOOME_K_HOME, GOOME_CTRL), w2_key(&k, GOOME_K_PGDN, 0), w2_key(&k, GOOME_K_PGDN, 0), w2_key(&k, '#', 0);
    w2_frames(&k, 12);
    hs[1] = k.hash;
    w2_dump(&k, "w2_page");
    CHECK(strstr(k.ed, "\n#14\n") != NULL);
    w2_key(&k, GOOME_K_PGUP, 0), w2_key(&k, '@', 0);
    w2_frames(&k, 1);
    CHECK(strstr(k.ed, "\n7@\n") != NULL);
    snprintf(k.ed, sizeof(k.ed), "a long line of words that has to wrap around inside the box because it is wider than it\nshort");
    k.flags = GOOME_EDIT_WRAP;
    w2_frames(&k, 2);
    w2_key(&k, GOOME_K_HOME, GOOME_CTRL), w2_key(&k, GOOME_K_DOWN, 0), w2_key(&k, '!', 0);
    w2_frames(&k, 12);
    hs[2] = k.hash;
    w2_dump(&k, "w2_wrap");
    char *bang = strchr(k.ed, '!');
    CHECK(bang && bang < strchr(k.ed, '\n'));
    w2_click(&k, 100, 245);
    w2_frames(&k, 1);
    w2_type(&k, "zzz");
    w2_key(&k, 'a', GOOME_CTRL), w2_key(&k, 'c', GOOME_CTRL), w2_key(&k, GOOME_K_BACKSPACE, 0);
    w2_frames(&k, 1);
    CHECK(!strcmp(k.ro, "read only text") && goome_clipboard_get(g, clip, sizeof(clip)) == 14);
    w2_click(&k, 100, 300);
    w2_frames(&k, 1);
    w2_type(&k, "0123456789abcdef");
    w2_frames(&k, 1);
    CHECK(!strcmp(k.small, "0123456789a"));
    goome_clipboard_set(g, "\xe2\x82\xac\xe2\x82\xac");
    w2_key(&k, GOOME_K_HOME, 0);
    for (int i = 0; i < 4; i++) w2_key(&k, GOOME_K_DELETE, 0);
    w2_key(&k, 'v', GOOME_CTRL);
    w2_frames(&k, 1);
    CHECK(!strcmp(k.small, "\xe2\x82\xac" "456789a"));
    w2_key(&k, GOOME_K_ESC, 0);
    w2_frames(&k, 1);

    k.page = 1;
    w2_frames(&k, 3);
    CHECK(k.lc_first == 0 && k.lc_last > 1 && k.lc_last < 10);
    hs[3] = k.hash;
    w2_dump(&k, "w2_table");
    w2_click(&k, 400, 28);
    w2_frames(&k, 2);
    CHECK(k.sort_col == 1 && k.sort_desc == 0 && k.sort_changes == 1);
    w2_click(&k, 400, 28);
    w2_frames(&k, 2);
    CHECK(k.sort_col == 1 && k.sort_desc == 1 && k.sort_changes == 2);
    w2_frames(&k, 10);
    hs[4] = k.hash;
    w2_dump(&k, "w2_sorted");
    w2_scroll(&k, 100, 320, 2);
    w2_frames(&k, 30);
    CHECK(k.lc_first > 0 && k.lc_last > k.lc_first && k.lc_last - k.lc_first < 10);
    w2_click(&k, 60, 203);
    w2_frames(&k, 2);
    CHECK(k.tree_open == 0);

    k.page = 2, k.fv = 5, k.nv = 1;
    w2_frames(&k, 3);
    w2_ev(&k, GOOME_MOTION, 100, 45, 0, 0, NULL), w2_ev(&k, GOOME_DOWN, 100, 45, 0, 0, NULL);
    w2_frames(&k, 1);
    w2_ev(&k, GOOME_MOTION, 150, 45, 0, 0, NULL);
    w2_frames(&k, 1);
    w2_ev(&k, GOOME_UP, 150, 45, 0, 0, NULL);
    w2_frames(&k, 1);
    CHECK(k.fv > 9.9 && k.fv < 10.1);
    w2_ev(&k, GOOME_MOTION, 300, 45, 0, 0, NULL), w2_ev(&k, GOOME_DOWN, 300, 45, 0, 0, NULL);
    w2_frames(&k, 1);
    w2_ev(&k, GOOME_MOTION, 280, 45, 0, 0, NULL);
    w2_frames(&k, 1);
    w2_ev(&k, GOOME_UP, 280, 45, 0, 0, NULL);
    w2_frames(&k, 1);
    CHECK(k.ival == -10);
    w2_click(&k, 460, 106);
    w2_frames(&k, 1);
    w2_click(&k, 460, 106);
    w2_frames(&k, 1);
    CHECK(k.nv == 5);
    w2_click(&k, 28, 106);
    w2_frames(&k, 1);
    CHECK(k.nv == 3);
    w2_click(&k, 200, 106);
    w2_frames(&k, 1);
    w2_key(&k, 'a', GOOME_CTRL), w2_type(&k, "42.5"), w2_key(&k, GOOME_K_ENTER, 0);
    w2_frames(&k, 2);
    CHECK(k.nv == 42.5);
    w2_click(&k, 200, 106);
    w2_frames(&k, 1);
    w2_key(&k, GOOME_K_UP, 0);
    w2_frames(&k, 1);
    w2_key(&k, GOOME_K_ESC, 0);
    w2_frames(&k, 1);
    CHECK(k.nv == 44.5);
    w2_ev(&k, GOOME_DOWN, 100, 45, 0, 0, NULL), w2_ev(&k, GOOME_UP, 100, 45, 0, 0, NULL);
    w2_ev(&k, GOOME_DOWN, 100, 45, 0, 0, NULL), w2_ev(&k, GOOME_UP, 100, 45, 0, 0, NULL);
    w2_frames(&k, 1);
    w2_type(&k, "7.5"), w2_key(&k, GOOME_K_ENTER, 0);
    w2_frames(&k, 2);
    CHECK(k.fv == 7.5);
    w2_click(&k, 40, 140);
    w2_frames(&k, 1);
    CHECK(k.links == 1);
    w2_ev(&k, GOOME_MOTION, 300, 200, 0, 0, NULL);
    w2_frames(&k, 4);
    hs[5] = k.hash;
    w2_dump(&k, "w2_misc");

    k.page = 3;
    w2_frames(&k, 3);
    hs[6] = k.hash;
    w2_dump(&k, "w2_split");
    w2_ev(&k, GOOME_MOTION, 192, 200, 0, 0, NULL), w2_ev(&k, GOOME_DOWN, 192, 200, 0, 0, NULL);
    w2_frames(&k, 1);
    w2_ev(&k, GOOME_MOTION, 240, 200, 0, 0, NULL);
    w2_frames(&k, 1);
    w2_ev(&k, GOOME_UP, 240, 200, 0, 0, NULL);
    w2_frames(&k, 1);
    CHECK(k.frac > 0.49 && k.frac < 0.51);
    w2_ev(&k, GOOME_MOTION, 60, 24, 0, 0, NULL), w2_ev(&k, GOOME_DOWN, 60, 24, 0, 0, NULL);
    w2_frames(&k, 1);
    w2_ev(&k, GOOME_MOTION, 80, 30, 0, 0, NULL);
    w2_frames(&k, 1);
    w2_ev(&k, GOOME_MOTION, 300, 70, 0, 0, NULL);
    w2_frames(&k, 1);
    w2_ev(&k, GOOME_MOTION, 300, 26, 0, 0, NULL);
    w2_frames(&k, 3);
    hs[7] = k.hash;
    w2_dump(&k, "w2_drag");
    CHECK(goome_cursor_shape(g) == GOOME_CURSOR_MOVE);
    w2_ev(&k, GOOME_UP, 300, 26, 0, 0, NULL);
    w2_frames(&k, 2);
    CHECK(k.dropped == 8 && !strcmp(k.got, "payload"));

    static const uint32_t want[8] = {0xb579c00cu, 0x20e6ca04u, 0x45cbca0cu, 0xabf8572bu, 0x4848dcbdu, 0x55ce3962u, 0xbe1be867u, 0x76ca64bbu};
    printf("widget hashes %08x %08x %08x %08x %08x %08x %08x %08x\n", hs[0], hs[1], hs[2], hs[3], hs[4], hs[5], hs[6], hs[7]);
    for (int i = 0; i < 8; i++) CHECK(hs[i] == want[i]);

    k.page = 0, k.flags = GOOME_EDIT_LINE_NUMBERS, k.ed[0] = 0, k.small[0] = 0;
    w2_frames(&k, 1);
    w2_click(&k, 100, 25);
    w2_fuzz(&k, getenv("GOOME_FUZZ") ? 200000 : 3000, 12345);

    static uint32_t px[120 * 90], bp[12 * 12];
    for (int y = 0; y < 12; y++)
        for (int x = 0; x < 12; x++) bp[y * 12 + x] = x < 4 || y < 4 || x >= 8 || y >= 8 ? 0xFF2050C0u : 0x80404040u;
    goome_bitmap b = {bp, 12, 12, 12, 0};
    uint32_t nh[2];
    for (int s = 0; s < 2; s++) {
        goome_canvas c;
        memset(&c, 0, sizeof(c));
        memset(px, 0, sizeof(px));
        c.px = px, c.w = 120, c.h = 90, c.stride = 120, c.scale = s ? 1.5 : 1;
        goome_draw_bitmap_nine(&c, &b, 3.3, 2.7, 70, 50, 4, 4, 4, 4);
        goome_draw_bitmap_nine(&c, &b, 2, 56, 5, 5, 4, 4, 4, 4);
        nh[s] = fnv(px, 120, 90, 120);
        if (!s) CHECK(px[2 * 120 + 3] == 0 && px[3 * 120 + 3] == 0xFF2050C0u && px[30 * 120 + 40] == 0x80404040u && px[30 * 120 + 72] == 0xFF2050C0u);
    }
    printf("nine hashes %08x %08x\n", nh[0], nh[1]);
    CHECK(nh[0] == 0x638fa7a8u && nh[1] == 0xdd482365u);
    goome_ui_free(k.u);
    goome_close(g);
}

static void grab_run(int autog, int *wants, int *grabbed, double *val, int n)
{
    int err;
    goome *g = goome_open(&(goome_opts){.backend = GOOME_TEST,
                                        .test_script = "screen 400x360 1; 1 hover 35,71; 2 hold 40,157 8; 3 hover 120,157; 4 hover 380,300; 6 grabbed; "
                                                       "9 hover 380,300; 10 click 100,213; 12 key \\e; 14 hover 55,240; 15 click 55,240; 18 key \\e; 19 hover 380,300; end 22"},
                          &err);
    CHECK(g != NULL);
    if (!g) return;
    CHECK(goome_caps(g) & GOOME_CAP_GRAB);
    goome_ui *u = goome_ui_new(g);
    if (u) goome_ui_theme(u)->font = NULL, goome_ui_theme(u)->font_size = 14;
    if (u && !autog) goome_ui_auto_grab(u, 0);
    double x = 10, y = 10, w = 220, h = 0;
    int quit = 0, frames = 0, modal = 0, ctl = -1;
    char text[16] = "";
    while (!quit && frames < n && u) {
        goome_event ev;
        while (goome_poll(g, &ev, 0) > 0) {
            if (ev.type == GOOME_QUIT) quit = 1;
            if (ev.type == GOOME_CONTROL && !strncmp(ev.msg, "grabbed ", 8)) ctl = ev.msg[8] - '0';
            goome_ui_feed(u, &ev);
        }
        if (!goome_ui_begin(u, 0)) break;
        if (goome_ui_window(u, "grab", &x, &y, &w, &h, 0, NULL)) {
            goome_ui_button(u, "Go");
            goome_ui_space(u, 22);
            goome_ui_slider(u, "Val", &val[frames], 0, 1, NULL);
            goome_ui_input(u, "Text", text, sizeof(text));
            if (goome_ui_button(u, "Modal")) modal = 1;
        }
        goome_ui_window_end(u);
        if (modal && goome_ui_modal_begin(u, "Sure", &modal)) {
            goome_ui_label(u, "really");
            goome_ui_modal_end(u);
        }
        goome_ui_end(u);
        wants[frames] = goome_ui_wants(u), grabbed[frames] = goome_grabbed(g);
        if (frames + 1 < n) val[frames + 1] = val[frames];
        frames++;
    }
    CHECK(ctl == autog);
    goome_ui_free(u);
    CHECK(!goome_grabbed(g));
    goome_close(g);
}

static void grab(void)
{
    enum { N = 24 };
    int wants[N] = {0}, grabbed[N] = {0}, w0[N] = {0}, g0[N] = {0};
    double val[N] = {0}, v0[N] = {0};
    grab_run(1, wants, grabbed, val, N);
    printf("grab wants");
    for (int i = 0; i < N; i++) printf(" %d%s", wants[i], grabbed[i] ? "g" : "");
    printf("  val %.2f\n", val[7]);
    CHECK(wants[1] == GOOME_WANT_MOUSE && !grabbed[1]);
    CHECK(wants[3] == GOOME_WANT_MOUSE && grabbed[3]);
    CHECK(wants[6] == GOOME_WANT_MOUSE && grabbed[6]);
    CHECK(val[7] > 0.99);
    CHECK(!grabbed[9] && wants[9] == 0);
    CHECK(wants[11] & GOOME_WANT_KEYBOARD);
    CHECK(wants[13] == 0);
    CHECK(wants[16] == (GOOME_WANT_MOUSE | GOOME_WANT_KEYBOARD) && !grabbed[16]);
    CHECK(wants[20] == 0);
    for (int i = 0; i < N; i++) CHECK(!grabbed[i] || (i >= 2 && i <= 8));
    grab_run(0, w0, g0, v0, N);
    for (int i = 0; i < N; i++) CHECK(!g0[i]);
    CHECK(w0[1] == GOOME_WANT_MOUSE && w0[6] == GOOME_WANT_MOUSE && w0[9] == 0 && (w0[11] & GOOME_WANT_KEYBOARD));

    int err;
    goome *g = goome_open(&(goome_opts){.mode = GOOME_MODE_OFFSCREEN, .width = 100, .height = 80}, &err);
    CHECK(g && !(goome_caps(g) & GOOME_CAP_GRAB));
    CHECK(goome_grab(g, 1) == GOOME_OK && goome_grabbed(g) == 1);
    CHECK(goome_grab(g, 0) == GOOME_OK && goome_grabbed(g) == 0);
    goome_close(g);
    CHECK(goome_grab(NULL, 1) == GOOME_EARG && goome_grabbed(NULL) == 0 && goome_ui_wants(NULL) == 0);
}

typedef struct {
    goome *g;
    goome_ui *u;
    int mode, push, flags, stn, rets;
    goome_ui_item it, pos, flow, sized;
    double *st, *st2;
    char buf[16];
    uint32_t hash;
} cs_t;

static void cs_ev(cs_t *k, int type, double x, double y, int button, int key, int mods)
{
    goome_event e;
    memset(&e, 0, sizeof(e));
    e.type = type, e.x = x, e.y = y, e.button = button, e.key = key, e.mods = mods;
    goome_push(k->g, &e);
}

static uint32_t cs_px(cs_t *k, int x, int y)
{
    goome_image f;
    if (goome_frame(k->g, &f) != GOOME_OK || x < 0 || y < 0 || x >= f.w || y >= f.h) return 0;
    return f.px[(size_t)y * f.stride + x];
}

static void cs_frame(cs_t *k)
{
    goome_event ev;
    while (goome_poll(k->g, &ev, 0) > 0) goome_ui_feed(k->u, &ev);
    goome_ui *u = k->u;
    goome_canvas *c = goome_ui_begin(u, 0);
    if (!c) return;
    if (k->mode == 0) {
        goome_theme t = *goome_ui_theme(u);
        goome_ui_theme_light(&t);
        if (k->push == 1) goome_ui_push_num(u, GOOME_S_RADIUS, 0);
        if (k->push == 2) goome_ui_push_radii(u, (goome_radii){14, 0, 14, 0});
        if (k->push == 3) goome_ui_push_color(u, GOOME_S_INPUT_BG, 0xFFC03030), goome_ui_push_num(u, GOOME_S_BORDER_WIDTH, 3);
        if (k->push == 4) goome_ui_push_theme(u, &t);
        if (k->push == 5) goome_ui_push_num(u, GOOME_S_FONT_SIZE, 22);
        if (k->push == 6) goome_ui_push_num(u, GOOME_S_RADIUS, 0), goome_ui_push_num(u, GOOME_S_PAD, 30);
        goome_ui_button(u, "Push");
        goome_ui_input(u, "In", k->buf, sizeof(k->buf));
        if (k->push >= 1 && k->push <= 5) goome_ui_pop(u, k->push == 3 ? 2 : 1);
        if (k->push == 7) goome_ui_pop(u, 3);
        goome_ui_button(u, "After");
    } else if (k->mode == 1) {
        goome_ui_pos(u, 37, 21);
        goome_ui_custom(u, "p", 50, 30, GOOME_ITEM_NO_HIT, &k->pos);
        goome_ui_custom(u, "f", 0, 0, GOOME_ITEM_NO_HIT, &k->flow);
        goome_ui_size(u, 120, 40);
        goome_ui_custom(u, "s", 0, 0, GOOME_ITEM_NO_HIT, &k->sized);
        goome_ui_pos(u, 150, 60);
        k->rets += goome_ui_custom(u, "k", 60, 60, k->flags, &k->it);
        k->st = goome_ui_store(u, "st", k->stn);
        goome_ui_push_id(u, "other");
        k->st2 = goome_ui_store(u, "st", 1);
        goome_ui_pop_id(u);
    } else {
        goome_canvas *r = goome_ui_canvas(u);
        if (r) goome_draw_rect(r, 30, 30, 20, 20, 0xFFFF00FF), goome_draw_rect(r, 240, 150, 20, 20, 0xFFFF00FF);
        double x = 20, y = 20, w = 200, h = 120;
        if (goome_ui_window(u, "cv", &x, &y, &w, &h, 0, NULL)) {
            goome_canvas *wc = goome_ui_canvas(u);
            if (wc) goome_draw_rect(wc, 40, 70, 150, 40, 0xFFFF00FF), goome_draw_rect(wc, 0, 0, 400, 300, 0x00000000), goome_draw_rect(wc, 250, 20, 30, 30, 0xFF00FFFF);
            goome_ui_space(u, 26);
            goome_ui_button(u, "Over");
        }
        goome_ui_window_end(u);
    }
    goome_ui_end(u);
    k->hash = fnv(c->px, c->w, c->h, c->stride);
}

static void custom_style(void)
{
    int err;
    cs_t k;
    memset(&k, 0, sizeof(k));
    k.g = goome_open(&(goome_opts){.backend = GOOME_TEST, .mode = GOOME_MODE_WINDOW, .width = 300, .height = 200, .test_script = "end 100000"}, &err);
    CHECK(k.g != NULL);
    if (!k.g) return;
    k.u = goome_ui_new(k.g);
    if (!k.u) return;
    goome_ui_theme(k.u)->font = NULL, goome_ui_theme(k.u)->font_size = 14;
    snprintf(k.buf, sizeof(k.buf), "hi");

    cs_frame(&k);
    cs_frame(&k);
    uint32_t base = k.hash, hs[8];
    for (int i = 1; i <= 7; i++) {
        if (i == 6) continue;
        k.push = i;
        cs_frame(&k);
        hs[i] = k.hash;
        k.push = 0;
        cs_frame(&k);
        CHECK(k.hash == base);
    }
    for (int i = 1; i <= 5; i++) CHECK(hs[i] != base);
    for (int i = 1; i < 5; i++) CHECK(hs[i] != hs[i + 1]);
    CHECK(strstr(goome_error(k.g), "pop") != NULL);
    CHECK(hs[7] == base);
    goome_ui_free(k.u);
    k.u = goome_ui_new(k.g);
    goome_ui_theme(k.u)->font = NULL, goome_ui_theme(k.u)->font_size = 14;
    k.push = 6;
    cs_frame(&k);
    CHECK(strstr(goome_error(k.g), "style stack") != NULL);
    k.push = 0;
    cs_frame(&k);
    cs_frame(&k);
    CHECK(k.hash == base);
    goome_ui_pop(k.u, 2);
    goome_ui_push_num(k.u, GOOME_S_RADIUS, 0);
    cs_frame(&k);
    CHECK(k.hash == base);

    k.mode = 1, k.stn = 3;
    cs_frame(&k);
    CHECK(k.pos.x == 47 && k.pos.y == 31 && k.pos.w == 50 && k.pos.h == 30);
    CHECK(k.flow.x == 10 && k.flow.y == 10 && k.flow.w == 280);
    CHECK(k.sized.w == 120 && k.sized.h == 40 && k.sized.y > k.flow.y);
    CHECK(k.it.x == 160 && k.it.y == 70 && !k.it.hovered && k.rets == 0);
    CHECK(k.st && k.st[0] == 0 && k.st[2] == 0 && k.st2 && k.st2 != k.st);
    if (k.st) k.st[0] = 4.5, k.st[2] = -1;
    if (k.st2) k.st2[0] = 9;
    cs_frame(&k);
    CHECK(k.st && k.st[0] == 4.5 && k.st[2] == -1 && k.st2 && k.st2[0] == 9);
    k.stn = 6;
    cs_frame(&k);
    CHECK(k.st && k.st[0] == 4.5 && k.st[2] == -1 && k.st[5] == 0);

    k.flags = GOOME_ITEM_DRAG_Y;
    cs_ev(&k, GOOME_MOTION, 190, 100, 0, 0, 0);
    cs_frame(&k);
    CHECK(k.it.hovered && !k.it.pressed && !goome_grabbed(k.g));
    cs_ev(&k, GOOME_DOWN, 190, 100, 1, 0, 0);
    cs_frame(&k);
    CHECK(k.it.pressed && k.it.held && goome_grabbed(k.g));
    k.rets = 0;
    cs_ev(&k, GOOME_MOTION, 203, 117, 0, 0, 0);
    cs_frame(&k);
    CHECK(k.it.held && !k.it.pressed && k.it.dx == 0 && k.it.dy == 17 && k.it.drag_y == 17 && k.it.drag_x == 0 && k.rets == 1);
    cs_ev(&k, GOOME_MOTION, 260, 180, 0, 0, 0);
    cs_frame(&k);
    CHECK(k.it.held && k.it.dy == 63 && k.it.drag_y == 80 && !k.it.hovered && goome_grabbed(k.g));
    cs_ev(&k, GOOME_UP, 260, 180, 1, 0, 0);
    cs_frame(&k);
    CHECK(k.it.released && !k.it.clicked && !k.it.held && !goome_grabbed(k.g));
    cs_frame(&k);
    CHECK(!k.it.released && k.it.drag_y == 0);

    k.flags = GOOME_ITEM_DRAG_X;
    cs_ev(&k, GOOME_MOTION, 190, 100, 0, 0, 0);
    cs_ev(&k, GOOME_DOWN, 190, 100, 1, 0, 0);
    cs_frame(&k);
    cs_ev(&k, GOOME_MOTION, 180, 140, 0, 0, 0);
    cs_frame(&k);
    CHECK(k.it.dx == -10 && k.it.dy == 0 && k.it.drag_x == -10 && k.it.drag_y == 0);
    k.flags = GOOME_ITEM_DRAG_X | GOOME_ITEM_DRAG_Y;
    cs_ev(&k, GOOME_MOTION, 185, 120, 0, 0, 0);
    cs_frame(&k);
    CHECK(k.it.dx == 5 && k.it.dy == -20 && k.it.drag_x == -5 && k.it.drag_y == 20);
    cs_ev(&k, GOOME_UP, 185, 120, 1, 0, 0);
    cs_frame(&k);
    CHECK(k.it.released && k.it.clicked);

    k.flags = 0;
    for (int i = 0; i < 32; i++) cs_frame(&k);
    cs_ev(&k, GOOME_DOWN, 185, 120, 1, 0, 0);
    cs_frame(&k);
    CHECK(k.it.pressed && !goome_grabbed(k.g) && !k.it.focused);
    cs_ev(&k, GOOME_UP, 185, 120, 1, 0, 0);
    cs_frame(&k);
    CHECK(k.it.clicked && !k.it.double_clicked);
    cs_ev(&k, GOOME_DOWN, 185, 120, 1, 0, 0);
    cs_frame(&k);
    CHECK(k.it.double_clicked);
    cs_ev(&k, GOOME_UP, 185, 120, 1, 0, 0);
    cs_ev(&k, GOOME_DOWN, 185, 120, 3, 0, 0);
    cs_frame(&k);
    CHECK(k.it.right_clicked && !k.it.pressed);
    cs_ev(&k, GOOME_UP, 185, 120, 3, 0, 0);
    cs_frame(&k);

    k.flags = GOOME_ITEM_REPEAT;
    int clicks = 0;
    cs_ev(&k, GOOME_DOWN, 185, 120, 1, 0, 0);
    for (int i = 0; i < 40; i++) cs_frame(&k), clicks += k.it.clicked;
    cs_ev(&k, GOOME_UP, 185, 120, 1, 0, 0);
    cs_frame(&k);
    clicks += k.it.clicked;
    CHECK(clicks >= 4 && clicks <= 8);

    k.flags = GOOME_ITEM_FOCUSABLE;
    cs_ev(&k, GOOME_MOTION, 5, 195, 0, 0, 0);
    cs_frame(&k);
    CHECK(!k.it.focused);
    cs_ev(&k, GOOME_KEY, 0, 0, 0, GOOME_K_TAB, 0);
    cs_frame(&k);
    cs_frame(&k);
    CHECK(k.it.focused && (goome_ui_wants(k.u) & GOOME_WANT_KEYBOARD));
    cs_ev(&k, GOOME_KEY, 0, 0, 0, GOOME_K_LEFT, GOOME_SHIFT);
    cs_frame(&k);
    CHECK(k.it.key == (GOOME_K_LEFT | GOOME_SHIFT) && !k.it.clicked);
    cs_ev(&k, GOOME_KEY, 0, 0, 0, GOOME_K_ENTER, 0);
    cs_frame(&k);
    CHECK(k.it.key == GOOME_K_ENTER && k.it.clicked);
    cs_ev(&k, GOOME_KEY, 0, 0, 0, GOOME_K_ESC, 0);
    cs_frame(&k);
    CHECK(k.it.key == GOOME_K_ESC && !k.it.focused);
    cs_frame(&k);
    CHECK(!k.it.focused && !k.it.key);
    cs_ev(&k, GOOME_MOTION, 190, 100, 0, 0, 0);
    cs_ev(&k, GOOME_DOWN, 190, 100, 1, 0, 0);
    cs_ev(&k, GOOME_UP, 190, 100, 1, 0, 0);
    cs_frame(&k);
    cs_frame(&k);
    CHECK(k.it.focused);
    cs_ev(&k, GOOME_DOWN, 20, 190, 1, 0, 0);
    cs_ev(&k, GOOME_UP, 20, 190, 1, 0, 0);
    cs_frame(&k);
    cs_frame(&k);
    CHECK(!k.it.focused);

    k.flags = GOOME_ITEM_NO_HIT;
    cs_ev(&k, GOOME_MOTION, 190, 100, 0, 0, 0);
    cs_ev(&k, GOOME_DOWN, 190, 100, 1, 0, 0);
    cs_frame(&k);
    CHECK(!k.it.hovered && !k.it.pressed);
    cs_ev(&k, GOOME_UP, 190, 100, 1, 0, 0);
    cs_frame(&k);

    k.mode = 2;
    cs_ev(&k, GOOME_MOTION, 290, 190, 0, 0, 0);
    cs_frame(&k);
    cs_frame(&k);
    CHECK(cs_px(&k, 185, 75) == 0xFFFF00FF);
    CHECK(cs_px(&k, 245, 155) == 0xFFFF00FF);
    CHECK(cs_px(&k, 45, 45) != 0xFFFF00FF);
    CHECK(cs_px(&k, 265, 35) != 0xFF00FFFF);
    CHECK(cs_px(&k, 189, 100) == 0xFFFF00FF && cs_px(&k, 200, 100) != 0xFFFF00FF);
    int over = 0;
    for (int x = 45; x < 75; x++) over += cs_px(&k, x, 95) != 0xFFFF00FF;
    CHECK(over > 20);
    printf("custom style %08x %08x %08x %08x %08x clicks %d\n", base, hs[1], hs[2], hs[3], hs[4], clicks);
    CHECK(base == 0x3aad9f4bu && hs[1] == 0x6f70852fu && hs[2] == 0xd88ed7d3u && hs[3] == 0x539c0ad8u && hs[4] == 0x45c11e92u);
    goome_ui_free(k.u);
    goome_close(k.g);

    goome *g = goome_open(&(goome_opts){.backend = GOOME_TEST, .test_script = "screen 300x200 1; end 100000"}, &err);
    CHECK(g != NULL);
    if (!g) return;
    goome_ui *u = goome_ui_new(g);
    for (int i = 0; u && i < 2; i++) {
        goome_ui_item it;
        if (!goome_ui_begin(u, 0)) break;
        goome_ui_pos(u, 100, 50);
        goome_ui_custom(u, "hit", 40, 30, 0, &it);
        goome_ui_pos(u, 10, 150);
        goome_ui_custom(u, "nohit", 40, 30, GOOME_ITEM_NO_HIT, &it);
        goome_ui_end(u);
    }
    CHECK(goome_hit(g, 125, 75) && !goome_hit(g, 105, 45) && !goome_hit(g, 30, 175));
    goome_ui_free(u);
    goome_close(g);
}

enum { IS_COMBO, IS_COMBO_OPEN, IS_HEADER, IS_DRAG, IS_CHECK, IS_RADIO, IS_WCLOSE, IS_APP, IS_APP_MAX, IS_WRESIZE, IS_SORT_UP, IS_SORT_DOWN, IS_NUMBER, IS_SLIDER, IS_TOGGLE, IS_WOPEN, IS_WSHUT, IS_SPLIT, IS_TOAST, IS_COLOR, IS_TABS, IS_SCROLL, IS_TIP, IS_MENU, IS_POPUP };

typedef struct {
    uint32_t hash;
    uint32_t px[320 * 240];
} ic_out;

static void ic_ev(goome *g, int type, double x, double y)
{
    goome_event e;
    memset(&e, 0, sizeof(e));
    e.type = type, e.x = x, e.y = y, e.button = 1;
    goome_push(g, &e);
}

static void ic_run(int scene, int icon, int skin, const goome_bitmap *bm, const goome_skin *sk, int mode, int tint, ic_out *out)
{
    int err;
    memset(out, 0, sizeof(*out));
    goome *g = goome_open(&(goome_opts){.backend = GOOME_TEST, .mode = GOOME_MODE_WINDOW, .width = 320, .height = 240, .test_script = "end 100000"}, &err);
    CHECK(g != NULL);
    if (!g) return;
    goome_ui *u = goome_ui_new(g);
    if (!u) {
        goome_close(g);
        return;
    }
    goome_theme *th = goome_ui_theme(u);
    th->font = NULL, th->font_size = 14, th->anim_ms = 0, th->icon_tint = (uint32_t)tint;
    if (mode == 3 && icon >= 0) th->icon[icon] = bm;
    if (scene == IS_APP_MAX) goome_window_state(g, GOOME_STATE_MAXIMIZED);
    static const char *const items[] = {"one", "two", "three"};
    static const char *const tabs[] = {"A", "B", "C"};
    int sel = 1, on = 1, off = 0, rad = 2, open = 1, col = 0, desc = scene == IS_SORT_DOWN, frames = scene == IS_TIP || scene == IS_TOAST ? 40 : 4;
    double v = 0.5, num = 3, wx = 20, wy = 20, ww = 200, wh = 0, frac = 0.5;
    uint32_t argb = 0xFF3080C0;
    for (int f = 0; f < frames; f++) {
        if (f == 1) {
            if (scene == IS_COMBO_OPEN || scene == IS_MENU) ic_ev(g, GOOME_DOWN, 40, 20), ic_ev(g, GOOME_UP, 40, 20);
            if (scene == IS_COLOR) ic_ev(g, GOOME_DOWN, 40, 18), ic_ev(g, GOOME_UP, 40, 18);
            if (scene == IS_WSHUT) ic_ev(g, GOOME_DOWN, 36, 32), ic_ev(g, GOOME_UP, 36, 32);
            if (scene == IS_DRAG || scene == IS_TIP) ic_ev(g, GOOME_MOTION, 60, 20);
        }
        if (f == 2 && (scene == IS_MENU || scene == IS_COMBO_OPEN || scene == IS_COLOR || scene == IS_WSHUT)) ic_ev(g, GOOME_MOTION, 300, 230);
        goome_event ev;
        while (goome_poll(g, &ev, 0) > 0) goome_ui_feed(u, &ev);
        goome_canvas *c = goome_ui_begin(u, 0);
        if (!c) break;
        if (mode == 1 || mode == 2) {
            if (icon >= 0) goome_ui_push_icon(u, icon, bm);
            if (skin >= 0) goome_ui_push_skin(u, skin, sk);
            if (mode == 2) goome_ui_pop(u, 1);
        }
        if (scene == IS_APP || scene == IS_APP_MAX) goome_ui_app(u, "app", GOOME_APP_TITLEBAR);
        if (scene == IS_COMBO || scene == IS_COMBO_OPEN) goome_ui_combo(u, "##c", &sel, items, 3);
        if (scene == IS_HEADER) goome_ui_collapsing(u, "head", &off);
        if (scene == IS_DRAG) goome_ui_drag_float(u, "##d", &v, 0.1, 0, 1, NULL);
        if (scene == IS_CHECK) goome_ui_checkbox(u, "check", &on);
        if (scene == IS_RADIO) goome_ui_radio(u, "radio", &rad, 2);
        if (scene == IS_NUMBER) goome_ui_number(u, "##n", &num, 1, "%.0f");
        if (scene == IS_SLIDER) goome_ui_slider(u, "##s", &v, 0, 1, NULL);
        if (scene == IS_TOGGLE) goome_ui_toggle(u, "toggle", &off);
        if (scene == IS_TABS) goome_ui_tabs(u, "t", tabs, 3, &sel);
        if (scene == IS_COLOR) goome_ui_color(u, "##col", &argb);
        if (scene == IS_TOAST && f == 0) goome_ui_toast(u, "hey", 5000);
        if (scene == IS_TIP) {
            goome_ui_button(u, "tip button");
            goome_ui_tooltip(u, "a tip");
        }
        if (scene == IS_MENU && goome_ui_menu_begin(u, "Menu")) {
            goome_ui_menu_item(u, "Open", NULL, NULL);
            goome_ui_menu_end(u);
        }
        if (scene == IS_POPUP) {
            goome_ui_button(u, "pop");
            if (f == 0) goome_ui_popup_open(u, "p");
            if (goome_ui_popup_begin(u, "p")) {
                goome_ui_label(u, "inside");
                goome_ui_popup_end(u);
            }
        }
        if (scene == IS_SCROLL) {
            goome_ui_scroll_begin(u, "sc", 80);
            for (int i = 0; i < 12; i++) goome_ui_label(u, "row %d", i);
            goome_ui_scroll_end(u);
        }
        if (scene == IS_SORT_UP || scene == IS_SORT_DOWN) {
            if (goome_ui_table_begin(u, "tb", 2, 0, GOOME_TABLE_SORTABLE)) {
                goome_ui_table_column(u, "Name", 0);
                goome_ui_table_column(u, "Size", 0);
                goome_ui_table_sort(u, &col, &desc);
                goome_ui_table_row(u);
                goome_ui_label(u, "a");
                goome_ui_table_next(u);
                goome_ui_label(u, "1");
            }
            goome_ui_table_end(u);
        }
        if (scene == IS_SPLIT) {
            goome_ui_split_begin(u, "sp", 0, &frac);
            goome_ui_label(u, "left");
            goome_ui_split_next(u);
            goome_ui_label(u, "right");
            goome_ui_split_end(u);
        }
        if (scene == IS_WCLOSE || scene == IS_WRESIZE || scene == IS_WOPEN || scene == IS_WSHUT) {
            int fl = scene == IS_WCLOSE ? GOOME_WIN_CLOSE : scene == IS_WRESIZE ? GOOME_WIN_RESIZE : GOOME_WIN_COLLAPSE;
            if (goome_ui_window(u, "win", &wx, &wy, &ww, &wh, fl, scene == IS_WCLOSE ? &open : NULL)) goome_ui_label(u, "body");
            goome_ui_window_end(u);
        }
        if (scene == IS_APP || scene == IS_APP_MAX) goome_ui_app_end(u);
        if (mode == 1) goome_ui_pop(u, 1);
        goome_ui_end(u);
        if (f == frames - 1) {
            out->hash = fnv(c->px, c->w, c->h, c->stride);
            for (int y = 0; y < 240 && y < c->h; y++)
                for (int x = 0; x < 320 && x < c->w; x++) out->px[y * 320 + x] = c->px[(size_t)y * c->stride + x];
        }
    }
    goome_ui_free(u);
    goome_close(g);
}

static int ic_diff(const ic_out *a, const ic_out *b, int *bw, int *bh, uint32_t want)
{
    int x0 = 320, y0 = 240, x1 = -1, y1 = -1, seen = 0;
    for (int y = 0; y < 240; y++)
        for (int x = 0; x < 320; x++) {
            uint32_t p = b->px[y * 320 + x];
            if (p == want || (want == 0xFFFF00FF && (p >> 16 & 255) > (p >> 8 & 255) + 60 && (p & 255) > (p >> 8 & 255) + 60)) seen = 1;
            if (a->px[y * 320 + x] == b->px[y * 320 + x]) continue;
            if (x < x0) x0 = x;
            if (y < y0) y0 = y;
            if (x > x1) x1 = x;
            if (y > y1) y1 = y;
        }
    *bw = x1 < 0 ? 0 : x1 - x0 + 1, *bh = y1 < 0 ? 0 : y1 - y0 + 1;
    return seen;
}

static void icons(void)
{
    static const int cases[][2] = {
        {GOOME_ICON_ARROW_DOWN, IS_COMBO}, {GOOME_ICON_ARROW_RIGHT, IS_HEADER}, {GOOME_ICON_ARROW_UP, IS_COMBO_OPEN}, {GOOME_ICON_ARROW_LEFT, IS_DRAG},
        {GOOME_ICON_CHECK, IS_CHECK}, {GOOME_ICON_RADIO_DOT, IS_RADIO}, {GOOME_ICON_CLOSE, IS_WCLOSE}, {GOOME_ICON_MINIMIZE, IS_APP},
        {GOOME_ICON_MAXIMIZE, IS_APP}, {GOOME_ICON_RESTORE, IS_APP_MAX}, {GOOME_ICON_RESIZE, IS_WRESIZE}, {GOOME_ICON_SORT_UP, IS_SORT_UP},
        {GOOME_ICON_SORT_DOWN, IS_SORT_DOWN}, {GOOME_ICON_PLUS, IS_NUMBER}, {GOOME_ICON_MINUS, IS_NUMBER}, {GOOME_ICON_SLIDER_KNOB, IS_SLIDER},
        {GOOME_ICON_TOGGLE_KNOB, IS_TOGGLE}, {GOOME_ICON_COLLAPSE, IS_WOPEN}, {GOOME_ICON_EXPAND, IS_WSHUT}, {GOOME_ICON_DRAG_HANDLE, IS_SPLIT},
        {GOOME_ICON_TOAST, IS_TOAST}, {GOOME_ICON_PICKER, IS_COLOR},
    };
    static const int skins[][2] = {
        {GOOME_SKIN_CHECKBOX, IS_CHECK}, {GOOME_SKIN_RADIO, IS_RADIO}, {GOOME_SKIN_TOGGLE, IS_TOGGLE}, {GOOME_SKIN_SLIDER_TRACK, IS_SLIDER},
        {GOOME_SKIN_SLIDER_FILL, IS_SLIDER}, {GOOME_SKIN_SCROLLBAR, IS_SCROLL}, {GOOME_SKIN_TAB, IS_TABS}, {GOOME_SKIN_TITLEBAR, IS_WOPEN},
        {GOOME_SKIN_TOOLTIP, IS_TIP}, {GOOME_SKIN_MENU, IS_MENU}, {GOOME_SKIN_POPUP, IS_POPUP}, {GOOME_SKIN_MENU, IS_COMBO_OPEN},
        {GOOME_SKIN_POPUP, IS_COLOR}, {GOOME_SKIN_TITLEBAR, IS_APP},
    };
    int nc = (int)(sizeof(cases) / sizeof(cases[0])), ns = (int)(sizeof(skins) / sizeof(skins[0]));
    CHECK(nc == GOOME_ICON_COUNT);
    uint32_t mag[64], cyan[36];
    for (int i = 0; i < 64; i++) mag[i] = 0xFFFF00FF;
    for (int i = 0; i < 36; i++) cyan[i] = 0xFF00FFFF;
    goome_bitmap bm, sb;
    CHECK(goome_bitmap_from_argb(mag, 8, 8, &bm) == GOOME_OK);
    CHECK(goome_bitmap_from_argb(cyan, 6, 6, &sb) == GOOME_OK);
    goome_skin sk = {&sb, 2, 2, 2, 2};
    ic_out *base = malloc(sizeof(ic_out)), *with = malloc(sizeof(ic_out)), *back = malloc(sizeof(ic_out));
    if (!base || !with || !back) {
        free(base), free(with), free(back);
        return;
    }
    uint32_t all = 2166136261u;
    int bad = 0;
    for (int i = 0; i < nc; i++) {
        int bw, bh;
        ic_run(cases[i][1], -1, -1, NULL, NULL, 0, 1, base);
        ic_run(cases[i][1], cases[i][0], -1, &bm, NULL, cases[i][1] == IS_TOAST ? 3 : 1, 1, with);
        int seen = ic_diff(base, with, &bw, &bh, 0xFFFF00FF);
        ic_run(cases[i][1], cases[i][0], -1, &bm, NULL, 2, 1, back);
        int ok = with->hash != base->hash && seen && bw > 0 && bw <= 30 && bh <= 30 && back->hash == base->hash;
        ic_run(cases[i][1], cases[i][0], -1, &bm, NULL, 3, 1, back);
        ok = ok && back->hash == with->hash;
        if (!ok) printf("icon %d scene %d: %08x %08x seen %d box %dx%d\n", cases[i][0], cases[i][1], base->hash, with->hash, seen, bw, bh), bad++;
        all = (all ^ with->hash) * 16777619u;
    }
    CHECK(bad == 0);
    ic_run(IS_CHECK, GOOME_ICON_CHECK, -1, &bm, NULL, 1, 0, back);
    ic_run(IS_CHECK, GOOME_ICON_CHECK, -1, &bm, NULL, 1, 1, with);
    CHECK(back->hash != with->hash);
    int bw, bh;
    ic_run(IS_CHECK, -1, -1, NULL, NULL, 0, 0, base);
    CHECK(!ic_diff(base, back, &bw, &bh, 0xFFFF00FF) && bw > 0 && bw <= 30);
    for (int i = 0; i < ns; i++) {
        ic_run(skins[i][1], -1, -1, NULL, NULL, 0, 1, base);
        ic_run(skins[i][1], -1, skins[i][0], NULL, &sk, 1, 1, with);
        int seen = ic_diff(base, with, &bw, &bh, 0xFF00FFFF);
        ic_run(skins[i][1], -1, skins[i][0], NULL, &sk, 2, 1, back);
        if (with->hash == base->hash || !seen || back->hash != base->hash) printf("skin %d scene %d: %08x %08x seen %d\n", skins[i][0], skins[i][1], base->hash, with->hash, seen), bad++;
        all = (all ^ with->hash) * 16777619u;
    }
    CHECK(bad == 0);
    printf("icon hashes %08x\n", all);
    CHECK(all == 0x171f72cau);
    free(base), free(with), free(back);
    goome_bitmap_free(&bm);
    goome_bitmap_free(&sb);
}

enum { AN_INPUT = 100, AN_KIT = 0, AN_FOCUS, AN_CLOSE, AN_TIP, AN_MODAL, AN_COMBO, AN_APP, AN_TOAST, AN_SPIN, AN_RESIZE, AN_HOVER, AN_PRESS, AN_COLLAPSE };

typedef struct {
    goome *g;
    goome_ui *u;
    goome_theme push_th;
    int scene, what, use_th, open, on, rad, sel, tab, fold, focused, flags, ret, setf;
    const char *hint;
    double v, val, wx, wy, ww, wh;
    char buf[24], ed[32];
    uint32_t hash;
} an_t;

static void an_ev(an_t *k, int type, double x, double y, int key, int mods)
{
    goome_event e;
    memset(&e, 0, sizeof(e));
    e.type = type, e.x = x, e.y = y, e.button = 1, e.key = key, e.mods = mods;
    if (type == GOOME_KEY && key >= 32 && key < 127 && !(mods & GOOME_CTRL)) e.text[0] = (char)key;
    goome_push(k->g, &e);
}

static int an_open(an_t *k, int scene)
{
    int err;
    memset(k, 0, sizeof(*k));
    k->g = goome_open(&(goome_opts){.backend = GOOME_TEST, .mode = GOOME_MODE_WINDOW, .width = 420, .height = 700, .test_script = "end 100000"}, &err);
    if (!k->g) return 0;
    k->u = goome_ui_new(k->g);
    if (!k->u) return goome_close(k->g), 0;
    goome_theme *t = goome_ui_theme(k->u);
    t->font = NULL, t->font_size = 14, t->anim_ms = scene >= AN_HOVER ? 200 : 0;
    goome_ui_cfg(k->u)->blink_ms = 0;
    k->scene = scene, k->what = -1, k->open = 1, k->on = 1, k->rad = 1, k->val = 0.4, k->wx = 10, k->wy = 10, k->ww = 300, k->wh = 200;
    snprintf(k->buf, sizeof(k->buf), "hello you");
    snprintf(k->ed, sizeof(k->ed), "goome HiAVWTy");
    return 1;
}

static void an_close(an_t *k)
{
    goome_ui_free(k->u);
    goome_close(k->g);
}

static void an_frame(an_t *k)
{
    static const char *const tabs[] = {"A", "B", "C"};
    static const char *const items[] = {"one", "two", "three"};
    static const float plot[] = {1, 3, 2, 5, 4, 6};
    goome_event ev;
    while (goome_poll(k->g, &ev, 0) > 0) goome_ui_feed(k->u, &ev);
    goome_ui *u = k->u;
    goome_canvas *c = goome_ui_begin(u, 0);
    if (!c) return;
    int n = 0;
    goome_theme *bt = goome_ui_theme(u);
    if (k->scene == AN_TOAST) bt->toast = k->what == GOOME_S_TOAST ? (uint32_t)k->v : 0, bt->toast_fade_ms = k->what == GOOME_S_TOAST_FADE_MS ? k->v : -1;
    if (k->use_th) goome_ui_push_theme(u, &k->push_th), n++;
    if (k->scene == AN_TOAST) {
    } else if (k->what >= GOOME_S_FOCUS && k->what <= GOOME_S_DIM) goome_ui_push_color(u, k->what, (uint32_t)k->v), n++;
    else if (k->what >= 0) goome_ui_push_num(u, k->what, k->v), n++;
    double x = k->wx, y = k->wy, w = k->ww, h = k->wh;
    switch (k->scene) {
    case AN_KIT:
        h = 680;
        if (goome_ui_window(u, "kit", &x, &y, &w, &h, GOOME_WIN_RESIZE, NULL)) {
            if (goome_ui_tree(u, "tree", &k->open)) {
                goome_ui_label(u, "leaf");
                goome_ui_tree_end(u);
            }
            goome_ui_plot_lines(u, "p", plot, 6, 0, 7, 40);
            goome_ui_text_edit(u, "ed", k->ed, sizeof(k->ed), 40, GOOME_EDIT_MONO);
            goome_ui_row(u, 2);
            goome_ui_link(u, "goome");
            goome_ui_badge(u, "Hi", 0);
            goome_ui_tabs(u, "t", tabs, 3, &k->tab);
            goome_ui_heading(u, "goome Hi");
            goome_ui_label(u, "goome");
            goome_ui_button(u, "Hi");
            goome_ui_checkbox(u, "c", &k->on);
            goome_ui_toggle(u, "t", &k->on);
            goome_ui_radio(u, "r", &k->rad, 1);
            goome_ui_slider(u, "s", &k->val, 0, 1, "%.1f");
            if (!k->focused) goome_ui_set_focus(u), k->focused = 1;
            goome_ui_input(u, "in", k->buf, sizeof(k->buf));
            goome_ui_separator(u);
            for (int i = 0; i < 40; i++) goome_ui_label(u, "row %d", i);
        }
        goome_ui_window_end(u);
        break;
    case AN_FOCUS:
        goome_ui_set_focus(u);
        goome_ui_button(u, "Hi");
        break;
    case AN_CLOSE:
    case AN_RESIZE:
        if (goome_ui_window(u, "w", &k->wx, &k->wy, &k->ww, &k->wh, GOOME_WIN_RESIZE | GOOME_WIN_CLOSE, &k->open)) goome_ui_label(u, "goome");
        goome_ui_window_end(u);
        break;
    case AN_TIP:
    case AN_HOVER:
    case AN_PRESS:
        goome_ui_button(u, "Hi");
        goome_ui_tooltip(u, "goome tip");
        break;
    case AN_MODAL:
        goome_ui_label(u, "behind");
        if (goome_ui_modal_begin(u, "m", &k->open)) {
            goome_ui_label(u, "goome");
            goome_ui_modal_end(u);
        }
        break;
    case AN_COMBO:
        goome_ui_combo(u, "##c", &k->sel, items, 3);
        break;
    case AN_APP:
        goome_ui_app(u, "app", GOOME_APP_TITLEBAR | GOOME_APP_MENUBAR | GOOME_APP_STATUSBAR);
        if (goome_ui_menubar_begin(u)) {
            if (goome_ui_menu_begin(u, "File")) {
                goome_ui_menu_item(u, "New", NULL, NULL);
                goome_ui_menu_separator(u);
                goome_ui_menu_item(u, "Quit", NULL, NULL);
                goome_ui_menu_end(u);
            }
            goome_ui_menubar_end(u);
        }
        goome_ui_statusbar(u, "status");
        goome_ui_app_end(u);
        break;
    case AN_TOAST:
        goome_ui_label(u, "goome");
        break;
    case AN_SPIN:
        goome_ui_spinner(u, 0);
        goome_ui_button(u, "Hi");
        break;
    case AN_COLLAPSE:
        goome_ui_collapsing(u, "fold", &k->fold);
        break;
    case AN_INPUT:
        goome_ui_button(u, "Hi");
        if (k->setf) goome_ui_set_focus(u), k->setf = 0;
        k->ret = goome_ui_input_ex(u, "in", k->buf, sizeof(k->buf), k->flags, k->hint);
        break;
    }
    if (n) goome_ui_pop(u, n);
    goome_ui_end(u);
    k->hash = fnv(c->px, c->w, c->h, c->stride);
}

static uint32_t an_run(int scene, int what, double v, int mode, const goome_theme *th)
{
    an_t k;
    if (!an_open(&k, scene)) return 0;
    double lh = goome_text_line_height(NULL, 14), fh = (double)(long)(lh + 12 + 0.5), tbar = fh + 6;
    int frames = scene == AN_TIP ? 40 : scene == AN_TOAST ? 20 : 6;
    for (int f = 0; f < frames; f++) {
        k.what = mode == 0 || (mode == 2 && f >= frames / 2) ? -1 : what, k.v = v;
        k.use_th = th && k.what >= 0;
        if (th) k.push_th = *th, k.what = -1;
        if (f == 1) {
            if (scene == AN_CLOSE) an_ev(&k, GOOME_MOTION, 291, 10 + tbar / 2, 0, 0);
            if (scene == AN_TIP || scene == AN_HOVER) an_ev(&k, GOOME_MOTION, 20, 20, 0, 0);
            if (scene == AN_PRESS) an_ev(&k, GOOME_DOWN, 20, 20, 0, 0);
            if (scene == AN_COMBO || scene == AN_COLLAPSE) an_ev(&k, GOOME_DOWN, 40, 20, 0, 0), an_ev(&k, GOOME_UP, 40, 20, 0, 0);
            if (scene == AN_APP) an_ev(&k, GOOME_DOWN, 20, 60, 0, 0), an_ev(&k, GOOME_UP, 20, 60, 0, 0);
            if (scene == AN_TOAST) goome_ui_toast(k.u, "goome toast", 5000);
            if (scene == AN_RESIZE) an_ev(&k, GOOME_DOWN, 10 + 300 - 6, 10 + 200 - 6, 0, 0);
        }
        if (f == 2 && scene == AN_RESIZE) an_ev(&k, GOOME_MOTION, 40, 40, 0, 0);
        an_frame(&k);
    }
    uint32_t hv = k.hash;
    an_close(&k);
    return hv;
}

static uint32_t in_run(int flags, const char *text, const char *hint, int focus, const char *type, int *ret, char *out)
{
    an_t k;
    if (!an_open(&k, AN_INPUT)) return 0;
    k.flags = flags, k.hint = hint, k.setf = focus;
    snprintf(k.buf, sizeof(k.buf), "%s", text);
    goome_clipboard_set(k.g, "paste9");
    an_frame(&k);
    for (const char *t = type ? type : ""; *t; t++) {
        if (*t == '^') an_ev(&k, GOOME_KEY, 0, 0, t[1], GOOME_CTRL), t++;
        else if (*t == '\n') an_ev(&k, GOOME_KEY, 0, 0, GOOME_K_ENTER, 0);
        else an_ev(&k, GOOME_KEY, 0, 0, (unsigned char)*t, 0);
        an_frame(&k);
        if (ret && k.ret) *ret = k.ret;
    }
    an_frame(&k);
    if (out) {
        char clip[32] = "";
        goome_clipboard_get(k.g, clip, sizeof(clip));
        snprintf(out, 64, "%s|%s", k.buf, clip);
    }
    uint32_t hv = k.hash;
    an_close(&k);
    return hv;
}

static void input_ex(void)
{
    char o[64];
    int ret = 0;
    uint32_t plain = in_run(0, "abc", NULL, 0, NULL, NULL, NULL), pw = in_run(GOOME_INPUT_PASSWORD, "abc", NULL, 0, NULL, NULL, NULL);
    CHECK(plain && pw != plain && in_run(GOOME_INPUT_PASSWORD, "xyz", NULL, 0, NULL, NULL, NULL) == pw);
    CHECK(in_run(0, "abc", NULL, 0, NULL, NULL, NULL) == in_run(0, "abc", "type here", 0, NULL, NULL, NULL));
    uint32_t empty = in_run(0, "", NULL, 0, NULL, NULL, NULL), hinted = in_run(0, "", "type here", 0, NULL, NULL, NULL);
    CHECK(empty != hinted && in_run(0, "", NULL, 1, NULL, NULL, NULL) == in_run(0, "", "type here", 1, NULL, NULL, NULL));
    in_run(GOOME_INPUT_PASSWORD, "secret", NULL, 1, "^c", NULL, o);
    CHECK(!strcmp(o, "secret|paste9"));
    in_run(GOOME_INPUT_PASSWORD, "secret", NULL, 1, "^xq", NULL, o);
    CHECK(!strcmp(o, "q|paste9"));
    in_run(0, "secret", NULL, 1, "^c", NULL, o);
    CHECK(!strcmp(o, "secret|secret"));
    in_run(GOOME_INPUT_READONLY, "keep", NULL, 1, "zz^v^x", NULL, o);
    CHECK(!strcmp(o, "keep|keep"));
    in_run(GOOME_INPUT_NUMBER, "", NULL, 1, "a1-.x2+^v", NULL, o);
    CHECK(!strcmp(o, "1-.2+9|paste9"));
    in_run(0, "ab", NULL, 1, "z", NULL, o);
    CHECK(!strcmp(o, "z|paste9"));
    in_run(GOOME_INPUT_NO_SELECT_ON_FOCUS, "ab", NULL, 1, "z", NULL, o);
    CHECK(!strcmp(o, "abz|paste9"));
    in_run(GOOME_INPUT_ENTER_RETURNS, "ab", NULL, 1, "\n", &ret, NULL);
    CHECK(ret == 2);
    ret = 0;
    in_run(0, "ab", NULL, 1, "\n", &ret, NULL);
    CHECK(ret == 0);
    an_t k;
    if (!an_open(&k, AN_INPUT)) return;
    goome_ui_cfg(k.u)->mask = "#";
    k.flags = GOOME_INPUT_PASSWORD;
    snprintf(k.buf, sizeof(k.buf), "abc");
    an_frame(&k), an_frame(&k);
    CHECK(k.hash != pw);
    an_close(&k);
}

static void anything(void)
{
    static const struct {
        int what, scene, anim;
        double v;
    } cases[] = {
        {GOOME_S_FOCUS, AN_FOCUS, 0, 0xFFFF00FF}, {GOOME_S_SELECT, AN_KIT, 0, 0xFFFF00FF}, {GOOME_S_CARET, AN_KIT, 0, 0xFFFF00FF},
        {GOOME_S_LINK, AN_KIT, 0, 0xFFFF00FF}, {GOOME_S_BADGE, AN_KIT, 0, 0xFFFF00FF}, {GOOME_S_TOAST, AN_TOAST, 0, 0xFFFF00FF},
        {GOOME_S_TOOLTIP, AN_TIP, 0, 0xFFFF00FF}, {GOOME_S_DANGER, AN_CLOSE, 0, 0xFFFF00FF}, {GOOME_S_KNOB, AN_KIT, 0, 0xFFFF00FF},
        {GOOME_S_DIM, AN_MODAL, 0, 0xC0FF00FF},
        {GOOME_S_CHECK_SIZE, AN_KIT, 0, 26}, {GOOME_S_CHECK_RADIUS, AN_KIT, 0, 0}, {GOOME_S_TOGGLE_W, AN_KIT, 0, 60}, {GOOME_S_TOGGLE_H, AN_KIT, 0, 30},
        {GOOME_S_SLIDER_H, AN_KIT, 0, 40}, {GOOME_S_SLIDER_TRACK, AN_KIT, 0, 12}, {GOOME_S_KNOB_SIZE, AN_KIT, 0, 14}, {GOOME_S_SCROLL_W, AN_KIT, 0, 12},
        {GOOME_S_SCROLL_MIN, AN_KIT, 0, 300}, {GOOME_S_ITEM_H, AN_KIT, 0, 40}, {GOOME_S_MENU_ITEM_H, AN_APP, 0, 40}, {GOOME_S_TAB_H, AN_KIT, 0, 50},
        {GOOME_S_WIN_TITLE_H, AN_KIT, 0, 50}, {GOOME_S_APP_TITLE_H, AN_APP, 0, 44}, {GOOME_S_MENUBAR_H, AN_APP, 0, 40}, {GOOME_S_STATUSBAR_H, AN_APP, 0, 40},
        {GOOME_S_FOCUS_W, AN_FOCUS, 0, 5}, {GOOME_S_FOCUS_GAP, AN_FOCUS, 0, 8}, {GOOME_S_SEP_W, AN_KIT, 0, 4}, {GOOME_S_GRIP, AN_KIT, 0, 30},
        {GOOME_S_INDENT, AN_KIT, 0, 40}, {GOOME_S_PLOT_W, AN_KIT, 0, 6}, {GOOME_S_SPINNER, AN_SPIN, 0, 40}, {GOOME_S_CARET_W, AN_KIT, 0, 8},
        {GOOME_S_WIN_MIN_W, AN_RESIZE, 0, 250}, {GOOME_S_WIN_MIN_H, AN_RESIZE, 0, 200}, {GOOME_S_POP_GAP, AN_COMBO, 0, 30}, {GOOME_S_MENU_GAP, AN_APP, 0, 20},
        {GOOME_S_TIP_PAD_X, AN_TIP, 0, 20}, {GOOME_S_TIP_PAD_Y, AN_TIP, 0, 15}, {GOOME_S_TIP_X, AN_TIP, 0, 40}, {GOOME_S_TIP_Y, AN_TIP, 0, 40},
        {GOOME_S_TIP_RADIUS, AN_TIP, 0, 0}, {GOOME_S_SHADOW_X, AN_KIT, 0, 10}, {GOOME_S_SHADOW_Y, AN_KIT, 0, 15}, {GOOME_S_ALIGN, AN_KIT, 0, GOOME_ALIGN_RIGHT},
        {GOOME_S_HEADING_SIZE, AN_KIT, 0, 12}, {GOOME_S_SMALL_SIZE, AN_KIT, 0, 20},
        {GOOME_S_EASE, AN_HOVER, 1, GOOME_EASE_LINEAR}, {GOOME_S_HOVER_MS, AN_HOVER, 1, 0}, {GOOME_S_PRESS_MS, AN_PRESS, 1, 0},
        {GOOME_S_POPUP_MS, AN_COMBO, 1, 1000}, {GOOME_S_COLLAPSE_MS, AN_COLLAPSE, 1, 0}, {GOOME_S_TOAST_FADE_MS, AN_TOAST, 1, 2000},
    };
    int n = (int)(sizeof(cases) / sizeof(cases[0])), bad = 0, seen[GOOME_S_COUNT];
    memset(seen, 0, sizeof(seen));
    for (int i = 0; i < n; i++) {
        int sc = cases[i].scene;
        if (cases[i].anim && sc == AN_COMBO) sc = AN_COMBO;
        uint32_t base = an_run(sc, -1, 0, 0, NULL), with = an_run(sc, cases[i].what, cases[i].v, 1, NULL);
        uint32_t back = cases[i].anim ? base : an_run(sc, cases[i].what, cases[i].v, 2, NULL);
        seen[cases[i].what] = 1;
        if (!base || with == base || back != base) printf("style %d scene %d: %08x %08x %08x\n", cases[i].what, sc, base, with, back), bad++;
    }
    CHECK(bad == 0);
    for (int i = GOOME_S_FOCUS; i < GOOME_S_COUNT; i++)
        if (!seen[i]) printf("style %d never checked\n", i), bad++;
    CHECK(bad == 0);

    goome_theme d;
    memset(&d, 0, sizeof(d));
    goome_ui_theme_dark(&d);
    CHECK(d.check_size == 18 && d.toggle_w == 38 && d.scroll_w == 4 && d.focus_w == 2 && d.shadow_y == 5 && d.hover_ms < 0 && d.focus == 0 && d.item_h == 0);
    goome_font *f = goome_font_load_mem(sub_ttf, sizeof(sub_ttf), 0);
    CHECK(f != NULL);
    uint32_t kit = an_run(AN_KIT, -1, 0, 0, NULL);
    goome_theme ft;
    memset(&ft, 0, sizeof(ft));
    goome_ui_theme_dark(&ft);
    ft.font_size = 14, ft.anim_ms = 0;
    CHECK(an_run(AN_KIT, 0, 0, 1, &ft) == kit);
    ft.font_heading = f;
    uint32_t fh = an_run(AN_KIT, 0, 0, 1, &ft);
    ft.font_heading = NULL, ft.font_small = f;
    uint32_t fs = an_run(AN_KIT, 0, 0, 1, &ft);
    ft.font_small = NULL, ft.font_mono = f;
    uint32_t fm = an_run(AN_KIT, 0, 0, 1, &ft);
    if (!(fh != kit && fs != kit && fm != kit && fh != fs && fs != fm)) printf("font slots %08x %08x %08x %08x\n", kit, fh, fs, fm);
    CHECK(fh != kit && fs != kit && fm != kit && fh != fs && fs != fm);
    goome_font_free(f);

    goome_theme p, q;
    uint32_t all = 2166136261u;
    for (int i = 0; i < GOOME_PRESET_COUNT; i++) {
        memset(&p, 0, sizeof(p)), memset(&q, 0, sizeof(q));
        CHECK(goome_ui_theme_preset(&p, i) == GOOME_OK && goome_ui_theme_preset(&q, i) == GOOME_OK && !memcmp(&p, &q, sizeof(p)));
        p.font_size = 14;
        all = (all ^ an_run(AN_KIT, 0, 0, 1, &p)) * 16777619u;
    }
    CHECK(goome_ui_theme_preset(&p, GOOME_PRESET_COUNT) == GOOME_EARG && goome_ui_theme_preset(NULL, 0) == GOOME_EARG);
    memset(&p, 0, sizeof(p)), memset(&q, 0, sizeof(q));
    goome_ui_theme_preset(&p, GOOME_PRESET_DARK), goome_ui_theme_dark(&q);
    CHECK(!memcmp(&p, &q, sizeof(p)));
    goome_ui_theme_preset(&p, GOOME_PRESET_LIGHT), goome_ui_theme_light(&q);
    CHECK(!memcmp(&p, &q, sizeof(p)));
    static const struct {
        uint32_t accent;
        int dark;
        double r;
    } makes[] = {{0xFF8B5CF6, 1, 12}, {0xFF2F7BF6, 0, 20}, {0xFFFFD60A, 1, 0}, {0x00E5484D, 0, 6}};
    for (int i = 0; i < 4; i++) {
        memset(&p, 0, sizeof(p)), memset(&q, 0, sizeof(q));
        goome_ui_theme_make(&p, makes[i].accent, makes[i].dark, makes[i].r);
        goome_ui_theme_make(&q, makes[i].accent, makes[i].dark, makes[i].r);
        CHECK(!memcmp(&p, &q, sizeof(p)) && p.accent == (makes[i].accent | 0xFF000000u) && p.radius == makes[i].r);
        p.font_size = 14;
        all = (all ^ an_run(AN_KIT, 0, 0, 1, &p)) * 16777619u;
    }
    CHECK(p.accent_text == 0xFFFFFFFFu);
    printf("preset hashes %08x\n", all);
    CHECK(all == 0x315ea656u);

    an_t k;
    if (!an_open(&k, AN_KIT)) return;
    goome_theme *t = goome_ui_theme(k.u);
    goome_ui_scale(k.u, 2);
    CHECK(t->font_size == 28 && t->pad == 20 && t->radius == 20 && t->scroll_w == 8 && t->focus_gap == 6 && t->tip_x == 28 && t->shadow_y == 10);
    CHECK(t->check_size == 18 && t->indent == 16 && t->item_h == 0 && t->hover_ms < 0 && t->align == 0);
    an_frame(&k);
    uint32_t big = k.hash;
    goome_ui_scale(k.u, 0.5);
    CHECK(t->font_size == 14 && t->pad == 10 && t->scroll_w == 4 && t->tip_x == 14);
    an_frame(&k);
    CHECK(k.hash == kit && big != kit);
    goome_ui_scale(k.u, 0), goome_ui_scale(k.u, -1), goome_ui_scale(NULL, 2);
    CHECK(t->font_size == 14);
    an_close(&k);

    if (!an_open(&k, AN_KIT)) return;
    goome_ui_config *cfg = goome_ui_cfg(k.u);
    CHECK(cfg && cfg->tip_delay_ms == 500 && cfg->dbl_click_ms == 450 && cfg->blink_ms == 0 && cfg->tab_nav && cfg->esc_closes && cfg->raise_on_click && !cfg->focus_on_mouse);
    CHECK(cfg->cursor[GOOME_CUR_INPUT] == GOOME_CURSOR_IBEAM && cfg->keys[GOOME_ACT_COPY][0].key == 'c' && cfg->keys[GOOME_ACT_COPY][0].mods == GOOME_CTRL);
    CHECK(cfg->keys[GOOME_ACT_REDO][1].key == 'z' && cfg->keys[GOOME_ACT_REDO][1].mods == (GOOME_CTRL | GOOME_SHIFT));
    char clip[32];
    an_frame(&k);
    an_ev(&k, GOOME_KEY, 0, 0, 'c', GOOME_CTRL);
    an_frame(&k);
    CHECK(goome_clipboard_get(k.g, clip, sizeof(clip)) == 9 && !strcmp(clip, "hello you"));
    CHECK(goome_ui_bind(k.u, GOOME_ACT_COPY, "ctrl+k") == GOOME_OK && goome_ui_bind(k.u, GOOME_ACT_SELECT_ALL, "alt+l, ctrl+q") == GOOME_OK);
    CHECK(cfg->keys[GOOME_ACT_SELECT_ALL][0].mods == GOOME_ALT && cfg->keys[GOOME_ACT_SELECT_ALL][1].key == 'q' && !cfg->keys[GOOME_ACT_SELECT_ALL][2].key);
    CHECK(goome_ui_bind(k.u, GOOME_ACT_COUNT, "ctrl+k") == GOOME_EARG && goome_ui_bind(k.u, GOOME_ACT_UNDO, "ctrl+nope") == GOOME_EARG);
    CHECK(goome_ui_bind(k.u, GOOME_ACT_PASTE, NULL) == GOOME_OK && !cfg->keys[GOOME_ACT_PASTE][0].key);
    goome_clipboard_set(k.g, "zz");
    an_ev(&k, GOOME_KEY, 0, 0, 'c', GOOME_CTRL);
    an_frame(&k);
    CHECK(goome_clipboard_get(k.g, clip, sizeof(clip)) == 2);
    an_ev(&k, GOOME_KEY, 0, 0, 'k', GOOME_CTRL);
    an_frame(&k);
    CHECK(goome_clipboard_get(k.g, clip, sizeof(clip)) == 9);
    an_ev(&k, GOOME_KEY, 0, 0, 'v', GOOME_CTRL);
    an_frame(&k);
    CHECK(!strcmp(k.buf, "hello you"));
    an_ev(&k, GOOME_KEY, 0, 0, GOOME_K_LEFT, 0), an_ev(&k, GOOME_KEY, 0, 0, GOOME_K_LEFT, 0);
    an_ev(&k, GOOME_KEY, 0, 0, 'q', GOOME_CTRL), an_ev(&k, GOOME_KEY, 0, 0, 'x', 0);
    an_frame(&k);
    CHECK(!strcmp(k.buf, "x"));
    goome_ui_bind(k.u, GOOME_ACT_WORD_LEFT, "alt+left");
    snprintf(k.buf, sizeof(k.buf), "ab cd");
    an_ev(&k, GOOME_KEY, 0, 0, GOOME_K_END, 0), an_ev(&k, GOOME_KEY, 0, 0, GOOME_K_LEFT, GOOME_ALT), an_ev(&k, GOOME_KEY, 0, 0, '_', 0);
    an_frame(&k);
    CHECK(!strcmp(k.buf, "ab _cd"));
    an_ev(&k, GOOME_KEY, 0, 0, GOOME_K_LEFT, GOOME_CTRL), an_ev(&k, GOOME_KEY, 0, 0, '-', 0);
    an_frame(&k);
    if (strcmp(k.buf, "ab -_cd")) printf("buf '%s'\n", k.buf);
    CHECK(!strcmp(k.buf, "ab -_cd"));
    an_close(&k);

    if (!an_open(&k, AN_COMBO)) return;
    an_frame(&k);
    an_ev(&k, GOOME_DOWN, 40, 20, 0, 0), an_ev(&k, GOOME_UP, 40, 20, 0, 0);
    an_frame(&k), an_frame(&k);
    uint32_t opened = k.hash;
    goome_ui_cfg(k.u)->esc_closes = 0;
    an_ev(&k, GOOME_KEY, 0, 0, GOOME_K_ESC, 0);
    an_frame(&k);
    CHECK(k.hash == opened);
    goome_ui_cfg(k.u)->esc_closes = 1;
    an_ev(&k, GOOME_KEY, 0, 0, GOOME_K_ESC, 0);
    an_frame(&k);
    CHECK(k.hash != opened);
    goome_ui_cfg(k.u)->cursor[GOOME_CUR_BUTTON] = GOOME_CURSOR_HAND;
    an_ev(&k, GOOME_MOTION, 40, 20, 0, 0);
    an_frame(&k);
    CHECK(goome_cursor_shape(k.g) == GOOME_CURSOR_HAND);
    an_close(&k);

    uint32_t tip[2];
    for (int j = 0; j < 2; j++) {
        if (!an_open(&k, AN_TIP)) return;
        if (j) goome_ui_cfg(k.u)->tip_delay_ms = 100;
        an_frame(&k);
        an_ev(&k, GOOME_MOTION, 20, 20, 0, 0);
        for (int i = 0; i < 12; i++) an_frame(&k);
        tip[j] = k.hash;
        if (!j) an_close(&k);
    }
    CHECK(tip[0] != tip[1]);
    goome_ui_cfg(k.u)->tab_nav = 0;
    an_ev(&k, GOOME_KEY, 0, 0, GOOME_K_TAB, 0);
    an_frame(&k), an_frame(&k);
    uint32_t no_tab = k.hash;
    goome_ui_cfg(k.u)->tab_nav = 1;
    an_ev(&k, GOOME_KEY, 0, 0, GOOME_K_TAB, 0);
    an_frame(&k), an_frame(&k);
    CHECK(k.hash != no_tab);
    an_close(&k);

    if (!an_open(&k, AN_FOCUS)) return;
    an_frame(&k);
    uint32_t ring = k.hash;
    an_close(&k);
    uint32_t clicked[2];
    for (int j = 0; j < 2; j++) {
        if (!an_open(&k, AN_TIP)) return;
        goome_ui_cfg(k.u)->focus_on_mouse = j;
        an_frame(&k);
        an_ev(&k, GOOME_DOWN, 20, 20, 0, 0), an_ev(&k, GOOME_UP, 20, 20, 0, 0);
        an_frame(&k);
        an_ev(&k, GOOME_MOTION, 200, 200, 0, 0);
        an_frame(&k), an_frame(&k);
        clicked[j] = k.hash;
        an_close(&k);
    }
    CHECK(ring != 0 && clicked[0] != clicked[1]);
}

typedef struct {
    goome *g;
    goome_ui *u;
    int on, dropped;
    double box, sp, last;
    uint32_t col, hash;
} am_t;

static void am_frame(am_t *k)
{
    goome_event ev;
    while (goome_poll(k->g, &ev, 0) > 0) goome_ui_feed(k->u, &ev);
    goome_canvas *c = goome_ui_begin(k->u, 0);
    if (!c) return;
    goome_canvas *r = goome_ui_canvas(k->u);
    k->box = goome_ui_anim(k->u, "box", k->on ? 200 : 10, 160, GOOME_EASE_OUT);
    k->sp = goome_ui_anim(k->u, "spring", k->on ? 1 : 0, 320, GOOME_EASE_SPRING);
    k->col = goome_ui_anim_color(k->u, "col", k->on ? 0xFF20D060u : 0x00000000u, 200, GOOME_EASE_LINEAR);
    if (!k->dropped) k->last = goome_ui_anim(k->u, "gone", k->on ? 5 : 0, 100, GOOME_EASE_LINEAR);
    if (r) {
        goome_draw_rect(r, k->box, 20, 60, 40, 0xFFE0A030u);
        goome_draw_rrect(r, 20 + k->sp * 180, 90, 50, 30, goome_radius(15), 0xFF4FB3F7u);
        goome_draw_rect(r, 20, 140, 260, 40, k->col);
    }
    goome_ui_end(k->u);
    k->hash = fnv(c->px, c->w, c->h, c->stride);
}

static void anims(void)
{
    CHECK(goome_ease(GOOME_EASE_LINEAR, 0.25) == 0.25);
    CHECK(goome_ease(GOOME_EASE_IN, 0.5) == 0.125);
    CHECK(goome_ease(GOOME_EASE_OUT, 0.5) == 0.875);
    CHECK(goome_ease(GOOME_EASE_IN_OUT, 0.25) == 0.0625);
    CHECK(goome_ease(GOOME_EASE_IN_OUT, 0.75) == 0.9375);
    CHECK(goome_ease(GOOME_EASE_SMOOTH, 0.5) == 0.5);
    CHECK(goome_ease(GOOME_EASE_BOUNCE, 0.25) == 0.47265625);
    CHECK(goome_ease(GOOME_EASE_OUT, -3) == 0 && goome_ease(GOOME_EASE_IN, 7) == 1);
    double peak = 0, low = 1, bpeak = 0, sp_half = goome_ease(GOOME_EASE_SPRING, 0.5);
    for (int e = GOOME_EASE_SMOOTH; e <= GOOME_EASE_BOUNCE; e++)
        if (e != GOOME_EASE_CUSTOM && e != GOOME_EASE_BACK) CHECK(goome_ease(e, 0) == 0 && goome_ease(e, 1) == 1);
    for (int i = 1; i < 100; i++) {
        double v = goome_ease(GOOME_EASE_SPRING, i / 100.0), b = goome_ease(GOOME_EASE_BOUNCE, i / 100.0);
        if (v > peak) peak = v;
        if (i > 30 && v < low) low = v;
        if (b > bpeak) bpeak = b;
        CHECK(b >= 0);
    }
    CHECK(peak > 1.1 && peak < 1.25 && low < 1 && low > 0.9 && bpeak <= 1);
    CHECK(sp_half > 0.95 && sp_half < 1.05 && goome_ease(GOOME_EASE_SPRING, 0.999) > 0.997);
    CHECK(goome_ui_anim(NULL, "x", 3, 100, 0) == 3);

    int err;
    am_t k;
    memset(&k, 0, sizeof(k));
    k.g = goome_open(&(goome_opts){.backend = GOOME_TEST, .mode = GOOME_MODE_WINDOW, .width = 300, .height = 200, .test_script = "end 100000"}, &err);
    CHECK(k.g != NULL);
    if (!k.g) return;
    k.u = goome_ui_new(k.g);
    am_frame(&k);
    CHECK(k.box == 10 && k.sp == 0 && k.col == 0 && k.last == 0);
    am_frame(&k);
    CHECK(!goome_ui_animating(k.u));
    uint32_t hs[4];
    k.on = 1;
    double prev = k.box, over = 0, mid = -1;
    int moving = 0, frames = 0;
    for (; frames < 60; frames++) {
        am_frame(&k);
        if (frames == 1) hs[0] = k.hash;
        if (frames == 4) hs[1] = k.hash;
        if (frames == 8) hs[2] = k.hash;
        CHECK(k.box >= prev);
        prev = k.box;
        if (k.sp > over) over = k.sp;
        if (frames == 6) mid = k.col;
        if (goome_ui_animating(k.u)) moving = frames + 1;
    }
    hs[3] = k.hash;
    CHECK(k.box == 200 && k.sp == 1 && k.col == 0xFF20D060u && k.last == 5);
    CHECK(over > 1.05 && moving >= 19 && moving <= 22 && !goome_ui_animating(k.u));
    uint32_t m = (uint32_t)mid;
    CHECK((m >> 24) > 0x70 && (m >> 24) < 0x90 && ((m >> 8) & 255) == 0xD0 && ((m >> 16) & 255) == 0x20);
    k.on = 0;
    am_frame(&k);
    CHECK(goome_ui_animating(k.u) && k.box == 200);
    am_frame(&k);
    CHECK(k.box < 200 && k.box > 10);
    k.on = 1;
    am_frame(&k);
    CHECK(k.box < 200 && goome_ui_animating(k.u));
    k.dropped = 1;
    for (int i = 0; i < 1600; i++) am_frame(&k);
    k.dropped = 0, k.on = 0;
    am_frame(&k);
    CHECK(k.last == 0);
    printf("anim hashes %08x %08x %08x %08x  moving %d over %.3f mid %08x\n", hs[0], hs[1], hs[2], hs[3], moving, over, m);
    static const uint32_t want[4] = {0x0226f2f5u, 0x906d3ce5u, 0xdb5848adu, 0x12bea72du};
    for (int i = 0; i < 4; i++) CHECK(hs[i] == want[i]);
    goome_ui_free(k.u);
    goome_close(k.g);
}

static uint32_t txt_hash(goome_font *f, double size, const char *s, uint32_t argb, int dir, int w, int h)
{
    static uint32_t px[400 * 120];
    goome_canvas c;
    memset(&c, 0, sizeof(c));
    memset(px, 0, sizeof(px));
    c.px = px, c.w = w, c.h = h, c.stride = 400, c.scale = 1, c.kx1 = w, c.ky1 = h;
    goome_text_dir(dir);
    goome_text(&c, f, size, 4, 4, s, argb);
    goome_text_dir(GOOME_DIR_AUTO);
    return fnv(px, 400, 120, 400);
}

static int near(double a, double b) { return a - b < 0.01 && b - a < 0.01; }

static void emoji_rtl(void)
{
    goome_font *f = goome_font_load_mem(colr_ttf, sizeof(colr_ttf), 0), *cb = goome_font_load_mem(cbdt_ttf, sizeof(cbdt_ttf), 0);
    goome_font *sb = goome_font_load_mem(sbix_ttf, sizeof(sbix_ttf), 0), *d = goome_font_load_mem(sub_ttf, sizeof(sub_ttf), 0);
    CHECK(f && cb && sb && d);
    if (!f || !cb || !sb || !d) return;
    CHECK(near(goome_text_measure(f, 30, "\xF0\x9F\x98\x80\xF0\x9F\x8F\xBB", NULL), 24));
    CHECK(near(goome_text_measure(f, 30, "\xF0\x9F\x98\x80\xE2\x80\x8D\xF0\x9F\x98\x80", NULL), 24));
    CHECK(near(goome_text_measure(f, 30, "\xE2\x9D\xA4", NULL), 18) && near(goome_text_measure(f, 30, "\xE2\x9D\xA4\xEF\xB8\x8F", NULL), 24));
    uint32_t got[6], want[6] = {0x3bab3875u, 0x85fe883au, 0xd3add387u, 0xb79518ddu, 0x49a72d7du, 0x469719e1u};
    got[0] = txt_hash(f, 30, "ab \xF0\x9F\x98\x80 \xF0\x9F\x98\x80\xF0\x9F\x8F\xBB \xF0\x9F\x98\x80\xE2\x80\x8D\xF0\x9F\x98\x80 \xE2\x9D\xA4 \xE2\x9D\xA4\xEF\xB8\x8F", 0xFF2040A0, 0, 400, 60);
    static uint32_t px[400 * 120];
    goome_canvas c;
    memset(&c, 0, sizeof(c));
    memset(px, 0, sizeof(px));
    c.px = px, c.w = 400, c.h = 120, c.stride = 400, c.scale = 1;
    goome_text(&c, cb, 16, 4, 4, "\xF0\x9F\x90\xB1", 0xFFFFFFFF);
    goome_text(&c, cb, 64, 30, 4, "\xF0\x9F\x90\xB1", 0xFFFFFFFF);
    goome_text(&c, sb, 40, 120, 4, "\xF0\x9F\x90\xB1\xF0\x9F\x90\xB1", 0x80FFFFFF);
    uint32_t mid = px[(4 + 40) * 400 + 30 + 35];
    CHECK((mid >> 24) == 255 && (mid >> 16 & 255) > 200 && (mid >> 8 & 255) > 100 && (mid >> 8 & 255) < 210 && (mid & 255) < 120);
    got[1] = fnv(px, 400, 120, 400);
    goome_font_fallback(d, f);
    goome_font_fallback(d, d);
    CHECK(near(goome_text_measure(d, 20, "goome! \xF0\x9F\x98\x80", NULL), goome_text_measure(d, 20, "goome! ", NULL) + 16));
    got[2] = txt_hash(d, 20, "goome! \xF0\x9F\x98\x80 Hi", 0xFFFFFFFF, 0, 400, 40);
    got[3] = txt_hash(f, 24, "ab \xD7\x90\xD7\x91\xD7\x92 (12) c", 0xFFFFFFFF, 0, 400, 40);
    CHECK(got[3] != txt_hash(f, 24, "ab \xD7\x90\xD7\x91\xD7\x92 (12) c", 0xFFFFFFFF, GOOME_DIR_RTL, 400, 40));
    CHECK(got[3] == txt_hash(f, 24, "ab \xD7\x90\xD7\x91\xD7\x92 (12) c", 0xFFFFFFFF, GOOME_DIR_LTR, 400, 40));
    got[4] = txt_hash(f, 24, "ab \xD7\x90\xD7\x91\xD7\x92 (12) c", 0xFFFFFFFF, GOOME_DIR_RTL, 400, 40);
    CHECK(near(goome_text_measure(f, 20, "\xD8\xA8\xD9\x84\xD8\xA7\xD9\x85 \xD8\xA8\xD8\xA8\xD8\xA8 \xD9\x84\xD9\x85", NULL), 93.2));
    got[5] = txt_hash(f, 30, "\xD8\xA8\xD9\x84\xD8\xA7\xD9\x85 \xD8\xA8\xD8\xA8\xD8\xA8 \xD9\x84\xD9\x85 \xD8\xA8\xD9\x8E\xD8\xA8", 0xFFFFFFFF, 0, 400, 50);
    CHECK(got[5] != txt_hash(f, 30, "\xD8\xA8 \xD9\x84\xD8\xA7\xD9\x85 \xD8\xA8 \xD8\xA8 \xD8\xA8 \xD9\x84\xD9\x85", 0xFFFFFFFF, 0, 400, 50));
    printf("emoji rtl hashes %08x %08x %08x %08x %08x %08x\n", got[0], got[1], got[2], got[3], got[4], got[5]);
    for (int i = 0; i < 6; i++) CHECK(got[i] == want[i]);
    for (size_t cut = 0; cut < sizeof(colr_ttf); cut += 5) {
        static unsigned char bad[sizeof(colr_ttf)];
        const unsigned char *src = cut % 3 == 0 ? colr_ttf : cut % 3 == 1 ? cbdt_ttf : sbix_ttf;
        size_t n = cut % 3 == 0 ? sizeof(colr_ttf) : cut % 3 == 1 ? sizeof(cbdt_ttf) : sizeof(sbix_ttf);
        memcpy(bad, src, n);
        bad[cut % n] ^= 0x5A;
        goome_font *b = goome_font_load_mem(bad, (cut & 1) ? n : cut % n + 12, 0);
        memset(&c, 0, sizeof(c));
        c.px = px, c.w = 400, c.h = 120, c.stride = 400, c.scale = 1;
        goome_text(&c, b, 30, 0, 0, "a\xF0\x9F\x98\x80\xF0\x9F\x8F\xBB\xF0\x9F\x90\xB1\xE2\x9D\xA4\xEF\xB8\x8F\xD8\xA8\xD9\x84\xD8\xA7 \xD7\x90", 0xFFFFFFFF);
        goome_font_free(b);
    }
    goome_font_free(d);
    goome_font_free(sb);
    goome_font_free(cb);
    goome_font_free(f);
}

typedef struct {
    goome *g;
    goome_ui *u;
    char in[64], ed[96];
    uint32_t hash;
} bd_t;

static void bd_key(bd_t *k, int key, int mods)
{
    goome_event e;
    memset(&e, 0, sizeof(e));
    e.type = GOOME_KEY, e.key = key, e.mods = mods;
    if (key >= 32 && key < 127 && !(mods & GOOME_CTRL)) e.text[0] = (char)key;
    goome_push(k->g, &e);
}

static void bd_frame(bd_t *k, int focus)
{
    goome_event ev;
    while (goome_poll(k->g, &ev, 0) > 0) goome_ui_feed(k->u, &ev);
    goome_canvas *c = goome_ui_begin(k->u, 0);
    if (!c) return;
    if (focus == 1) goome_ui_set_focus(k->u);
    goome_ui_input_ex(k->u, "in", k->in, sizeof(k->in), GOOME_INPUT_NO_SELECT_ON_FOCUS, NULL);
    if (focus == 2) goome_ui_set_focus(k->u);
    goome_ui_text_edit(k->u, "ed", k->ed, sizeof(k->ed), 80, 0);
    goome_ui_end(k->u);
    k->hash = fnv(c->px, c->w, c->h, c->stride);
}

static const char *bd_run(goome_font *f, int edit, const char *text, const int *keys, uint32_t *hash)
{
    static bd_t k;
    int err;
    memset(&k, 0, sizeof(k));
    k.g = goome_open(&(goome_opts){.backend = GOOME_TEST, .mode = GOOME_MODE_WINDOW, .width = 300, .height = 200, .test_script = "end 100000"}, &err);
    if (!k.g) return "";
    k.u = goome_ui_new(k.g);
    if (!k.u) return goome_close(k.g), "";
    goome_theme *t = goome_ui_theme(k.u);
    t->font = f, t->font_size = 14, t->anim_ms = 0;
    goome_ui_cfg(k.u)->blink_ms = 0;
    snprintf(edit ? k.ed : k.in, edit ? sizeof(k.ed) : sizeof(k.in), "%s", text);
    bd_frame(&k, edit ? 2 : 1);
    bd_frame(&k, 0);
    for (; *keys; keys += 2) bd_key(&k, keys[0], keys[1]), bd_frame(&k, 0);
    bd_frame(&k, 0);
    if (hash) *hash = k.hash;
    goome_ui_free(k.u);
    goome_close(k.g);
    return edit ? k.ed : k.in;
}

static void bidi_caret(void)
{
    goome_font *f = goome_font_load_mem(colr_ttf, sizeof(colr_ttf), 0);
    CHECK(f != NULL);
    if (!f) return;
    enum { L = GOOME_K_LEFT, R = GOOME_K_RIGHT, H = GOOME_K_HOME, E = GOOME_K_END, S = GOOME_SHIFT };
    uint32_t hs[6];
    const char *heb = "\xD7\x90\xD7\x91\xD7\x92";
    CHECK(!strcmp(bd_run(f, 0, heb, (int[]){H, 0, L, 0, 'a', 0, 0}, &hs[0]), "\xD7\x90" "a\xD7\x91\xD7\x92"));
    CHECK(!strcmp(bd_run(f, 0, heb, (int[]){E, 0, R, 0, 'b', 0, 0}, NULL), "\xD7\x90\xD7\x91" "b\xD7\x92"));
    CHECK(!strcmp(bd_run(f, 0, heb, (int[]){H, 0, R, 0, 'c', 0, 0}, NULL), "c\xD7\x90\xD7\x91\xD7\x92"));
    CHECK(!strcmp(bd_run(f, 0, "abc \xD7\x90\xD7\x91\xD7\x92", (int[]){E, 0, L, 0, 'z', 0, 0}, NULL), "abcz \xD7\x90\xD7\x91\xD7\x92"));
    CHECK(!strcmp(bd_run(f, 0, "ab \xD7\x90\xD7\x91 cd", (int[]){H, 0, R, 0, R, 0, R, 0, R, 0, 'x', 0, 0}, NULL), "ab \xD7\x90x\xD7\x91 cd"));
    bd_run(f, 0, "ab \xD7\x90\xD7\x91\xD7\x92 cd", (int[]){H, 0, R, 0, R, 0, R, 0, R, S, R, S, 0}, &hs[1]);
    CHECK(!strcmp(bd_run(f, 0, "\xF0\x9F\x98\x80\xF0\x9F\x8F\xBB" "a", (int[]){H, 0, R, 0, 'q', 0, 0}, NULL), "\xF0\x9F\x98\x80\xF0\x9F\x8F\xBBqa"));
    CHECK(!strcmp(bd_run(f, 1, "\xD7\x90\xD7\x91\xD7\x92\nabc", (int[]){H, 0, L, 0, 'x', 0, 0}, &hs[2]), "\xD7\x90x\xD7\x91\xD7\x92\nabc"));
    CHECK(!strcmp(bd_run(f, 1, "\xD7\x90\xD7\x91\xD7\x92\nabc", (int[]){H, 0, L, 0, L, 0, L, 0, L, 0, 'y', 0, 0}, NULL), "\xD7\x90\xD7\x91\xD7\x92\ny" "abc"));
    bd_run(f, 1, "\xD7\x90\xD7\x91 ab \xD7\x92\n\xD8\xA8\xD8\xA8", (int[]){H, 0, L, S, L, S, L, S, 0}, &hs[3]);
    bd_run(f, 0, "\xD8\xA8\xD9\x84\xD8\xA7\xD9\x85 12", (int[]){E, 0, L, S, 0}, &hs[4]);
    bd_run(f, 0, "\xD8\xA8\xD9\x84\xD8\xA7\xD9\x85 12", (int[]){E, 0, R, S, 0}, &hs[5]);
    CHECK(hs[4] != hs[5]);
    printf("bidi caret hashes %08x %08x %08x %08x %08x %08x\n", hs[0], hs[1], hs[2], hs[3], hs[4], hs[5]);
    uint32_t want[6] = {0xf96a8023u, 0x8c422f63u, 0xeb1c9776u, 0xfb019b26u, 0x31cd9204u, 0xe15c011au};
    for (int i = 0; i < 6; i++) CHECK(hs[i] == want[i]);
    goome_font_free(f);
}

int main(void)
{
    gm_dump_dir = getenv("GOOME_UNIT_DUMP");
    combos();
    pacer();
    drawing();
    shapes();
    fonts();
    bitmaps();
    scripted();
    ui();
    offscreen();
    frame_copy();
    window_mode();
    app_kit();
    widgets2();
    grab();
    custom_style();
    icons();
    anything();
    input_ex();
    anims();
    emoji_rtl();
    bidi_caret();
    printf(fails ? "%d failed\n" : "all good\n", fails);
    return fails != 0;
}
