// goome's own checks, no screen needed. ./build.sh test runs it
#include <stdio.h>
#include <string.h>

#include "goome.h"
#include "fontdata.h"
#include "images.h"

static int fails;
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

int main(void)
{
    combos();
    pacer();
    drawing();
    shapes();
    fonts();
    bitmaps();
    scripted();
    ui();
    printf(fails ? "%d failed\n" : "all good\n", fails);
    return fails != 0;
}
