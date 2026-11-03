#include "internal.h"

#ifndef GOOME_NO_UI
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__clang__)
#pragma STDC FP_CONTRACT OFF
#elif defined(_MSC_VER)
#pragma fp_contract(off)
#endif

#define UI_MAXWIN 64
#define UI_MAXHIT 256
#define UI_MAXKEY 64
#define UI_MAXFR 16
#define UI_MAXSCR 16
#define UI_MAXCLIP 32
#define UI_MAXID 32
#define UI_BLK 256
#define UI_NBLK 256

enum { UI_ROOT = 1, UI_POPUP, UI_TIP };
enum { UC_RECT, UC_LINE, UC_GRAD, UC_SHADOW, UC_CIRCLE, UC_RING, UC_SEG, UC_TRI, UC_TEXT, UC_BITMAP, UC_CLIP, UC_UNCLIP };
enum { UI_POP_COMBO = 1, UI_POP_COLOR };

typedef struct {
    double x, y, w, h;
} ui_box;

typedef struct {
    int type, flag;
    double a[6];
    goome_radii r;
    uint32_t c0, c1;
    const goome_bitmap *bm;
    goome_font *font;
    int text;
} ui_cmd;

typedef struct ui_list_s {
    ui_cmd *cmd;
    int n, cap;
    char *txt;
    int tn, tcap;
} ui_list;

typedef struct {
    uint32_t id;
    int seen, born, keep;
    double v[6];
    int i[4];
} ui_st;

typedef struct {
    uint32_t id;
    int z, seen, used;
    ui_box box, prev;
    double open, content;
    int collapsed;
    ui_list list;
} ui_win;

typedef struct {
    double x0, w, ny, line_y, line_h, prev_x1, max_y, next_w;
    int same, has_next_w, row_cols, row_i, any;
} ui_lay;

typedef struct {
    uint32_t id;
    ui_box view;
    double off, start, pad;
    ui_lay saved;
    int overflow;
} ui_scr;

typedef struct {
    struct ui_list_s *cl;
    uint32_t layer;
    double alpha;
    int nclip;
} ui_save;

typedef struct {
    ui_win *w;
    ui_save sv;
    int dead, flags, nscr0, nclip0, nid0, nid_out, bg, nbg, autoid, clipcmd;
    double *px, *py, *pw, *ph;
    double title_h;
    ui_box box;
    ui_lay lay;
} ui_frame;

typedef struct {
    int key, mods;
    char text[8];
} ui_key;

struct goome_ui {
    goome *g;
    goome_theme th;
    goome_font *own;
    double em;
    goome_canvas *c;
    int mon, nmon, global_ptr, fixed_time;
    goome_monitor mons[GM_MAX_MON];
    goome_rect area;
    long now, last;
    double dt;
    int frame;

    double mx, my, pressx, pressy;
    int down, got_pos;
    int p_pressed, p_released, p_wheel;
    int pressed, released, wheel;
    ui_key keys[UI_MAXKEY];
    int nkeys;

    uint32_t ids[UI_MAXID];
    int nid;
    uint32_t active, focus, hover_layer, layer;
    int focus_seen, kb;
    double grab_x, grab_y;

    uint32_t last_id;
    ui_box last_box;
    int last_hover;
    uint32_t tip_id;
    double tip_ms;
    int tip_seen;

    uint32_t popup, pop_closed;
    int pop_kind, pop_seen;
    ui_box pop_box, pop_prev;

    ui_win wins[UI_MAXWIN];
    int zc;
    ui_list root, pop, tip;
    ui_list *cl;

    ui_frame fr[UI_MAXFR];
    int nfr;
    ui_scr scr[UI_MAXSCR];
    int nscr;
    ui_box clip[UI_MAXCLIP];
    int nclip;
    double alpha;

    ui_st *blk[UI_NBLK];
    int nst;
    int *idx;
    int idx_cap;
    ui_st dummy;

    goome_rect hits[UI_MAXHIT];
    int nhits;
    goome_rect extra[UI_MAXHIT];
    int nextra;
    int anim, anim_next;
    double lh, fh, size;
    char buf[1024];
};

static double ui_floor(double v)
{
    double t = (double)(long long)v;
    return t > v ? t - 1 : t;
}
static double ui_round(double v) { return ui_floor(v + 0.5); }
static double ui_min(double a, double b) { return a < b ? a : b; }
static double ui_max(double a, double b) { return a > b ? a : b; }
static double ui_clamp(double v, double a, double b) { return v < a ? a : v > b ? b : v; }
static double ui_abs(double v) { return v < 0 ? -v : v; }
static double ui_ease(double t)
{
    t = ui_clamp(t, 0, 1);
    return t * t * (3 - 2 * t);
}
static double ui_sin(double x)
{
    const double pi = 3.14159265358979323846;
    while (x > pi) x -= 2 * pi;
    while (x < -pi) x += 2 * pi;
    double x2 = x * x;
    return x * (1 - x2 / 6 * (1 - x2 / 20 * (1 - x2 / 42 * (1 - x2 / 72 * (1 - x2 / 110)))));
}
static double ui_cos(double x) { return ui_sin(x + 1.57079632679489661923); }
static int ui_in(ui_box b, double x, double y) { return x >= b.x && y >= b.y && x < b.x + b.w && y < b.y + b.h; }
static ui_box ui_bx(double x, double y, double w, double h)
{
    ui_box b;
    b.x = x, b.y = y, b.w = w, b.h = h;
    return b;
}
static ui_box ui_cut(ui_box a, ui_box b)
{
    double x0 = ui_max(a.x, b.x), y0 = ui_max(a.y, b.y), x1 = ui_min(a.x + a.w, b.x + b.w), y1 = ui_min(a.y + a.h, b.y + b.h);
    return ui_bx(x0, y0, ui_max(0, x1 - x0), ui_max(0, y1 - y0));
}

static uint32_t ui_mix(uint32_t a, uint32_t b, double t)
{
    int k = (int)(ui_clamp(t, 0, 1) * 256 + 0.5);
    uint32_t out = 0;
    for (int s = 0; s < 32; s += 8) {
        int x = (int)((a >> s) & 255), y = (int)((b >> s) & 255);
        out |= (uint32_t)(x + (((y - x) * k) >> 8)) << s;
    }
    return out;
}
static uint32_t ui_fade(uint32_t c, double k)
{
    uint32_t a = (uint32_t)(((c >> 24) & 255) * ui_clamp(k, 0, 1) + 0.5);
    return a << 24 | (c & 0xFFFFFF);
}

static uint32_t ui_hash(uint32_t seed, const char *s, size_t n)
{
    uint32_t h = seed ^ 2166136261u;
    for (size_t i = 0; i < n; i++) h = (h ^ (unsigned char)s[i]) * 16777619u;
    return h < 16 ? h + 16 : h;
}
static uint32_t ui_hashi(uint32_t seed, uint32_t v)
{
    uint32_t h = (seed ^ v) * 0x9E3779B1u;
    h ^= h >> 15;
    h *= 0x85EBCA77u;
    h ^= h >> 13;
    return h < 16 ? h + 16 : h;
}
static uint32_t ui_seed(goome_ui *u) { return u->nid ? u->ids[u->nid - 1] : 0x51u; }
static uint32_t ui_id(goome_ui *u, const char *s) { return ui_hash(ui_seed(u), s ? s : "", s ? strlen(s) : 0); }
static int ui_shown(const char *s)
{
    if (!s) return 0;
    const char *e = strstr(s, "##");
    return e ? (int)(e - s) : (int)strlen(s);
}

// entries live in fixed blocks, pointers survive the index rehashing
static void ui_reindex(goome_ui *u, int cap)
{
    int *n = calloc((size_t)cap, sizeof(int));
    if (!n) return;
    free(u->idx);
    u->idx = n, u->idx_cap = cap;
    for (int k = 0; k < u->nst; k++) {
        ui_st *s = &u->blk[k / UI_BLK][k % UI_BLK];
        uint32_t p = (s->id * 2654435761u) & (uint32_t)(cap - 1);
        while (n[p]) p = (p + 1) & (uint32_t)(cap - 1);
        n[p] = k + 1;
    }
}

static ui_st *ui_get(goome_ui *u, uint32_t id, int *fresh)
{
    if (fresh) *fresh = 0;
    if (u->idx) {
        uint32_t p = (id * 2654435761u) & (uint32_t)(u->idx_cap - 1);
        while (u->idx[p]) {
            int k = u->idx[p] - 1;
            ui_st *s = &u->blk[k / UI_BLK][k % UI_BLK];
            if (s->id == id) {
                s->seen = u->frame;
                return s;
            }
            p = (p + 1) & (uint32_t)(u->idx_cap - 1);
        }
    }
    if (fresh) *fresh = 1;
    memset(&u->dummy, 0, sizeof(u->dummy));
    if (u->nst >= UI_BLK * UI_NBLK) return &u->dummy;
    if ((u->nst + 1) * 2 > u->idx_cap) {
        ui_reindex(u, u->idx_cap ? u->idx_cap * 2 : 1024);
        if ((u->nst + 1) * 2 > u->idx_cap) return &u->dummy;
    }
    int b = u->nst / UI_BLK;
    if (!u->blk[b] && !(u->blk[b] = calloc(UI_BLK, sizeof(ui_st)))) return &u->dummy;
    ui_st *s = &u->blk[b][u->nst % UI_BLK];
    memset(s, 0, sizeof(*s));
    s->id = id, s->seen = s->born = u->frame;
    uint32_t p = (id * 2654435761u) & (uint32_t)(u->idx_cap - 1);
    while (u->idx[p]) p = (p + 1) & (uint32_t)(u->idx_cap - 1);
    u->idx[p] = ++u->nst;
    return s;
}

// drops old state. moves entries so only call it between frames
static void ui_gc(goome_ui *u)
{
    int n = 0;
    for (int k = 0; k < u->nst; k++) {
        ui_st *s = &u->blk[k / UI_BLK][k % UI_BLK];
        if (u->frame - s->seen > (s->keep ? 36000 : 1200)) continue;
        if (n != k) u->blk[n / UI_BLK][n % UI_BLK] = *s;
        n++;
    }
    if (n == u->nst) return;
    u->nst = n;
    ui_reindex(u, u->idx_cap);
}

static void ui_step(goome_ui *u, double *v, double target)
{
    if (*v == target) return;
    double k = u->th.anim_ms > 0 ? u->dt / u->th.anim_ms : 1;
    if (*v < target) *v = ui_min(target, *v + k);
    else *v = ui_max(target, *v - k);
    if (*v != target) u->anim_next = 1;
}
static double ui_anim(goome_ui *u, ui_st *s, int slot, int on)
{
    if (s->born == u->frame) s->v[slot] = on ? 1 : 0;
    ui_step(u, &s->v[slot], on ? 1 : 0);
    return ui_ease(s->v[slot]);
}
static void ui_follow(goome_ui *u, double *v, double target, int snap)
{
    if (snap || u->th.anim_ms <= 0) {
        *v = target;
        return;
    }
    double k = u->dt / (u->dt + u->th.anim_ms * 0.3);
    *v += (target - *v) * k;
    if (ui_abs(target - *v) < 0.2) *v = target;
    else u->anim_next = 1;
}

// draw commands, recorded per window and played back in z order at the end
static ui_cmd *ui_cmd_new(goome_ui *u, int type)
{
    ui_list *l = u->cl;
    if (l->n == l->cap) {
        int cap = l->cap ? l->cap * 2 : 256;
        ui_cmd *n = realloc(l->cmd, sizeof(ui_cmd) * (size_t)cap);
        if (!n) return NULL;
        l->cmd = n, l->cap = cap;
    }
    ui_cmd *c = &l->cmd[l->n++];
    memset(c, 0, sizeof(*c));
    c->type = type;
    return c;
}

static int ui_vis(goome_ui *u, double x, double y, double w, double h)
{
    if (!u->nclip) return 1;
    ui_box k = u->clip[u->nclip - 1];
    return x < k.x + k.w && y < k.y + k.h && x + w > k.x && y + h > k.y && k.w > 0 && k.h > 0;
}

