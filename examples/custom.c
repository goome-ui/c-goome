#include <stdio.h>
#include <string.h>

#include "goome.h"

static const uint32_t swatches[] = {0xFF6FDC5F, 0xFF3FC8B4, 0xFF4FB3F7, 0xFF7C8CFF, 0xFFB57CFF, 0xFFFF6FA8, 0xFFFF7A59, 0xFFFFB443, 0xFFE8D85A};
enum { NSW = 9, NNODE = 3 };

typedef struct {
    const char *name, *info;
    uint32_t color;
    double x0, y0;
} node_def;

static const node_def nodes[NNODE] = {
    {"Noise", "seed 42", 0xFFB57CFF, 20, 40},
    {"Blur", "radius 6", 0xFF4FB3F7, 240, 120},
    {"Output", "1280x720", 0xFFFF7A59, 460, 40},
};

static double clampd(double v, double a, double b) { return v < a ? a : v > b ? b : v; }

static double snap(double v, double step)
{
    double k = v / step + 0.5;
    long long i = (long long)k;
    if ((double)i > k) i--;
    return (double)i * step;
}

static double sine(double x)
{
    while (x > 3.14159265358979) x -= 6.28318530717959;
    while (x < -3.14159265358979) x += 6.28318530717959;
    double x2 = x * x;
    return x * (1 - x2 / 6 * (1 - x2 / 20 * (1 - x2 / 42 * (1 - x2 / 72 * (1 - x2 / 110)))));
}
static double cosine(double x) { return sine(x + 1.5707963267949); }

static uint32_t fade(uint32_t c, double k) { return (uint32_t)(((c >> 24) & 255) * clampd(k, 0, 1) + 0.5) << 24 | (c & 0xFFFFFF); }

static uint32_t mix(uint32_t a, uint32_t b, double t)
{
    uint32_t out = 0;
    for (int s = 0; s < 32; s += 8) {
        double x = (a >> s) & 255, y = (b >> s) & 255;
        out |= (uint32_t)(x + (y - x) * clampd(t, 0, 1) + 0.5) << s;
    }
    return out;
}

static void text_at(goome_canvas *c, goome_font *f, double size, double x, double cy, const char *s, uint32_t col, int center)
{
    double h = 0, w = goome_text_measure(f, size, s, &h);
    if (h <= 0) h = goome_text_line_height(f, size);
    goome_text(c, f, size, center ? x - w / 2 : x, (double)(int)(cy - h / 2 + 0.5), s, col);
}

static void knob(goome_ui *u, const char *name, double def, double scale, const char *fmt)
{
    double *v = goome_ui_store(u, name, 2);
    if (!v) return;
    if (!v[1]) v[0] = def, v[1] = 1;
    goome_ui_item it;
    goome_ui_custom(u, name, 92, 112, GOOME_ITEM_DRAG_Y | GOOME_ITEM_FOCUSABLE, &it);
    int key = it.key & 0xFF;
    if (it.held) v[0] = clampd(v[0] - it.dy / 180, 0, 1);
    if (key == GOOME_K_UP || key == GOOME_K_RIGHT) v[0] = clampd(v[0] + 0.05, 0, 1);
    if (key == GOOME_K_DOWN || key == GOOME_K_LEFT) v[0] = clampd(v[0] - 0.05, 0, 1);
    if (it.double_clicked) v[0] = def;
    goome_ui_tooltip(u, "drag up or down, arrows work too, double click resets it");

    goome_theme *th = goome_ui_theme(u);
    goome_canvas *c = goome_ui_canvas(u);
    if (!c) return;
    double cx = it.x + it.w / 2, cy = it.y + 40, r = 31, ri = 21;
    double a0 = 2.35619449, a1 = 7.06858347, at = a0 + (a1 - a0) * v[0];
    int on = it.hovered || it.held;
    if (on) goome_draw_circle(c, cx, cy, r + 9, fade(th->accent, it.held ? 0.16 : 0.09));
    goome_draw_arc(c, cx, cy, r, 6, a0, a1, fade(th->text, 0.09));
    if (v[0] > 0.004) goome_draw_arc(c, cx, cy, r, 6, a0, at, th->accent);
    goome_draw_shadow(c, cx - ri, cy - ri + 4, 2 * ri, 2 * ri, ri, 10, 0x70000000);
    goome_draw_rrect_grad(c, cx - ri, cy - ri, 2 * ri, 2 * ri, goome_radius(ri), 0xFF3A3F49, 0xFF24272E, 0);
    goome_draw_ring(c, cx, cy, ri - 0.5, 1, 0x1AFFFFFF);
    double px = cx + cosine(at) * (ri - 7), py = cy + sine(at) * (ri - 7);
    goome_draw_line(c, cx + cosine(at) * (ri - 15), cy + sine(at) * (ri - 15), px, py, 2.6, on ? th->text : mix(th->text_dim, th->text, 0.5));
    char val[32];
    snprintf(val, sizeof(val), fmt, v[0] * scale);
    text_at(c, th->font, 14, cx, it.y + 84, val, th->text, 1);
    text_at(c, th->font, 12.5, cx, it.y + 103, name, th->text_dim, 1);
}

