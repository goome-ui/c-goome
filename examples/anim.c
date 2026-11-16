#include <stdio.h>
#include <string.h>

#include "goome.h"

typedef struct {
    const char *name;
    int ease;
    uint32_t color;
} ease_row;

static const ease_row rows[] = {
    {"linear", GOOME_EASE_LINEAR, 0xFF7C8CFF}, {"in", GOOME_EASE_IN, 0xFF4FB3F7},       {"out", GOOME_EASE_OUT, 0xFF3FC8B4},
    {"in out", GOOME_EASE_IN_OUT, 0xFF6FDC5F}, {"smooth", GOOME_EASE_SMOOTH, 0xFFE8D85A}, {"back", GOOME_EASE_BACK, 0xFFFFB443},
    {"spring", GOOME_EASE_SPRING, 0xFFFF7A59}, {"bounce", GOOME_EASE_BOUNCE, 0xFFFF6FA8},
};
enum { NROW = sizeof(rows) / sizeof(rows[0]) };

static const uint32_t moods[] = {0xFF2D6CDF, 0xFFE0483E, 0xFF21A66B, 0xFFF2B33D, 0x00000000};
static const char *const mood_names[] = {"blue", "red", "green", "gold", "see through"};
enum { NMOOD = 5 };

static uint32_t fade(uint32_t c, double k)
{
    if (k < 0) k = 0;
    if (k > 1) k = 1;
    return (uint32_t)(((c >> 24) & 255) * k + 0.5) << 24 | (c & 0xFFFFFF);
}

static void text_at(goome_canvas *c, goome_font *f, double size, double x, double cy, const char *s, uint32_t col, int center)
{
    double h = 0, w = goome_text_measure(f, size, s, &h);
    if (h <= 0) h = goome_text_line_height(f, size);
    goome_text(c, f, size, center ? x - w / 2 : x, (double)(int)(cy - h / 2 + 0.5), s, col);
}

static void ease_rows(goome_ui *u, int side)
{
    goome_theme *th = goome_ui_theme(u);
    for (int i = 0; i < NROW; i++) {
        goome_ui_item it;
        goome_ui_custom(u, rows[i].name, 0, 30, GOOME_ITEM_NO_HIT, &it);
        double k = goome_ui_anim(u, rows[i].name, side, 900, rows[i].ease);
        goome_canvas *c = goome_ui_canvas(u);
        if (!c) continue;
        double tx = it.x + 86, tw = it.w - 96, bw = 46, run = (tw - bw) * 0.76, x = tx + (tw - bw - run) / 2 + k * run;
        text_at(c, th->font, 13, it.x + 4, it.y + 15, rows[i].name, th->text_dim, 0);
        goome_draw_rrect(c, tx, it.y + 3, tw, 24, goome_radius(12), 0x14FFFFFF);
        goome_draw_rect(c, tx + (tw - bw - run) / 2 + bw / 2, it.y + 14, 1, 2, 0x30FFFFFF);
        goome_draw_rect(c, tx + (tw - bw - run) / 2 + run + bw / 2, it.y + 14, 1, 2, 0x30FFFFFF);
        goome_draw_shadow(c, x, it.y + 8, bw, 16, 8, 8, fade(rows[i].color, 0.45));
        goome_draw_rrect(c, x, it.y + 6, bw, 18, goome_radius(9), rows[i].color);
    }
}

static void color_card(goome_ui *u, int *mood)
{
    goome_theme *th = goome_ui_theme(u);
    goome_ui_item it;
    goome_ui_custom(u, "mood", 0, 92, 0, &it);
    if (it.clicked) *mood = (*mood + 1) % NMOOD;
    goome_ui_tooltip(u, "click for the next color");
    uint32_t col = goome_ui_anim_color(u, "mood", moods[*mood], 600, GOOME_EASE_IN_OUT);
    double lift = goome_ui_anim(u, "mood lift", it.hovered ? 1 : 0, 160, GOOME_EASE_OUT);
    goome_canvas *c = goome_ui_canvas(u);
    if (!c) return;
    double y = it.y - lift * 2;
    for (int i = 0; i < 12; i++) goome_draw_rect(c, it.x + i * it.w / 12, it.y + 8, it.w / 24, 76, 0x10FFFFFF);
    goome_draw_shadow(c, it.x, y + 6, it.w, 84, 14, 18, fade(col, 0.4 + lift * 0.2));
    goome_draw_rrect(c, it.x, y, it.w, 84, goome_radius(14), col);
    goome_draw_ring(c, it.x + 30, y + 42, 12, 2, 0x50FFFFFF);
    char buf[64];
    snprintf(buf, sizeof(buf), "%s  #%08X", mood_names[*mood], (unsigned)col);
    text_at(c, th->font, 15, it.x + 54, y + 42, buf, th->text, 0);
}

