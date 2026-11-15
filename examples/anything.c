#include <stdio.h>
#include <string.h>

#include "goome.h"

enum { RETRO, SOFT, HUD, DENSE, NLOOK };

typedef struct {
    int tab, sound, online, level, mode;
    double volume;
    char name[48];
} card;

typedef struct {
    uint32_t px[24 * 24];
    goome_bitmap bm;
} sprite;

typedef struct {
    card c[NLOOK + 1];
    goome_theme look[NLOOK];
    sprite term[7], hud[8], frame, plate, tab;
    goome_font *mono, *bold;
    int start, focus_once;
    char pass[32], code[8], server[16];
    double zoom;
} world;

static goome_canvas pen(sprite *s, int w, int h)
{
    goome_canvas c;
    memset(&c, 0, sizeof(c));
    memset(s->px, 0, sizeof(s->px));
    c.px = s->px, c.w = c.stride = w, c.h = h, c.scale = 1;
    s->bm.px = s->px, s->bm.w = w, s->bm.h = h, s->bm.stride = w, s->bm.owned = 0;
    return c;
}

static void term_icons(world *w)
{
    goome_canvas c = pen(&w->term[0], 12, 12);
    goome_draw_rect(&c, 2, 2, 8, 8, 0xFFFFFFFF);
    c = pen(&w->term[1], 10, 10);
    goome_draw_rect(&c, 2, 2, 6, 6, 0xFFFFFFFF);
    c = pen(&w->term[2], 12, 12);
    goome_draw_rect(&c, 0, 0, 12, 12, 0xFFFFFFFF);
    c = pen(&w->term[3], 12, 12);
    goome_draw_rect(&c, 1, 3, 10, 2, 0xFFFFFFFF), goome_draw_rect(&c, 3, 5, 6, 2, 0xFFFFFFFF), goome_draw_rect(&c, 5, 7, 2, 2, 0xFFFFFFFF);
    c = pen(&w->term[4], 12, 12);
    goome_draw_rect(&c, 3, 1, 2, 10, 0xFFFFFFFF), goome_draw_rect(&c, 5, 3, 2, 6, 0xFFFFFFFF), goome_draw_rect(&c, 7, 5, 2, 2, 0xFFFFFFFF);
    c = pen(&w->term[5], 12, 12);
    goome_draw_rect(&c, 0, 0, 12, 12, 0xFFFFFFFF), goome_draw_rect(&c, 3, 3, 6, 6, 0xFF000000);
    c = pen(&w->term[6], 12, 12);
    goome_draw_rect(&c, 0, 0, 12, 12, 0xFF1FA64C), goome_draw_rect(&c, 1, 1, 10, 10, 0xFF031408);
}

static void diamond(goome_canvas *c, double cx, double cy, double r, uint32_t col)
{
    double xy[8] = {cx, cy - r, cx + r, cy, cx, cy + r, cx - r, cy};
    goome_draw_poly(c, xy, 4, col);
}