static void timeline(goome_ui *u, double ox, double oy)
{
    double *t = goome_ui_store(u, "playhead", 2);
    if (!t) return;
    if (!t[1]) t[0] = 2.4, t[1] = 1;
    goome_theme *th = goome_ui_theme(u);
    goome_ui_item tr, hd;
    goome_ui_custom(u, "track", 0, 92, GOOME_ITEM_NO_HIT, &tr);
    double x0 = tr.x + 14, x1 = tr.x + tr.w - 14, secs = 10, per = (x1 - x0) / secs, ly0 = tr.y + 34, ly1 = tr.y + 61;
    goome_canvas *c = goome_ui_canvas(u);
    if (!c) return;
    goome_draw_rrect(c, tr.x, tr.y, tr.w, tr.h, goome_radius(10), 0xFF1E2127);
    goome_draw_rrect_line(c, tr.x, tr.y, tr.w, tr.h, goome_radius(10), 1, th->border);
    for (int i = 0; i <= 20; i++) {
        double x = x0 + i * per / 2;
        goome_draw_rect(c, x - 0.5, tr.y + (i % 2 ? 18 : 12), 1, i % 2 ? 5 : 11, fade(th->text, i % 2 ? 0.14 : 0.28));
        if (!(i % 2) && i < 20) {
            char s[8];
            snprintf(s, sizeof(s), "%ds", i / 2);
            goome_text(c, th->font, 11, x + 4, tr.y + 9, s, th->text_dim);
        }
    }
    goome_draw_rrect(c, x0 + 0.4 * per, ly0, 3.6 * per, 22, goome_radius(6), fade(0xFFB57CFF, 0.30));
    goome_draw_rrect_line(c, x0 + 0.4 * per, ly0, 3.6 * per, 22, goome_radius(6), 1, fade(0xFFB57CFF, 0.65));
    goome_text(c, th->font, 12, x0 + 0.4 * per + 9, ly0 + 4, "intro.wav", 0xFFE6DAFF);
    goome_draw_rrect(c, x0 + 3.1 * per, ly1, 5.3 * per, 22, goome_radius(6), fade(0xFF4FB3F7, 0.28));
    goome_draw_rrect_line(c, x0 + 3.1 * per, ly1, 5.3 * per, 22, goome_radius(6), 1, fade(0xFF4FB3F7, 0.6));
    goome_text(c, th->font, 12, x0 + 3.1 * per + 9, ly1 + 4, "drums.wav", 0xFFD6EEFF);

    goome_ui_pos(u, x0 + t[0] * per - 9 - ox, tr.y + 4 - oy);
    goome_ui_push_num(u, GOOME_S_RADIUS, 6);
    goome_ui_custom(u, "handle", 18, tr.h - 8, GOOME_ITEM_DRAG_X | GOOME_ITEM_FOCUSABLE, &hd);
    goome_ui_pop(u, 1);
    int key = hd.key & 0xFF;
    if (hd.held && per > 0) t[0] = clampd(t[0] + hd.dx / per, 0, secs);
    if (key == GOOME_K_LEFT || key == GOOME_K_RIGHT) t[0] = clampd(snap(t[0], 0.5) + (key == GOOME_K_RIGHT ? 0.5 : -0.5), 0, secs);
    if (hd.released) t[0] = snap(t[0], 0.1);
    goome_ui_tooltip(u, "this one only moves sideways");
    double hx = x0 + t[0] * per;

    c = goome_ui_canvas(u);
    if (!c) return;
    static const double keys[] = {1.2, 2.6, 5.0, 6.4, 7.7};
    for (int i = 0; i < 5; i++) {
        double kx = x0 + keys[i] * per, ky = ly1 + 11, d = 4.5;
        double pts[] = {kx, ky - d, kx + d, ky, kx, ky + d, kx - d, ky};
        goome_draw_poly(c, pts, 4, keys[i] <= t[0] ? th->accent : 0xFFD6EEFF);
    }
    goome_draw_rect(c, x0, tr.y + tr.h - 5, hx - x0, 2, fade(th->accent, 0.7));
    int on = hd.hovered || hd.held;
    goome_draw_rect(c, hx - 1, tr.y + 20, 2, tr.h - 25, th->accent);
    uint32_t hc = on ? mix(th->accent, 0xFFFFFFFF, 0.25) : th->accent;
    goome_draw_shadow(c, hx - 8, tr.y + 7, 16, 13, 4, 6, 0x60000000);
    goome_draw_rrect(c, hx - 8, tr.y + 5, 16, 13, goome_radius(4), hc);
    goome_draw_triangle(c, hx - 6, tr.y + 17, hx + 6, tr.y + 17, hx, tr.y + 23, hc);
    char s[16];
    snprintf(s, sizeof(s), "%.1fs", t[0]);
    double w = goome_text_measure(th->font, 11.5, s, NULL);
    double bx = clampd(hx + 12, tr.x + 6, tr.x + tr.w - w - 18);
    goome_draw_rrect(c, bx, tr.y + 4, w + 10, 16, goome_radius(5), fade(th->accent, 0.18));
    goome_text(c, th->font, 11.5, bx + 5, tr.y + 5, s, th->accent);
}