static uint32_t ui_col(goome_ui *u, uint32_t c) { return u->alpha < 1 ? ui_fade(c, u->alpha) : c; }

static void ui_put(goome_ui *u, int type, double x, double y, double w, double h, goome_radii r, uint32_t c0, uint32_t c1, double e0, double e1)
{
    if (!ui_vis(u, x - e0 - 2, y - e0 - 2, w + 2 * e0 + 4, h + 2 * e0 + 4)) return;
    ui_cmd *c = ui_cmd_new(u, type);
    if (!c) return;
    c->a[0] = x, c->a[1] = y, c->a[2] = w, c->a[3] = h, c->a[4] = e0, c->a[5] = e1;
    c->r = r, c->c0 = ui_col(u, c0), c->c1 = ui_col(u, c1);
}
static void ui_rect(goome_ui *u, ui_box b, double r, uint32_t col) { ui_put(u, UC_RECT, b.x, b.y, b.w, b.h, goome_radius(r), col, 0, 0, 0); }
static void ui_rrect(goome_ui *u, ui_box b, goome_radii r, uint32_t col) { ui_put(u, UC_RECT, b.x, b.y, b.w, b.h, r, col, 0, 0, 0); }
static void ui_outline(goome_ui *u, ui_box b, double r, double w, uint32_t col) { ui_put(u, UC_LINE, b.x, b.y, b.w, b.h, goome_radius(r), col, 0, w, 0); }
static void ui_grad(goome_ui *u, ui_box b, goome_radii r, uint32_t from, uint32_t to, int side)
{
    ui_put(u, UC_GRAD, b.x, b.y, b.w, b.h, r, from, to, 0, 0);
    if (u->cl->n && u->cl->cmd[u->cl->n - 1].type == UC_GRAD) u->cl->cmd[u->cl->n - 1].flag = side;
}
static void ui_shadow(goome_ui *u, ui_box b, double r, double blur, uint32_t col) { ui_put(u, UC_SHADOW, b.x, b.y, b.w, b.h, goome_radius(r), col, 0, blur, 0); }
static void ui_circle(goome_ui *u, double cx, double cy, double r, uint32_t col) { ui_put(u, UC_CIRCLE, cx - r, cy - r, 2 * r, 2 * r, goome_radius(r), col, 0, 0, 0); }
static void ui_ring(goome_ui *u, double cx, double cy, double r, double w, uint32_t col) { ui_put(u, UC_RING, cx - r, cy - r, 2 * r, 2 * r, goome_radius(r), col, 0, w, 0); }
static void ui_seg(goome_ui *u, double x0, double y0, double x1, double y1, double w, uint32_t col)
{
    double x = ui_min(x0, x1), y = ui_min(y0, y1);
    ui_put(u, UC_SEG, x, y, ui_abs(x1 - x0), ui_abs(y1 - y0), goome_radius(0), col, 0, w, 0);
    if (u->cl->n && u->cl->cmd[u->cl->n - 1].type == UC_SEG) {
        ui_cmd *c = &u->cl->cmd[u->cl->n - 1];
        c->a[0] = x0, c->a[1] = y0, c->a[2] = x1, c->a[3] = y1;
    }
}
static void ui_tri(goome_ui *u, double x0, double y0, double x1, double y1, double x2, double y2, uint32_t col)
{
    double x = ui_min(x0, ui_min(x1, x2)), y = ui_min(y0, ui_min(y1, y2));
    ui_put(u, UC_TRI, x, y, ui_max(x0, ui_max(x1, x2)) - x, ui_max(y0, ui_max(y1, y2)) - y, goome_radius(0), col, 0, 0, 0);
    if (u->cl->n && u->cl->cmd[u->cl->n - 1].type == UC_TRI) {
        ui_cmd *c = &u->cl->cmd[u->cl->n - 1];
        c->a[0] = x0, c->a[1] = y0, c->a[2] = x1, c->a[3] = y1, c->a[4] = x2, c->a[5] = y2;
    }
}
static void ui_bitmap(goome_ui *u, const goome_bitmap *b, ui_box d, double r, uint32_t tint)
{
    if (!b || !b->px || b->w <= 0 || b->h <= 0) return;
    ui_put(u, UC_BITMAP, d.x, d.y, d.w, d.h, goome_radius(r), tint, 0, 0, 0);
    if (u->cl->n && u->cl->cmd[u->cl->n - 1].type == UC_BITMAP) u->cl->cmd[u->cl->n - 1].bm = b;
}

static double ui_tw(goome_ui *u, const char *s, int n, double size)
{
    if (n <= 0) return 0;
    if (n > (int)sizeof(u->buf) - 1) n = (int)sizeof(u->buf) - 1;
    char tmp[1024];
    memcpy(tmp, s, (size_t)n);
    tmp[n] = 0;
    return goome_text_measure(u->th.font, size > 0 ? size : u->size, tmp, NULL);
}

static void ui_text(goome_ui *u, double x, double y, const char *s, int n, double size, uint32_t col)
{
    if (n <= 0 || !s) return;
    double sz = size > 0 ? size : u->size;
    double lh = goome_text_line_height(u->th.font, sz);
    if (!ui_vis(u, x, y, 100000, lh)) return;
    ui_list *l = u->cl;
    if (l->tn + n + 1 > l->tcap) {
        int cap = l->tcap ? l->tcap : 4096;
        while (cap < l->tn + n + 1) cap *= 2;
        char *t = realloc(l->txt, (size_t)cap);
        if (!t) return;
        l->txt = t, l->tcap = cap;
    }
    ui_cmd *c = ui_cmd_new(u, UC_TEXT);
    if (!c) return;
    memcpy(l->txt + l->tn, s, (size_t)n);
    l->txt[l->tn + n] = 0;
    c->text = l->tn;
    l->tn += n + 1;
    c->a[0] = x, c->a[1] = y, c->a[2] = sz;
    c->c0 = ui_col(u, col);
    c->font = u->th.font;
}

// text centered on cy
static double ui_text_mid(goome_ui *u, double x, double cy, const char *s, int n, double size, uint32_t col)
{
    double sz = size > 0 ? size : u->size;
    double h = 0;
    char tmp[1024];
    if (n > 1023) n = 1023;
    if (n <= 0) return 0;
    memcpy(tmp, s, (size_t)n);
    tmp[n] = 0;
    double w = goome_text_measure(u->th.font, sz, tmp, &h);
    if (h <= 0) h = goome_text_line_height(u->th.font, sz);
    ui_text(u, x, ui_round(cy - h / 2), s, n, sz, col);
    return w;
}

static void ui_clip_push(goome_ui *u, ui_box b)
{
    if (u->nclip) b = ui_cut(b, u->clip[u->nclip - 1]);
    if (u->nclip < UI_MAXCLIP) u->clip[u->nclip++] = b;
    ui_cmd *c = ui_cmd_new(u, UC_CLIP);
    if (c) c->a[0] = b.x, c->a[1] = b.y, c->a[2] = b.w, c->a[3] = b.h;
}
static void ui_clip_pop(goome_ui *u)
{
    if (u->nclip) u->nclip--;
    ui_cmd_new(u, UC_UNCLIP);
}

static void ui_play(goome_ui *u, ui_list *l)
{
    goome_canvas *c = u->c;
    int depth = 0;
    for (int i = 0; i < l->n; i++) {
        ui_cmd *k = &l->cmd[i];
        double *a = k->a;
        switch (k->type) {
        case UC_RECT: goome_draw_rrect(c, a[0], a[1], a[2], a[3], k->r, k->c0); break;
        case UC_LINE: goome_draw_rrect_line(c, a[0], a[1], a[2], a[3], k->r, a[4], k->c0); break;
        case UC_GRAD: goome_draw_rrect_grad(c, a[0], a[1], a[2], a[3], k->r, k->c0, k->c1, k->flag); break;
        case UC_SHADOW: goome_draw_shadow(c, a[0], a[1], a[2], a[3], k->r.tl, a[4], k->c0); break;
        case UC_CIRCLE: goome_draw_circle(c, a[0] + a[2] / 2, a[1] + a[3] / 2, a[2] / 2, k->c0); break;
        case UC_RING: goome_draw_ring(c, a[0] + a[2] / 2, a[1] + a[3] / 2, a[2] / 2, a[4], k->c0); break;
        case UC_SEG: goome_draw_line(c, a[0], a[1], a[2], a[3], a[4], k->c0); break;
        case UC_TRI: goome_draw_triangle(c, a[0], a[1], a[2], a[3], a[4], a[5], k->c0); break;
        case UC_TEXT: goome_text(c, k->font, a[2], a[0], a[1], l->txt + k->text, k->c0); break;
        case UC_BITMAP: goome_draw_bitmap(c, k->bm, a[0], a[1], a[2], a[3], k->r, k->c0); break;
        case UC_CLIP: goome_clip_push(c, a[0], a[1], a[2], a[3]), depth++; break;
        case UC_UNCLIP:
            if (depth) goome_clip_pop(c), depth--;
            break;
        }
    }
    while (depth--) goome_clip_pop(c);
}

static void ui_list_free(ui_list *l)
{
    free(l->cmd), free(l->txt);
    memset(l, 0, sizeof(*l));
}

static ui_frame *ui_top(goome_ui *u)
{
    while (u->nfr > 1 && u->fr[u->nfr - 1].dead) u->nfr--;
    return &u->fr[u->nfr - 1];
}
static ui_lay *ui_lay_cur(goome_ui *u) { return &ui_top(u)->lay; }

static void ui_lay_init(ui_lay *l, double x0, double y0, double w)
{
    memset(l, 0, sizeof(*l));
    l->x0 = x0, l->w = w, l->ny = y0, l->line_y = y0, l->max_y = y0;
}

static ui_box ui_item(goome_ui *u, double want, double h)
{
    ui_lay *l = ui_lay_cur(u);
    double gap = u->th.gap, x, y, w;
    int same = l->same && l->any;
    if (l->row_cols > 1) {
        double cw = (l->w - gap * (l->row_cols - 1)) / l->row_cols;
        same = l->row_i > 0;
        x = l->x0 + l->row_i * (cw + gap);
        w = cw;
        if (++l->row_i >= l->row_cols) l->row_cols = 0, l->row_i = 0;
    } else {
        x = same ? l->prev_x1 + gap : l->x0;
        double room = l->x0 + l->w - x;
        w = l->has_next_w ? (l->next_w > 0 ? l->next_w : room) : (want > 0 ? want : room);
        if (w > room && !same) w = room;
    }
    if (same) {
        y = l->line_y;
        if (h < l->line_h) y += ui_round((l->line_h - h) / 2);
        if (h > l->line_h) l->line_h = h;
    } else {
        y = l->ny;
        l->line_y = y, l->line_h = h;
    }
    l->ny = l->line_y + l->line_h + gap;
    l->prev_x1 = x + w;
    if (l->line_y + l->line_h > l->max_y) l->max_y = l->line_y + l->line_h;
    l->same = 0, l->has_next_w = 0, l->any = 1;
    ui_box b = ui_bx(ui_round(x), ui_round(y), ui_round(w), ui_round(h));
    u->last_box = b, u->last_hover = 0, u->last_id = 0;
    return b;
}

static uint32_t ui_auto(goome_ui *u) { return ui_hashi(ui_seed(u) ^ 0x6A09E667u, (uint32_t)++ui_top(u)->autoid); }

static int ui_hover(goome_ui *u, uint32_t id, ui_box b)
{
    int h = u->hover_layer == u->layer && ui_in(b, u->mx, u->my) && (!u->nclip || ui_in(u->clip[u->nclip - 1], u->mx, u->my)) &&
            (!u->active || u->active == id);
    if (h) {
        if (u->tip_id != id) u->tip_id = id, u->tip_ms = 0;
        u->tip_seen = 1;
    }
    u->last_id = id, u->last_hover = h;
    return h;
}