static void hud_art(world *w)
{
    goome_canvas c = pen(&w->hud[0], 16, 16);
    diamond(&c, 8, 8, 6, 0xFFFFFFFF);
    c = pen(&w->hud[1], 16, 16);
    diamond(&c, 8, 8, 5, 0xFFFFFFFF);
    c = pen(&w->hud[2], 20, 20);
    diamond(&c, 10, 10, 9, 0xFFFFFFFF), diamond(&c, 10, 10, 5.5, 0xFF0A1A24);
    c = pen(&w->hud[3], 12, 12);
    goome_draw_triangle(&c, 1, 3, 11, 3, 6, 10, 0xFFFFFFFF);
    c = pen(&w->hud[4], 12, 12);
    goome_draw_triangle(&c, 3, 1, 10, 6, 3, 11, 0xFFFFFFFF);
    c = pen(&w->hud[5], 12, 12);
    goome_draw_line(&c, 2, 2, 10, 10, 1.6, 0xFFFFFFFF), goome_draw_line(&c, 10, 2, 2, 10, 1.6, 0xFFFFFFFF);
    c = pen(&w->hud[6], 18, 18);
    goome_draw_rect(&c, 0, 0, 18, 18, 0xC0081C28), goome_draw_frame(&c, 0.5, 0.5, 17, 17, 1.2, 0xFF39E1FF);
    c = pen(&w->hud[7], 18, 18);
    diamond(&c, 9, 9, 8.6, 0xFF39E1FF), diamond(&c, 9, 9, 7.2, 0xF0081C28);

    c = pen(&w->frame, 24, 24);
    goome_draw_rect(&c, 0, 0, 24, 24, 0xA00A1622);
    goome_draw_frame(&c, 0.5, 0.5, 23, 23, 1, 0x5539E1FF);
    goome_draw_rect(&c, 0, 0, 8, 2, 0xFF39E1FF), goome_draw_rect(&c, 0, 0, 2, 8, 0xFF39E1FF);
    goome_draw_rect(&c, 16, 22, 8, 2, 0xFF39E1FF), goome_draw_rect(&c, 22, 16, 2, 8, 0xFF39E1FF);
    c = pen(&w->plate, 24, 24);
    double xy[12] = {6, 0, 24, 0, 24, 18, 18, 24, 0, 24, 0, 6};
    goome_draw_poly(&c, xy, 6, 0xC0103346);
    double in[12] = {6.8, 1, 23, 1, 23, 17.6, 17.6, 23, 1, 23, 1, 6.8};
    goome_draw_poly(&c, in, 6, 0xFF0D2533);
    goome_draw_line(&c, 6, 0.5, 23.5, 0.5, 1, 0xFF39E1FF), goome_draw_line(&c, 0.5, 6, 0.5, 23.5, 1, 0x9939E1FF);
    goome_draw_line(&c, 0.5, 6, 6, 0.5, 1, 0xFF39E1FF);
    c = pen(&w->tab, 24, 24);
    double tb[12] = {5, 0, 24, 0, 24, 19, 19, 24, 0, 24, 0, 5};
    goome_draw_poly(&c, tb, 6, 0xFF39E1FF);
}

static void retro(world *w, goome_theme *t)
{
    goome_ui_theme_dark(t);
    t->bg = 0xFF000000, t->panel = 0xFF020A04, t->border = 0xFF1F7A3A, t->text = 0xFF3DFF7A, t->text_dim = 0xFF1FA64C;
    t->accent = 0xFF3DFF7A, t->accent_text = 0xFF001A08, t->hover = 0xFF0A2A14, t->active = 0xFF124422, t->input_bg = 0xFF031408;
    t->shadow = 0x5533FF77, t->shadow_blur = 18, t->shadow_y = 0;
    t->radius = 0, t->check_radius = 0, t->tip_radius = 0, t->border_width = 1, t->anim_ms = 0, t->ease = GOOME_EASE_LINEAR;
    t->font = w->mono, t->font_mono = w->mono, t->font_heading = w->mono, t->font_size = 15, t->heading_size = 17;
    t->caret_w = 9, t->caret = 0xB03DFF7A, t->select = 0x663DFF7A, t->knob = 0xFF3DFF7A, t->focus_w = 1, t->focus_gap = 2;
    t->toggle_w = 34, t->toggle_h = 18, t->slider_track = 2, t->knob_size = 6;
    t->icon[GOOME_ICON_CHECK] = &w->term[0].bm, t->icon[GOOME_ICON_RADIO_DOT] = &w->term[1].bm;
    t->icon[GOOME_ICON_SLIDER_KNOB] = &w->term[2].bm, t->icon[GOOME_ICON_TOGGLE_KNOB] = &w->term[5].bm;
    t->icon[GOOME_ICON_ARROW_DOWN] = &w->term[3].bm, t->icon[GOOME_ICON_ARROW_RIGHT] = &w->term[4].bm;
    goome_skin box = {&w->term[6].bm, 3, 3, 3, 3};
    t->skin_checkbox = t->skin_radio = t->skin_toggle = box;
}

static void soft(goome_theme *t)
{
    goome_ui_theme_make(t, 0xFF2F7BF6, 0, 20);
    t->bg = 0xFFF2F4F8, t->panel = 0xFFFFFFFF, t->border = 0x14000000, t->input_bg = 0xFFEEF1F6, t->hover = 0xFFE3E8F0;
    t->font_size = 17, t->heading_size = 22, t->pad = 14, t->gap = 10, t->item_h = 44, t->tab_h = 42, t->win_title_h = 48;
    t->check_size = 22, t->check_radius = 11, t->toggle_w = 46, t->toggle_h = 28, t->slider_h = 28, t->knob_size = 12, t->slider_track = 8;
    t->shadow = 0x22203060, t->shadow_blur = 34, t->shadow_y = 10, t->focus_w = 3, t->focus_gap = 4;
    t->anim_ms = 260, t->hover_ms = 200, t->press_ms = 160, t->popup_ms = 320, t->ease = GOOME_EASE_BACK;
}