static int swatch_strip(goome_ui *u, int *sel)
{
    int ch = 0;
    for (int i = 0; i < NSW; i++) {
        char id[8];
        snprintf(id, sizeof(id), "sw%d", i);
        if (i) goome_ui_same_line(u);
        goome_ui_item it;
        goome_ui_custom(u, id, 34, 34, GOOME_ITEM_FOCUSABLE, &it);
        if (it.clicked && *sel != i) *sel = i, ch = 1;
        goome_canvas *c = goome_ui_canvas(u);
        if (!c) continue;
        double lift = it.hovered && !it.held ? 2 : 0;
        if (i == *sel) goome_draw_rrect_line(c, it.x - 4, it.y - 4 - lift, it.w + 8, it.h + 8, goome_radius(13), 2, 0xFFFFFFFF);
        goome_draw_shadow(c, it.x, it.y + 3 - lift, it.w, it.h, 9, 8 + lift * 2, 0x66000000);
        goome_draw_rrect_grad(c, it.x, it.y - lift, it.w, it.h, goome_radius(9), mix(swatches[i], 0xFFFFFFFF, 0.12), mix(swatches[i], 0xFF000000, 0.12), 0);
        goome_draw_rrect_line(c, it.x, it.y - lift, it.w, it.h, goome_radius(9), 1, 0x26FFFFFF);
        if (i == *sel) {
            double cx = it.x + it.w / 2, cy = it.y + it.h / 2 - lift;
            goome_draw_line(c, cx - 6, cy, cx - 2, cy + 4, 2.4, 0xE0141618);
            goome_draw_line(c, cx - 2, cy + 4, cx + 6.5, cy - 5, 2.4, 0xE0141618);
        }
    }
    return ch;
}