// a click is down and up on it
static int ui_button_logic(goome_ui *u, uint32_t id, ui_box b, int *hov, int *held)
{
    int h = ui_hover(u, id, b);
    if (h && u->pressed && !u->active) u->active = id;
    int click = h && u->released && u->active == id;
    if (hov) *hov = h;
    if (held) *held = u->active == id && (u->down || u->pressed);
    return click;
}

static uint32_t ui_btn_bg(goome_ui *u, ui_st *s, int hov, int held)
{
    double a = ui_anim(u, s, 0, hov || held), p = ui_anim(u, s, 1, held);
    return ui_mix(ui_mix(u->th.input_bg, u->th.hover, a), u->th.active, p);
}

static void ui_keyboard(goome_ui *u, int on)
{
    if (u->kb == on) return;
    u->kb = on;
    goome_keyboard(u->g, on);
}
static void ui_unfocus(goome_ui *u)
{
    u->focus = 0;
    ui_keyboard(u, 0);
}

goome_ui *goome_ui_new(goome *g)
{
    if (!g) return NULL;
    goome_ui *u = calloc(1, sizeof(*u));
    if (!u) return NULL;
    u->g = g;
    goome_ui_theme_dark(&u->th);
    u->own = u->th.font = goome_font_system(NULL);
    const char *b = goome_backend_name(g);
    u->fixed_time = b && !strncmp(b, "test", 4) && (!b[4] || b[4] == ' '); // fixed clock for test shots
    u->global_ptr = (goome_caps(g) & GOOME_CAP_GLOBAL_POINTER) != 0;
    u->nmon = goome_monitors(g, u->mons, GM_MAX_MON);
    u->mx = u->my = -100000;
    u->alpha = 1;
    u->anim = 1;
    return u;
}

void goome_ui_free(goome_ui *u)
{
    if (!u) return;
    if (u->kb) goome_keyboard(u->g, 0);
    for (int i = 0; i < UI_MAXWIN; i++) ui_list_free(&u->wins[i].list);
    ui_list_free(&u->root), ui_list_free(&u->pop), ui_list_free(&u->tip);
    for (int i = 0; i < UI_NBLK; i++) free(u->blk[i]);
    free(u->idx);
    goome_font_free(u->own);
    free(u);
}

goome_theme *goome_ui_theme(goome_ui *u) { return u ? &u->th : NULL; }

void goome_ui_theme_dark(goome_theme *t)
{
    if (!t) return;
    t->bg = 0xFF14161A, t->panel = 0xEC1B1D22, t->border = 0x1CFFFFFF;
    t->text = 0xFFE9EBEE, t->text_dim = 0xFF969CA6;
    t->accent = 0xFF6FDC5F, t->accent_text = 0xFF0F1D0C;
    t->hover = 0xFF363A44, t->active = 0xFF424752, t->input_bg = 0xFF272A31, t->shadow = 0x8C000000;
    t->radius = 10, t->pad = 10, t->gap = 8, t->border_width = 1, t->font_size = 15, t->shadow_blur = 22, t->anim_ms = 120;
}

void goome_ui_theme_light(goome_theme *t)
{
    if (!t) return;
    t->bg = 0xFFF4F5F7, t->panel = 0xF4FDFDFE, t->border = 0x24000000;
    t->text = 0xFF1C1F24, t->text_dim = 0xFF656C77;
    t->accent = 0xFF45B934, t->accent_text = 0xFFFFFFFF;
    t->hover = 0xFFDCE0E5, t->active = 0xFFCDD2D9, t->input_bg = 0xFFE9ECF0, t->shadow = 0x40000000;
    t->radius = 10, t->pad = 10, t->gap = 8, t->border_width = 1, t->font_size = 15, t->shadow_blur = 22, t->anim_ms = 120;
}

static int ui_over(goome_ui *u, double x, double y)
{
    if (u->popup && ui_in(u->pop_prev, x, y)) return 1;
    for (int i = 0; i < UI_MAXWIN; i++)
        if (u->wins[i].id && u->frame - u->wins[i].seen <= 2 && ui_in(u->wins[i].prev, x, y)) return 1;
    return 0;
}

int goome_ui_feed(goome_ui *u, const goome_event *e)
{
    if (!u || !e) return 0;
    switch (e->type) {
    case GOOME_MOTION:
        u->mx = e->x, u->my = e->y, u->got_pos = 1;
        return u->active || ui_over(u, e->x, e->y);
    case GOOME_DOWN:
        if (e->button > 1) return ui_over(u, e->x, e->y);
        u->mx = u->pressx = e->x, u->my = u->pressy = e->y, u->got_pos = 1;
        u->down = 1, u->p_pressed = 1;
        return ui_over(u, e->x, e->y);
    case GOOME_UP:
        if (e->button > 1) return ui_over(u, e->x, e->y);
        if (!u->global_ptr) u->mx = e->x, u->my = e->y;
        u->down = 0, u->p_released = 1;
        return u->active || ui_over(u, e->x, e->y);
    case GOOME_SCROLL:
        u->mx = e->x, u->my = e->y, u->got_pos = 1;
        u->p_wheel += e->dy;
        return ui_over(u, e->x, e->y);
    case GOOME_KEY:
        if (!u->focus || u->nkeys >= UI_MAXKEY) return u->focus != 0;
        u->keys[u->nkeys].key = e->key, u->keys[u->nkeys].mods = e->mods;
        memcpy(u->keys[u->nkeys].text, e->text, sizeof(e->text));
        u->keys[u->nkeys].text[7] = 0;
        u->nkeys++;
        return 1;
    case GOOME_MONITORS: u->nmon = goome_monitors(u->g, u->mons, GM_MAX_MON); return 0;
    }
    return 0;
}

goome_canvas *goome_ui_begin(goome_ui *u, int mon)
{
    if (!u) return NULL;
    goome_canvas *c = goome_begin(u->g, mon);
    u->c = c;
    if (!c) return NULL;
    u->mon = mon;
    u->frame++;
    if (u->nmon <= 0) u->nmon = goome_monitors(u->g, u->mons, GM_MAX_MON);
    u->area = mon < u->nmon ? u->mons[mon].area : (goome_rect){(int)c->ox, (int)c->oy, (int)(c->w / c->scale), (int)(c->h / c->scale)};
    if (u->fixed_time) u->dt = 16, u->now += 16;
    else {
        long t = goome_now_ms();
        u->dt = u->last ? (double)(t - u->last) : 16;
        if (u->dt < 0) u->dt = 0;
        if (u->dt > 100) u->dt = 100;
        u->last = t, u->now = t;
    }
    if (!(u->frame & 255)) ui_gc(u);

    double px, py;
    int pb;
    if (!u->got_pos && goome_pointer(u->g, &px, &py, &pb)) {
        u->mx = px, u->my = py;
        if (u->down && !(pb & 1) && !u->p_pressed) u->down = 0, u->p_released = 1; // the up went somewhere else
    }
    u->got_pos = 0;

    // with more than one monitor the clicks go to the pass whose monitor has the mouse
    goome_rect a = u->area;
    int here = u->nmon <= 1 || (u->mx >= a.x && u->my >= a.y && u->mx < a.x + a.w && u->my < a.y + a.h);
    if (here) {
        u->pressed = u->p_pressed, u->released = u->p_released, u->wheel = u->p_wheel;
        u->p_pressed = u->p_released = u->p_wheel = 0;
    } else
        u->pressed = u->released = u->wheel = 0;

    if (u->th.font_size <= 0) u->th.font_size = 15;
    u->size = u->th.font_size;
    u->lh = goome_text_line_height(u->th.font, u->size);
    u->em = ui_max(1, u->size / 15);
    u->fh = ui_round(u->lh + ui_max(6, u->th.pad * 1.2));

    u->hover_layer = UI_ROOT;
    if (u->popup && ui_in(u->pop_prev, u->mx, u->my)) u->hover_layer = UI_POPUP;
    else {
        int best = -1;
        for (int i = 0; i < UI_MAXWIN; i++) {
            ui_win *w = &u->wins[i];
            if (w->id && u->frame - w->seen <= 2 && w->z > best && ui_in(w->prev, u->mx, u->my)) best = w->z, u->hover_layer = w->id;
        }
    }
    u->pop_closed = 0;
    if (u->pressed && u->popup && !ui_in(u->pop_prev, u->mx, u->my)) u->pop_closed = u->popup, u->popup = 0, u->pressed = 0, u->released = 0; // that click only closes it
    if (u->pressed && u->hover_layer > UI_TIP)
        for (int i = 0; i < UI_MAXWIN; i++)
            if (u->wins[i].id == u->hover_layer && u->wins[i].z != u->zc) u->wins[i].z = ++u->zc;

    if (!u->tip_seen) u->tip_id = 0, u->tip_ms = 0;
    else u->tip_ms += u->dt;
    if (u->pressed) u->tip_ms = -100000;
    u->tip_seen = 0;

    for (int i = 0; i < UI_MAXWIN; i++) u->wins[i].used = 0, u->wins[i].list.n = 0, u->wins[i].list.tn = 0;
    u->root.n = u->root.tn = u->pop.n = u->pop.tn = u->tip.n = u->tip.tn = 0;
    u->cl = &u->root;
    u->layer = UI_ROOT;
    u->nclip = 0, u->nscr = 0, u->nid = 0, u->nextra = 0, u->nhits = 0;
    u->alpha = 1;
    u->anim_next = 0;
    u->focus_seen = 0, u->pop_seen = 0;
    memset(&u->fr[0], 0, sizeof(u->fr[0]));
    ui_lay_init(&u->fr[0].lay, a.x + u->th.pad, a.y + u->th.pad, a.w - 2 * u->th.pad);
    u->nfr = 1;
    return c;
}

static void ui_scroll_close(goome_ui *u);

static void ui_hit_add(goome_ui *u, ui_box b)
{
    if (u->nhits >= UI_MAXHIT || b.w <= 0 || b.h <= 0) return;
    int x0 = (int)ui_floor(b.x), y0 = (int)ui_floor(b.y), x1 = (int)-ui_floor(-(b.x + b.w)), y1 = (int)-ui_floor(-(b.y + b.h));
    u->hits[u->nhits++] = (goome_rect){x0, y0, x1 - x0, y1 - y0};
}

void goome_ui_hit(goome_ui *u, double x, double y, double w, double h)
{
    if (!u || u->nextra >= UI_MAXHIT || w <= 0 || h <= 0) return;
    int x0 = (int)ui_floor(x), y0 = (int)ui_floor(y), x1 = (int)-ui_floor(-(x + w)), y1 = (int)-ui_floor(-(y + h));
    u->extra[u->nextra++] = (goome_rect){x0, y0, x1 - x0, y1 - y0};
}

int goome_ui_animating(goome_ui *u) { return u ? u->anim : 0; }

void goome_ui_end(goome_ui *u)
{
    if (!u || !u->c) return;
    while (u->nfr > 1) {
        if (u->fr[u->nfr - 1].dead) u->nfr--;
        else goome_ui_window_end(u);
    }
    while (u->nscr > 0) ui_scroll_close(u);
    if (u->focus && !u->focus_seen) ui_unfocus(u);
    if (u->popup && !u->pop_seen) u->popup = 0;
    if (!u->down) u->active = 0;
    if (u->focus) u->anim_next = 1; // the cursor blinks

    ui_play(u, &u->root);
    int order[UI_MAXWIN], n = 0;
    for (int i = 0; i < UI_MAXWIN; i++)
        if (u->wins[i].id && u->wins[i].used) {
            int k = n++;
            while (k > 0 && u->wins[order[k - 1]].z > u->wins[i].z) order[k] = order[k - 1], k--;
            order[k] = i;
        }
    for (int i = 0; i < n; i++) {
        ui_win *w = &u->wins[order[i]];
        ui_play(u, &w->list);
        ui_hit_add(u, w->box);
        w->prev = w->box;
        w->seen = u->frame;
    }
    ui_play(u, &u->pop);
    if (u->popup) ui_hit_add(u, u->pop_box);
    u->pop_prev = u->popup ? u->pop_box : ui_bx(0, 0, 0, 0);
    ui_play(u, &u->tip);
    for (int i = 0; i < u->nextra && u->nhits < UI_MAXHIT; i++) u->hits[u->nhits++] = u->extra[i];
    goome_end(u->g, u->c, u->hits, u->nhits);
    u->anim = u->anim_next;
    u->nkeys = 0;
    u->c = NULL;
}