static int spring_toggle(goome_ui *u, const char *id, int *on)
{
    goome_theme *th = goome_ui_theme(u);
    goome_ui_item it;
    int hit = goome_ui_custom(u, id, 64, 36, GOOME_ITEM_FOCUSABLE, &it);
    int key = it.key & 0xFF;
    if (it.clicked || key == ' ' || key == GOOME_K_ENTER) *on = !*on;
    double k = goome_ui_anim(u, id, *on, 520, GOOME_EASE_SPRING);
    double squish = goome_ui_anim(u, "squish", it.held ? 1 : 0, 220, GOOME_EASE_SPRING);
    uint32_t track = goome_ui_anim_color(u, id, *on ? th->accent : 0xFF3A3F49, 260, GOOME_EASE_OUT);
    goome_canvas *c = goome_ui_canvas(u);
    if (!c) return hit;
    double x = it.x + 2, y = it.y + 4, w = 60, h = 28, r = 11, kw = 2 * r + squish * 8;
    if (it.focused) goome_draw_rrect(c, x - 3, y - 3, w + 6, h + 6, goome_radius(17), fade(th->focus ? th->focus : th->accent, 0.6));
    goome_draw_rrect(c, x, y, w, h, goome_radius(14), track);
    double kx = x + 3 + k * (w - 6 - kw);
    goome_draw_shadow(c, kx, y + 5, kw, 2 * r, r, 6, 0x60000000);
    goome_draw_rrect(c, kx, y + 3, kw, 2 * r, goome_radius(r), 0xFFF4F5F7);
    return hit;
}

static void bell(goome_ui *u, int *count, int *kick)
{
    goome_theme *th = goome_ui_theme(u);
    goome_ui_item it;
    goome_ui_custom(u, "bell", 140, 64, 0, &it);
    if (it.clicked) (*count)++, *kick = 1;
    double drop = goome_ui_anim(u, "drop", *kick ? -46 : 0, *kick ? 0 : 900, GOOME_EASE_BOUNCE);
    double pop = goome_ui_anim(u, "pop", *kick ? 0.4 : 1, *kick ? 0 : 500, GOOME_EASE_SPRING);
    double shown = goome_ui_anim(u, "shown", *count > 0, 200, GOOME_EASE_OUT);
    *kick = 0;
    goome_canvas *c = goome_ui_canvas(u);
    if (!c) return;
    uint32_t bg = goome_ui_anim_color(u, "bell bg", it.hovered ? 0xFF2E333C : 0xFF262A31, 140, GOOME_EASE_OUT);
    goome_draw_rrect(c, it.x, it.y + 8, 120, 52, goome_radius(12), bg);
    text_at(c, th->font, 14, it.x + 60, it.y + 34, "notify me", th->text, 1);
    if (shown <= 0.01) return;
    char n[16];
    snprintf(n, sizeof(n), "%d", *count);
    double r = 13 * pop, cx = it.x + 116, cy = it.y + 10 + drop;
    goome_draw_shadow(c, cx - r, cy - r + 3, 2 * r, 2 * r, r, 8, fade(0x90000000, shown));
    goome_draw_circle(c, cx, cy, r, fade(0xFFE0483E, shown));
    if (pop > 0.6) text_at(c, th->font, 12 * pop, cx, cy, n, fade(0xFFFFFFFF, shown), 1);
}

int main(void)
{
    int err;
    goome *g = goome_open(&(goome_opts){.title = "Animations", .mode = GOOME_MODE_WINDOW, .width = 880, .height = 760, .min_width = 620, .min_height = 700,
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
    int running = 1, test = !strncmp(goome_backend_name(g), "test", 4), side = 0, autoplay = 1, mood = 0, on = 0, count = 0, kick = 0;
    long frame = 0, flipped = 0;
    goome_pacer pace = {0};
    while (running) {
        goome_event ev;
        int wait = test || autoplay || goome_ui_animating(u) ? goome_pace(&pace, 60) : 500;
        while (goome_poll(g, &ev, wait) > 0) {
            if (ev.type == GOOME_QUIT || ev.type == GOOME_CLOSE) running = 0;
            goome_ui_feed(u, &ev);
            wait = 0;
        }
        if (!running) break;
        long now = test ? frame * 16 : goome_now_ms();
        frame++;
        if (autoplay && now - flipped >= 1500) {
            flipped = now, side = !side;
            if (side) mood = (mood + 1) % NMOOD, on = !on, count++, kick = 1;
        }
        if (!goome_ui_begin(u, 0)) continue;
        goome_ui_app(u, "Animations", GOOME_APP_STATUSBAR);
        goome_ui_heading(u, "Every ease, same 900 ms");
        if (goome_ui_button(u, "Swap sides")) side = !side, flipped = now;
        goome_ui_same_line(u);
        goome_ui_toggle(u, "Play by itself", &autoplay);
        goome_ui_space(u, 4);
        ease_rows(u, side);
        goome_ui_space(u, 8);
        goome_ui_heading(u, "Color fade");
        color_card(u, &mood);
        goome_ui_space(u, 8);
        goome_ui_heading(u, "Spring and bounce");
        spring_toggle(u, "springy", &on);
        goome_ui_same_line(u);
        goome_ui_label(u, on ? "on, and it wobbled a bit" : "off");
        goome_ui_same_line(u);
        goome_ui_space(u, 40);
        goome_ui_same_line(u);
        bell(u, &count, &kick);
        goome_ui_statusbar(u, goome_ui_animating(u) ? "moving" : "still");
        goome_ui_end(u);
    }
    goome_ui_free(u);
    goome_close(g);
    return 0;
}