static void faders(goome_ui *u)
{
    static const char *const hz[] = {"60", "150", "400", "1k", "2.4k", "6k", "15k"};
    static const double start[] = {0.7, 0.62, 0.45, 0.5, 0.58, 0.68, 0.74};
    double *v = goome_ui_store(u, "eq", 8);
    if (!v) return;
    if (!v[7]) {
        for (int i = 0; i < 7; i++) v[i] = start[i];
        v[7] = 1;
    }
    for (int i = 0; i < 7; i++) {
        char id[8];
        snprintf(id, sizeof(id), "eq%d", i);
        if (i) goome_ui_same_line(u);
        goome_ui_item it;
        goome_ui_custom(u, id, 28, 138, GOOME_ITEM_DRAG_Y | GOOME_ITEM_FOCUSABLE, &it);
        double top = it.y + 8, bot = it.y + it.h - 26, len = bot - top;
        int key = it.key & 0xFF;
        if (it.pressed && (it.my < top + (1 - v[i]) * len - 8 || it.my > top + (1 - v[i]) * len + 8)) v[i] = clampd(1 - (it.my - top) / len, 0, 1);
        else if (it.held) v[i] = clampd(v[i] - it.dy / len, 0, 1);
        if (key == GOOME_K_UP || key == GOOME_K_DOWN) v[i] = clampd(v[i] + (key == GOOME_K_UP ? 0.05 : -0.05), 0, 1);
        if (it.double_clicked) v[i] = 0.5;
        goome_theme *th = goome_ui_theme(u);
        goome_canvas *c = goome_ui_canvas(u);
        if (!c) continue;
        double cx = it.x + it.w / 2, ty = top + (1 - v[i]) * len, mid = top + len / 2;
        goome_draw_rrect(c, cx - 2, top, 4, len, goome_radius(2), fade(th->text, 0.09));
        goome_draw_rect(c, cx - 6, mid - 0.5, 12, 1, fade(th->text, 0.2));
        goome_draw_rrect(c, cx - 2, ty < mid ? ty : mid, 4, ty < mid ? mid - ty : ty - mid, goome_radius(2), th->accent);
        int on = it.hovered || it.held;
        goome_draw_shadow(c, cx - 11, ty - 5 + 2, 22, 10, 5, 6, 0x70000000);
        goome_draw_rrect_grad(c, cx - 11, ty - 5, 22, 10, goome_radius(5), on ? 0xFFFFFFFF : 0xFFE4E7EC, on ? 0xFFDDE1E7 : 0xFFB9BEC7, 0);
        goome_draw_rect(c, cx - 5, ty - 0.5, 10, 1, fade(th->accent, 0.9));
        text_at(c, th->font, 11, cx, it.y + it.h - 9, hz[i], on ? th->text : th->text_dim, 1);
    }
}

static void xy_pad(goome_ui *u)
{
    double *v = goome_ui_store(u, "xy", 3);
    if (!v) return;
    if (!v[2]) v[0] = 0.62, v[1] = 0.38, v[2] = 1;
    goome_ui_item it;
    goome_ui_custom(u, "xy", 0, 138, GOOME_ITEM_DRAG_X | GOOME_ITEM_DRAG_Y, &it);
    if (it.held) v[0] = clampd((it.mx - it.x - 8) / (it.w - 16), 0, 1), v[1] = clampd((it.my - it.y - 8) / (it.h - 16), 0, 1);
    goome_theme *th = goome_ui_theme(u);
    goome_canvas *c = goome_ui_canvas(u);
    if (!c) return;
    double px = it.x + 8 + v[0] * (it.w - 16), py = it.y + 8 + v[1] * (it.h - 16);
    goome_draw_rrect(c, it.x, it.y, it.w, it.h, goome_radius(12), 0xFF1B1E23);
    goome_draw_rrect_grad(c, it.x, it.y, it.w, it.h, goome_radius(12), fade(th->accent, 0.0), fade(th->accent, 0.10), 1);
    for (int i = 1; i < 4; i++) {
        goome_draw_rect(c, it.x + it.w * i / 4, it.y + 6, 1, it.h - 12, 0x0FFFFFFF);
        goome_draw_rect(c, it.x + 6, it.y + it.h * i / 4, it.w - 12, 1, 0x0FFFFFFF);
    }
    goome_draw_rect(c, px - 0.5, it.y + 4, 1, it.h - 8, fade(th->accent, 0.35));
    goome_draw_rect(c, it.x + 4, py - 0.5, it.w - 8, 1, fade(th->accent, 0.35));
    goome_draw_circle(c, px, py, it.held ? 18 : 14, fade(th->accent, it.held ? 0.22 : 0.14));
    goome_draw_circle(c, px, py, 7, th->accent);
    goome_draw_ring(c, px, py, 7, 2, 0xFFFFFFFF);
    char s[32];
    snprintf(s, sizeof(s), "cutoff %.2f  res %.2f", v[0], 1 - v[1]);
    goome_text(c, th->font, 11.5, it.x + 12, it.y + 9, s, th->text_dim);
    goome_draw_rrect_line(c, it.x, it.y, it.w, it.h, goome_radius(12), 1, it.hovered || it.held ? fade(th->accent, 0.6) : th->border);
}