void goome_ui_push_id(goome_ui *u, const char *id)
{
    if (!u) return;
    uint32_t h = ui_id(u, id);
    if (u->nid < UI_MAXID) u->ids[u->nid++] = h;
}

void goome_ui_pop_id(goome_ui *u)
{
    if (u && u->c && u->nid > ui_top(u)->nid0) u->nid--;
}

static void ui_scroll_open(goome_ui *u, uint32_t id, ui_box view, double pad)
{
    ui_lay *lay = ui_lay_cur(u);
    if (u->nscr >= UI_MAXSCR) return;
    int fresh;
    ui_st *s = ui_get(u, id, &fresh);
    double maxo = ui_max(0, s->v[2] - view.h);
    s->v[1] = ui_clamp(s->v[1], 0, maxo);
    if (u->active == ui_hashi(id, 1)) s->v[0] = s->v[1];
    else ui_follow(u, &s->v[0], s->v[1], fresh);
    s->v[0] = ui_clamp(s->v[0], 0, maxo);
    ui_scr *r = &u->scr[u->nscr++];
    r->id = id, r->view = view, r->off = ui_round(s->v[0]), r->saved = *lay, r->overflow = maxo > 0.5;
    r->start = view.y - r->off, r->pad = pad;
    ui_clip_push(u, view);
    ui_lay_init(lay, view.x + pad, view.y + pad - r->off, view.w - 2 * pad - (r->overflow ? 10 : 0));
    lay->max_y = view.y + pad - r->off;
    s->v[3] = pad;
}

static void ui_scroll_close(goome_ui *u)
{
    if (u->nscr <= 0) return;
    ui_scr *r = &u->scr[--u->nscr];
    ui_lay *lay = ui_lay_cur(u);
    ui_st *s = ui_get(u, r->id, NULL);
    double content = lay->max_y + s->v[3] - r->start, maxo = ui_max(0, content - r->view.h);
    s->v[2] = content;
    ui_clip_pop(u);
    *lay = r->saved;
    ui_box v = r->view;
    int inside = u->hover_layer == u->layer && ui_in(v, u->mx, u->my) && (!u->nclip || ui_in(u->clip[u->nclip - 1], u->mx, u->my));
    if (u->wheel && maxo > 0 && inside) {
        s->v[1] = ui_clamp(s->v[1] + u->wheel * 3 * u->lh, 0, maxo);
        u->wheel = 0;
        u->anim_next = 1;
    }
    if (maxo <= 0) return;
    ui_box track = ui_bx(v.x + v.w - 12, v.y + 6, 10, v.h - 12);
    double th = ui_clamp(track.h * v.h / content, ui_min(28, track.h), track.h);
    double ty = track.y + (track.h - th) * (r->off / maxo);
    uint32_t bid = ui_hashi(r->id, 1);
    ui_st *bs = ui_get(u, ui_hashi(r->id, 2), NULL);
    int hov = ui_hover(u, bid, track);
    if (hov && u->pressed && !u->active) {
        u->active = bid;
        u->grab_y = u->my >= ty && u->my < ty + th ? u->my - ty : th / 2;
    }
    int held = u->active == bid && (u->down || u->pressed);
    if (held && track.h > th) {
        double t = (u->my - u->grab_y - track.y) / (track.h - th);
        s->v[1] = s->v[0] = ui_clamp(t, 0, 1) * maxo;
        ty = track.y + (track.h - th) * ui_clamp(t, 0, 1);
    }
    double a = ui_anim(u, bs, 0, hov || held);
    double bw = 4 + 2 * a;
    ui_rect(u, ui_bx(track.x + track.w - bw - 1, ty, bw, th), bw / 2, ui_fade(u->th.text, 0.2 + 0.25 * a));
}

int goome_ui_scroll_begin(goome_ui *u, const char *id, double h)
{
    if (!u || !u->c) return 0;
    int page = h <= 0;
    if (page) {
        ui_lay *l = ui_lay_cur(u);
        double bottom = u->area.y + u->area.h - u->th.pad;
        if (u->nscr > ui_top(u)->nscr0 || (u->nscr && ui_top(u)->w)) {
            ui_scr *r = &u->scr[u->nscr - 1];
            bottom = r->view.y + r->view.h - r->pad - r->off;
        }
        h = ui_max(60, bottom - l->ny);
    }
    ui_box b = ui_item(u, 0, h);
    if (!page) ui_rect(u, b, ui_max(0, u->th.radius - 2), ui_fade(u->th.input_bg, 0.55));
    uint32_t sid = ui_id(u, id);
    if (u->nid < UI_MAXID) u->ids[u->nid++] = sid;
    if (page) b.x -= 6, b.w += 12;
    ui_scroll_open(u, sid, b, 6);
    return 1;
}

void goome_ui_scroll_end(goome_ui *u)
{
    if (!u || !u->c) return;
    ui_frame *f = ui_top(u);
    if (u->nscr <= f->nscr0 + (f->w && f->ph && *f->ph > 0 && !f->dead)) return;
    ui_scroll_close(u);
    if (u->nid > f->nid0) u->nid--;
}

static ui_win *ui_win_get(goome_ui *u, uint32_t id)
{
    int spare = -1, old = -1;
    for (int i = 0; i < UI_MAXWIN; i++) {
        if (u->wins[i].id == id) return &u->wins[i];
        if (!u->wins[i].id && spare < 0) spare = i;
        if (u->wins[i].id && !u->wins[i].used && (old < 0 || u->wins[i].seen < u->wins[old].seen)) old = i;
    }
    int k = spare >= 0 ? spare : old;
    if (k < 0) return NULL;
    ui_win *w = &u->wins[k];
    ui_list keep = w->list;
    memset(w, 0, sizeof(*w));
    w->list = keep;
    w->list.n = w->list.tn = 0;
    w->id = id, w->z = ++u->zc, w->open = 1, w->seen = u->frame;
    return w;
}

static void ui_chevron(goome_ui *u, double cx, double cy, double ang, double r, uint32_t col)
{
    double c = ui_cos(ang), s = ui_sin(ang);
    double ax = -r * 0.45, ay = -r, bx = r * 0.55, cx2 = -r * 0.45, cy2 = r;
    ui_tri(u, cx + ax * c - ay * s, cy + ax * s + ay * c, cx + bx * c, cy + bx * s, cx + cx2 * c - cy2 * s, cy + cx2 * s + cy2 * c, col);
}

static void ui_enter(goome_ui *u, ui_save *sv, ui_list *l, uint32_t layer, double alpha)
{
    sv->cl = u->cl, sv->layer = u->layer, sv->alpha = u->alpha, sv->nclip = u->nclip;
    u->cl = l, u->layer = layer, u->alpha = alpha;
    if (u->nclip < UI_MAXCLIP) u->clip[u->nclip++] = ui_bx(-1e7, -1e7, 2e7, 2e7);
}
static void ui_leave(goome_ui *u, ui_save *sv) { u->cl = sv->cl, u->layer = sv->layer, u->alpha = sv->alpha, u->nclip = sv->nclip; }

int goome_ui_window(goome_ui *u, const char *title, double *x, double *y, double *w, double *h, int flags, int *open)
{
    if (!u || !u->c || !x || !y || !w || !h) return 0;
    ui_top(u);
    if (u->nfr >= UI_MAXFR) return 0;
    ui_frame *f = &u->fr[u->nfr++];
    memset(f, 0, sizeof(*f));
    f->dead = 1, f->nscr0 = u->nscr, f->nclip0 = u->nclip, f->nid0 = u->nid;
    if (open && !*open) return 0;
    const char *t = title ? title : "";
    uint32_t id = ui_hash(0x77, t, strlen(t));
    ui_win *wn = ui_win_get(u, id);
    if (!wn) return 0;
    const goome_theme *th = &u->th;
    wn->used = 1, wn->seen = u->frame;
    f->w = wn, f->flags = flags, f->px = x, f->py = y, f->pw = w, f->ph = h;
    ui_enter(u, &f->sv, &wn->list, id, 1);
    f->nid_out = u->nid;
    if (u->nid < UI_MAXID) u->ids[u->nid++] = id;

    double tbar = (flags & GOOME_WIN_NO_TITLE) ? 0 : u->fh + 6;
    f->title_h = tbar;
    uint32_t mv = ui_hashi(id, 1), rs = ui_hashi(id, 2);
    goome_rect a = u->area;
    if (u->active == mv && u->down) {
        *x = ui_round(ui_clamp(u->mx - u->grab_x, a.x - *w + 60, a.x + a.w - 60));
        *y = ui_round(ui_clamp(u->my - u->grab_y, a.y, a.y + a.h - ui_max(tbar, 24)));
    }
    if (u->active == rs && u->down) {
        *w = ui_round(ui_max(140, u->mx + u->grab_x - *x));
        *h = ui_round(ui_max(tbar + 50, u->my + u->grab_y - *y));
    }
    double ww = ui_max(*w, 80);
    int coll = wn->collapsed && (flags & GOOME_WIN_COLLAPSE) && tbar > 0;
    ui_step(u, &wn->open, coll ? 0 : 1);
    double op = ui_ease(wn->open);
    double full = *h > 0 ? ui_max(*h, tbar) : tbar + wn->content;
    double vh = ui_round(tbar + (full - tbar) * op);
    ui_box box = ui_bx(*x, *y, ww, vh);
    f->box = box;
    double r = th->radius;

    f->bg = u->cl->n;
    ui_shadow(u, ui_bx(box.x, box.y + 5, box.w, box.h), r, th->shadow_blur, th->shadow);
    ui_rect(u, box, r, th->panel);
    f->nbg = u->cl->n - f->bg;

    if (tbar > 0) {
        goome_radii tr = goome_radius(r);
        if (vh > tbar + 1) tr.bl = tr.br = 0;
        ui_rrect(u, ui_bx(box.x, box.y, ww, tbar), tr, ui_fade(th->text, 0.04));
        if (vh > tbar + 0.5) ui_rect(u, ui_bx(box.x, box.y + tbar - 1, ww, 1), 0, th->border);
        double cy = box.y + tbar / 2, tx = box.x + th->pad + 4, right = box.x + ww - th->pad;
        if (flags & GOOME_WIN_COLLAPSE) {
            uint32_t cid = ui_hashi(id, 4);
            double cs = ui_round(22 * u->em);
            ui_box cb = ui_bx(box.x + th->pad - 4, ui_round(cy - cs / 2), cs, cs);
            int hov, held;
            if (ui_button_logic(u, cid, cb, &hov, &held)) wn->collapsed = !wn->collapsed;
            ui_st *s = ui_get(u, cid, NULL);
            double k = ui_anim(u, s, 0, hov || held);
            if (k > 0) ui_rect(u, cb, 6, ui_fade(th->hover, k));
            ui_chevron(u, cb.x + cs / 2, cy, op * 1.5707963267948966, 4.5 * u->em, ui_mix(th->text_dim, th->text, k));
            tx = cb.x + cb.w + 6;
        }
        if ((flags & GOOME_WIN_CLOSE) && open) {
            uint32_t xid = ui_hashi(id, 3);
            double xs = ui_round(18 * u->em);
            ui_box xb = ui_bx(right - xs, ui_round(cy - xs / 2), xs, xs);
            int hov, held;
            if (ui_button_logic(u, xid, xb, &hov, &held)) *open = 0;
            ui_st *s = ui_get(u, xid, NULL);
            double k = ui_anim(u, s, 0, hov || held);
            double ccx = xb.x + xs / 2, d = 3.6 * u->em;
            if (k > 0) ui_circle(u, ccx, cy, xs / 2, ui_fade(ui_mix(0xFFE5534B, 0xFFC93C35, held), k));
            uint32_t xc = ui_mix(th->text_dim, 0xFFFFFFFF, k);
            ui_seg(u, ccx - d, cy - d, ccx + d, cy + d, 1.6 * u->em, xc);
            ui_seg(u, ccx + d, cy - d, ccx - d, cy + d, 1.6 * u->em, xc);
            right = xb.x - 6;
        }
        int n = ui_shown(t);
        ui_clip_push(u, ui_bx(tx, box.y, ui_max(0, right - tx), tbar));
        ui_text_mid(u, tx, cy, t, n, 0, th->text);
        ui_clip_pop(u);
    }

    if (coll && wn->open <= 0) {
        ui_outline(u, box, r, th->border_width, th->border);
        wn->box = box;
        ui_leave(u, &f->sv);
        u->nid = f->nid_out;
        return 0;
    }
    f->dead = 0;
    ui_box body = ui_bx(box.x, box.y + tbar, ww, ui_max(0, vh - tbar));
    f->clipcmd = u->cl->n;
    ui_clip_push(u, body);
    if (*h <= 0 && u->nclip) u->clip[u->nclip - 1].h = 1e7; // real height is known at window_end
    if (*h > 0) ui_scroll_open(u, ui_hashi(id, 5), body, th->pad);
    else ui_lay_init(&f->lay, box.x + th->pad, body.y + th->pad, ww - 2 * th->pad);
    f->nid0 = u->nid;
    return 1;
}