static void hud(world *w, goome_theme *t)
{
    goome_ui_theme_dark(t);
    t->bg = 0x00000000, t->panel = 0x8A06121C, t->border = 0x6639E1FF, t->text = 0xFFE6FBFF, t->text_dim = 0xFF7FC4D6;
    t->accent = 0xFF39E1FF, t->accent_text = 0xFF041820, t->hover = 0xFF12384A, t->active = 0xFF1A4C63, t->input_bg = 0xB0081C28;
    t->shadow = 0x5539E1FF, t->shadow_blur = 26, t->shadow_y = 0, t->radius = 0, t->check_radius = 0, t->tip_radius = 0;
    t->font = w->bold ? w->bold : t->font, t->font_size = 16, t->heading_size = 22, t->item_h = 38, t->win_title_h = 42;
    t->knob = 0xFF39E1FF, t->knob_size = 9, t->slider_track = 4, t->check_size = 18, t->ease = GOOME_EASE_OUT, t->anim_ms = 160;
    t->skin_panel = (goome_skin){&w->frame.bm, 8, 8, 8, 8};
    t->skin_button = (goome_skin){&w->plate.bm, 8, 8, 8, 8};
    t->skin_input = (goome_skin){&w->plate.bm, 8, 8, 8, 8};
    t->skin_tab = (goome_skin){&w->tab.bm, 7, 7, 7, 7};
    t->skin_checkbox = (goome_skin){&w->hud[6].bm, 3, 3, 3, 3};
    t->skin_radio = (goome_skin){&w->hud[7].bm, 0, 0, 0, 0};
    t->icon[GOOME_ICON_CHECK] = &w->hud[0].bm, t->icon[GOOME_ICON_RADIO_DOT] = &w->hud[1].bm;
    t->icon[GOOME_ICON_SLIDER_KNOB] = &w->hud[2].bm, t->icon[GOOME_ICON_TOGGLE_KNOB] = &w->hud[2].bm;
    t->icon[GOOME_ICON_ARROW_DOWN] = &w->hud[3].bm, t->icon[GOOME_ICON_ARROW_RIGHT] = &w->hud[4].bm, t->icon[GOOME_ICON_CLOSE] = &w->hud[5].bm;
}

static void dense(goome_theme *t)
{
    goome_ui_theme_dark(t);
    t->bg = 0xFF1B1C1F, t->panel = 0xFF232428, t->border = 0xFF34363C, t->text = 0xFFD5D7DC, t->text_dim = 0xFF8A8F98;
    t->accent = 0xFF4C8DF6, t->accent_text = 0xFFFFFFFF, t->hover = 0xFF2E3036, t->active = 0xFF383B42, t->input_bg = 0xFF1A1B1E;
    t->shadow = 0x66000000, t->shadow_blur = 10, t->shadow_y = 3, t->radius = 3, t->check_radius = 2, t->tip_radius = 3;
    t->font_size = 12, t->heading_size = 13, t->pad = 6, t->gap = 6, t->item_h = 24, t->tab_h = 24, t->win_title_h = 26;
    t->check_size = 13, t->toggle_w = 26, t->toggle_h = 14, t->slider_h = 14, t->knob_size = 5, t->slider_track = 3;
    t->scroll_w = 3, t->focus_w = 1, t->focus_gap = 1, t->anim_ms = 70, t->tip_pad_x = 6, t->tip_pad_y = 4;
}

static void widgets(goome_ui *u, card *c, int big, int focus)
{
    static const char *const tabs[] = {"Play", "Stats", "Gear"};
    static const char *const modes[] = {"Story", "Arena", "Sandbox"};
    goome_ui_tabs(u, "tabs", tabs, 3, &c->tab);
    goome_ui_heading(u, "Player one");
    if (focus) goome_ui_set_focus(u);
    goome_ui_input(u, "Name", c->name, sizeof(c->name));
    goome_ui_slider(u, "Volume", &c->volume, 0, 100, "%.0f");
    goome_ui_row(u, 2);
    goome_ui_checkbox(u, "Sound", &c->sound);
    goome_ui_toggle(u, "Online", &c->online);
    goome_ui_radio(u, "Easy", &c->level, 0);
    goome_ui_same_line(u);
    goome_ui_radio(u, "Normal", &c->level, 1);
    goome_ui_same_line(u);
    goome_ui_radio(u, "Hard", &c->level, 2);
    if (!big) {
        goome_ui_combo(u, "Mode", &c->mode, modes, 3);
        goome_ui_progress(u, c->volume / 100, "loading");
    }
    goome_ui_row(u, 2);
    goome_ui_button(u, "Start");
    goome_ui_button(u, "Quit");
}