static void node_box(goome_canvas *c, const goome_theme *th, const node_def *d, goome_ui_item *it, double x, double y, int top)
{
    double w = it->w, h = it->h;
    goome_draw_shadow(c, x, y + (top ? 10 : 5), w, h, 12, top ? 22 : 12, top ? 0x90000000 : 0x70000000);
    goome_draw_rrect(c, x, y, w, h, goome_radius(12), 0xFF2A2E36);
    goome_radii hr = {12, 12, 0, 0};
    goome_draw_rrect_grad(c, x, y, w, 26, hr, mix(d->color, 0xFF2A2E36, 0.25), mix(d->color, 0xFF2A2E36, 0.55), 1);
    goome_draw_circle(c, x + 15, y + 13, 4, 0xFFFFFFFF);
    goome_text(c, th->font, 13.5, x + 26, y + 5, d->name, 0xFFFFFFFF);
    goome_text(c, th->font, 12, x + 12, y + 35, d->info, th->text_dim);
    uint32_t edge = it->held || it->hovered ? mix(th->accent, 0xFFFFFFFF, it->held ? 0.2 : 0) : 0x22FFFFFF;
    goome_draw_rrect_line(c, x, y, w, h, goome_radius(12), it->held || it->hovered ? 1.5 : 1, edge);
    goome_draw_circle(c, x, y + 41, 5.5, 0xFF1A1C21);
    goome_draw_ring(c, x, y + 41, 5, 2, d->color);
    goome_draw_circle(c, x + w, y + 41, 5.5, 0xFF1A1C21);
    goome_draw_ring(c, x + w, y + 41, 5, 2, d->color);
}

static void wire(goome_canvas *c, double ax, double ay, double bx, double by, uint32_t from, uint32_t to)
{
    double px = ax, py = ay, pull = clampd((bx - ax) * 0.5, 40, 200);
    for (int i = 1; i <= 32; i++) {
        double t = i / 32.0, s = 1 - t;
        double x = s * s * s * ax + 3 * s * s * t * (ax + pull) + 3 * s * t * t * (bx - pull) + t * t * t * bx;
        double y = s * s * s * ay + 3 * s * s * t * ay + 3 * s * t * t * by + t * t * t * by;
        goome_draw_line(c, px, py, x, y, 2.5, mix(from, to, t - 1 / 64.0));
        px = x, py = y;
    }
}