void goome_ui_window_end(goome_ui *u)
{
    if (!u || !u->c || u->nfr <= 1) return;
    ui_frame *f = &u->fr[u->nfr - 1];
    if (f->dead || !f->w) {
        u->nfr--;
        return;
    }
    ui_win *wn = f->w;
    const goome_theme *th = &u->th;
    int own = *f->ph > 0;
    while (u->nscr > f->nscr0 + own) ui_scroll_close(u);
    int clipcmd = f->clipcmd;
    double tbar = f->title_h, op = ui_ease(wn->open);
    if (own) ui_scroll_close(u);
    else {
        double content = ui_max(f->lay.max_y + th->pad - (f->box.y + tbar), 2 * th->pad);
        wn->content = ui_round(content);
        f->box.h = ui_round(tbar + wn->content * op);
        for (int i = f->bg; i < f->bg + f->nbg && i < u->cl->n; i++) u->cl->cmd[i].a[3] = f->box.h;
        if (clipcmd < u->cl->n && u->cl->cmd[clipcmd].type == UC_CLIP) u->cl->cmd[clipcmd].a[3] = f->box.h - tbar;
    }
    ui_clip_pop(u);
    ui_box box = f->box;
    uint32_t mv = ui_hashi(wn->id, 1), rs = ui_hashi(wn->id, 2);
    if ((f->flags & GOOME_WIN_RESIZE) && op > 0.99) {
        ui_box g = ui_bx(box.x + box.w - 18, box.y + box.h - 18, 18, 18);
        int hov, held;
        ui_button_logic(u, rs, g, &hov, &held);
        if (u->active == rs && u->pressed) {
            u->grab_x = *f->px + box.w - u->mx, u->grab_y = *f->py + box.h - u->my;
            if (*f->ph <= 0) *f->ph = box.h;
        }
        ui_st *s = ui_get(u, rs, NULL);
        double k = ui_anim(u, s, 0, hov || held);
        uint32_t col = ui_mix(ui_fade(th->text_dim, 0.5), th->accent, k);
        double ex = box.x + box.w - 5, ey = box.y + box.h - 5;
        ui_seg(u, ex - 9, ey, ex, ey - 9, 1.5, col);
        ui_seg(u, ex - 4.5, ey, ex, ey - 4.5, 1.5, col);
    }
    if ((f->flags & GOOME_WIN_MOVE) && u->pressed && !u->active && u->hover_layer == wn->id && ui_in(box, u->mx, u->my)) {
        u->active = mv;
        u->grab_x = u->mx - *f->px, u->grab_y = u->my - *f->py;
    }
    ui_outline(u, box, th->radius, th->border_width, th->border);
    wn->box = box;
    ui_leave(u, &f->sv);
    u->nid = f->nid_out;
    u->nfr--;
}

void goome_ui_row(goome_ui *u, int cols)
{
    if (!u || !u->c) return;
    ui_lay *l = ui_lay_cur(u);
    l->row_cols = cols > 1 ? cols : 0, l->row_i = 0, l->same = 0;
}

void goome_ui_same_line(goome_ui *u)
{
    if (u && u->c) ui_lay_cur(u)->same = 1;
}

void goome_ui_space(goome_ui *u, double px)
{
    if (!u || !u->c) return;
    ui_lay *l = ui_lay_cur(u);
    l->ny += px, l->same = 0;
}

void goome_ui_width(goome_ui *u, double w)
{
    if (!u || !u->c) return;
    ui_lay *l = ui_lay_cur(u);
    l->has_next_w = 1, l->next_w = w;
}

void goome_ui_separator(goome_ui *u)
{
    if (!u || !u->c) return;
    ui_box b = ui_item(u, 0, 1);
    ui_rect(u, b, 0, u->th.border);
}