static void scene(goome_ui *u, double x, double y, double w, double h)
{
    goome_canvas *c = goome_ui_canvas(u);
    if (!c) return;
    goome_draw_rrect_grad(c, x, y, w, h, goome_radius(14), 0xFF1B2747, 0xFFE0794F, 0);
    goome_draw_circle(c, x + w * 0.72, y + h * 0.62, 46, 0xFFFFC27A);
    double hills[] = {x, y + h, x, y + h * 0.7, x + w * 0.22, y + h * 0.56, x + w * 0.45, y + h * 0.74, x + w * 0.7, y + h * 0.6, x + w, y + h * 0.72, x + w, y + h};
    goome_draw_poly(c, hills, 7, 0xFF1A1F38);
    double near[] = {x, y + h, x, y + h * 0.86, x + w * 0.3, y + h * 0.8, x + w * 0.62, y + h * 0.9, x + w, y + h * 0.82, x + w, y + h};
    goome_draw_poly(c, near, 6, 0xFF0D1022);
}

static void look(goome_ui *u, world *w, int k, const char *title, double x, double y, double ww, double hh)
{
    double px = x, py = y, pw = ww, ph = hh;
    goome_ui_push_theme(u, &w->look[k]);
    goome_ui_scale(u, w->zoom);
    goome_ui_push_id(u, title);
    if (goome_ui_window(u, title, &px, &py, &pw, &ph, 0, NULL)) {
        widgets(u, &w->c[k], k == SOFT || k == HUD, k == RETRO && w->focus_once);
        if (k == RETRO) w->focus_once = 0;
    }
    goome_ui_window_end(u);
    goome_ui_pop_id(u);
    goome_ui_pop(u, 1);
}

static void simple(goome_ui *u, world *w, double x, double y, double ww, double hh)
{
    static const char *const starts[] = {"theme_make", "Dark", "Light", "Midnight", "Paper", "Mint", "High contrast", "Rose"};
    static const char *const code[] = {"goome_theme t = *goome_ui_theme(u);", "goome_ui_theme_make(&t, 0xFF8B5CF6, 1, 12);", "goome_ui_push_theme(u, &t);"};
    goome_theme t = *goome_ui_theme(u);
    if (w->start == 0) goome_ui_theme_make(&t, 0xFF8B5CF6, 1, 12);
    else goome_ui_theme_preset(&t, w->start - 1);
    goome_ui_push_theme(u, &t);
    goome_ui_scale(u, w->zoom);
    double px = x, py = y, pw = ww, ph = hh;
    if (goome_ui_window(u, "The simple path", &px, &py, &pw, &ph, 0, NULL)) {
        goome_ui_wrapped(u, "three lines, one accent color, dark or light and a corner radius:");
        goome_ui_push_font(u, w->mono);
        goome_ui_push_num(u, GOOME_S_FONT_SIZE, 12);
        for (int i = 0; i < 3; i++) goome_ui_label(u, "%s", code[i]);
        goome_ui_pop(u, 2);
        goome_ui_space(u, 4);
        goome_ui_combo(u, "Start from", &w->start, starts, 8);
        goome_ui_slider(u, "UI size", &w->zoom, 0.8, 1.3, "%.2fx");
        goome_ui_separator(u);
        card *c = &w->c[NLOOK];
        goome_ui_row(u, 2);
        goome_ui_input(u, "Name", c->name, sizeof(c->name));
        if (goome_ui_input_ex(u, "Password", w->pass, sizeof(w->pass), GOOME_INPUT_PASSWORD | GOOME_INPUT_ENTER_RETURNS, "8 letters or more") == 2) goome_ui_toast(u, "logged in", 1500);
        goome_ui_row(u, 2);
        goome_ui_input_ex(u, "Room code", w->code, sizeof(w->code), GOOME_INPUT_NUMBER, "numbers only");
        goome_ui_input_ex(u, "Server", w->server, sizeof(w->server), GOOME_INPUT_READONLY, NULL);
        goome_ui_row(u, 2);
        goome_ui_checkbox(u, "Sound", &c->sound);
        goome_ui_toggle(u, "Online", &c->online);
        goome_ui_separator(u);
        goome_ui_label(u, "one push changes one widget");
        goome_ui_row(u, 3);
        goome_ui_push_num(u, GOOME_S_RADIUS, 0);
        goome_ui_button(u, "Square");
        goome_ui_pop(u, 1);
        goome_ui_push_num(u, GOOME_S_RADIUS, 99);
        goome_ui_push_num(u, GOOME_S_ALIGN, GOOME_ALIGN_LEFT);
        goome_ui_button(u, "Pill");
        goome_ui_pop(u, 2);
        goome_ui_push_color(u, GOOME_S_ACCENT, 0xFFF59E0B);
        goome_ui_push_num(u, GOOME_S_CHECK_SIZE, 24);
        goome_ui_push_num(u, GOOME_S_CHECK_RADIUS, 12);
        goome_ui_checkbox(u, "Big", &c->sound);
        goome_ui_pop(u, 3);
        goome_ui_separator(u);
        goome_ui_label(u, "every preset, same widgets");
        for (int i = 0; i < GOOME_PRESET_COUNT; i++) {
            goome_theme p = *goome_ui_theme(u);
            goome_ui_theme_preset(&p, i);
            goome_ui_push_theme(u, &p);
            goome_ui_push_id(u, starts[i + 1]);
            goome_ui_row(u, 3);
            goome_ui_badge(u, starts[i + 1], 0);
            goome_ui_button(u, "Open");
            int on = i & 1;
            goome_ui_toggle(u, "##t", &on);
            goome_ui_pop_id(u);
            goome_ui_pop(u, 1);
        }
    }
    goome_ui_window_end(u);
    goome_ui_pop(u, 1);
}