static void node_area(goome_ui *u, double ox, double oy)
{
    goome_ui_item ar, it[NNODE];
    goome_ui_custom(u, "area", 0, 232, GOOME_ITEM_NO_HIT, &ar);
    double *pos[NNODE];
    int top = -1;
    for (int i = 0; i < NNODE; i++) {
        pos[i] = goome_ui_store(u, nodes[i].name, 3);
        if (!pos[i]) return;
        if (!pos[i][2]) pos[i][0] = nodes[i].x0, pos[i][1] = nodes[i].y0, pos[i][2] = 1;
        goome_ui_pos(u, ar.x - ox + pos[i][0], ar.y - oy + pos[i][1]);
        goome_ui_custom(u, nodes[i].name, 150, 58, GOOME_ITEM_DRAG_X | GOOME_ITEM_DRAG_Y, &it[i]);
        double *p = pos[i];
        if (it[i].held) p[0] += it[i].dx, p[1] += it[i].dy, top = i;
        p[0] = clampd(p[0], 8, ar.w - it[i].w - 8), p[1] = clampd(p[1], 8, ar.h - it[i].h - 8);
        if (it[i].released) p[0] = clampd(snap(p[0], 20), 0, ar.w - it[i].w), p[1] = clampd(snap(p[1], 20), 0, ar.h - it[i].h);
    }
    goome_theme *th = goome_ui_theme(u);
    goome_canvas *c = goome_ui_canvas(u);
    if (!c) return;
    goome_draw_rrect(c, ar.x, ar.y, ar.w, ar.h, goome_radius(12), 0xFF17191E);
    goome_clip_push(c, ar.x + 1, ar.y + 1, ar.w - 2, ar.h - 2);
    for (double y = ar.y + 20; y < ar.y + ar.h; y += 20)
        for (double x = ar.x + 20; x < ar.x + ar.w; x += 20) goome_draw_rect(c, x - 1, y - 1, 2, 2, 0x16FFFFFF);
    if (top >= 0) {
        double sx = clampd(snap(pos[top][0], 20), 0, ar.w - it[top].w), sy = clampd(snap(pos[top][1], 20), 0, ar.h - it[top].h);
        goome_draw_rrect(c, ar.x + sx, ar.y + sy, it[top].w, it[top].h, goome_radius(12), fade(th->accent, 0.08));
        goome_draw_rrect_line(c, ar.x + sx, ar.y + sy, it[top].w, it[top].h, goome_radius(12), 1.5, fade(th->accent, 0.45));
    }
    for (int i = 0; i + 1 < NNODE; i++)
        wire(c, ar.x + pos[i][0] + it[i].w, ar.y + pos[i][1] + 41, ar.x + pos[i + 1][0], ar.y + pos[i + 1][1] + 41, nodes[i].color, nodes[i + 1].color);
    for (int i = 0; i < NNODE; i++)
        if (i != top) node_box(c, th, &nodes[i], &it[i], ar.x + pos[i][0], ar.y + pos[i][1], 0);
    if (top >= 0) node_box(c, th, &nodes[top], &it[top], ar.x + pos[top][0], ar.y + pos[top][1], 1);
    goome_text(c, th->font, 12, ar.x + 14, ar.y + ar.h - 24, "drag the boxes, they snap to the grid when you let go", fade(th->text_dim, 0.8));
    goome_clip_pop(c);
    goome_draw_rrect_line(c, ar.x, ar.y, ar.w, ar.h, goome_radius(12), 1, th->border);
}

static void button_rows(goome_ui *u, int *seg)
{
    goome_ui_heading(u, "Buttons your way");
    goome_ui_wrapped(u, "push a radius, a color or per corner radii, then pop it. only the widgets in between change.");
    goome_ui_button(u, "Normal");
    goome_ui_same_line(u);
    goome_ui_push_num(u, GOOME_S_RADIUS, 0);
    goome_ui_button(u, "Square");
    goome_ui_pop(u, 1);
    goome_ui_same_line(u);
    goome_ui_push_num(u, GOOME_S_RADIUS, 999);
    goome_ui_button(u, "Pill");
    goome_ui_pop(u, 1);
    goome_ui_same_line(u);
    goome_ui_push_radii(u, (goome_radii){16, 3, 16, 3});
    goome_ui_button(u, "Leaf");
    goome_ui_pop(u, 1);

    static const char *const names[] = {"Day", "Week", "Month", "Year"};
    goome_ui_push_num(u, GOOME_S_GAP, 1);
    for (int i = 0; i < 4; i++) {
        goome_radii r = {i == 0 ? 9 : 2, i == 3 ? 9 : 2, i == 3 ? 9 : 2, i == 0 ? 9 : 2};
        if (i) goome_ui_same_line(u);
        goome_ui_push_radii(u, r);
        if (*seg == i) goome_ui_push_color(u, GOOME_S_INPUT_BG, 0xFF3B4250), goome_ui_push_color(u, GOOME_S_BORDER, 0x40FFFFFF);
        else goome_ui_push_color(u, GOOME_S_TEXT, 0xFF969CA6), goome_ui_push_color(u, GOOME_S_BORDER, 0x00000000);
        goome_ui_size(u, 74, 0);
        if (goome_ui_button(u, names[i])) *seg = i;
        goome_ui_pop(u, 3);
    }
    goome_ui_pop(u, 1);
    goome_ui_space(u, 8);

    goome_ui_push_color(u, GOOME_S_INPUT_BG, 0xFF6FDC5F);
    goome_ui_push_color(u, GOOME_S_HOVER, 0xFF86E878);
    goome_ui_push_color(u, GOOME_S_ACTIVE, 0xFF58C24A);
    goome_ui_push_color(u, GOOME_S_TEXT, 0xFF0F1D0C);
    goome_ui_push_color(u, GOOME_S_BORDER, 0x00000000);
    goome_ui_push_num(u, GOOME_S_RADIUS, 999);
    goome_ui_button(u, "Save it");
    goome_ui_pop(u, 6);
    goome_ui_same_line(u);
    goome_ui_push_color(u, GOOME_S_INPUT_BG, 0x00000000);
    goome_ui_push_color(u, GOOME_S_HOVER, 0x24E5534B);
    goome_ui_push_color(u, GOOME_S_ACTIVE, 0x40E5534B);
    goome_ui_push_color(u, GOOME_S_TEXT, 0xFFFF7A72);
    goome_ui_push_color(u, GOOME_S_BORDER, 0x80E5534B);
    goome_ui_push_num(u, GOOME_S_BORDER_WIDTH, 1.5);
    goome_ui_button(u, "Delete");
    goome_ui_pop(u, 6);
    goome_ui_same_line(u);
    goome_ui_push_num(u, GOOME_S_FONT_SIZE, 19);
    goome_ui_push_num(u, GOOME_S_PAD, 14);
    goome_ui_push_radii(u, (goome_radii){4, 18, 4, 18});
    goome_ui_button(u, "Big one");
    goome_ui_pop(u, 3);

    goome_ui_push_radii(u, (goome_radii){12, 12, 0, 0});
    goome_ui_push_num(u, GOOME_S_GAP, 2);
    static const char *const tabs[] = {"Files", "Search", "Git"};
    static int tab = 0;
    for (int i = 0; i < 3; i++) {
        if (i) goome_ui_same_line(u);
        goome_ui_push_color(u, GOOME_S_INPUT_BG, tab == i ? 0xFF2F333C : 0xFF1F2228);
        goome_ui_push_color(u, GOOME_S_BORDER, tab == i ? 0x30FFFFFF : 0x00000000);
        goome_ui_push_color(u, GOOME_S_TEXT, tab == i ? 0xFFE9EBEE : 0xFF8C929C);
        if (goome_ui_button(u, tabs[i])) tab = i;
        goome_ui_pop(u, 3);
    }
    goome_ui_pop(u, 2);
}