void goome_ui_label(goome_ui *u, const char *fmt, ...)
{
    if (!u || !u->c || !fmt) return;
    char s[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(s, sizeof(s), fmt, ap);
    va_end(ap);
    int n = (int)strlen(s);
    ui_box b = ui_item(u, ui_tw(u, s, n, 0), u->lh);
    ui_hover(u, ui_auto(u), b);
    ui_text_mid(u, b.x, b.y + b.h / 2, s, n, 0, u->th.text);
}

void goome_ui_heading(goome_ui *u, const char *text)
{
    if (!u || !u->c || !text) return;
    ui_lay *l = ui_lay_cur(u);
    if (l->any && !l->same && !l->row_cols) l->ny += ui_round(u->th.gap * 0.5);
    double sz = ui_round(u->size * 1.3);
    int n = ui_shown(text);
    ui_box b = ui_item(u, ui_tw(u, text, n, sz), goome_text_line_height(u->th.font, sz));
    ui_hover(u, ui_auto(u), b);
    ui_text_mid(u, b.x, b.y + b.h / 2, text, n, sz, u->th.text);
}

static int ui_u8_prev(const char *s, int i)
{
    if (i <= 0) return 0;
    i--;
    while (i > 0 && ((unsigned char)s[i] & 0xC0) == 0x80) i--;
    return i;
}
static int ui_u8_next(const char *s, int i, int len)
{
    if (i >= len) return len;
    i++;
    while (i < len && ((unsigned char)s[i] & 0xC0) == 0x80) i++;
    return i;
}

static int ui_wrap(goome_ui *u, const char *s, double w, int *st, int *ln, int max)
{
    int n = 0, len = (int)strlen(s), i = 0;
    while (i < len && n < max) {
        int ok = i;
        for (;;) {
            int j = ok;
            while (j < len && s[j] == ' ') j++;
            while (j < len && s[j] != ' ' && s[j] != '\n') j++;
            if (j == ok || ui_tw(u, s + i, j - i, 0) > w) break;
            ok = j;
        }
        if (ok == i)
            while (ok < len && s[ok] != '\n') {
                int k = ui_u8_next(s, ok, len);
                if (ok > i && ui_tw(u, s + i, k - i, 0) > w) break;
                ok = k;
            }
        st[n] = i, ln[n] = ok - i, n++;
        i = ok;
        while (i < len && s[i] == ' ') i++;
        if (i < len && s[i] == '\n') i++;
    }
    return n;
}

static double ui_avail(goome_ui *u)
{
    ui_lay *l = ui_lay_cur(u);
    if (l->has_next_w && l->next_w > 0) return l->next_w;
    if (l->row_cols > 1) return (l->w - u->th.gap * (l->row_cols - 1)) / l->row_cols;
    if (l->same && l->any) return l->x0 + l->w - l->prev_x1 - u->th.gap;
    return l->w;
}

void goome_ui_wrapped(goome_ui *u, const char *text)
{
    if (!u || !u->c || !text) return;
    int st[256], ln[256];
    double w = ui_max(20, ui_avail(u));
    int n = ui_wrap(u, text, w, st, ln, 256);
    ui_box b = ui_item(u, w, ui_max(1, n) * u->lh);
    ui_hover(u, ui_auto(u), b);
    for (int i = 0; i < n; i++) ui_text_mid(u, b.x, b.y + u->lh * (i + 0.5), text + st[i], ln[i], 0, u->th.text_dim);
}

int goome_ui_button(goome_ui *u, const char *label)
{
    if (!u || !u->c) return 0;
    const goome_theme *th = &u->th;
    int n = ui_shown(label);
    double tw = ui_tw(u, label, n, 0);
    ui_box b = ui_item(u, ui_round(tw + th->pad * 2.6), u->fh);
    uint32_t id = ui_id(u, label);
    int hov, held, click = ui_button_logic(u, id, b, &hov, &held);
    ui_st *s = ui_get(u, id, NULL);
    ui_rect(u, b, th->radius, ui_btn_bg(u, s, hov, held));
    ui_outline(u, b, th->radius, 1, th->border);
    ui_clip_push(u, b);
    ui_text_mid(u, b.x + ui_round((b.w - tw) / 2), b.y + b.h / 2, label, n, 0, th->text);
    ui_clip_pop(u);
    return click;
}

static ui_box ui_fit(const goome_bitmap *b, double w, double h, double room)
{
    double bw = b && b->w > 0 ? b->w : 64, bh = b && b->h > 0 ? b->h : 64;
    if (w <= 0 && h <= 0) w = ui_min(bw, room), h = w * bh / bw;
    else if (w <= 0) w = h * bw / bh;
    else if (h <= 0) h = w * bh / bw;
    if (w > room && room > 0) h = h * room / w, w = room;
    return ui_bx(0, 0, ui_round(w), ui_round(h));
}

static void ui_img_box(goome_ui *u, const goome_bitmap *b, ui_box d, double r)
{
    if (b && b->px) ui_bitmap(u, b, d, r, 0xFFFFFFFF);
    else ui_rect(u, d, r, ui_fade(u->th.text_dim, 0.25));
}

void goome_ui_image(goome_ui *u, const goome_bitmap *b, double w, double h, double radius)
{
    if (!u || !u->c) return;
    ui_box sz = ui_fit(b, w, h, ui_avail(u));
    ui_box d = ui_item(u, sz.w, sz.h);
    ui_hover(u, ui_auto(u), d);
    ui_img_box(u, b, d, radius);
}

int goome_ui_image_button(goome_ui *u, const char *id, const goome_bitmap *b, double w, double h)
{
    if (!u || !u->c) return 0;
    const goome_theme *th = &u->th;
    ui_box sz = ui_fit(b, w, h, ui_avail(u) - 12);
    ui_box d = ui_item(u, sz.w + 12, sz.h + 12);
    uint32_t hid = ui_id(u, id);
    int hov, held, click = ui_button_logic(u, hid, d, &hov, &held);
    ui_st *s = ui_get(u, hid, NULL);
    ui_rect(u, d, th->radius, ui_btn_bg(u, s, hov, held));
    double p = ui_ease(s->v[1]);
    ui_outline(u, d, th->radius, 1, ui_mix(th->border, th->accent, ui_ease(s->v[0]) * 0.8));
    double in = 6 + p;
    ui_img_box(u, b, ui_bx(d.x + in, d.y + in, d.w - 2 * in, d.h - 2 * in), ui_max(0, th->radius - 4));
    return click;
}

int goome_ui_checkbox(goome_ui *u, const char *label, int *v)
{
    if (!u || !u->c || !v) return 0;
    const goome_theme *th = &u->th;
    int n = ui_shown(label);
    double tw = ui_tw(u, label, n, 0), sz = ui_round(18 * u->em), q = sz / 18;
    ui_box b = ui_item(u, sz + (n ? 8 + tw : 0), u->fh - 6);
    uint32_t id = ui_id(u, label);
    int hov, held, ch = 0;
    if (ui_button_logic(u, id, b, &hov, &held)) *v = !*v, ch = 1;
    ui_st *s = ui_get(u, id, NULL);
    double h = ui_anim(u, s, 0, hov || held), k = ui_anim(u, s, 2, *v);
    double cy = b.y + b.h / 2;
    ui_box cb = ui_bx(b.x, ui_round(cy - sz / 2), sz, sz);
    ui_rect(u, cb, 5, ui_mix(ui_mix(th->input_bg, th->hover, h), th->accent, k));
    if (k < 1) ui_outline(u, cb, 5, 1.5, ui_fade(ui_mix(th->text_dim, th->text, h * 0.6), 0.75 * (1 - k)));
    if (k > 0) {
        uint32_t c = ui_fade(th->accent_text, k);
        double x = cb.x, y = cb.y, d = 1 - k;
        ui_seg(u, x + 4.5 * q, y + (9.5 + d) * q, x + 7.8 * q, y + 12.8 * q, 2 * q, c);
        ui_seg(u, x + 7.8 * q, y + 12.8 * q, x + (13.6 - d * 3) * q, y + (6 + d * 3) * q, 2 * q, c);
    }
    if (n) ui_text_mid(u, cb.x + sz + 8, cy, label, n, 0, th->text);
    return ch;
}

int goome_ui_toggle(goome_ui *u, const char *label, int *v)
{
    if (!u || !u->c || !v) return 0;
    const goome_theme *th = &u->th;
    int n = ui_shown(label);
    double sw = ui_round(38 * u->em), sh = ui_round(22 * u->em), tw = ui_tw(u, label, n, 0);
    ui_box b = ui_item(u, 0, u->fh - 4);
    if (b.w < sw) b.w = sw;
    uint32_t id = ui_id(u, label);
    int hov, held, ch = 0;
    if (ui_button_logic(u, id, b, &hov, &held)) *v = !*v, ch = 1;
    ui_st *s = ui_get(u, id, NULL);
    double h = ui_anim(u, s, 0, hov || held), p = ui_anim(u, s, 1, held), k = ui_anim(u, s, 2, *v);
    double cy = b.y + b.h / 2;
    ui_box tb = ui_bx(b.x + b.w - sw, ui_round(cy - sh / 2), sw, sh);
    ui_rect(u, tb, sh / 2, ui_mix(ui_mix(th->input_bg, th->hover, h), th->accent, k));
    if (k < 1) ui_outline(u, tb, sh / 2, 1, ui_fade(th->text_dim, 0.35 * (1 - k)));
    double r = sh / 2 - 3 * u->em, stretch = 5 * p * u->em, kx = tb.x + 3 * u->em + (sw - 6 * u->em - 2 * r - stretch) * k;
    ui_shadow(u, ui_bx(kx, cy - r + 1, 2 * r + stretch, 2 * r), r, 4, 0x40000000);
    ui_rect(u, ui_bx(kx, cy - r, 2 * r + stretch, 2 * r), r, ui_mix(ui_mix(0xFFB9BEC7, 0xFFFFFFFF, h * 0.5), 0xFFFFFFFF, k));
    if (n && tw + 10 > b.w - sw) {
        ui_clip_push(u, ui_bx(b.x, b.y, ui_max(0, b.w - sw - 8), b.h));
        ui_text_mid(u, b.x, cy, label, n, 0, th->text);
        ui_clip_pop(u);
    } else if (n)
        ui_text_mid(u, b.x, cy, label, n, 0, th->text);
    return ch;
}

int goome_ui_radio(goome_ui *u, const char *label, int *v, int value)
{
    if (!u || !u->c || !v) return 0;
    const goome_theme *th = &u->th;
    int n = ui_shown(label);
    double tw = ui_tw(u, label, n, 0), sz = ui_round(18 * u->em);
    ui_box b = ui_item(u, sz + (n ? 8 + tw : 0), u->fh - 6);
    uint32_t id = ui_id(u, label);
    int hov, held, ch = 0;
    if (ui_button_logic(u, id, b, &hov, &held) && *v != value) *v = value, ch = 1;
    ui_st *s = ui_get(u, id, NULL);
    double h = ui_anim(u, s, 0, hov || held), k = ui_anim(u, s, 2, *v == value);
    double cy = b.y + b.h / 2, cx = b.x + sz / 2;
    ui_circle(u, cx, cy, sz / 2, ui_mix(ui_mix(th->input_bg, th->hover, h), th->accent, k));
    if (k < 1) ui_ring(u, cx, cy, sz / 2, 1.5, ui_fade(ui_mix(th->text_dim, th->text, h * 0.6), 0.75 * (1 - k)));
    if (k > 0) ui_circle(u, cx, cy, 4 * k * u->em, th->accent_text);
    if (n) ui_text_mid(u, b.x + sz + 8, cy, label, n, 0, th->text);
    return ch;
}

static int ui_slide(goome_ui *u, uint32_t id, const char *label, double *v, double lo, double hi, const char *fmt, int whole)
{
    const goome_theme *th = &u->th;
    int n = ui_shown(label);
    double top = n ? u->lh + 2 : 0;
    double th22 = ui_round(22 * u->em);
    ui_box b = ui_item(u, 0, top + th22);
    ui_box tb = ui_bx(b.x, b.y + top, b.w, th22);
    double kr = 8 * u->em, x0 = tb.x + kr, x1 = tb.x + tb.w - kr, cy = tb.y + tb.h / 2;
    int hov = ui_hover(u, id, tb), ch = 0;
    if (hov && u->pressed && !u->active) u->active = id;
    int held = u->active == id && (u->down || u->pressed);
    double span = hi - lo;
    if (held && x1 > x0) {
        double t = ui_clamp((u->mx - x0) / (x1 - x0), 0, 1), nv = lo + t * span;
        if (nv != *v) *v = nv, ch = 1;
    }
    if (whole) *v = ui_floor(*v + 0.5);
    char val[64];
    if (whole) snprintf(val, sizeof(val), "%d", (int)*v);
    else snprintf(val, sizeof(val), fmt ? fmt : "%.2f", *v);
    double cur = span != 0 ? ui_clamp((*v - lo) / span, 0, 1) : 0;
    int fresh;
    ui_st *s = ui_get(u, id, &fresh);
    ui_follow(u, &s->v[2], cur, fresh || held);
    double h = ui_anim(u, s, 0, hov || held), p = ui_anim(u, s, 1, held);
    if (n) {
        ui_text_mid(u, b.x, b.y + u->lh / 2, label, n, 0, th->text);
        double vw = ui_tw(u, val, (int)strlen(val), 0);
        ui_text_mid(u, b.x + b.w - vw, b.y + u->lh / 2, val, (int)strlen(val), 0, ui_mix(th->text_dim, th->text, h));
    }
    double kx = x0 + (x1 - x0) * s->v[2];
    ui_rect(u, ui_bx(tb.x, cy - 3, tb.w, 6), 3, ui_mix(th->input_bg, th->hover, h));
    if (kx > tb.x) ui_rect(u, ui_bx(tb.x, cy - 3, kx - tb.x, 6), 3, th->accent);
    if (p > 0) ui_circle(u, kx, cy, kr + 6 * p, ui_fade(th->accent, 0.22 * p));
    ui_shadow(u, ui_bx(kx - kr, cy - kr + 1.5, 2 * kr, 2 * kr), kr, 5, 0x50000000);
    ui_circle(u, kx, cy, kr + h, 0xFFFFFFFF);
    ui_ring(u, kx, cy, kr + h, 1, ui_fade(th->text, 0.12));
    ui_circle(u, kx, cy, 3 * p, th->accent);
    return ch;
}

int goome_ui_slider(goome_ui *u, const char *label, double *v, double min, double max, const char *fmt)
{
    if (!u || !u->c || !v) return 0;
    return ui_slide(u, ui_id(u, label), label, v, min, max, fmt, 0);
}

int goome_ui_slider_int(goome_ui *u, const char *label, int *v, int min, int max)
{
    if (!u || !u->c || !v) return 0;
    double d = *v;
    ui_slide(u, ui_id(u, label), label, &d, min, max, NULL, 1);
    int k = (int)d;
    if (k == *v) return 0;
    *v = k;
    return 1;
}

static ui_box ui_field_label(goome_ui *u, const char *label, int *n)
{
    *n = ui_shown(label);
    double top = *n ? u->lh + 4 : 0;
    ui_box b = ui_item(u, 0, top + u->fh);
    if (*n) ui_text_mid(u, b.x, b.y + u->lh / 2, label, *n, 0, u->th.text_dim);
    return ui_bx(b.x, b.y + top, b.w, u->fh);
}

int goome_ui_input(goome_ui *u, const char *label, char *buf, int cap)
{
    if (!u || !u->c || !buf || cap <= 0) return 0;
    const goome_theme *th = &u->th;
    int n, ch = 0;
    ui_box b = ui_field_label(u, label, &n);
    uint32_t id = ui_id(u, label);
    int fresh;
    ui_st *s = ui_get(u, id, &fresh);
    buf[cap - 1] = 0;
    int len = (int)strlen(buf);
    int cur = s->i[0] < 0 ? 0 : s->i[0] > len ? len : s->i[0];
    while (cur > 0 && cur < len && ((unsigned char)buf[cur] & 0xC0) == 0x80) cur--;
    double tx = b.x + 10, iw = b.w - 20;
    int hov = ui_hover(u, id, b);
    if (hov && u->pressed && !u->active) {
        u->active = id;
        if (u->focus != id) {
            u->focus = id;
            ui_keyboard(u, 1);
            u->nkeys = 0;
        }
        double want = u->mx - (tx - s->v[2]), best = 1e9;
        for (int i = 0; i <= len; i = ui_u8_next(buf, i, len)) {
            double d = ui_abs(ui_tw(u, buf, i, 0) - want);
            if (d < best) best = d, cur = i;
            if (i == len) break;
        }
        s->v[3] = (double)u->now;
    } else if (u->focus == id && u->pressed && !hov)
        ui_unfocus(u);
    if (u->focus == id) {
        u->focus_seen = 1;
        for (int k = 0; k < u->nkeys && u->focus == id; k++) {
            ui_key *e = &u->keys[k];
            s->v[3] = (double)u->now;
            if (e->key == GOOME_K_BACKSPACE && !(e->mods & GOOME_CTRL)) {
                int p = ui_u8_prev(buf, cur);
                if (p < cur) memmove(buf + p, buf + cur, (size_t)(len - cur + 1)), len -= cur - p, cur = p, ch = 1;
            } else if (e->key == GOOME_K_DELETE) {
                int q = ui_u8_next(buf, cur, len);
                if (q > cur) memmove(buf + cur, buf + q, (size_t)(len - q + 1)), len -= q - cur, ch = 1;
            } else if (e->key == GOOME_K_LEFT) cur = ui_u8_prev(buf, cur);
            else if (e->key == GOOME_K_RIGHT) cur = ui_u8_next(buf, cur, len);
            else if (e->key == GOOME_K_HOME || e->key == GOOME_K_UP) cur = 0;
            else if (e->key == GOOME_K_END || e->key == GOOME_K_DOWN) cur = len;
            else if (e->key == GOOME_K_ENTER || e->key == GOOME_K_ESC) ui_unfocus(u);
            else if ((unsigned char)e->text[0] >= 32 && e->text[0] != 127 && !(e->mods & (GOOME_CTRL | GOOME_ALT | GOOME_SUPER))) {
                int t = (int)strlen(e->text);
                if (len + t < cap) {
                    memmove(buf + cur + t, buf + cur, (size_t)(len - cur + 1));
                    memcpy(buf + cur, e->text, (size_t)t);
                    len += t, cur += t, ch = 1;
                }
            }
        }
        u->nkeys = 0;
    }
    s->i[0] = cur;
    int focused = u->focus == id;
    double h = ui_anim(u, s, 0, hov), f = ui_anim(u, s, 1, focused);
    double cx = ui_tw(u, buf, cur, 0);
    if (cx - s->v[2] > iw - 2) s->v[2] = cx - iw + 2;
    if (cx - s->v[2] < 0) s->v[2] = cx;
    if (!focused && s->v[2] > 0 && ui_tw(u, buf, len, 0) <= iw) s->v[2] = 0;
    if (f > 0) ui_outline(u, ui_bx(b.x - 3, b.y - 3, b.w + 6, b.h + 6), th->radius + 3, 3, ui_fade(th->accent, 0.28 * f));
    ui_rect(u, b, th->radius, ui_mix(th->input_bg, ui_mix(th->input_bg, th->hover, 0.5), h * (1 - f)));
    ui_outline(u, b, th->radius, 1, ui_mix(ui_mix(th->border, ui_fade(th->text_dim, 0.5), h), th->accent, f));
    ui_clip_push(u, ui_bx(b.x + 6, b.y, b.w - 12, b.h));
    double x = ui_round(tx - s->v[2]);
    ui_text_mid(u, x, b.y + b.h / 2, buf, len, 0, th->text);
    if (focused) {
        long t = u->now - (long)s->v[3];
        if ((t / 530) % 2 == 0) ui_rect(u, ui_bx(x + cx, b.y + b.h / 2 - u->lh / 2 + 1, 1.5, u->lh - 2), 0, th->accent);
    }
    ui_clip_pop(u);
    return ch;
}

static void ui_checker(goome_ui *u, ui_box b, double r)
{
    ui_rect(u, b, r, 0xFFFFFFFF);
    double c = 5;
    ui_clip_push(u, ui_bx(b.x + r * 0.3, b.y, b.w - r * 0.6, b.h));
    for (int j = 0; j * c < b.h; j++)
        for (int i = (j & 1); i * c < b.w; i += 2) ui_rect(u, ui_bx(b.x + i * c, b.y + j * c, c, ui_min(c, b.h - j * c)), 0, 0xFFCDD1D6);
    ui_clip_pop(u);
}

static void ui_swatch(goome_ui *u, ui_box b, double r, uint32_t c)
{
    if ((c >> 24) < 255) ui_checker(u, b, r);
    ui_rect(u, b, r, c);
    ui_outline(u, b, r, 1, ui_fade(u->th.text, 0.18));
}

static ui_box ui_popup_place(goome_ui *u, ui_box anchor, double w, double h)
{
    goome_rect a = u->area;
    double x = ui_min(anchor.x, a.x + a.w - w - 6), y = anchor.y + anchor.h + 6;
    if (y + h > a.y + a.h - 6 && anchor.y - h - 6 >= a.y) y = anchor.y - h - 6;
    return ui_bx(ui_round(ui_max(x, a.x + 6)), ui_round(y), w, h);
}

static void ui_popup_bg(goome_ui *u, ui_box p)
{
    const goome_theme *th = &u->th;
    uint32_t bg = th->panel | 0xFF000000u;
    ui_shadow(u, ui_bx(p.x, p.y + 6, p.w, p.h), th->radius, th->shadow_blur, th->shadow);
    ui_rect(u, p, th->radius, bg);
    ui_outline(u, p, th->radius, th->border_width, th->border);
}

int goome_ui_combo(goome_ui *u, const char *label, int *sel, const char *const *items, int n)
{
    if (!u || !u->c || !sel) return 0;
    const goome_theme *th = &u->th;
    int ln, ch = 0;
    ui_box b = ui_field_label(u, label, &ln);
    uint32_t id = ui_id(u, label);
    int hov, held;
    ui_st *s = ui_get(u, id, NULL);
    if (ui_button_logic(u, id, b, &hov, &held)) {
        if (u->popup == id) u->popup = 0;
        else if (u->pop_closed != id && n > 0) u->popup = id, u->pop_kind = UI_POP_COMBO, s->v[4] = 0, s->v[2] = 0;
    }
    int open = u->popup == id;
    ui_rect(u, b, th->radius, ui_btn_bg(u, s, hov || open, held));
    double h = ui_ease(s->v[0]), k = ui_anim(u, s, 4, open);
    ui_outline(u, b, th->radius, 1, ui_mix(th->border, th->accent, k * 0.9));
    const char *cur = *sel >= 0 && *sel < n && items && items[*sel] ? items[*sel] : "";
    ui_clip_push(u, ui_bx(b.x, b.y, b.w - 28, b.h));
    ui_text_mid(u, b.x + 12, b.y + b.h / 2, cur, (int)strlen(cur), 0, th->text);
    ui_clip_pop(u);
    double ax = b.x + b.w - 16 * u->em, ay = b.y + b.h / 2, d = 4 * u->em, f = 1 - 2 * k;
    uint32_t ac = ui_mix(th->text_dim, th->text, h);
    ui_seg(u, ax - d, ay - d * 0.5 * f, ax, ay + d * 0.5 * f, 1.6, ac);
    ui_seg(u, ax, ay + d * 0.5 * f, ax + d, ay - d * 0.5 * f, 1.6, ac);
    if (!open || !items) return 0;

    u->pop_seen = 1;
    double rh = u->fh - 2, ph = ui_min(n * rh + 8, 8 * rh + 8);
    ui_box p = ui_popup_place(u, b, b.w, ph);
    u->pop_box = p;
    ui_save sv;
    ui_enter(u, &sv, &u->pop, UI_POPUP, k);
    double slide = ui_round((1 - k) * (p.y < b.y ? 6 : -6));
    ui_box pv = ui_bx(p.x, p.y + slide, p.w, p.h);
    ui_popup_bg(u, pv);
    double maxo = ui_max(0, n * rh + 8 - ph);
    if (u->wheel && u->hover_layer == UI_POPUP && ui_in(p, u->mx, u->my)) s->v[2] = ui_clamp(s->v[2] + u->wheel * rh, 0, maxo), u->wheel = 0;
    s->v[2] = ui_clamp(s->v[2], 0, maxo);
    ui_clip_push(u, ui_bx(pv.x + 1, pv.y + 1, pv.w - 2, pv.h - 2));
    for (int i = 0; i < n; i++) {
        ui_box r = ui_bx(pv.x + 4, pv.y + 4 + i * rh - s->v[2], pv.w - 8, rh);
        if (r.y + r.h < pv.y || r.y > pv.y + pv.h) continue;
        uint32_t iid = ui_hashi(id, (uint32_t)i + 100);
        int ih, iheld;
        if (ui_button_logic(u, iid, r, &ih, &iheld)) {
            if (*sel != i) ch = 1;
            *sel = i;
            u->popup = 0;
        }
        ui_st *is = ui_get(u, iid, NULL);
        double a = ui_anim(u, is, 0, ih || iheld);
        if (a > 0) ui_rect(u, r, ui_max(0, th->radius - 4), ui_fade(th->hover, a));
        const char *t = items[i] ? items[i] : "";
        int on = i == *sel;
        ui_text_mid(u, r.x + 10, r.y + r.h / 2, t, (int)strlen(t), 0, on ? th->accent : th->text);
        if (on) {
            double cx = r.x + r.w - 20, cy = r.y + r.h / 2;
            ui_seg(u, cx, cy, cx + 3.5, cy + 3.5, 1.8, th->accent);
            ui_seg(u, cx + 3.5, cy + 3.5, cx + 10, cy - 3.5, 1.8, th->accent);
        }
    }
    if (maxo > 0) {
        double tr = pv.h - 8, tht = ui_max(20, tr * ph / (n * rh + 8));
        ui_rect(u, ui_bx(pv.x + pv.w - 7, pv.y + 4 + (tr - tht) * s->v[2] / maxo, 4, tht), 2, ui_fade(th->text, 0.25));
    }
    ui_clip_pop(u);
    ui_leave(u, &sv);
    return ch;
}

int goome_ui_selectable(goome_ui *u, const char *label, int selected)
{
    if (!u || !u->c) return 0;
    const goome_theme *th = &u->th;
    int n = ui_shown(label);
    ui_box b = ui_item(u, 0, u->fh - 4);
    uint32_t id = ui_id(u, label);
    int hov, held, click = ui_button_logic(u, id, b, &hov, &held);
    ui_st *s = ui_get(u, id, NULL);
    double h = ui_anim(u, s, 0, hov || held), k = ui_anim(u, s, 2, selected);
    double r = ui_max(0, th->radius - 3);
    if (k > 0) ui_rect(u, b, r, ui_fade(th->accent, 0.16 * k));
    if (h > 0) ui_rect(u, b, r, ui_fade(th->hover, h * (0.8 - 0.5 * k)));
    if (k > 0) ui_rect(u, ui_bx(b.x + 4, b.y + b.h / 2 - 7 * k, 3, 14 * k), 1.5, th->accent);
    ui_clip_push(u, b);
    ui_text_mid(u, b.x + 14, b.y + b.h / 2, label, n, 0, ui_mix(th->text, ui_mix(th->text, th->accent, 0.6), k));
    ui_clip_pop(u);
    return click;
}

int goome_ui_tabs(goome_ui *u, const char *id, const char *const *names, int n, int *sel)
{
    if (!u || !u->c || !sel || n <= 0 || !names) return 0;
    const goome_theme *th = &u->th;
    ui_box b = ui_item(u, 0, u->fh + 6);
    uint32_t tid = ui_id(u, id);
    int fresh, ch = 0;
    ui_st *s = ui_get(u, tid, &fresh);
    ui_rect(u, b, th->radius, th->input_bg);
    double tw = (b.w - 6) / n;
    if (*sel >= n) *sel = n - 1;
    if (*sel < 0) *sel = 0;
    for (int i = 0; i < n; i++) {
        ui_box t = ui_bx(b.x + 3 + i * tw, b.y + 3, tw, b.h - 6);
        uint32_t iid = ui_hashi(tid, (uint32_t)i + 1);
        int hov, held;
        if (ui_button_logic(u, iid, t, &hov, &held) && *sel != i) *sel = i, ch = 1;
        ui_st *is = ui_get(u, iid, NULL);
        ui_anim(u, is, 0, hov || held);
    }
    ui_follow(u, &s->v[0], *sel * tw, fresh);
    ui_box pill = ui_bx(ui_round(b.x + 3 + s->v[0]), b.y + 3, ui_round(tw), b.h - 6);
    ui_shadow(u, ui_bx(pill.x, pill.y + 1, pill.w, pill.h), th->radius - 3, 6, 0x30000000);
    ui_rect(u, pill, ui_max(0, th->radius - 3), th->accent);
    for (int i = 0; i < n; i++) {
        ui_box t = ui_bx(b.x + 3 + i * tw, b.y + 3, tw, b.h - 6);
        ui_st *is = ui_get(u, ui_hashi(tid, (uint32_t)i + 1), NULL);
        double over = ui_clamp(1 - ui_abs(t.x - pill.x) / tw, 0, 1);
        uint32_t c = ui_mix(ui_mix(th->text_dim, th->text, ui_ease(is->v[0])), th->accent_text, over);
        const char *nm = names[i] ? names[i] : "";
        int ln = ui_shown(nm);
        double w = ui_tw(u, nm, ln, 0);
        ui_clip_push(u, t);
        ui_text_mid(u, ui_round(t.x + (t.w - w) / 2), t.y + t.h / 2, nm, ln, 0, c);
        ui_clip_pop(u);
    }
    u->last_id = tid, u->last_box = b;
    u->last_hover = u->hover_layer == u->layer && ui_in(b, u->mx, u->my);
    return ch;
}

int goome_ui_collapsing(goome_ui *u, const char *label, int *open)
{
    if (!u || !u->c) return 0;
    const goome_theme *th = &u->th;
    int n = ui_shown(label);
    ui_box b = ui_item(u, 0, u->fh);
    uint32_t id = ui_id(u, label);
    int fresh;
    ui_st *s = ui_get(u, id, &fresh);
    s->keep = 1;
    int *o = open ? open : &s->i[1];
    int hov, held;
    if (ui_button_logic(u, id, b, &hov, &held)) *o = !*o;
    if (fresh) s->v[2] = *o ? 1 : 0;
    double h = ui_anim(u, s, 0, hov || held), k = ui_anim(u, s, 2, *o);
    ui_rect(u, b, th->radius, ui_mix(ui_fade(th->input_bg, 0.7), th->hover, h));
    ui_chevron(u, b.x + 16 * u->em, b.y + b.h / 2, k * 1.5707963267948966, 4.5 * u->em, ui_mix(th->text_dim, th->text, h));
    ui_clip_push(u, b);
    ui_text_mid(u, b.x + ui_round(30 * u->em), b.y + b.h / 2, label, n, 0, th->text);
    ui_clip_pop(u);
    return *o;
}

void goome_ui_progress(goome_ui *u, double frac, const char *text)
{
    if (!u || !u->c) return;
    const goome_theme *th = &u->th;
    int n = text ? (int)strlen(text) : 0;
    double bh = n ? u->lh + 6 : 8;
    ui_box b = ui_item(u, 0, bh);
    uint32_t id = ui_auto(u);
    int fresh;
    ui_st *s = ui_get(u, id, &fresh);
    ui_hover(u, id, b);
    double r = ui_min(th->radius, bh / 2);
    ui_rect(u, b, r, th->input_bg);
    ui_box fill;
    if (frac < 0) {
        double t = (double)(u->now % 1600) / 1600, seg = b.w * 0.3;
        double x = b.x - seg + (b.w + seg) * ui_ease(t);
        fill = ui_cut(ui_bx(x, b.y, seg, bh), b);
        u->anim_next = 1;
    } else {
        ui_follow(u, &s->v[0], ui_clamp(frac, 0, 1), fresh);
        fill = ui_bx(b.x, b.y, ui_round(b.w * s->v[0]), bh);
        if (fill.w > 0 && fill.w < 2 * r) fill.w = ui_min(b.w, 2 * r);
    }
    if (fill.w > 0) ui_rect(u, fill, r, th->accent);
    if (!n) return;
    double w = ui_tw(u, text, n, 0), tx = ui_round(b.x + (b.w - w) / 2);
    if (tx < b.x + r) tx = ui_round(b.x + r);
    ui_clip_push(u, b);
    ui_text_mid(u, tx, b.y + bh / 2, text, n, 0, th->text);
    if (fill.w > 0) {
        ui_clip_push(u, fill);
        ui_text_mid(u, tx, b.y + bh / 2, text, n, 0, th->accent_text);
        ui_clip_pop(u);
    }
    ui_clip_pop(u);
}

static uint32_t ui_hsv(double h, double s, double v)
{
    h = (h - ui_floor(h)) * 6;
    int i = (int)h;
    double f = h - i, p = v * (1 - s), q = v * (1 - s * f), t = v * (1 - s * (1 - f)), r, g, b;
    switch (i % 6) {
    case 0: r = v, g = t, b = p; break;
    case 1: r = q, g = v, b = p; break;
    case 2: r = p, g = v, b = t; break;
    case 3: r = p, g = q, b = v; break;
    case 4: r = t, g = p, b = v; break;
    default: r = v, g = p, b = q; break;
    }
    return 0xFF000000u | (uint32_t)(r * 255 + 0.5) << 16 | (uint32_t)(g * 255 + 0.5) << 8 | (uint32_t)(b * 255 + 0.5);
}

static void ui_to_hsv(uint32_t c, double *h, double *s, double *v)
{
    double r = ((c >> 16) & 255) / 255.0, g = ((c >> 8) & 255) / 255.0, b = (c & 255) / 255.0;
    double mx = ui_max(r, ui_max(g, b)), mn = ui_min(r, ui_min(g, b)), d = mx - mn;
    *v = mx;
    *s = mx > 0 ? d / mx : 0;
    if (d <= 0) return; // grey, keep the old hue
    double x = mx == r ? (g - b) / d : mx == g ? 2 + (b - r) / d : 4 + (r - g) / d;
    x /= 6;
    *h = x < 0 ? x + 1 : x;
}

static int ui_drag(goome_ui *u, uint32_t id, ui_box b)
{
    int hov = ui_hover(u, id, b);
    if (hov && u->pressed && !u->active) u->active = id;
    return u->active == id && (u->down || u->pressed);
}

int goome_ui_color(goome_ui *u, const char *label, uint32_t *argb)
{
    if (!u || !u->c || !argb) return 0;
    const goome_theme *th = &u->th;
    int n = ui_shown(label), ch = 0;
    ui_box b = ui_item(u, 0, u->fh - 4);
    uint32_t id = ui_id(u, label);
    int fresh, hov, held;
    ui_st *s = ui_get(u, id, &fresh);
    if (fresh || (uint32_t)s->i[1] != *argb) ui_to_hsv(*argb, &s->v[2], &s->v[3], &s->v[4]), s->i[1] = (int)*argb;
    if (ui_button_logic(u, id, b, &hov, &held)) {
        if (u->popup == id) u->popup = 0;
        else if (u->pop_closed != id) u->popup = id, u->pop_kind = UI_POP_COLOR, s->v[1] = 0;
    }
    int open = u->popup == id;
    double h = ui_anim(u, s, 0, hov || held || open), k = ui_anim(u, s, 1, open);
    if (h > 0) ui_rect(u, b, th->radius - 2, ui_fade(th->hover, h * 0.7));
    double swh = ui_round(20 * u->em);
    ui_box sw = ui_bx(b.x + 4, b.y + ui_round((b.h - swh) / 2), ui_round(34 * u->em), swh);
    ui_swatch(u, sw, 6, *argb);
    if (n) ui_text_mid(u, sw.x + sw.w + 10, b.y + b.h / 2, label, n, 0, th->text);
    char hex[16];
    if ((*argb >> 24) == 255) snprintf(hex, sizeof(hex), "#%06X", (unsigned)(*argb & 0xFFFFFF));
    else snprintf(hex, sizeof(hex), "#%08X", (unsigned)((*argb << 8) | (*argb >> 24)));
    double hw = ui_tw(u, hex, (int)strlen(hex), 0);
    ui_text_mid(u, b.x + b.w - hw - 8, b.y + b.h / 2, hex, (int)strlen(hex), 0, th->text_dim);
    if (!open) return 0;

    u->pop_seen = 1;
    double pw = 236, ph = 250;
    ui_box p = ui_popup_place(u, sw, pw, ph);
    u->pop_box = p;
    ui_save sv;
    ui_enter(u, &sv, &u->pop, UI_POPUP, k);
    p.y += ui_round((1 - k) * (p.y < sw.y ? 6 : -6));
    ui_popup_bg(u, p);
    ui_box sq = ui_bx(p.x + 12, p.y + 12, pw - 24, 140), hb = ui_bx(sq.x, sq.y + sq.h + 12, sq.w, 12), ab = ui_bx(sq.x, hb.y + 22, sq.w, 12);
    double a = ((*argb >> 24) & 255) / 255.0;
    if (ui_drag(u, ui_hashi(id, 11), sq)) {
        s->v[3] = ui_clamp((u->mx - sq.x) / sq.w, 0, 1), s->v[4] = 1 - ui_clamp((u->my - sq.y) / sq.h, 0, 1);
        ch = 1;
    }
    if (ui_drag(u, ui_hashi(id, 12), ui_bx(hb.x - 4, hb.y - 4, hb.w + 8, hb.h + 8))) s->v[2] = ui_clamp((u->mx - hb.x) / hb.w, 0, 0.9999), ch = 1;
    if (ui_drag(u, ui_hashi(id, 13), ui_bx(ab.x - 4, ab.y - 4, ab.w + 8, ab.h + 8))) a = ui_clamp((u->mx - ab.x) / ab.w, 0, 1), ch = 1;
    uint32_t hue = ui_hsv(s->v[2], 1, 1), rgb = ui_hsv(s->v[2], s->v[3], s->v[4]);
    if (ch) {
        uint32_t nv = (uint32_t)(a * 255 + 0.5) << 24 | (rgb & 0xFFFFFF);
        ch = nv != *argb;
        *argb = nv;
        s->i[1] = (int)nv;
    }
    ui_grad(u, sq, goome_radius(6), 0xFFFFFFFF, hue, 1);
    ui_grad(u, sq, goome_radius(6), 0x00000000, 0xFF000000, 0);
    double mx = sq.x + s->v[3] * sq.w, my = sq.y + (1 - s->v[4]) * sq.h;
    ui_ring(u, mx, my, 8, 1, 0x50000000);
    ui_ring(u, mx, my, 7, 2.5, 0xFFFFFFFF);
    ui_circle(u, mx, my, 4.8, rgb);
    for (int i = 0; i < 6; i++) {
        goome_radii r = goome_radius(0);
        if (i == 0) r.tl = r.bl = 6;
        if (i == 5) r.tr = r.br = 6;
        double x0 = hb.x + hb.w * i / 6, x1 = hb.x + hb.w * (i + 1) / 6;
        ui_grad(u, ui_bx(x0, hb.y, x1 - x0 + (i < 5 ? 1 : 0), hb.h), r, ui_hsv(i / 6.0, 1, 1), ui_hsv((i + 1) / 6.0 - 1e-9, 1, 1), 1);
    }
    double hx = hb.x + s->v[2] * hb.w;
    ui_circle(u, hx, hb.y + hb.h / 2, 8, 0x50000000);
    ui_circle(u, hx, hb.y + hb.h / 2, 7.5, 0xFFFFFFFF);
    ui_circle(u, hx, hb.y + hb.h / 2, 5, hue);
    ui_checker(u, ab, 6);
    ui_grad(u, ab, goome_radius(6), rgb & 0xFFFFFF, rgb, 1);
    double axx = ab.x + a * ab.w;
    ui_circle(u, axx, ab.y + ab.h / 2, 8, 0x50000000);
    ui_circle(u, axx, ab.y + ab.h / 2, 7.5, 0xFFFFFFFF);
    ui_circle(u, axx, ab.y + ab.h / 2, 5, rgb);
    ui_box pr = ui_bx(sq.x, ab.y + ab.h + 12, 46, 26);
    ui_swatch(u, pr, 6, *argb);
    char info[48];
    snprintf(info, sizeof(info), "%s  a %d", hex, (int)((*argb >> 24) & 255));
    ui_text_mid(u, pr.x + pr.w + 10, pr.y + pr.h / 2, info, (int)strlen(info), 0, th->text);
    ui_leave(u, &sv);
    u->last_id = id, u->last_box = b;
    return ch;
}

void goome_ui_tooltip(goome_ui *u, const char *text)
{
    if (!u || !u->c || !text || !*text || !u->last_hover || u->tip_id != u->last_id) return;
    const goome_theme *th = &u->th;
    double wait = 500;
    if (u->tip_ms < wait) {
        if (u->tip_ms > -1000) u->anim_next = 1;
        return;
    }
    double k = th->anim_ms > 0 ? ui_ease((u->tip_ms - wait) / th->anim_ms) : 1;
    if (k < 1) u->anim_next = 1;
    int st[32], ln[32];
    int n = ui_wrap(u, text, 260, st, ln, 32);
    double w = 0;
    for (int i = 0; i < n; i++) w = ui_max(w, ui_tw(u, text + st[i], ln[i], 0));
    double bw = ui_round(w + 20), bh = ui_round(n * u->lh + 14);
    goome_rect a = u->area;
    double x = ui_min(u->mx + 14, a.x + a.w - bw - 4), y = u->my + 20;
    if (y + bh > a.y + a.h - 4) y = u->my - bh - 8;
    ui_box b = ui_bx(ui_round(x), ui_round(y + (1 - k) * 4), bw, bh);
    ui_save sv;
    ui_enter(u, &sv, &u->tip, UI_TIP, k);
    ui_shadow(u, ui_bx(b.x, b.y + 3, b.w, b.h), 8, 12, th->shadow);
    ui_rect(u, b, 8, ui_mix(th->bg, th->hover, 0.35) | 0xFF000000u);
    ui_outline(u, b, 8, 1, th->border);
    for (int i = 0; i < n; i++) ui_text_mid(u, b.x + 10, b.y + 7 + u->lh * (i + 0.5), text + st[i], ln[i], 0, th->text);
    ui_leave(u, &sv);
}

int goome_ui_hovered(goome_ui *u) { return u && u->c ? u->last_hover : 0; }

#endif
