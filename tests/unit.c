// goome's own checks, no screen needed. ./build.sh test runs it
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "goome.h"
#include "fontdata.h"
#include "images.h"

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
    window_mode();
    app_kit();
    widgets2();
    printf(fails ? "%d failed\n" : "all good\n", fails);
    return fails != 0;
}