static void more_widgets(goome_ui *u, double *amt, int *chk, char *find, int cap)
{
    goome_ui_heading(u, "Every widget reads it");
    goome_ui_push_color(u, GOOME_S_ACCENT, 0xFFFF6FA8);
    goome_ui_push_color(u, GOOME_S_ACCENT_TEXT, 0xFF2A0614);
    goome_ui_row(u, 2);
    goome_ui_checkbox(u, "Pink", chk);
    goome_ui_toggle(u, "Also pink", chk);
    goome_ui_slider(u, "Amount", amt, 0, 1, "%.2f");
    goome_ui_pop(u, 2);
    goome_ui_push_num(u, GOOME_S_RADIUS, 999);
    goome_ui_input(u, "##find", find, cap);
    goome_ui_pop(u, 1);
    goome_ui_push_num(u, GOOME_S_RADIUS, 0);
    goome_ui_push_color(u, GOOME_S_ACCENT, 0xFF4FB3F7);
    goome_ui_push_color(u, GOOME_S_ACCENT_TEXT, 0xFF06202E);
    goome_ui_progress(u, *amt, "square corners");
    goome_ui_pop(u, 3);
}

static void light_card(goome_ui *u, double *vol, int *on, int *pick, char *name, int cap)
{
    goome_ui_item at;
    goome_ui_custom(u, "light-spot", 0, 262, GOOME_ITEM_NO_HIT, &at);
    goome_theme t = *goome_ui_theme(u);
    goome_ui_theme_light(&t);
    t.radius = 8, t.shadow = 0x60000000;
    goome_ui_push_theme(u, &t);
    double x = at.x, y = at.y, w = at.w, h = at.h;
    if (goome_ui_window(u, "A whole other theme", &x, &y, &w, &h, 0, NULL)) {
        goome_ui_toggle(u, "Wifi", on);
        goome_ui_slider(u, "Volume", vol, 0, 1, "%.2f");
        goome_ui_row(u, 2);
        goome_ui_radio(u, "Small", pick, 0);
        goome_ui_radio(u, "Large", pick, 1);
        goome_ui_input(u, "##name", name, cap);
        goome_ui_push_num(u, GOOME_S_RADIUS, 999);
        goome_ui_button(u, "Cancel");
        goome_ui_same_line(u);
        goome_ui_push_color(u, GOOME_S_INPUT_BG, t.accent);
        goome_ui_push_color(u, GOOME_S_HOVER, 0xFF55C944);
        goome_ui_push_color(u, GOOME_S_ACTIVE, 0xFF3AA12B);
        goome_ui_push_color(u, GOOME_S_TEXT, 0xFFFFFFFF);
        goome_ui_push_color(u, GOOME_S_BORDER, 0x00000000);
        goome_ui_button(u, "Apply");
        goome_ui_pop(u, 6);
    }
    goome_ui_window_end(u);
    goome_ui_pop(u, 1);
}