int main(void)
{
    int err;
    goome *g = goome_open(&(goome_opts){.title = "goome can look like anything", .mode = GOOME_MODE_WINDOW, .width = 1320, .height = 940,
                                        .min_width = 900, .min_height = 600, .window_flags = GOOME_WINDOW_RESIZABLE | GOOME_WINDOW_CENTER},
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
    static world w;
    w.mono = goome_font_system("monospace");
    w.bold = goome_font_system("sans-serif:bold");
    w.zoom = 1, w.focus_once = 1;
    snprintf(w.pass, sizeof(w.pass), "hunter22");
    snprintf(w.server, sizeof(w.server), "eu-west-2");
    term_icons(&w);
    hud_art(&w);
    retro(&w, &w.look[RETRO]);
    soft(&w.look[SOFT]);
    hud(&w, &w.look[HUD]);
    dense(&w.look[DENSE]);
    for (int i = 0; i < NLOOK; i++) {
        goome_theme *t = &w.look[i];
        if (!t->font) t->font = goome_ui_theme(u)->font;
    }
    static const char *const names[] = {"guest_01", "Mia", "NOVA-7", "render_pass_b"};
    for (int i = 0; i <= NLOOK; i++) {
        card *c = &w.c[i];
        c->sound = 1, c->online = i != DENSE, c->level = 1 + (i & 1), c->volume = 35 + i * 12, c->mode = i % 3, c->tab = i % 3;
        snprintf(c->name, sizeof(c->name), "%s", i < NLOOK ? names[i] : "sev");
    }
    goome_ui_config *cfg = goome_ui_cfg(u);
    cfg->tip_delay_ms = 300;
    cfg->cursor[GOOME_CUR_BUTTON] = GOOME_CURSOR_HAND;
    goome_ui_bind(u, GOOME_ACT_SELECT_ALL, "ctrl+a super+a ctrl+l");

    int running = 1, test = !strncmp(goome_backend_name(g), "test", 4);
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
        goome_ui_app(u, "anything", GOOME_APP_NO_PAD);
        scene(u, 452, 496, 420, 428);
        goome_ui_app_end(u);
        look(u, &w, RETRO, "> retro terminal_", 16, 16, 420, 0);
        look(u, &w, SOFT, "Soft and round", 452, 16, 420, 0);
        look(u, &w, HUD, "GAME HUD", 476, 520, 372, 0);
        look(u, &w, DENSE, "dense pro tool", 16, 456, 420, 0);
        simple(u, &w, 888, 16, 416, 908);
        goome_ui_end(u);
    }
    goome_ui_free(u);
    goome_font_free(w.mono);
    goome_font_free(w.bold);
    goome_close(g);
    return 0;
}