int main(void)
{
    int err;
    goome *g = goome_open(&(goome_opts){.title = "Custom", .mode = GOOME_MODE_WINDOW, .width = 1120, .height = 800, .min_width = 800, .min_height = 560,
                                        .window_flags = GOOME_WINDOW_RESIZABLE | GOOME_WINDOW_CENTER},
                          &err);
    if (!g) {
        fprintf(stderr, "cant open a window: %s\n", goome_strerror(err));
        return 1;
    }
    goome_ui *u = goome_ui_new(g);
    if (!u) {
        fprintf(stderr, "no memory for the ui\n");
        goome_close(g);
        return 1;
    }
    int running = 1, test = !strncmp(goome_backend_name(g), "test", 4), seg = 1, sel = 0, on = 1, pick = 1, chk = 1;
    double frac = 0.4, vol = 0.62, amt = 0.7;
    char name[64] = "living room", find[64] = "search, but round";
    goome_pacer pace = {0};
    while (running) {
        goome_event ev;
        int wait = test || goome_ui_animating(u) ? goome_pace(&pace, 60) : 500;
        while (goome_poll(g, &ev, wait) > 0) {
            if (ev.type == GOOME_QUIT || ev.type == GOOME_CLOSE) running = 0;
            goome_ui_feed(u, &ev);
            wait = 0;
        }
        if (!running) break;
        if (!goome_ui_begin(u, 0)) continue;
        goome_ui_app(u, "Custom", GOOME_APP_STATUSBAR | GOOME_APP_NO_PAD);
        goome_ui_split_begin(u, "main", 0, &frac);
        button_rows(u, &seg);
        goome_ui_space(u, 4);
        more_widgets(u, &amt, &chk, find, sizeof(find));
        goome_ui_space(u, 6);
        light_card(u, &vol, &on, &pick, name, sizeof(name));
        goome_ui_split_next(u);

        goome_ui_item o;
        goome_ui_pos(u, 0, 0);
        goome_ui_custom(u, "##origin", 1, 1, GOOME_ITEM_NO_HIT, &o);
        goome_ui_push_color(u, GOOME_S_ACCENT, swatches[sel]);
        goome_ui_heading(u, "Custom widgets");
        goome_ui_pos(u, 176, 3);
        goome_ui_badge(u, "pinned with goome_ui_pos", 0x26FFFFFF);
        goome_ui_space(u, 2);
        knob(u, "Gain", 0.62, 12, "%.1f dB");
        goome_ui_same_line(u);
        knob(u, "Tone", 0.35, 100, "%.0f%%");
        goome_ui_same_line(u);
        knob(u, "Mix", 0.8, 100, "%.0f%%");
        goome_ui_same_line(u);
        knob(u, "Width", 0.5, 200, "%.0f%%");
        goome_ui_space(u, 4);
        timeline(u, o.x, o.y);
        goome_ui_space(u, 4);
        swatch_strip(u, &sel);
        goome_ui_space(u, 4);
        node_area(u, o.x, o.y);
        goome_ui_space(u, 4);
        faders(u);
        goome_ui_same_line(u);
        xy_pad(u);
        double *ph = goome_ui_store(u, "playhead", 2), *gain = goome_ui_store(u, "Gain", 2);
        goome_ui_pop(u, 1);
        goome_ui_split_end(u);

        char st[96];
        snprintf(st, sizeof(st), "gain %.1f dB   playhead %.1fs", gain ? gain[0] * 12 : 0, ph ? ph[0] : 0);
        goome_ui_statusbar(u, st);
        goome_ui_app_end(u);
        goome_ui_end(u);
    }
    goome_ui_free(u);
    goome_close(g);
    return 0;
}
