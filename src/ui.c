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
#define UI_IME_KEY 0x7E000
#define UI_MAXFR 16
#define UI_MAXSCR 16
#define UI_MAXCLIP 32
#define UI_MAXID 32
#define UI_BLK 256
#define UI_NBLK 256
#define UI_MAXPOP 8
#define UI_MAXTOAST 8
#define UI_MAXED 16
#define UI_MAXTBL 4
#define UI_MAXCOL 16
#define UI_MAXTREE 32
#define UI_MAXSPL 8

enum { UI_ROOT = 1, UI_POPUP, UI_TIP, UI_BLOCK, UI_TOASTL };
enum { UP_MENU = 1, UP_POPUP, UP_MODAL };
enum { UA_BELOW, UA_RIGHT, UA_POINT, UA_CENTER };
enum { UN_PLAIN, UN_RADIO, UN_INPUT, UN_EDIT, UN_CUSTOM };
enum { UC_RECT, UC_LINE, UC_GRAD, UC_SHADOW, UC_CIRCLE, UC_RING, UC_SEG, UC_TRI, UC_TEXT, UC_BITMAP, UC_CLIP, UC_UNCLIP, UC_ARC, UC_POLY, UC_NINE, UC_LAYER };
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
    double x0, y0, w, ny, line_y, line_h, prev_x1, max_y, next_w, max_x, gap;
    int same, has_next_w, row_cols, row_i, any, tight;
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
    int dead, flags, nscr0, nclip0, nid0, nid_out, bg, nbg, autoid, clipcmd, pop;
    double *px, *py, *pw, *ph;
    int *flag;
    double minw, pad;
    double title_h;
    ui_box box;
    ui_lay lay;
} ui_frame;

typedef struct {
    int key, mods;
    char text[8];
} ui_key;

typedef struct {
    uint32_t id;
    int kind, side, seen, born, close_req, sel, nitems, items, fire, sub, chk, has_chk;
    uint32_t bar;
    ui_box anchor, box, prev;
    double w, h, open;
} ui_pop;

typedef struct {
    uint32_t id, sort_id;
    int cols, ncol, flags, row, cell, in_row, head, scroll, nid_out, bg_cmd, row_cmd, nclip0, live;
    int clip_cmd[UI_MAXCOL], cmd0[UI_MAXCOL], cmd1[UI_MAXCOL];
    double cell_h[UI_MAXCOL];
    double w[UI_MAXCOL], x[UI_MAXCOL], want[UI_MAXCOL], y, row_y, cell_max, hh, clip_end, min_row;
    char names[UI_MAXCOL][48];
    ui_box box;
    ui_lay saved;
    ui_lay *lay;
} ui_tbl;

typedef struct {
    ui_lay *lay;
    double ind, x, y;
} ui_tree;

typedef struct {
    uint32_t id;
    int vertical, pane, nid_out, live;
    ui_box box, a, b, bar;
} ui_split;

typedef struct {
    char text[200];
    double age, y;
    int ms, fresh;
    ui_box box;
} ui_toast;

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
    struct ui_ax_s *ax;
    int ax_on;

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

    ui_pop ps[UI_MAXPOP];
    ui_list plist[UI_MAXPOP], tl;
    int npop, pdepth;
    ui_toast toasts[UI_MAXTOAST];
    int ntoast;

    uint32_t nav_layer, nav_first, nav_last, nav_prev, tab_new, focus_ptr_id;
    int tab_req, tab_take, rad_only, want_focus, focus_vis, focus_kind, tab_came, tab_kind, nav_first_kind;
    int nav_act, nav_dx, nav_home, nav_prev_kind, nav_prev_val;
    const void *nav_prev_ptr, *focus_ptr;

    int p_npress, p_rpressed, rpressed, clicks, moved, unfocused, p_mods, pmods;
    long press_ms;
    double omx, omy, pmx, pmy;
    int cur_want, cur_set;

    int app_on, app_was, app_flags, app_mb_used;
    ui_box app_box, app_mb, app_sb;
    double app_r, page_bottom;
    char status[256];

    int mb_on, mb_i, mb_open_i, mb_want, mb_base, mb_shift, mb_hit, nmbs, nmbs_prev;
    double mb_x;
    uint32_t mb_id, mh_id;
    double mh_ms;
    int mh_seen;
    ui_box mb_box, mbs[4], mbs_prev[4];

    char *undo;
    int undo_cap, undo_cur, undo_kind;
    uint32_t undo_id;
    char *ime_buf, *ime_pre;
    int ime_len, ime_cap, ime_at[UI_MAXKEY], ime_plen, ime_pcap, ime_pcur, ime_want, ime_on;
    uint32_t ime_id;
    goome_rect ime_rect, ime_sent;

    struct ui_ed_s *eds[UI_MAXED];
    goome_font *mono;
    int mono_tried;
    ui_tbl tbs[UI_MAXTBL];
    int ntb;
    ui_tree trees[UI_MAXTREE];
    int ntree;
    ui_split sps[UI_MAXSPL];
    int nsp;
    uint32_t num_id;
    char num_buf[64];
    int num_cancel;
    uint32_t dd_src;
    int dd_on, dd_len, dd_cap, dd_seen;
    int auto_grab, grab, grab_frame, press_ui;
    double dd_px, dd_py, dd_ox, dd_oy;
    char dd_type[32];
    unsigned char *dd_data;
    ui_box dd_box;

    struct ui_sty_s *sty;
    int nsty, sty_cap, sty_warn, rad_on;
    goome_radii rad;
    int pos_on, sized;
    double pos_x, pos_y, size_w, size_h;
    goome_canvas lay;
    uint32_t *lay_px;
    size_t lay_cap;
    ui_list *lay_list;
    double lay_alpha;
    struct ui_keep_s *keeps;
    int nkeep, keep_cap;
    goome_ui_config cfg;
};

typedef struct ui_sty_s {
    goome_theme th;
    goome_radii rad;
    int rad_on;
} ui_sty;

typedef struct ui_keep_s {
    uint32_t id;
    int n, seen;
    double *v;
} ui_keep;

enum { UI_AXW = 1, UI_AXP, UI_AXT, UI_AXB, UI_AXA, UI_AXL, UI_AXG, UI_AXM };
static int ui_ax_add(goome_ui *u, uint32_t id, ui_box b, int role, const char *s, int n, int states, int kind);
static void ui_ax_push(goome_ui *u, int i, int kind);
static void ui_ax_pop(goome_ui *u, int kind, const ui_box *b);
static int ui_ax_do(goome_ui *u, uint32_t id, int verb);
static int ui_ax_feed(goome_ui *u, const goome_event *e);
static void ui_ax_begin(goome_ui *u);
static void ui_ax_end(goome_ui *u);
static void ui_ax_free(goome_ui *u);
static void ui_ax_text(goome_ui *u, ui_box b, const char *s, int n, int role);
static void ui_ax_val(goome_ui *u, int i, const char *s, int n);
static void ui_ax_btn(goome_ui *u, uint32_t id, ui_box b, const char *s, int n);
static void ui_ax_check(goome_ui *u, uint32_t id, ui_box b, const char *s, int n, int role, int on);
static int ui_ax_slider(goome_ui *u, uint32_t id, ui_box b, const char *s, int n, double *v, double lo, double hi, const char *val);
static int ui_ax_input(goome_ui *u, uint32_t id, char *buf, int cap, int *len, int flags);
static void ui_ax_field(goome_ui *u, uint32_t id, ui_box b, const char *label, int n, const char *hint, const char *shown, int len, int cur, int flags);
static int ui_ax_combo(goome_ui *u, uint32_t id, ui_box b, const char *s, int n, int *sel, const char *const *items, int count, int open);
static void ui_ax_list(goome_ui *u, uint32_t id, ui_box b, int on);
static void ui_ax_item(goome_ui *u, uint32_t id, ui_box b, const char *s, int sel, int role);
static void ui_ax_tabs(goome_ui *u, uint32_t id, ui_box b, const char *s, int on);
static void ui_ax_expander(goome_ui *u, uint32_t id, ui_box b, const char *s, int n, int role, int open, int push);
static void ui_ax_progress(goome_ui *u, ui_box b, double frac, const char *s);
static void ui_ax_window(goome_ui *u, uint32_t id, ui_box b, const char *t, int flags, int collapsed);
static void ui_ax_menu(goome_ui *u, uint32_t id, ui_box b, const char *s, int n, int open, int push);
static void ui_ax_menuitem(goome_ui *u, uint32_t id, ui_box b, const char *s, int n, const int *checked);
static void ui_ax_pop_node(goome_ui *u, uint32_t id, int role, const char *s, int n, int ok);
static int ui_ax_spin(goome_ui *u, uint32_t id, ui_box b, const char *s, int n, double *v, double lo, double hi, const char *fmt, int whole);
static int ui_ax_color(goome_ui *u, uint32_t id, ui_box b, const char *s, int n, uint32_t *argb, int open);
static int ui_ax_edit(goome_ui *u, uint32_t id, char *buf, int cap);
static void ui_ax_custom(goome_ui *u, uint32_t id, ui_box b, int focusable, int *click);

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
static uint32_t ui_c(uint32_t c, uint32_t def) { return c ? c : def; }
static int ui_bits(int m)
{
    int n = 0;
    for (; m; m &= m - 1) n++;
    return n;
}
static int ui_act(goome_ui *u, int key, int mods)
{
    int best = -1, most = -1;
    if (!key) return -1;
    for (int a = 0; a < GOOME_ACT_COUNT; a++)
        for (int i = 0; i < 4; i++) {
            goome_combo k = u->cfg.keys[a][i];
            if (!k.key || k.key != key || (k.mods & ~mods) || (mods & ~k.mods & ~GOOME_SHIFT)) continue;
            if (ui_bits(k.mods) > most) most = ui_bits(k.mods), best = a;
        }
    return best;
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
static double ui_exp(double x)
{
    int n = 0;
    while ((x < -0.125 || x > 0.125) && n < 60) x /= 2, n++;
    double r = 1 + x * (1 + x / 2 * (1 + x / 3 * (1 + x / 4 * (1 + x / 5 * (1 + x / 6 * (1 + x / 7))))));
    while (n--) r *= r;
    return r;
}
static double ui_sqrt(double v)
{
    if (v <= 0) return 0;
    double r = v > 1 ? v : 1;
    for (int i = 0; i < 40; i++) {
        double n = (r + v / r) / 2;
        if (n >= r) break;
        r = n;
    }
    return r;
}

double goome_ease(int ease, double t)
{
    t = ui_clamp(t, 0, 1);
    double k = t - 1;
    switch (ease) {
    case GOOME_EASE_LINEAR: return t;
    case GOOME_EASE_IN: return t * t * t;
    case GOOME_EASE_OUT: return 1 - (1 - t) * (1 - t) * (1 - t);
    case GOOME_EASE_IN_OUT: return t < 0.5 ? 4 * t * t * t : 1 + 4 * k * k * k;
    case GOOME_EASE_BACK: return 1 + 2.70158 * k * k * k + 1.70158 * k * k;
    case GOOME_EASE_SPRING:
        if (t <= 0 || t >= 1) return t;
        return 1 - ui_exp(-7 * t) * (ui_cos(12 * t) + 7.0 / 12 * ui_sin(12 * t));
    case GOOME_EASE_BOUNCE:
        if (t >= 1) return 1;
        if (t < 1 / 2.75) return 7.5625 * t * t;
        if (t < 2 / 2.75) return t -= 1.5 / 2.75, 7.5625 * t * t + 0.75;
        if (t < 2.5 / 2.75) return t -= 2.25 / 2.75, 7.5625 * t * t + 0.9375;
        return t -= 2.625 / 2.75, 7.5625 * t * t + 0.984375;
    }
    return t * t * (3 - 2 * t);
}
static double ui_ease(goome_ui *u, int e, double t)
{
    if (e == GOOME_EASE_CUSTOM && u->cfg.ease) return u->cfg.ease(ui_clamp(t, 0, 1), u->cfg.ease_user);
    return goome_ease(e, t);
}
static double ui_ez(goome_ui *u, double t) { return ui_ease(u, (int)u->th.ease, t); }
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
    int n = 0, m = 0;
    for (int k = 0; k < u->nkeep; k++) {
        if (u->frame - u->keeps[k].seen > 36000) {
            free(u->keeps[k].v);
            continue;
        }
        u->keeps[m++] = u->keeps[k];
    }
    u->nkeep = m;
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

static double ui_ms(goome_ui *u, double v) { return v < 0 ? u->th.anim_ms : v; }
static void ui_step(goome_ui *u, double *v, double target, double ms)
{
    if (*v == target) return;
    double k = ms > 0 ? u->dt / ms : 1;
    if (*v < target) *v = ui_min(target, *v + k);
    else *v = ui_max(target, *v - k);
    if (*v != target) u->anim_next = 1;
}
static double ui_anim_ms(goome_ui *u, ui_st *s, int slot, int on, double ms)
{
    if (s->born == u->frame) s->v[slot] = on ? 1 : 0;
    ui_step(u, &s->v[slot], on ? 1 : 0, ms);
    return ui_ez(u, s->v[slot]);
}
static double ui_anim(goome_ui *u, ui_st *s, int slot, int on)
{
    return ui_anim_ms(u, s, slot, on, slot == 0 ? ui_ms(u, u->th.hover_ms) : slot == 1 ? ui_ms(u, u->th.press_ms) : u->th.anim_ms);
}
static ui_box ui_shb(goome_ui *u, ui_box b, double dy)
{
    b.x += dy * (u->th.shadow_x / 5), b.y += dy * (u->th.shadow_y / 5);
    return b;
}
static uint32_t ui_red(goome_ui *u, double held, uint32_t a, uint32_t b)
{
    uint32_t d = u->th.danger;
    return d ? ui_mix(d, ui_mix(d, 0xFF000000u, 0.2), held) : ui_mix(a, b, held);
}
static int ui_al(goome_ui *u, int def)
{
    int a = (int)u->th.align;
    return a >= GOOME_ALIGN_LEFT && a <= GOOME_ALIGN_RIGHT ? a : def;
}
static double ui_ax(int a, double x, double w, double tw, double in)
{
    return a == GOOME_ALIGN_LEFT ? x + in : a == GOOME_ALIGN_RIGHT ? x + ui_round(w - in - tw) : x + ui_round((w - tw) / 2);
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
static ui_cmd *ui_cmd_raw(ui_list *l, int type)
{
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

static int ui_txt_room(ui_list *l, int need)
{
    if (need <= l->tcap) return 1;
    int cap = l->tcap ? l->tcap : 4096;
    while (cap < need && cap < (1 << 29)) cap *= 2;
    if (cap < need) return 0;
    char *t = realloc(l->txt, (size_t)cap);
    if (!t) return 0;
    l->txt = t, l->tcap = cap;
    return 1;
}

static void ui_lay_flush(goome_ui *u)
{
    ui_list *l = u->lay_list;
    goome_canvas *k = &u->lay;
    u->lay_list = NULL;
    int x0 = k->dx0 < 0 ? 0 : k->dx0, y0 = k->dy0 < 0 ? 0 : k->dy0, x1 = k->dx1 > k->w ? k->w : k->dx1, y1 = k->dy1 > k->h ? k->h : k->dy1;
    k->dx0 = k->dy0 = k->dx1 = k->dy1 = 0;
    if (!k->px || x1 <= x0 || y1 <= y0) return;
    int w = x1 - x0, h = y1 - y0, at = l ? (l->tn + 3) & ~3 : 0;
    int ok = l && (double)w * h * 4 < (double)(1 << 28) && ui_txt_room(l, at + w * h * 4);
    ui_cmd *c = ok ? ui_cmd_raw(l, UC_LAYER) : NULL;
    for (int y = 0; y < h; y++) {
        uint32_t *row = k->px + (size_t)(y0 + y) * k->stride + x0;
        if (c) memcpy(l->txt + at + (size_t)y * w * 4, row, (size_t)w * 4);
        memset(row, 0, (size_t)w * 4);
    }
    if (!c) return;
    c->a[0] = k->ox + x0 / k->scale, c->a[1] = k->oy + y0 / k->scale, c->a[2] = u->lay_alpha, c->a[3] = h;
    c->flag = w, c->text = at;
    l->tn = at + w * h * 4;
}

static ui_cmd *ui_cmd_new(goome_ui *u, int type)
{
    if (u->lay_list) ui_lay_flush(u);
    return ui_cmd_raw(u->cl, type);
}

static int ui_n(goome_ui *u)
{
    if (u->lay_list) ui_lay_flush(u);
    return u->cl->n;
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
static void ui_rline(goome_ui *u, ui_box b, goome_radii r, double w, uint32_t col) { ui_put(u, UC_LINE, b.x, b.y, b.w, b.h, r, col, 0, w, 0); }
static goome_radii ui_rgrow(goome_radii r, double d)
{
    r.tl = ui_max(0, r.tl + d), r.tr = ui_max(0, r.tr + d), r.br = ui_max(0, r.br + d), r.bl = ui_max(0, r.bl + d);
    return r;
}
static goome_radii ui_tr(goome_ui *u, double r) { return u->rad_on ? ui_rgrow(u->rad, r - u->th.radius) : goome_radius(r); }
static goome_radii ui_trc(goome_ui *u, double r, double most)
{
    goome_radii k = ui_tr(u, r);
    k.tl = ui_min(k.tl, most), k.tr = ui_min(k.tr, most), k.br = ui_min(k.br, most), k.bl = ui_min(k.bl, most);
    return k;
}
static double ui_rmid(goome_radii r) { return (r.tl + r.tr + r.br + r.bl) / 4; }
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
static void ui_bitmap(goome_ui *u, const goome_bitmap *b, ui_box d, goome_radii r, uint32_t tint)
{
    if (!b || !b->px || b->w <= 0 || b->h <= 0) return;
    ui_put(u, UC_BITMAP, d.x, d.y, d.w, d.h, r, tint, 0, 0, 0);
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
    if (u->lay_list) ui_lay_flush(u);
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
        case UC_ARC: goome_draw_arc(c, a[0], a[1], a[2], a[3], a[4], a[5], k->c0); break;
        case UC_POLY: {
            double tmp[256], *pt = k->flag * 2 <= 256 ? tmp : malloc(sizeof(double) * 2 * (size_t)k->flag);
            if (!pt) break;
            memcpy(pt, l->txt + k->text, sizeof(double) * 2 * (size_t)k->flag);
            goome_draw_poly(c, pt, k->flag, k->c0);
            if (pt != tmp) free(pt);
            break;
        }
        case UC_NINE: goome_draw_bitmap_nine(c, k->bm, a[0], a[1], a[2], a[3], k->r.tl, k->r.tr, k->r.br, k->r.bl); break;
        case UC_LAYER: {
            goome_canvas s;
            memset(&s, 0, sizeof(s));
            s.px = (uint32_t *)(void *)(l->txt + k->text), s.w = s.stride = s.dx1 = k->flag, s.h = s.dy1 = (int)a[3];
            s.ox = a[0], s.oy = a[1], s.scale = c->scale;
            goome_canvas_blit(c, &s, a[2], 0, 0);
            break;
        }
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
    l->x0 = x0, l->y0 = y0, l->w = w, l->ny = y0, l->line_y = y0, l->max_y = y0, l->max_x = x0;
}

static ui_box ui_item(goome_ui *u, double want, double h)
{
    ui_lay *l = ui_lay_cur(u);
    double gap = l->tight ? l->gap : u->th.gap, x, y, w;
    if (u->lay_list) ui_lay_flush(u);
    u->sized = u->size_h > 0;
    if (u->size_h > 0) h = u->size_h;
    if (u->size_w > 0) want = u->size_w, l->has_next_w = 1, l->next_w = u->size_w;
    u->size_w = u->size_h = 0;
    if (u->pos_on) {
        u->pos_on = 0;
        x = l->x0 + u->pos_x, y = l->y0 + u->pos_y;
        w = l->has_next_w && l->next_w > 0 ? l->next_w : want > 0 ? want : ui_max(0, l->x0 + l->w - x);
        l->has_next_w = 0;
        if (x + w > l->max_x) l->max_x = x + w;
        if (y + h > l->max_y) l->max_y = y + h;
        ui_box b = ui_bx(ui_round(x), ui_round(y), ui_round(w), ui_round(h));
        u->last_box = b, u->last_hover = 0, u->last_id = 0;
        return b;
    }
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
    if (want > 0 && x + want > l->max_x) l->max_x = x + want;
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

static void ui_grabby(goome_ui *u, uint32_t id)
{
    if (id && u->active == id && (u->down || u->pressed)) u->grab_frame = u->frame;
}

static void ui_grab_set(goome_ui *u, int on)
{
    if (u->grab == on) return;
    u->grab = on;
    goome_grab(u->g, on);
}

// a click is down and up on it
static int ui_button_logic(goome_ui *u, uint32_t id, ui_box b, int *hov, int *held)
{
    int h = ui_hover(u, id, b);
    if (h && u->cfg.cursor[GOOME_CUR_BUTTON] != GOOME_CURSOR_ARROW) u->cur_want = u->cfg.cursor[GOOME_CUR_BUTTON];
    if (h && u->pressed && !u->active) u->active = id;
    int click = h && u->released && u->active == id;
    if (u->nav_act && id && u->focus == id) click = 1, u->nav_act = 0;
    if (ui_ax_do(u, id, 'p')) click = 1;
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
    on = on && (u->auto_grab & GOOME_WANT_KEYBOARD);
    if (u->kb == on) return;
    u->kb = on;
    goome_keyboard(u->g, on);
}
static void ui_unfocus(goome_ui *u)
{
    u->focus = 0, u->focus_vis = 0, u->focus_kind = UN_PLAIN;
    ui_keyboard(u, 0);
}
static void ui_focus_to(goome_ui *u, uint32_t id, int vis, int kind)
{
    u->focus = id, u->focus_vis = vis, u->focus_kind = kind;
    ui_keyboard(u, 1);
}

static int ui_plain(int kind) { return kind < UN_INPUT || kind == UN_CUSTOM; }

static int ui_nav(goome_ui *u, uint32_t id, ui_box b, goome_radii r, int kind, int *rv, int value)
{
    int scope = u->nav_layer ? u->layer == u->nav_layer : u->pdepth == 0, got = 0;
    if (!scope) {
        if (u->focus == id) ui_unfocus(u);
        return 0;
    }
    if (u->want_focus) u->want_focus = 0, ui_focus_to(u, id, ui_plain(kind), kind), u->tab_came = !ui_plain(kind);
    if (ui_ax_do(u, id, 'f')) ui_focus_to(u, id, 1, kind), u->tab_came = !ui_plain(kind);
    if (u->tab_req > 0 && u->tab_take) {
        u->tab_req = 0, u->tab_take = 0;
        if (!u->rad_only) u->tab_new = id, u->tab_kind = kind;
        else if (kind == UN_RADIO && rv && (const void *)rv == u->focus_ptr) u->tab_new = id, u->tab_kind = kind, *rv = value, got = 2;
    } else if (u->tab_req > 0 && u->focus == id)
        u->tab_take = 1;
    else if (u->tab_req < 0 && u->focus == id) {
        if (u->rad_only) {
            if (u->nav_prev && u->nav_prev_kind == UN_RADIO && rv && u->nav_prev_ptr == (const void *)rv) u->tab_new = u->nav_prev, u->tab_kind = UN_RADIO, *rv = u->nav_prev_val;
            u->tab_req = 0;
        } else if (u->nav_prev)
            u->tab_new = u->nav_prev, u->tab_kind = u->nav_prev_kind, u->tab_req = 0;
    }
    if (u->cfg.focus_on_mouse && ui_plain(kind) && u->pressed && !u->active && u->hover_layer == u->layer && ui_in(b, u->mx, u->my) && (!u->nclip || ui_in(u->clip[u->nclip - 1], u->mx, u->my)))
        ui_focus_to(u, id, 1, kind);
    if (!u->nav_first) u->nav_first = id, u->nav_first_kind = kind;
    u->nav_last = id, u->nav_prev = id, u->nav_prev_kind = kind, u->nav_prev_ptr = rv, u->nav_prev_val = value;
    if (u->focus != id) return got;
    u->focus_seen = 1, u->focus_kind = kind, u->focus_ptr = rv;
    if (ui_plain(kind) && u->pressed && !ui_in(b, u->mx, u->my)) {
        ui_unfocus(u);
        return got;
    }
    double fg = u->th.focus_gap;
    if (u->focus_vis && ui_plain(kind)) ui_rline(u, ui_bx(b.x - fg, b.y - fg, b.w + 2 * fg, b.h + 2 * fg), ui_rgrow(r, fg), u->th.focus_w, ui_c(u->th.focus, u->th.accent));
    return got | 1;
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
    u->fixed_time = b && !strncmp(b, "test", 4) && (!b[4] || b[4] == ' ');
    u->global_ptr = (goome_caps(g) & GOOME_CAP_GLOBAL_POINTER) != 0;
    u->nmon = goome_monitors(g, u->mons, GM_MAX_MON);
    u->mx = u->my = -100000;
    u->alpha = 1;
    u->anim = 1;
    u->mb_want = -1;
    u->auto_grab = GOOME_WANT_MOUSE | GOOME_WANT_KEYBOARD;
    goome_ui_config_default(&u->cfg);
    goome_ui_a11y(u, goome_mode(g) == GOOME_MODE_WINDOW);
    return u;
}

static void ui_ed_free_all(goome_ui *u);

void goome_ui_free(goome_ui *u)
{
    if (!u) return;
    if (u->kb) goome_keyboard(u->g, 0);
    if (u->grab) goome_grab(u->g, 0);
    for (int i = 0; i < UI_MAXWIN; i++) ui_list_free(&u->wins[i].list);
    ui_list_free(&u->root), ui_list_free(&u->pop), ui_list_free(&u->tip), ui_list_free(&u->tl);
    for (int i = 0; i < UI_MAXPOP; i++) ui_list_free(&u->plist[i]);
    for (int i = 0; i < UI_NBLK; i++) free(u->blk[i]);
    free(u->undo);
    if (u->ime_on) goome_text_input(u->g, 0, NULL);
    free(u->ime_buf), free(u->ime_pre);
    free(u->idx);
    ui_ed_free_all(u);
    free(u->dd_data);
    free(u->sty);
    free(u->lay_px);
    ui_ax_free(u);
    for (int i = 0; i < u->nkeep; i++) free(u->keeps[i].v);
    free(u->keeps);
    goome_font_free(u->mono);
    goome_font_free(u->own);
    free(u);
}

goome_theme *goome_ui_theme(goome_ui *u) { return u ? &u->th : NULL; }

static void ui_theme_rest(goome_theme *t)
{
    t->focus = t->select = t->caret = t->link = t->badge = t->toast = t->tooltip = t->danger = t->knob = t->dim = 0;
    t->check_size = 18, t->check_radius = 5, t->toggle_w = 38, t->toggle_h = 22, t->slider_h = 22, t->slider_track = 6, t->knob_size = 8;
    t->scroll_w = 4, t->scroll_min = 28, t->item_h = t->menu_item_h = t->tab_h = t->win_title_h = t->app_title_h = t->menubar_h = t->statusbar_h = 0;
    t->focus_w = 2, t->focus_gap = 3, t->sep_w = 1, t->grip = 18, t->indent = 16, t->plot_w = 2, t->spinner = 0, t->caret_w = 0;
    t->win_min_w = 140, t->win_min_h = 50, t->pop_gap = 6, t->menu_gap = 3, t->tip_pad_x = 10, t->tip_pad_y = 7, t->tip_x = 14, t->tip_y = 20, t->tip_radius = 8;
    t->shadow_x = 0, t->shadow_y = 5, t->align = GOOME_ALIGN_AUTO, t->heading_size = 0, t->small_size = 0, t->ease = GOOME_EASE_SMOOTH;
    t->hover_ms = t->press_ms = t->popup_ms = t->collapse_ms = t->toast_fade_ms = -1;
}

void goome_ui_theme_dark(goome_theme *t)
{
    if (!t) return;
    t->bg = 0xFF14161A, t->panel = 0xEC1B1D22, t->border = 0x1CFFFFFF;
    t->text = 0xFFE9EBEE, t->text_dim = 0xFF969CA6;
    t->accent = 0xFF6FDC5F, t->accent_text = 0xFF0F1D0C;
    t->hover = 0xFF363A44, t->active = 0xFF424752, t->input_bg = 0xFF272A31, t->shadow = 0x8C000000;
    t->radius = 10, t->pad = 10, t->gap = 8, t->border_width = 1, t->font_size = 15, t->shadow_blur = 22, t->anim_ms = 120;
    ui_theme_rest(t);
}

void goome_ui_theme_light(goome_theme *t)
{
    if (!t) return;
    t->bg = 0xFFF4F5F7, t->panel = 0xF4FDFDFE, t->border = 0x24000000;
    t->text = 0xFF1C1F24, t->text_dim = 0xFF656C77;
    t->accent = 0xFF45B934, t->accent_text = 0xFFFFFFFF;
    t->hover = 0xFFDCE0E5, t->active = 0xFFCDD2D9, t->input_bg = 0xFFE9ECF0, t->shadow = 0x40000000;
    t->radius = 10, t->pad = 10, t->gap = 8, t->border_width = 1, t->font_size = 15, t->shadow_blur = 22, t->anim_ms = 120;
    ui_theme_rest(t);
}

static void ui_sty_apply(goome_ui *u)
{
    goome_theme *t = &u->th;
    if (t->font_size <= 0) t->font_size = 15;
    if (!(t->check_size > 0)) t->check_size = 18;
    if (!(t->toggle_w > 0)) t->toggle_w = 38;
    if (!(t->toggle_h > 0)) t->toggle_h = 22;
    if (!(t->slider_h > 0)) t->slider_h = 22;
    if (!(t->slider_track > 0)) t->slider_track = 6;
    if (!(t->knob_size > 0)) t->knob_size = 8;
    if (!(t->scroll_w > 0)) t->scroll_w = 4;
    if (!(t->scroll_min > 0)) t->scroll_min = 28;
    if (!(t->grip > 0)) t->grip = 18;
    u->size = t->font_size;
    u->lh = goome_text_line_height(t->font, u->size);
    u->em = ui_max(1, u->size / 15);
    u->fh = t->item_h > 0 ? ui_round(t->item_h) : ui_round(u->lh + ui_max(6, t->pad * 1.2));
}

static int ui_over(goome_ui *u, double x, double y)
{
    if (u->popup && ui_in(u->pop_prev, x, y)) return 1;
    if (u->app_was && ui_in(u->app_box, x, y)) return 1;
    for (int i = 0; i < u->npop; i++)
        if (ui_in(u->ps[i].prev, x, y)) return 1;
    for (int i = 0; i < u->ntoast; i++)
        if (ui_in(u->toasts[i].box, x, y)) return 1;
    for (int i = 0; i < UI_MAXWIN; i++)
        if (u->wins[i].id && u->frame - u->wins[i].seen <= 2 && ui_in(u->wins[i].prev, x, y)) return 1;
    return 0;
}


static void ui_pop_cut(goome_ui *u, int n)
{
    if (n < 0) n = 0;
    if (n < u->npop) u->npop = n;
}

static int ui_toast_at(goome_ui *u, double x, double y)
{
    for (int i = 0; i < u->ntoast; i++)
        if (ui_in(u->toasts[i].box, x, y)) return i + 1;
    return 0;
}

static uint32_t ui_stack_hover(goome_ui *u)
{
    for (int i = u->npop - 1; i >= 0; i--) {
        if (ui_in(u->ps[i].prev, u->mx, u->my)) return u->ps[i].id;
        if (u->ps[i].kind == UP_MODAL) return UI_BLOCK;
    }
    return 0;
}

static int ui_in_bar(goome_ui *u, double x, double y)
{
    for (int i = 0; i < u->nmbs_prev; i++)
        if (ui_in(u->mbs_prev[i], x, y)) return 1;
    return 0;
}

static void ui_close_outside(goome_ui *u, int left)
{
    int top = -1, keep = 0;
    for (int i = u->npop - 1; i >= 0; i--) {
        if (ui_in(u->ps[i].prev, u->mx, u->my)) {
            top = i;
            break;
        }
        if (u->ps[i].kind == UP_MODAL) {
            keep = i + 1;
            break;
        }
    }
    if (top >= 0) keep = top + 1;
    if (keep >= u->npop) return;
    if (left && top < 0 && ui_in_bar(u, u->mx, u->my)) return;
    ui_pop_cut(u, keep);
    if (left && top < 0) u->pressed = 0, u->released = 0;
}

static void ui_keys_pre(goome_ui *u)
{
    u->nav_act = u->nav_dx = u->nav_home = 0;
    for (int k = 0; k < u->nkeys; k++) {
        ui_key *e = &u->keys[k];
        int key = e->key, m = e->mods, used = 1;
        ui_pop *t = u->npop ? &u->ps[u->npop - 1] : NULL;
        if (key == GOOME_K_ESC && !(m & (GOOME_CTRL | GOOME_ALT | GOOME_SUPER))) {
            int esc = u->cfg.esc_closes;
            if (u->popup && esc) u->popup = 0;
            else if (t && esc && t->kind == UP_MODAL) t->close_req = 1;
            else if (t && esc) ui_pop_cut(u, u->npop - 1);
            else if (u->focus && u->focus_kind < UN_INPUT) ui_unfocus(u);
            else used = 0;
        } else if (key == GOOME_K_TAB && u->focus_kind == UN_EDIT && u->focus && !(m & (GOOME_CTRL | GOOME_ALT | GOOME_SUPER)))
            used = 0;
        else if (key == GOOME_K_TAB && u->cfg.tab_nav && !(m & (GOOME_ALT | GOOME_SUPER)) && (!(m & GOOME_CTRL) || (u->focus && u->focus_kind == UN_EDIT)))
            u->tab_req = (m & GOOME_SHIFT) ? -1 : 1, u->rad_only = 0;
        else if (t && t->kind == UP_MENU && !u->popup) {
            int n = t->nitems;
            if (key == GOOME_K_DOWN) t->sel = n ? (t->sel < 0 ? 0 : (t->sel + 1) % n) : -1;
            else if (key == GOOME_K_UP) t->sel = n ? (t->sel <= 0 ? n - 1 : t->sel - 1) : -1;
            else if (key == GOOME_K_ENTER || key == GOOME_K_SPACE) t->fire = t->sel;
            else if (key == GOOME_K_RIGHT) t->sub = t->sel < 0 ? -2 : t->sel;
            else if (key == GOOME_K_LEFT) {
                if (u->npop >= 2 && u->ps[u->npop - 2].kind == UP_MENU && !t->bar) ui_pop_cut(u, u->npop - 1);
                else if (t->bar) u->mb_shift = -1;
            } else
                used = 0;
        } else if (u->focus && u->focus_kind < UN_INPUT) {
            int d = key == GOOME_K_RIGHT || key == GOOME_K_DOWN ? 1 : key == GOOME_K_LEFT || key == GOOME_K_UP ? -1 : 0;
            if (d && u->focus_kind == UN_RADIO) u->tab_req = d, u->rad_only = 1;
            else if (d) u->nav_dx += d;
            else if (key == GOOME_K_ENTER || key == GOOME_K_SPACE) u->nav_act = 1;
            else if (key == GOOME_K_HOME) u->nav_home = -1;
            else if (key == GOOME_K_END) u->nav_home = 1;
            else used = 0;
        } else
            used = 0;
        if (used) e->key = 0, e->text[0] = 0;
    }
}

static int ui_ime_feed(goome_ui *u, const goome_event *e);
static void ui_ime_sync(goome_ui *u);

int goome_ui_feed(goome_ui *u, const goome_event *e)
{
    if (!u || !e) return 0;
    switch (e->type) {
    case GOOME_TEXT:
    case GOOME_PREEDIT: return ui_ime_feed(u, e);
    case GOOME_MOTION:
        u->mx = e->x, u->my = e->y, u->got_pos = 1;
        return u->active || ui_over(u, e->x, e->y);
    case GOOME_DOWN:
        if (e->button == 3) u->p_rpressed = 1, u->pmx = e->x, u->pmy = e->y;
        if (e->button > 1) return ui_over(u, e->x, e->y);
        u->mx = u->pressx = e->x, u->my = u->pressy = e->y, u->got_pos = 1;
        u->down = 1, u->p_pressed = 1, u->p_npress++, u->p_mods = e->mods;
        u->press_ui = ui_over(u, e->x, e->y) || goome_hit(u->g, e->x, e->y);
        return ui_over(u, e->x, e->y);
    case GOOME_UP:
        if (e->button > 1) return ui_over(u, e->x, e->y);
        if (!u->global_ptr) u->mx = e->x, u->my = e->y;
        u->down = 0, u->p_released = 1;
        ui_grab_set(u, 0);
        return u->active || ui_over(u, e->x, e->y);
    case GOOME_SCROLL:
        u->mx = e->x, u->my = e->y, u->got_pos = 1;
        u->p_wheel += e->dy;
        return ui_over(u, e->x, e->y);
    case GOOME_KEY: {
        int want = u->focus || u->npop || u->popup || u->app_was || e->key == GOOME_K_TAB;
        if (!want || u->nkeys >= UI_MAXKEY) return want;
        u->keys[u->nkeys].key = e->key, u->keys[u->nkeys].mods = e->mods;
        memcpy(u->keys[u->nkeys].text, e->text, sizeof(e->text));
        u->keys[u->nkeys].text[7] = 0;
        u->nkeys++;
        return 1;
    }
    case GOOME_FOCUS:
        u->unfocused = e->id == 0;
        if (u->unfocused && u->grab) u->down = 0, u->p_released = 1, ui_grab_set(u, 0);
        return 0;
    case GOOME_MONITORS: u->nmon = goome_monitors(u->g, u->mons, GM_MAX_MON); return 0;
    case GOOME_CONTROL: return ui_ax_feed(u, e);
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
    int npress = u->p_npress;
    if (here) {
        u->pressed = u->p_pressed, u->released = u->p_released, u->wheel = u->p_wheel, u->rpressed = u->p_rpressed;
        u->p_pressed = u->p_released = u->p_wheel = u->p_rpressed = u->p_npress = 0;
    } else
        u->pressed = u->released = u->wheel = u->rpressed = 0;
    if (u->pressed) {
        int close_by = ui_abs(u->mx - u->omx) < u->cfg.dbl_click_px && ui_abs(u->my - u->omy) < u->cfg.dbl_click_px;
        u->clicks = npress >= 2 ? 2 : u->press_ms && u->now - u->press_ms < u->cfg.dbl_click_ms && close_by && u->clicks < 3 ? u->clicks + 1 : 1;
        u->press_ms = u->now;
        u->pmods = u->p_mods;
    }
    u->moved = u->mx != u->omx || u->my != u->omy;
    u->omx = u->mx, u->omy = u->my;

    u->lay_list = NULL, u->pos_on = 0, u->size_w = u->size_h = 0;
    ui_sty_apply(u);

    u->hover_layer = UI_ROOT;
    uint32_t sh = ui_stack_hover(u);
    int tk = ui_toast_at(u, u->mx, u->my);
    if (tk) u->hover_layer = UI_TOASTL;
    else if (u->popup && ui_in(u->pop_prev, u->mx, u->my)) u->hover_layer = UI_POPUP;
    else if (sh) u->hover_layer = sh;
    else {
        int best = -1;
        for (int i = 0; i < UI_MAXWIN; i++) {
            ui_win *w = &u->wins[i];
            if (w->id && u->frame - w->seen <= 2 && w->z > best && ui_in(w->prev, u->mx, u->my)) best = w->z, u->hover_layer = w->id;
        }
    }
    u->pop_closed = 0;
    if (u->pressed && u->popup && !ui_in(u->pop_prev, u->mx, u->my)) u->pop_closed = u->popup, u->popup = 0, u->pressed = 0, u->released = 0; // that click only closes it
    if (tk && u->pressed) {
        ui_toast *t = &u->toasts[tk - 1];
        if (t->ms > t->age + 200) t->ms = (int)t->age + 200;
        u->pressed = u->released = 0;
    }
    if (!tk && (u->pressed || u->rpressed) && u->npop) ui_close_outside(u, u->pressed);
    for (int i = 0; i < u->ntoast; i++) {
        ui_toast *t = &u->toasts[i];
        if (i != tk - 1) t->age += u->dt;
        if (t->age >= t->ms) memmove(t, t + 1, sizeof(*t) * (size_t)(u->ntoast - i - 1)), u->ntoast--, i--;
    }
    if (!u->mh_seen) u->mh_id = 0;
    u->mh_seen = 0;
    u->nav_layer = 0;
    for (int i = u->npop - 1; i >= 0; i--)
        if (u->ps[i].kind != UP_MENU) {
            u->nav_layer = u->ps[i].id;
            break;
        }
    ui_keys_pre(u);
    u->nav_first = u->nav_last = u->nav_prev = 0, u->tab_take = 0, u->nav_prev_ptr = NULL;
    u->cur_want = GOOME_CURSOR_ARROW;
    u->app_was = u->app_on, u->app_on = 0, u->app_mb_used = 0, u->pdepth = 0, u->mb_on = 0;
    memcpy(u->mbs_prev, u->mbs, sizeof(u->mbs));
    u->nmbs_prev = u->nmbs, u->nmbs = 0;
    for (int i = 0; i < UI_MAXPOP; i++) u->plist[i].n = u->plist[i].tn = 0;
    u->tl.n = u->tl.tn = 0;
    if (u->pressed && u->cfg.raise_on_click && u->hover_layer > UI_TIP)
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
    u->ntb = 0, u->ntree = 0, u->nsp = 0;
    memset(&u->fr[0], 0, sizeof(u->fr[0]));
    ui_lay_init(&u->fr[0].lay, a.x + u->th.pad, a.y + u->th.pad, a.w - 2 * u->th.pad);
    u->fr[0].box = ui_bx(a.x, a.y, a.w, a.h);
    u->page_bottom = a.y + a.h;
    u->nfr = 1;
    ui_ax_begin(u);
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

int goome_ui_wants(goome_ui *u)
{
    if (!u) return 0;
    int modal = 0;
    for (int i = 0; i < u->npop; i++)
        if (u->ps[i].kind == UP_MODAL) modal = 1;
    int m = u->grab || u->npop || u->popup || u->dd_on || (u->down && (u->active || u->press_ui)) || ui_over(u, u->mx, u->my) || goome_hit(u->g, u->mx, u->my);
    return (m ? GOOME_WANT_MOUSE : 0) | (u->focus || modal ? GOOME_WANT_KEYBOARD : 0);
}

void goome_ui_auto_grab(goome_ui *u, int wants)
{
    if (!u) return;
    u->auto_grab = wants & (GOOME_WANT_MOUSE | GOOME_WANT_KEYBOARD);
    if (!(u->auto_grab & GOOME_WANT_MOUSE)) ui_grab_set(u, 0);
    if (!(u->auto_grab & GOOME_WANT_KEYBOARD)) ui_keyboard(u, 0);
}

static void ui_pop_finish(goome_ui *u);
static void ui_toasts_draw(goome_ui *u);
static void ui_dd_end(goome_ui *u);
static int ui_skin(goome_ui *u, const goome_skin *k, ui_box b, double hov, double held);
static int ui_icon(goome_ui *u, int kind, ui_box b, uint32_t col);
static double ui_skin_r(const goome_skin *k, ui_box b);
static int ui_icon2(goome_ui *u, int a, int b, ui_box box, uint32_t col, double t);
static int ui_has_icon(goome_ui *u, int kind);

void goome_ui_end(goome_ui *u)
{
    if (!u || !u->c) return;
    if (u->lay_list) ui_lay_flush(u);
    if (u->nsty) {
        if (!u->sty_warn) gm_fail(u->g, GOOME_EARG, "ui style stack had %d pushes left at ui_end", u->nsty), u->sty_warn = 1;
        goome_ui_pop(u, u->nsty);
    }
    u->pos_on = 0, u->size_w = u->size_h = 0;
    while (u->nfr > 1) {
        if (u->fr[u->nfr - 1].dead) u->nfr--;
        else if (u->fr[u->nfr - 1].pop) ui_pop_finish(u);
        else goome_ui_window_end(u);
    }
    while (u->nscr > 0) ui_scroll_close(u);
    for (int i = 0; i < u->npop; i++)
        if (u->ps[i].seen != u->frame && u->ps[i].born != u->frame) {
            ui_pop_cut(u, i);
            break;
        }
    for (int i = 0; i < u->npop; i++) {
        ui_pop *p = &u->ps[i];
        p->prev = p->kind == UP_MODAL ? ui_bx(u->area.x, u->area.y, u->area.w, u->area.h) : p->seen == u->frame ? p->box : ui_bx(0, 0, 0, 0);
    }
    if (u->focus && !u->focus_seen) ui_unfocus(u);
    u->tab_came = 0;
    if (u->tab_req && !u->rad_only && !u->tab_new) u->tab_new = u->tab_req > 0 ? u->nav_first : u->nav_last, u->tab_kind = u->tab_req > 0 ? u->nav_first_kind : u->nav_prev_kind;
    if (u->tab_new) ui_focus_to(u, u->tab_new, 1, u->tab_kind), u->tab_came = 1;
    u->tab_new = 0, u->tab_req = 0, u->rad_only = 0, u->want_focus = 0;
    if (goome_mode(u->g) == GOOME_MODE_OVERLAY) ui_keyboard(u, u->focus || u->npop);
    ui_ime_sync(u);
    if (u->popup && !u->pop_seen) u->popup = 0;
    ui_grab_set(u, u->down && u->grab_frame > 0 && u->frame - u->grab_frame < (u->nmon > 1 ? u->nmon : 1) && (u->auto_grab & GOOME_WANT_MOUSE));
    if (!u->down) u->active = 0;
    if (u->focus && u->cfg.blink_ms > 0) u->anim_next = 1; // the cursor blinks

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
    for (int i = 0; i < u->npop; i++) {
        if (u->ps[i].seen != u->frame) continue;
        ui_play(u, &u->plist[i]);
        ui_hit_add(u, u->ps[i].prev);
    }
    ui_play(u, &u->pop);
    if (u->popup) ui_hit_add(u, u->pop_box);
    u->pop_prev = u->popup ? u->pop_box : ui_bx(0, 0, 0, 0);
    ui_toasts_draw(u);
    ui_play(u, &u->tl);
    for (int i = 0; i < u->ntoast; i++) ui_hit_add(u, u->toasts[i].box);
    ui_dd_end(u);
    ui_play(u, &u->tip);
    if (u->cur_want != u->cur_set) u->cur_set = u->cur_want, goome_cursor(u->g, u->cur_want);
    for (int i = 0; i < u->nextra && u->nhits < UI_MAXHIT; i++) u->hits[u->nhits++] = u->extra[i];
    ui_ax_end(u);
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
    ui_lay_init(lay, view.x + pad, view.y + pad - r->off, view.w - 2 * pad - (r->overflow ? u->th.scroll_w + 6 : 0));
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
        s->v[1] = ui_clamp(s->v[1] + u->wheel * u->cfg.scroll_lines * u->lh, 0, maxo);
        u->wheel = 0;
        u->anim_next = 1;
    }
    if (maxo <= 0) return;
    double sw = u->th.scroll_w;
    ui_box track = ui_bx(v.x + v.w - (sw + 8), v.y + 6, sw + 6, v.h - 12);
    double th = ui_clamp(track.h * v.h / content, ui_min(u->th.scroll_min, track.h), track.h);
    double ty = track.y + (track.h - th) * (r->off / maxo);
    uint32_t bid = ui_hashi(r->id, 1);
    ui_st *bs = ui_get(u, ui_hashi(r->id, 2), NULL);
    int hov = ui_hover(u, bid, track);
    if (hov && u->pressed && !u->active) {
        u->active = bid;
        u->grab_y = u->my >= ty && u->my < ty + th ? u->my - ty : th / 2;
    }
    int held = u->active == bid && (u->down || u->pressed);
    ui_grabby(u, bid);
    if (held && track.h > th) {
        double t = (u->my - u->grab_y - track.y) / (track.h - th);
        s->v[1] = s->v[0] = ui_clamp(t, 0, 1) * maxo;
        ty = track.y + (track.h - th) * ui_clamp(t, 0, 1);
    }
    double a = ui_anim(u, bs, 0, hov || held);
    double bw = sw + 2 * a;
    if (!ui_skin(u, &u->th.skin_scrollbar, ui_bx(track.x + track.w - bw - 1, ty, bw, th), a, held)) ui_rect(u, ui_bx(track.x + track.w - bw - 1, ty, bw, th), bw / 2, ui_fade(u->th.text, 0.2 + 0.25 * a));
}

int goome_ui_scroll_begin(goome_ui *u, const char *id, double h)
{
    if (!u || !u->c) return 0;
    int page = h <= 0;
    if (page) {
        ui_lay *l = ui_lay_cur(u);
        double bottom = u->page_bottom - u->th.pad;
        if (u->nscr > ui_top(u)->nscr0 || (u->nscr && ui_top(u)->w)) {
            ui_scr *r = &u->scr[u->nscr - 1];
            bottom = r->view.y + r->view.h - r->pad - r->off;
        }
        h = ui_max(60, bottom - (l->same && l->any ? l->line_y : l->ny));
    }
    ui_box b = ui_item(u, 0, h);
    if (!page) ui_rrect(u, b, ui_tr(u, ui_max(0, u->th.radius - 2)), ui_fade(u->th.input_bg, 0.55));
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

    double tbar = (flags & GOOME_WIN_NO_TITLE) ? 0 : th->win_title_h > 0 ? ui_round(th->win_title_h) : u->fh + 6;
    f->title_h = tbar;
    uint32_t mv = ui_hashi(id, 1), rs = ui_hashi(id, 2);
    goome_rect a = u->area;
    ui_grabby(u, mv), ui_grabby(u, rs);
    if (u->active == mv && u->down) {
        *x = ui_round(ui_clamp(u->mx - u->grab_x, a.x - *w + 60, a.x + a.w - 60));
        *y = ui_round(ui_clamp(u->my - u->grab_y, a.y, a.y + a.h - ui_max(tbar, 24)));
    }
    if (u->active == rs && u->down) {
        *w = ui_round(ui_max(th->win_min_w, u->mx + u->grab_x - *x));
        *h = ui_round(ui_max(tbar + th->win_min_h, u->my + u->grab_y - *y));
    }
    double ww = ui_max(*w, 80);
    int coll = wn->collapsed && (flags & GOOME_WIN_COLLAPSE) && tbar > 0;
    ui_step(u, &wn->open, coll ? 0 : 1, ui_ms(u, th->collapse_ms));
    double op = ui_ez(u, wn->open);
    double full = *h > 0 ? ui_max(*h, tbar) : tbar + wn->content;
    double vh = ui_round(tbar + (full - tbar) * op);
    ui_box box = ui_bx(*x, *y, ww, vh);
    f->box = box;
    ui_ax_window(u, id, box, t, flags, wn->collapsed);
    goome_radii rr = ui_tr(u, th->radius);

    f->bg = ui_n(u);
    ui_shadow(u, ui_shb(u, box, 5), ui_rmid(rr), th->shadow_blur, th->shadow);
    if (!ui_skin(u, &th->skin_panel, box, 0, 0)) ui_rrect(u, box, rr, th->panel);
    f->nbg = u->cl->n - f->bg;

    if (tbar > 0) {
        goome_radii tr = rr;
        if (vh > tbar + 1) tr.bl = tr.br = 0;
        if (!ui_skin(u, &th->skin_titlebar, ui_bx(box.x, box.y, ww, tbar), 0, 0)) {
            ui_rrect(u, ui_bx(box.x, box.y, ww, tbar), tr, ui_fade(th->text, 0.04));
            if (vh > tbar + 0.5) ui_rect(u, ui_bx(box.x, box.y + tbar - 1, ww, 1), 0, th->border);
        }
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
            uint32_t cc = ui_mix(th->text_dim, th->text, k);
            double is = ui_round(12 * u->em);
            if (!ui_icon2(u, GOOME_ICON_EXPAND, GOOME_ICON_COLLAPSE, ui_bx(cb.x + (cs - is) / 2, ui_round(cy - is / 2), is, is), cc, op))
                ui_chevron(u, cb.x + cs / 2, cy, op * 1.5707963267948966, 4.5 * u->em, cc);
            tx = cb.x + cb.w + 6;
        }
        if ((flags & GOOME_WIN_CLOSE) && open) {
            uint32_t xid = ui_hashi(id, 3);
            double xs = ui_round(18 * u->em);
            ui_box xb = ui_bx(right - xs, ui_round(cy - xs / 2), xs, xs);
            int hov, held;
            if (ui_button_logic(u, xid, xb, &hov, &held)) *open = 0;
            ui_ax_btn(u, xid, xb, "Close", 5);
            ui_st *s = ui_get(u, xid, NULL);
            double k = ui_anim(u, s, 0, hov || held);
            double ccx = xb.x + xs / 2, d = 3.6 * u->em;
            if (k > 0) ui_circle(u, ccx, cy, xs / 2, ui_fade(ui_red(u, held, 0xFFE5534B, 0xFFC93C35), k));
            uint32_t xc = ui_mix(th->text_dim, 0xFFFFFFFF, k);
            double is = ui_round(10 * u->em);
            if (!ui_icon(u, GOOME_ICON_CLOSE, ui_bx(xb.x + (xs - is) / 2, ui_round(cy - is / 2), is, is), xc)) {
                ui_seg(u, ccx - d, cy - d, ccx + d, cy + d, 1.6 * u->em, xc);
                ui_seg(u, ccx + d, cy - d, ccx - d, cy + d, 1.6 * u->em, xc);
            }
            right = xb.x - 6;
        }
        int n = ui_shown(t);
        ui_clip_push(u, ui_bx(tx, box.y, ui_max(0, right - tx), tbar));
        ui_text_mid(u, tx, cy, t, n, 0, th->text);
        ui_clip_pop(u);
    }

    if (coll && wn->open <= 0) {
        ui_ax_pop(u, UI_AXW, NULL);
        ui_rline(u, box, rr, th->border_width, th->border);
        wn->box = box;
        ui_leave(u, &f->sv);
        u->nid = f->nid_out;
        return 0;
    }
    f->dead = 0;
    ui_box body = ui_bx(box.x, box.y + tbar, ww, ui_max(0, vh - tbar));
    f->clipcmd = ui_n(u);
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
    double tbar = f->title_h, op = ui_ez(u, wn->open);
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
        double gs = th->grip, q = gs / 18;
        ui_box g = ui_bx(box.x + box.w - gs, box.y + box.h - gs, gs, gs);
        int hov, held;
        ui_button_logic(u, rs, g, &hov, &held);
        if (hov || held) u->cur_want = u->cfg.cursor[GOOME_CUR_GRIP];
        if (u->active == rs && u->pressed) {
            u->grab_x = *f->px + box.w - u->mx, u->grab_y = *f->py + box.h - u->my;
            if (*f->ph <= 0) *f->ph = box.h;
        }
        ui_st *s = ui_get(u, rs, NULL);
        double k = ui_anim(u, s, 0, hov || held);
        uint32_t col = ui_mix(ui_fade(th->text_dim, 0.5), th->accent, k);
        double ex = box.x + box.w - 5 * q, ey = box.y + box.h - 5 * q;
        if (!ui_icon(u, GOOME_ICON_RESIZE, ui_bx(ex - 11 * q, ey - 11 * q, 12 * q, 12 * q), col)) {
            ui_seg(u, ex - 9 * q, ey, ex, ey - 9 * q, 1.5 * q, col);
            ui_seg(u, ex - 4.5 * q, ey, ex, ey - 4.5 * q, 1.5 * q, col);
        }
    }
    if ((f->flags & GOOME_WIN_MOVE) && u->pressed && !u->active && u->hover_layer == wn->id && ui_in(box, u->mx, u->my)) {
        u->active = mv;
        u->grab_x = u->mx - *f->px, u->grab_y = u->my - *f->py;
    }
    if (!th->skin_panel.bm) ui_rline(u, box, ui_tr(u, th->radius), th->border_width, th->border);
    wn->box = box;
    ui_ax_pop(u, UI_AXW, &box);
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
    ui_box b = ui_item(u, 0, u->th.sep_w);
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
    int n = (int)strlen(s), al = ui_al(u, GOOME_ALIGN_LEFT);
    double tw = ui_tw(u, s, n, 0);
    ui_box b = ui_item(u, al == GOOME_ALIGN_LEFT ? tw : 0, u->lh);
    ui_hover(u, ui_auto(u), b);
    ui_ax_text(u, b, s, n, GOOME_ROLE_TEXT);
    ui_text_mid(u, ui_ax(al, b.x, b.w, tw, 0), b.y + b.h / 2, s, n, 0, u->th.text);
}

void goome_ui_heading(goome_ui *u, const char *text)
{
    if (!u || !u->c || !text) return;
    ui_lay *l = ui_lay_cur(u);
    if (l->any && !l->same && !l->row_cols && !u->pos_on) l->ny += ui_round(u->th.gap * 0.5);
    double sz = u->th.heading_size > 0 ? u->th.heading_size : ui_round(u->size * 1.3);
    int n = ui_shown(text), al = ui_al(u, GOOME_ALIGN_LEFT);
    goome_font *keep = u->th.font;
    if (u->th.font_heading) u->th.font = u->th.font_heading;
    double tw = ui_tw(u, text, n, sz);
    ui_box b = ui_item(u, al == GOOME_ALIGN_LEFT ? tw : 0, goome_text_line_height(u->th.font, sz));
    ui_hover(u, ui_auto(u), b);
    ui_ax_text(u, b, text, n, GOOME_ROLE_TEXT);
    ui_text_mid(u, ui_ax(al, b.x, b.w, tw, 0), b.y + b.h / 2, text, n, sz, u->th.text);
    u->th.font = keep;
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

static int ui_bd(goome_ui *u, const char *s, int n) { return u->th.font && n > 0 ? gm_text_rtl(u->th.font, s, n) : 0; }

static double ui_bd_x(const gm_tc *b, int nb, int i)
{
    if (nb <= 0) return 0;
    for (int k = 0; k < nb; k++)
        if (b[k].lo < i && i <= b[k].hi) return b[k].rtl ? b[k].x0 : b[k].x1;
    for (int k = 0; k < nb; k++)
        if (b[k].lo >= i) return b[k].rtl ? b[k].x1 : b[k].x0;
    return b[nb - 1].rtl ? b[nb - 1].x0 : b[nb - 1].x1;
}

static int ui_bd_hit(const gm_tc *b, int nb, double x)
{
    if (nb <= 0) return 0;
    int kl = 0, kr = 0;
    for (int k = 0; k < nb; k++) {
        if (x >= b[k].x0 && x < b[k].x1) return ((x < (b[k].x0 + b[k].x1) / 2) != (b[k].rtl != 0)) ? b[k].lo : b[k].hi;
        if (b[k].x0 < b[kl].x0) kl = k;
        if (b[k].x1 > b[kr].x1) kr = k;
    }
    if (x < b[kl].x0) return b[kl].rtl ? b[kl].hi : b[kl].lo;
    return b[kr].rtl ? b[kr].lo : b[kr].hi;
}

static int ui_bd_move(const gm_tc *b, int nb, int i, int right)
{
    double cx = ui_bd_x(b, nb, i), bx = 0;
    int best = -1;
    for (int k = -1; k < nb; k++) {
        int c = k < 0 ? 0 : b[k].hi;
        double x = ui_bd_x(b, nb, c);
        if (right ? x > cx + 0.01 && (best < 0 || x < bx) : x < cx - 0.01 && (best < 0 || x > bx)) best = c, bx = x;
    }
    return best;
}

static int ui_bd_lay(goome_ui *u, const char *s, int n, gm_tc **b) { return gm_text_layout(u->th.font, u->size, s, n, b); }

static void ui_bd_sel(goome_ui *u, const gm_tc *b, int nb, int lo, int hi, double ox, double y, double h, double r, uint32_t col, double extra)
{
    double *sp = malloc(sizeof(double) * 2 * (size_t)(nb + 1));
    int n = 0;
    if (!sp) return;
    for (int k = 0; k < nb; k++) {
        if (b[k].lo < lo || b[k].hi > hi || b[k].x1 <= b[k].x0) continue;
        int j = n++;
        while (j > 0 && sp[(j - 1) * 2] > b[k].x0) sp[j * 2] = sp[(j - 1) * 2], sp[j * 2 + 1] = sp[(j - 1) * 2 + 1], j--;
        sp[j * 2] = b[k].x0, sp[j * 2 + 1] = b[k].x1;
    }
    if (extra > 0) {
        double e = nb ? b[0].x1 : 0;
        for (int k = 0; k < nb; k++) e = ui_max(e, b[k].x1);
        sp[n * 2] = e, sp[n * 2 + 1] = e + extra, n++;
    }
    for (int k = 0; k < n;) {
        double a = sp[k * 2], z = sp[k * 2 + 1];
        int j = k + 1;
        while (j < n && sp[j * 2] <= z + 0.01) z = ui_max(z, sp[j * 2 + 1]), j++;
        ui_rect(u, ui_bx(ox + a, y, ui_max(1, z - a), h), r, col);
        k = j;
    }
    free(sp);
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
    if (u->size_w > 0) return u->size_w;
    if (l->has_next_w && l->next_w > 0) return l->next_w;
    if (u->pos_on) return ui_max(0, l->w - u->pos_x);
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
    ui_ax_text(u, b, text, (int)strlen(text), GOOME_ROLE_TEXT);
    int al = ui_al(u, GOOME_ALIGN_LEFT);
    for (int i = 0; i < n; i++) ui_text_mid(u, al == GOOME_ALIGN_LEFT ? b.x : ui_ax(al, b.x, b.w, ui_tw(u, text + st[i], ln[i], 0), 0), b.y + u->lh * (i + 0.5), text + st[i], ln[i], 0, u->th.text_dim);
}

int goome_ui_button(goome_ui *u, const char *label)
{
    if (!u || !u->c) return 0;
    const goome_theme *th = &u->th;
    int n = ui_shown(label);
    double tw = ui_tw(u, label, n, 0);
    ui_box b = ui_item(u, ui_round(tw + th->pad * 2.6), u->fh);
    uint32_t id = ui_id(u, label);
    ui_nav(u, id, b, ui_tr(u, th->radius), UN_PLAIN, NULL, 0);
    int hov, held, click = ui_button_logic(u, id, b, &hov, &held);
    ui_ax_btn(u, id, b, label, n);
    ui_st *s = ui_get(u, id, NULL);
    uint32_t bg = ui_btn_bg(u, s, hov, held);
    if (!ui_skin(u, &th->skin_button, b, ui_ez(u, s->v[0]), ui_ez(u, s->v[1]))) {
        goome_radii r = ui_tr(u, th->radius);
        ui_rrect(u, b, r, bg);
        ui_rline(u, b, r, th->border_width, th->border);
    }
    ui_clip_push(u, b);
    ui_text_mid(u, ui_ax(ui_al(u, GOOME_ALIGN_CENTER), b.x, b.w, tw, ui_round(th->pad * 1.3)), b.y + b.h / 2, label, n, 0, th->text);
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

static void ui_img_box(goome_ui *u, const goome_bitmap *b, ui_box d, goome_radii r)
{
    if (b && b->px) ui_bitmap(u, b, d, r, 0xFFFFFFFF);
    else ui_rrect(u, d, r, ui_fade(u->th.text_dim, 0.25));
}

void goome_ui_image(goome_ui *u, const goome_bitmap *b, double w, double h, double radius)
{
    if (!u || !u->c) return;
    ui_box sz = ui_fit(b, w, h, ui_avail(u));
    ui_box d = ui_item(u, sz.w, sz.h);
    ui_hover(u, ui_auto(u), d);
    ui_ax_text(u, d, NULL, 0, GOOME_ROLE_IMAGE);
    ui_img_box(u, b, d, goome_radius(radius));
}

int goome_ui_image_button(goome_ui *u, const char *id, const goome_bitmap *b, double w, double h)
{
    if (!u || !u->c) return 0;
    const goome_theme *th = &u->th;
    ui_box sz = ui_fit(b, w, h, ui_avail(u) - 12);
    ui_box d = ui_item(u, sz.w + 12, sz.h + 12);
    uint32_t hid = ui_id(u, id);
    ui_nav(u, hid, d, ui_tr(u, th->radius), UN_PLAIN, NULL, 0);
    int hov, held, click = ui_button_logic(u, hid, d, &hov, &held);
    ui_ax_btn(u, hid, d, id, ui_shown(id));
    ui_st *s = ui_get(u, hid, NULL);
    goome_radii rr = ui_tr(u, th->radius);
    ui_rrect(u, d, rr, ui_btn_bg(u, s, hov, held));
    double p = ui_ez(u, s->v[1]);
    ui_rline(u, d, rr, th->border_width, ui_mix(th->border, th->accent, ui_ez(u, s->v[0]) * 0.8));
    double in = 6 + p;
    ui_img_box(u, b, ui_bx(d.x + in, d.y + in, d.w - 2 * in, d.h - 2 * in), ui_tr(u, ui_max(0, th->radius - 4)));
    return click;
}

int goome_ui_checkbox(goome_ui *u, const char *label, int *v)
{
    if (!u || !u->c || !v) return 0;
    const goome_theme *th = &u->th;
    int n = ui_shown(label);
    double tw = ui_tw(u, label, n, 0), sz = ui_round(th->check_size * u->em), q = sz / 18, cr = th->check_radius;
    ui_box b = ui_item(u, sz + (n ? 8 + tw : 0), u->fh - 6);
    uint32_t id = ui_id(u, label);
    ui_nav(u, id, b, goome_radius(cr + 1), UN_PLAIN, NULL, 0);
    int hov, held, ch = 0;
    if (ui_button_logic(u, id, b, &hov, &held)) *v = !*v, ch = 1;
    ui_ax_check(u, id, b, label, n, GOOME_ROLE_CHECKBOX, *v);
    ui_st *s = ui_get(u, id, NULL);
    double h = ui_anim(u, s, 0, hov || held), k = ui_anim(u, s, 2, *v);
    double cy = b.y + b.h / 2;
    ui_box cb = ui_bx(b.x, ui_round(cy - sz / 2), sz, sz);
    int sk = ui_skin(u, &th->skin_checkbox, cb, h, 0);
    if (!sk) {
        ui_rect(u, cb, cr, ui_mix(ui_mix(th->input_bg, th->hover, h), th->accent, k));
        if (k < 1) ui_outline(u, cb, cr, 1.5, ui_fade(ui_mix(th->text_dim, th->text, h * 0.6), 0.75 * (1 - k)));
    }
    if (k > 0) {
        uint32_t c = ui_fade(sk ? th->accent : th->accent_text, k);
        double x = cb.x, y = cb.y, d = 1 - k;
        if (!ui_icon(u, GOOME_ICON_CHECK, ui_bx(x + 3 * q, y + 3 * q, sz - 6 * q, sz - 6 * q), c)) {
            ui_seg(u, x + 4.5 * q, y + (9.5 + d) * q, x + 7.8 * q, y + 12.8 * q, 2 * q, c);
            ui_seg(u, x + 7.8 * q, y + 12.8 * q, x + (13.6 - d * 3) * q, y + (6 + d * 3) * q, 2 * q, c);
        }
    }
    if (n) ui_text_mid(u, cb.x + sz + 8, cy, label, n, 0, th->text);
    return ch;
}

int goome_ui_toggle(goome_ui *u, const char *label, int *v)
{
    if (!u || !u->c || !v) return 0;
    const goome_theme *th = &u->th;
    int n = ui_shown(label);
    double sw = ui_round(th->toggle_w * u->em), sh = ui_round(th->toggle_h * u->em), tw = ui_tw(u, label, n, 0);
    ui_box b = ui_item(u, 0, u->fh - 4);
    if (b.w < sw) b.w = sw;
    uint32_t id = ui_id(u, label);
    ui_nav(u, id, b, goome_radius(sh / 2), UN_PLAIN, NULL, 0);
    int hov, held, ch = 0;
    if (ui_button_logic(u, id, b, &hov, &held)) *v = !*v, ch = 1;
    ui_ax_check(u, id, b, label, n, GOOME_ROLE_CHECKBOX, *v);
    ui_st *s = ui_get(u, id, NULL);
    double h = ui_anim(u, s, 0, hov || held), p = ui_anim(u, s, 1, held), k = ui_anim(u, s, 2, *v);
    double cy = b.y + b.h / 2;
    ui_box tb = ui_bx(b.x + b.w - sw, ui_round(cy - sh / 2), sw, sh);
    if (ui_skin(u, &th->skin_toggle, tb, h, 0)) {
        if (k > 0) ui_rect(u, tb, ui_skin_r(&th->skin_toggle, tb), ui_fade(th->accent, 0.85 * k));
    } else {
        ui_rect(u, tb, sh / 2, ui_mix(ui_mix(th->input_bg, th->hover, h), th->accent, k));
        if (k < 1) ui_outline(u, tb, sh / 2, 1, ui_fade(th->text_dim, 0.35 * (1 - k)));
    }
    double r = sh / 2 - 3 * u->em, stretch = 5 * p * u->em, kx = tb.x + 3 * u->em + (sw - 6 * u->em - 2 * r - stretch) * k;
    uint32_t kw = ui_c(th->knob, 0xFFFFFFFF), ko = th->knob ? ui_mix(th->knob, th->text_dim, 0.3) : 0xFFB9BEC7;
    uint32_t kc = ui_mix(ui_mix(ko, kw, h * 0.5), kw, k);
    if (!ui_icon(u, GOOME_ICON_TOGGLE_KNOB, ui_bx(kx + stretch / 2, cy - r, 2 * r, 2 * r), kc)) {
        ui_shadow(u, ui_bx(kx, cy - r + 1, 2 * r + stretch, 2 * r), r, 4, 0x40000000);
        ui_rect(u, ui_bx(kx, cy - r, 2 * r + stretch, 2 * r), r, kc);
    }
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
    double tw = ui_tw(u, label, n, 0), sz = ui_round(th->check_size * u->em), ck = th->check_size / 18;
    ui_box b = ui_item(u, sz + (n ? 8 + tw : 0), u->fh - 6);
    uint32_t id = ui_id(u, label);
    int hov, held, ch = (ui_nav(u, id, b, goome_radius(sz / 2), UN_RADIO, v, value) & 2) != 0;
    if (ui_button_logic(u, id, b, &hov, &held) && *v != value) *v = value, ch = 1;
    ui_ax_check(u, id, b, label, n, GOOME_ROLE_RADIO, *v == value);
    ui_st *s = ui_get(u, id, NULL);
    double h = ui_anim(u, s, 0, hov || held), k = ui_anim(u, s, 2, *v == value);
    double cy = b.y + b.h / 2, cx = b.x + sz / 2;
    ui_box rb = ui_bx(b.x, ui_round(cy - sz / 2), sz, sz);
    int sk = ui_skin(u, &th->skin_radio, rb, h, 0);
    if (!sk) {
        ui_circle(u, cx, cy, sz / 2, ui_mix(ui_mix(th->input_bg, th->hover, h), th->accent, k));
        if (k < 1) ui_ring(u, cx, cy, sz / 2, 1.5, ui_fade(ui_mix(th->text_dim, th->text, h * 0.6), 0.75 * (1 - k)));
    }
    double ds = ui_round(10 * u->em * ck);
    if (k > 0 && !ui_icon(u, GOOME_ICON_RADIO_DOT, ui_bx(rb.x + (sz - ds) / 2, rb.y + (sz - ds) / 2, ds, ds), ui_fade(sk ? th->accent : th->accent_text, k)))
        ui_circle(u, cx, cy, 4 * k * u->em * ck, sk ? th->accent : th->accent_text);
    if (n) ui_text_mid(u, b.x + sz + 8, cy, label, n, 0, th->text);
    return ch;
}

static int ui_slide(goome_ui *u, uint32_t id, const char *label, double *v, double lo, double hi, const char *fmt, int whole)
{
    const goome_theme *th = &u->th;
    int n = ui_shown(label);
    double top = n ? u->lh + 2 : 0;
    double th22 = ui_round(th->slider_h * u->em), st = th->slider_track;
    ui_box b = ui_item(u, 0, top + th22);
    ui_box tb = ui_bx(b.x, b.y + top, b.w, u->sized ? ui_max(1, b.h - top) : th22);
    double kr = th->knob_size * u->em, x0 = tb.x + kr, x1 = tb.x + tb.w - kr, cy = tb.y + tb.h / 2;
    int nav = ui_nav(u, id, tb, goome_radius(tb.h / 2), UN_PLAIN, NULL, 0) & 1;
    int hov = ui_hover(u, id, tb), ch = 0;
    if (hov && u->pressed && !u->active) u->active = id;
    int held = u->active == id && (u->down || u->pressed);
    ui_grabby(u, id);
    double span = hi - lo;
    if (nav && (u->nav_dx || u->nav_home)) {
        double nv = u->nav_home ? (u->nav_home < 0 ? lo : hi) : *v + u->nav_dx * (whole ? 1 : span / 100);
        nv = span >= 0 ? ui_clamp(nv, lo, hi) : ui_clamp(nv, hi, lo);
        if (nv != *v) *v = nv, ch = 1;
    }
    if (held && x1 > x0) {
        double t = ui_clamp((u->mx - x0) / (x1 - x0), 0, 1), nv = lo + t * span;
        if (nv != *v) *v = nv, ch = 1;
    }
    if (whole) *v = ui_floor(*v + 0.5);
    char val[64];
    if (whole) snprintf(val, sizeof(val), "%d", (int)*v);
    else snprintf(val, sizeof(val), fmt ? fmt : "%.2f", *v);
    if (ui_ax_slider(u, id, b, label, n, v, lo, hi, val)) ch = 1;
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
    uint32_t kc = ui_c(th->knob, 0xFFFFFFFF);
    if (!ui_skin(u, &th->skin_slider, ui_bx(tb.x, cy - st / 2, tb.w, st), h, 0)) ui_rect(u, ui_bx(tb.x, cy - st / 2, tb.w, st), st / 2, ui_mix(th->input_bg, th->hover, h));
    if (kx > tb.x && !ui_skin(u, &th->skin_slider_fill, ui_bx(tb.x, cy - st / 2, kx - tb.x, st), 0, 0)) ui_rect(u, ui_bx(tb.x, cy - st / 2, kx - tb.x, st), st / 2, th->accent);
    if (p > 0) ui_circle(u, kx, cy, kr + 6 * p, ui_fade(th->accent, 0.22 * p));
    if (!ui_icon(u, GOOME_ICON_SLIDER_KNOB, ui_bx(kx - kr - h, cy - kr - h, 2 * (kr + h), 2 * (kr + h)), kc)) {
        ui_shadow(u, ui_bx(kx - kr, cy - kr + 1.5, 2 * kr, 2 * kr), kr, 5, 0x50000000);
        ui_circle(u, kx, cy, kr + h, kc);
        ui_ring(u, kx, cy, kr + h, 1, ui_fade(th->text, 0.12));
        ui_circle(u, kx, cy, 3 * p, th->accent);
    }
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
    return ui_bx(b.x, b.y + top, b.w, u->sized ? ui_max(1, b.h - top) : u->fh);
}

static int ui_word_ch(unsigned char c) { return c >= 0x80 || c == '_' || (c >= '0' && c <= '9') || ((c | 32) >= 'a' && (c | 32) <= 'z'); }
static int ui_word_left(const char *s, int i)
{
    while (i > 0 && !ui_word_ch((unsigned char)s[i - 1])) i--;
    while (i > 0 && ui_word_ch((unsigned char)s[i - 1])) i--;
    return i;
}
static int ui_word_right(const char *s, int i, int len)
{
    while (i < len && !ui_word_ch((unsigned char)s[i])) i++;
    while (i < len && ui_word_ch((unsigned char)s[i])) i++;
    return i;
}

static double ui_iw(goome_ui *u, const char *buf, int i, const char *disp, int ml)
{
    if (!disp) return ui_tw(u, buf, i, 0);
    int n = 0;
    for (int k = 0; k < i; k++) n += ((unsigned char)buf[k] & 0xC0) != 0x80;
    n *= ml;
    return ui_tw(u, disp, n < (int)strlen(disp) ? n : (int)strlen(disp), 0);
}

static double ui_cx(goome_ui *u, const char *buf, int len, int i, const char *disp, int ml)
{
    if (disp || !ui_bd(u, buf, len)) return ui_iw(u, buf, i, disp, ml);
    gm_tc *b;
    int nb = ui_bd_lay(u, buf, len, &b);
    double x = ui_bd_x(b, nb, i);
    free(b);
    return x;
}

static int ui_ic_step(goome_ui *u, const char *buf, int len, int cur, int right, const char *disp)
{
    if (disp) return right ? ui_u8_next(buf, cur, len) : ui_u8_prev(buf, cur);
    if (ui_bd(u, buf, len)) {
        gm_tc *b;
        int nb = ui_bd_lay(u, buf, len, &b), to = ui_bd_move(b, nb, cur, right);
        free(b);
        return to < 0 ? cur : to;
    }
    return right ? gm_text_next(buf, cur, len) : gm_text_prev(buf, cur);
}

static int ui_caret_at(goome_ui *u, const char *buf, int len, double want, const char *disp, int ml)
{
    if (!disp && ui_bd(u, buf, len)) {
        gm_tc *b;
        int nb = ui_bd_lay(u, buf, len, &b), at = ui_bd_hit(b, nb, want);
        free(b);
        return at;
    }
    double best = 1e9;
    int cur = 0;
    for (int i = 0; i <= len; i = ui_u8_next(buf, i, len)) {
        double d = ui_abs(ui_iw(u, buf, i, disp, ml) - want);
        if (d < best) best = d, cur = i;
        if (i == len) break;
    }
    return cur;
}

static void ui_undo_keep(goome_ui *u, uint32_t id, const char *buf, int len, int cur, int kind)
{
    if (u->undo_id == id && kind != 3 && kind == u->undo_kind) return;
    if (len + 1 > u->undo_cap) {
        char *n = realloc(u->undo, (size_t)len + 64);
        if (!n) return;
        u->undo = n, u->undo_cap = len + 64;
    }
    memcpy(u->undo, buf, (size_t)len + 1);
    u->undo_cur = cur, u->undo_id = id, u->undo_kind = kind;
}

static int ui_del(char *buf, int *len, int a, int b)
{
    if (b > a) memmove(buf + a, buf + b, (size_t)(*len - b + 1)), *len -= b - a;
    return a;
}

static int ui_ins(char *buf, int *len, int cap, int at, const char *t, int n)
{
    if (*len + n > cap - 1) {
        n = cap - 1 - *len;
        while (n > 0 && ((unsigned char)t[n] & 0xC0) == 0x80) n--;
    }
    if (n <= 0) return 0;
    memmove(buf + at + n, buf + at, (size_t)(*len - at + 1));
    memcpy(buf + at, t, (size_t)n);
    *len += n;
    return n;
}

static int ui_mask(goome_ui *u, const char *buf, int len, char *disp, int cap)
{
    const char *mk = u->cfg.mask && *u->cfg.mask ? u->cfg.mask : "*";
    int ml = (int)strlen(mk), dl = 0;
    if (ml > 8) ml = 8;
    for (int k = 0; k < len && dl + ml < cap; k++)
        if (((unsigned char)buf[k] & 0xC0) != 0x80) memcpy(disp + dl, mk, (size_t)ml), dl += ml;
    disp[dl] = 0;
    return ml;
}

static int ui_num_ok(const char *t)
{
    for (; *t; t++)
        if (!((*t >= '0' && *t <= '9') || *t == '-' || *t == '+' || *t == '.')) return 0;
    return 1;
}

static int ui_ime_keep(char **b, int *cap, int at, const char *s, int n)
{
    if (at + n + 1 > *cap) {
        int c = (at + n + 1) * 2;
        char *m = realloc(*b, (size_t)c);
        if (!m) return 0;
        *b = m, *cap = c;
    }
    memcpy(*b + at, s, (size_t)n);
    (*b)[at + n] = 0;
    return 1;
}

static int ui_ime_feed(goome_ui *u, const goome_event *e)
{
    const char *full = goome_event_text(u->g), *s = e->msg;
    size_t ml = strlen(e->msg);
    if (ml >= sizeof(e->msg) - 4 && !strncmp(full, e->msg, ml)) s = full;
    int n = (int)strlen(s);
    if (e->type == GOOME_PREEDIT) {
        if (!ui_ime_keep(&u->ime_pre, &u->ime_pcap, 0, s, n)) return 0;
        u->ime_plen = n, u->ime_pcur = e->id < 0 ? 0 : e->id > n ? n : e->id;
        return u->focus != 0;
    }
    if (!u->focus || u->nkeys >= UI_MAXKEY || !n) return u->focus != 0;
    if (!u->nkeys) u->ime_len = 0;
    if (!ui_ime_keep(&u->ime_buf, &u->ime_cap, u->ime_len, s, n)) return 1;
    u->ime_at[u->nkeys] = u->ime_len, u->ime_len += n + 1;
    ui_key *k = &u->keys[u->nkeys++];
    k->key = UI_IME_KEY, k->mods = 0, k->text[0] = 0;
    return 1;
}

static char *ui_ime_take(goome_ui *u, int k, int lines, int num, int *n)
{
    const char *s = u->ime_buf + u->ime_at[k];
    int len = (int)strlen(s), w = 0;
    char *t = malloc((size_t)len + 1);
    *n = 0;
    if (!t) return NULL;
    for (int i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        char one[2] = {(char)c, 0};
        if (c == '\r') continue;
        if (c == '\n' || c == '\t') c = lines && c == '\n' ? '\n' : ' ';
        else if (c < 32 || c == 127) continue;
        if (num && !ui_num_ok(one)) continue;
        t[w++] = (char)c;
    }
    t[w] = 0;
    *n = w;
    return t;
}

static int ui_ime_commit(goome_ui *u, int k, uint32_t id, char *buf, int *len, int cap, int *cur, int *anc, int num)
{
    int n, ch = 0, lo = *anc < *cur ? *anc : *cur, hi = *anc < *cur ? *cur : *anc;
    char *t = ui_ime_take(u, k, 0, num, &n);
    if (t && n > 0) {
        ui_undo_keep(u, id, buf, *len, *cur, 3);
        *cur = ui_del(buf, len, lo, hi);
        *cur += ui_ins(buf, len, cap, *cur, t, n);
        *anc = *cur, ch = 1;
        u->undo_kind = 0;
    }
    free(t);
    return ch;
}

static void ui_ime_show(goome_ui *u, uint32_t id, double x, double y, double h)
{
    const goome_theme *th = &u->th;
    if (u->ime_id != id) u->ime_id = id, u->ime_plen = 0;
    u->ime_want = 1;
    double w = 0;
    if (u->ime_plen > 0) {
        double lh = goome_text_line_height(th->font, u->size), ty = ui_round(y + (h - lh) / 2);
        double cx = ui_tw(u, u->ime_pre, u->ime_pcur, 0);
        w = ui_tw(u, u->ime_pre, u->ime_plen, 0);
        ui_rect(u, ui_bx(x - 1, y, w + 3, h), 2, th->input_bg | 0xFF000000u);
        ui_text(u, x, ty, u->ime_pre, u->ime_plen, 0, th->text);
        ui_rect(u, ui_bx(x, ty + lh - 1, w, 1), 0, th->text);
        ui_rect(u, ui_bx(x + cx, y + 1, th->caret_w > 0 ? th->caret_w : 1.5, h - 2), 0, ui_c(th->caret, th->accent));
    }
    u->ime_rect = (goome_rect){(int)ui_floor(x), (int)ui_floor(y), (int)ui_floor(w + 1.999), (int)ui_floor(h + 0.999)};
}

static void ui_ime_sync(goome_ui *u)
{
    int want = u->ime_want && u->focus && u->focus == u->ime_id;
    u->ime_want = 0;
    if (want && (!u->ime_on || memcmp(&u->ime_rect, &u->ime_sent, sizeof(goome_rect)))) {
        goome_text_input(u->g, 1, &u->ime_rect);
        u->ime_on = 1, u->ime_sent = u->ime_rect;
    } else if (!want && u->ime_on) {
        goome_text_input(u->g, 0, NULL);
        u->ime_on = 0, u->ime_plen = 0, u->ime_id = 0;
    }
}

int goome_ui_input(goome_ui *u, const char *label, char *buf, int cap) { return goome_ui_input_ex(u, label, buf, cap, 0, NULL); }

int goome_ui_input_ex(goome_ui *u, const char *label, char *buf, int cap, int flags, const char *hint)
{
    if (!u || !u->c || !buf || cap <= 0) return 0;
    const goome_theme *th = &u->th;
    int n, ch = 0, enter = 0, pw = flags & GOOME_INPUT_PASSWORD, ro = flags & GOOME_INPUT_READONLY, num = flags & GOOME_INPUT_NUMBER;
    ui_box b = ui_field_label(u, label, &n);
    uint32_t id = ui_id(u, label);
    int fresh;
    ui_st *s = ui_get(u, id, &fresh);
    buf[cap - 1] = 0;
    int len = (int)strlen(buf);
    int cur = s->i[0] < 0 ? 0 : s->i[0] > len ? len : s->i[0];
    while (cur > 0 && cur < len && ((unsigned char)buf[cur] & 0xC0) == 0x80) cur--;
    int anc = s->i[2] < 0 ? 0 : s->i[2] > len ? len : s->i[2];
    while (anc > 0 && anc < len && ((unsigned char)buf[anc] & 0xC0) == 0x80) anc--;
    double tx = b.x + 10, iw = b.w - 20;
    char disp[1024];
    const char *dp = NULL;
    int ml = 0;
    if (pw) ml = ui_mask(u, buf, len, disp, (int)sizeof(disp)), dp = disp;
    ui_nav(u, id, b, ui_tr(u, th->radius), UN_INPUT, NULL, 0);
    if (ui_ax_input(u, id, buf, cap, &len, flags)) cur = anc = len, ch = 1;
    if (u->focus == id && u->tab_came) {
        if (flags & GOOME_INPUT_NO_SELECT_ON_FOCUS) anc = cur = len;
        else anc = 0, cur = len;
    }
    int hov = ui_hover(u, id, b);
    if (hov || u->active == id) u->cur_want = u->cfg.cursor[GOOME_CUR_INPUT];
    if (hov && u->pressed && !u->active) {
        u->active = id;
        if (u->focus != id) {
            ui_focus_to(u, id, 0, UN_INPUT);
            u->nkeys = 0;
        }
        cur = anc = ui_caret_at(u, buf, len, u->mx - (tx - s->v[2]), dp, ml);
        if (u->clicks == 2 && pw) anc = 0, cur = len;
        else if (u->clicks == 2) {
            int w = (cur < len && ui_word_ch((unsigned char)buf[cur])) || (cur > 0 && ui_word_ch((unsigned char)buf[cur - 1]));
            anc = cur;
            while (anc > 0 && ui_word_ch((unsigned char)buf[anc - 1]) == w) anc--;
            while (cur < len && ui_word_ch((unsigned char)buf[cur]) == w) cur++;
        }
        else if (u->clicks >= 3) anc = 0, cur = len;
        s->i[3] = u->clicks;
        u->undo_kind = 0;
        s->v[3] = (double)u->now;
    } else if (u->active == id && u->down && s->i[3] < 2) {
        int c2 = ui_caret_at(u, buf, len, u->mx - (tx - s->v[2]), dp, ml);
        if (c2 != cur) cur = c2, s->v[3] = (double)u->now;
    } else if (u->focus == id && u->pressed && !hov)
        ui_unfocus(u);
    if (u->focus == id) {
        u->focus_seen = 1;
        for (int k = 0; k < u->nkeys && u->focus == id; k++) {
            ui_key *e = &u->keys[k];
            int key = e->key, m = e->mods, sh = m & GOOME_SHIFT, act = ui_act(u, key, m);
            int lo = anc < cur ? anc : cur, hi = anc < cur ? cur : anc, has = lo != hi;
            if (key == UI_IME_KEY) {
                if (!ro) ch |= ui_ime_commit(u, k, id, buf, &len, cap, &cur, &anc, num);
                s->v[3] = (double)u->now;
                continue;
            }
            if (!key && !e->text[0]) continue;
            s->v[3] = (double)u->now;
            if (ro && (act == GOOME_ACT_PASTE || act == GOOME_ACT_UNDO || act == GOOME_ACT_REDO || act == GOOME_ACT_DEL_WORD_LEFT || act == GOOME_ACT_DEL_WORD_RIGHT)) continue;
            if (act == GOOME_ACT_SELECT_ALL) anc = 0, cur = len;
            else if (act == GOOME_ACT_COPY || act == GOOME_ACT_CUT) {
                if (!has || pw) continue;
                char *t = malloc((size_t)(hi - lo) + 1);
                if (t) {
                    memcpy(t, buf + lo, (size_t)(hi - lo));
                    t[hi - lo] = 0;
                    goome_clipboard_set(u->g, t);
                    free(t);
                }
                if (act == GOOME_ACT_CUT && !ro) ui_undo_keep(u, id, buf, len, cur, 3), cur = anc = ui_del(buf, &len, lo, hi), ch = 1;
            } else if (act == GOOME_ACT_PASTE) {
                char small[512], *t = small;
                int got = goome_clipboard_get(u->g, small, sizeof(small)), w = 0;
                if (got >= (int)sizeof(small)) {
                    t = malloc((size_t)got + 1);
                    if (!t) continue;
                    got = goome_clipboard_get(u->g, t, got + 1);
                }
                for (int i = 0; i < got && t[i]; i++) {
                    unsigned char c = (unsigned char)t[i];
                    char one[2] = {(char)c, 0};
                    if (num && !ui_num_ok(one)) continue;
                    if (c == '\n' || c == '\r' || c == '\t') t[w++] = ' ';
                    else if (c >= 32 && c != 127) t[w++] = (char)c;
                }
                if (w > 0) {
                    ui_undo_keep(u, id, buf, len, cur, 3);
                    cur = ui_del(buf, &len, lo, hi);
                    cur += ui_ins(buf, &len, cap, cur, t, w);
                    anc = cur, ch = 1;
                }
                if (t != small) free(t);
            } else if (act == GOOME_ACT_UNDO || act == GOOME_ACT_REDO) {
                if (u->undo_id != id || !u->undo) continue;
                int ul = (int)strlen(u->undo), oc = cur;
                char *old = malloc((size_t)len + 1);
                if (!old) continue;
                memcpy(old, buf, (size_t)len + 1);
                if (ul > cap - 1) {
                    ul = cap - 1;
                    while (ul > 0 && ((unsigned char)u->undo[ul] & 0xC0) == 0x80) ul--;
                }
                memcpy(buf, u->undo, (size_t)ul);
                buf[ul] = 0;
                cur = anc = u->undo_cur > ul ? ul : u->undo_cur;
                u->undo_kind = 0;
                ui_undo_keep(u, id, old, len, oc, 3);
                u->undo_kind = 0;
                free(old);
                len = ul, ch = 1;
            } else if (act == GOOME_ACT_DEL_WORD_LEFT || act == GOOME_ACT_DEL_WORD_RIGHT || (act < 0 && (key == GOOME_K_BACKSPACE || key == GOOME_K_DELETE))) {
                if (ro) continue;
                int a0 = lo, b0 = hi, wd = act >= 0, fw = act == GOOME_ACT_DEL_WORD_RIGHT || (act < 0 && key == GOOME_K_DELETE);
                if (!has && !fw) a0 = wd ? (pw ? 0 : ui_word_left(buf, cur)) : ui_u8_prev(buf, cur);
                if (!has && fw) b0 = wd ? (pw ? len : ui_word_right(buf, cur, len)) : ui_u8_next(buf, cur, len);
                if (b0 > a0) ui_undo_keep(u, id, buf, len, cur, 2), cur = anc = ui_del(buf, &len, a0, b0), ch = 1;
            } else if (act == GOOME_ACT_WORD_LEFT || act == GOOME_ACT_WORD_RIGHT || (act < 0 && (key == GOOME_K_LEFT || key == GOOME_K_RIGHT))) {
                int r = act == GOOME_ACT_WORD_RIGHT || (act < 0 && key == GOOME_K_RIGHT), wd = act >= 0;
                if (has && !sh) cur = r ? hi : lo;
                else if (r) cur = wd ? (pw ? len : ui_word_right(buf, cur, len)) : ui_ic_step(u, buf, len, cur, 1, dp);
                else cur = wd ? (pw ? 0 : ui_word_left(buf, cur)) : ui_ic_step(u, buf, len, cur, 0, dp);
                if (!sh) anc = cur;
                u->undo_kind = 0;
            } else if (act == GOOME_ACT_DOC_START || act == GOOME_ACT_DOC_END || key == GOOME_K_HOME || key == GOOME_K_UP || key == GOOME_K_END || key == GOOME_K_DOWN) {
                cur = act == GOOME_ACT_DOC_START || (act != GOOME_ACT_DOC_END && (key == GOOME_K_HOME || key == GOOME_K_UP)) ? 0 : len;
                if (!sh) anc = cur;
                u->undo_kind = 0;
            } else if (key == GOOME_K_ENTER || key == GOOME_K_ESC) {
                if (key == GOOME_K_ENTER && (flags & GOOME_INPUT_ENTER_RETURNS)) enter = 1;
                ui_unfocus(u);
            } else if (!ro && (!num || ui_num_ok(e->text)) && (unsigned char)e->text[0] >= 32 && e->text[0] != 127 && !(m & (GOOME_CTRL | GOOME_ALT | GOOME_SUPER))) {
                int t = (int)strlen(e->text);
                if (len - (hi - lo) + t < cap) {
                    ui_undo_keep(u, id, buf, len, cur, 1);
                    cur = ui_del(buf, &len, lo, hi);
                    cur += ui_ins(buf, &len, cap, cur, e->text, t);
                    anc = cur, ch = 1;
                }
            }
        }
        u->nkeys = 0;
    }
    s->i[0] = cur, s->i[2] = anc;
    int focused = u->focus == id;
    double h = ui_anim(u, s, 0, hov), f = ui_anim_ms(u, s, 1, focused, th->anim_ms);
    if (pw) ui_mask(u, buf, len, disp, (int)sizeof(disp));
    double cx = ui_cx(u, buf, len, cur, dp, ml);
    if (cx - s->v[2] > iw - 2) s->v[2] = cx - iw + 2;
    if (cx - s->v[2] < 0) s->v[2] = cx;
    if (!focused && s->v[2] > 0 && ui_iw(u, buf, len, dp, ml) <= iw) s->v[2] = 0;
    goome_radii rr = ui_tr(u, th->radius);
    double fg = th->focus_gap;
    if (f > 0) ui_rline(u, ui_bx(b.x - fg, b.y - fg, b.w + 2 * fg, b.h + 2 * fg), ui_rgrow(rr, fg), th->focus_w + 1, ui_fade(ui_c(th->focus, th->accent), 0.28 * f));
    if (!ui_skin(u, &th->skin_input, b, h * (1 - f), 0)) ui_rrect(u, b, rr, ui_mix(th->input_bg, ui_mix(th->input_bg, th->hover, 0.5), h * (1 - f)));
    ui_rline(u, b, rr, th->border_width, ui_mix(ui_mix(th->border, ui_fade(th->text_dim, 0.5), h), th->accent, f));
    ui_clip_push(u, ui_bx(b.x + 6, b.y, b.w - 12, b.h));
    double x = ui_round(tx - s->v[2]);
    if (focused && anc != cur) {
        int lo = anc < cur ? anc : cur, hi = anc < cur ? cur : anc;
        if (!dp && ui_bd(u, buf, len)) {
            gm_tc *bx;
            int nb = ui_bd_lay(u, buf, len, &bx);
            ui_bd_sel(u, bx, nb, lo, hi, x, b.y + b.h / 2 - u->lh / 2, u->lh, 3, ui_c(th->select, ui_fade(th->accent, 0.32)), 0);
            free(bx);
        } else {
            double x0 = ui_iw(u, buf, lo, dp, ml), x1 = ui_iw(u, buf, hi, dp, ml);
            ui_rect(u, ui_bx(x + x0, b.y + b.h / 2 - u->lh / 2, x1 - x0, u->lh), 3, ui_c(th->select, ui_fade(th->accent, 0.32)));
        }
    }
    if (!len && !focused && hint && *hint) ui_text_mid(u, x, b.y + b.h / 2, hint, (int)strlen(hint), 0, th->text_dim);
    else ui_text_mid(u, x, b.y + b.h / 2, dp ? dp : buf, dp ? (int)strlen(dp) : len, 0, th->text);
    if (focused) {
        long t = u->now - (long)s->v[3], bl = u->cfg.blink_ms;
        if (bl <= 0 || (t / bl) % 2 == 0) ui_rect(u, ui_bx(x + cx, b.y + b.h / 2 - u->lh / 2 + 1, th->caret_w > 0 ? th->caret_w : 1.5, u->lh - 2), 0, ui_c(th->caret, th->accent));
        if (!pw && !ro) ui_ime_show(u, id, x + ui_cx(u, buf, len, anc < cur ? anc : cur, NULL, 0), b.y + b.h / 2 - u->lh / 2, u->lh);
    }
    ui_clip_pop(u);
    ui_ax_field(u, id, b, label, n, hint, dp ? dp : buf, dp ? (int)strlen(dp) : len, dp ? ml * gm_ax_chars(buf, cur) : cur, flags);
    return enter ? 2 : ch;
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
    double m = u->th.pop_gap, x = ui_min(anchor.x, a.x + a.w - w - m), y = anchor.y + anchor.h + m;
    if (y + h > a.y + a.h - m && anchor.y - h - m >= a.y) y = anchor.y - h - m;
    return ui_bx(ui_round(ui_max(x, a.x + m)), ui_round(y), w, h);
}

static void ui_popup_bg(goome_ui *u, ui_box p, const goome_skin *sk)
{
    const goome_theme *th = &u->th;
    uint32_t bg = th->panel | 0xFF000000u;
    goome_radii r = ui_tr(u, th->radius);
    ui_shadow(u, ui_shb(u, p, 6), ui_rmid(r), th->shadow_blur, th->shadow);
    if (ui_skin(u, sk, p, 0, 0)) return;
    ui_rrect(u, p, r, bg);
    ui_rline(u, p, r, th->border_width, th->border);
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
    if ((ui_nav(u, id, b, ui_tr(u, th->radius), UN_PLAIN, NULL, 0) & 1) && u->nav_dx && n > 0) {
        int k = *sel + u->nav_dx;
        k = k < 0 ? 0 : k >= n ? n - 1 : k;
        if (k != *sel) *sel = k, ch = 1;
    }
    if (ui_button_logic(u, id, b, &hov, &held)) {
        if (u->popup == id) u->popup = 0;
        else if (u->pop_closed != id && n > 0) u->popup = id, u->pop_kind = UI_POP_COMBO, s->v[4] = 0, s->v[2] = 0;
    }
    int open = u->popup == id;
    if (ui_ax_combo(u, id, b, label, ln, sel, items, n, open)) ch = 1;
    goome_radii rr = ui_tr(u, th->radius);
    ui_rrect(u, b, rr, ui_btn_bg(u, s, hov || open, held));
    double h = ui_ez(u, s->v[0]), k = ui_anim_ms(u, s, 4, open, ui_ms(u, th->popup_ms));
    ui_rline(u, b, rr, th->border_width, ui_mix(th->border, th->accent, k * 0.9));
    const char *cur = *sel >= 0 && *sel < n && items && items[*sel] ? items[*sel] : "";
    ui_clip_push(u, ui_bx(b.x, b.y, b.w - 28, b.h));
    ui_text_mid(u, b.x + 12, b.y + b.h / 2, cur, (int)strlen(cur), 0, th->text);
    ui_clip_pop(u);
    double ax = b.x + b.w - 16 * u->em, ay = b.y + b.h / 2, d = 4 * u->em, f = 1 - 2 * k;
    uint32_t ac = ui_mix(th->text_dim, th->text, h);
    if (!ui_icon2(u, GOOME_ICON_ARROW_DOWN, GOOME_ICON_ARROW_UP, ui_bx(ax - d - 1, ay - d - 1, 2 * d + 2, 2 * d + 2), ac, k)) {
        ui_seg(u, ax - d, ay - d * 0.5 * f, ax, ay + d * 0.5 * f, 1.6, ac);
        ui_seg(u, ax, ay + d * 0.5 * f, ax + d, ay - d * 0.5 * f, 1.6, ac);
    }
    if (!open || !items) return 0;

    u->pop_seen = 1;
    double rh = u->fh - 2, ph = ui_min(n * rh + 8, 8 * rh + 8);
    ui_box p = ui_popup_place(u, b, b.w, ph);
    u->pop_box = p;
    ui_save sv;
    ui_enter(u, &sv, &u->pop, UI_POPUP, k);
    double slide = ui_round((1 - k) * (p.y < b.y ? 6 : -6));
    ui_box pv = ui_bx(p.x, p.y + slide, p.w, p.h);
    ui_popup_bg(u, pv, &th->skin_menu);
    double maxo = ui_max(0, n * rh + 8 - ph);
    if (u->wheel && u->hover_layer == UI_POPUP && ui_in(p, u->mx, u->my)) s->v[2] = ui_clamp(s->v[2] + u->wheel * rh * (u->cfg.scroll_lines / 3), 0, maxo), u->wheel = 0;
    s->v[2] = ui_clamp(s->v[2], 0, maxo);
    ui_clip_push(u, ui_bx(pv.x + 1, pv.y + 1, pv.w - 2, pv.h - 2));
    ui_ax_list(u, id, pv, 1);
    for (int i = 0; i < n; i++) {
        ui_box r = ui_bx(pv.x + 4, pv.y + 4 + i * rh - s->v[2], pv.w - 8, rh);
        ui_ax_item(u, ui_hashi(id, (uint32_t)i + 100), r, items[i], i == *sel, GOOME_ROLE_ITEM);
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
        if (a > 0) ui_rrect(u, r, ui_tr(u, ui_max(0, th->radius - 4)), ui_fade(th->hover, a));
        const char *t = items[i] ? items[i] : "";
        int on = i == *sel;
        ui_text_mid(u, r.x + 10, r.y + r.h / 2, t, (int)strlen(t), 0, on ? th->accent : th->text);
        if (on) {
            double cx = r.x + r.w - 20, cy = r.y + r.h / 2;
            if (!ui_icon(u, GOOME_ICON_CHECK, ui_bx(cx - 1, cy - 6, 12, 12), th->accent)) {
                ui_seg(u, cx, cy, cx + 3.5, cy + 3.5, 1.8, th->accent);
                ui_seg(u, cx + 3.5, cy + 3.5, cx + 10, cy - 3.5, 1.8, th->accent);
            }
        }
    }
    if (maxo > 0) {
        double tr = pv.h - 8, tht = ui_max(th->scroll_min * 5 / 7, tr * ph / (n * rh + 8));
        ui_box tk = ui_bx(pv.x + pv.w - (th->scroll_w + 3), pv.y + 4 + (tr - tht) * s->v[2] / maxo, th->scroll_w, tht);
        if (!ui_skin(u, &th->skin_scrollbar, tk, 0, 0)) ui_rect(u, tk, th->scroll_w / 2, ui_fade(th->text, 0.25));
    }
    ui_ax_list(u, id, pv, 0);
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
    ui_nav(u, id, b, ui_tr(u, ui_max(0, th->radius - 3)), UN_PLAIN, NULL, 0);
    int hov, held, click = ui_button_logic(u, id, b, &hov, &held);
    ui_ax_item(u, id, b, label, selected, GOOME_ROLE_ITEM);
    ui_st *s = ui_get(u, id, NULL);
    double h = ui_anim(u, s, 0, hov || held), k = ui_anim(u, s, 2, selected);
    goome_radii r = ui_tr(u, ui_max(0, th->radius - 3));
    if (k > 0) ui_rrect(u, b, r, ui_fade(th->accent, 0.16 * k));
    if (h > 0) ui_rrect(u, b, r, ui_fade(th->hover, h * (0.8 - 0.5 * k)));
    if (k > 0) ui_rect(u, ui_bx(b.x + 4, b.y + b.h / 2 - 7 * k, 3, 14 * k), 1.5, th->accent);
    ui_clip_push(u, b);
    int al = ui_al(u, GOOME_ALIGN_LEFT);
    ui_text_mid(u, ui_ax(al, b.x, b.w, al == GOOME_ALIGN_LEFT ? 0 : ui_tw(u, label, n, 0), 14), b.y + b.h / 2, label, n, 0, ui_mix(th->text, ui_mix(th->text, th->accent, 0.6), k));
    ui_clip_pop(u);
    return click;
}

int goome_ui_tabs(goome_ui *u, const char *id, const char *const *names, int n, int *sel)
{
    if (!u || !u->c || !sel || n <= 0 || !names) return 0;
    const goome_theme *th = &u->th;
    ui_box b = ui_item(u, 0, th->tab_h > 0 ? ui_round(th->tab_h) : u->fh + 6);
    uint32_t tid = ui_id(u, id);
    int fresh, ch = 0;
    ui_st *s = ui_get(u, tid, &fresh);
    if (*sel >= n) *sel = n - 1;
    if (*sel < 0) *sel = 0;
    if ((ui_nav(u, tid, b, ui_tr(u, th->radius), UN_PLAIN, NULL, 0) & 1) && u->nav_dx) {
        int k = *sel + u->nav_dx;
        k = k < 0 ? 0 : k >= n ? n - 1 : k;
        if (k != *sel) *sel = k, ch = 1;
    }
    ui_rrect(u, b, ui_tr(u, th->radius), th->input_bg);
    double tw = (b.w - 6) / n;
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
    if (!ui_skin(u, &th->skin_tab, pill, 0, 0)) {
        ui_shadow(u, ui_bx(pill.x, pill.y + 1, pill.w, pill.h), u->rad_on ? ui_rmid(ui_tr(u, th->radius - 3)) : th->radius - 3, 6, 0x30000000);
        ui_rrect(u, pill, ui_tr(u, ui_max(0, th->radius - 3)), th->accent);
    }
    ui_ax_tabs(u, tid, b, id, 1);
    for (int i = 0; i < n; i++) {
        ui_box t = ui_bx(b.x + 3 + i * tw, b.y + 3, tw, b.h - 6);
        ui_ax_item(u, ui_hashi(tid, (uint32_t)i + 1), t, names[i], i == *sel, GOOME_ROLE_TAB);
        ui_st *is = ui_get(u, ui_hashi(tid, (uint32_t)i + 1), NULL);
        double over = ui_clamp(1 - ui_abs(t.x - pill.x) / tw, 0, 1);
        uint32_t c = ui_mix(ui_mix(th->text_dim, th->text, ui_ez(u, is->v[0])), th->accent_text, over);
        const char *nm = names[i] ? names[i] : "";
        int ln = ui_shown(nm);
        double w = ui_tw(u, nm, ln, 0);
        ui_clip_push(u, t);
        int al = ui_al(u, GOOME_ALIGN_AUTO);
        ui_text_mid(u, al ? ui_ax(al, t.x, t.w, w, 10) : ui_round(t.x + (t.w - w) / 2), t.y + t.h / 2, nm, ln, 0, c);
        ui_clip_pop(u);
    }
    ui_ax_tabs(u, tid, b, id, 0);
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
    if ((ui_nav(u, id, b, ui_tr(u, th->radius), UN_PLAIN, NULL, 0) & 1) && u->nav_dx) *o = u->nav_dx > 0;
    if (ui_button_logic(u, id, b, &hov, &held)) *o = !*o;
    ui_ax_expander(u, id, b, label, n, GOOME_ROLE_BUTTON, *o, 0);
    if (fresh) s->v[2] = *o ? 1 : 0;
    double h = ui_anim(u, s, 0, hov || held), k = ui_anim_ms(u, s, 2, *o, ui_ms(u, th->collapse_ms));
    ui_rrect(u, b, ui_tr(u, th->radius), ui_mix(ui_fade(th->input_bg, 0.7), th->hover, h));
    uint32_t cc = ui_mix(th->text_dim, th->text, h);
    double is = ui_round(12 * u->em);
    if (!ui_icon2(u, GOOME_ICON_ARROW_RIGHT, GOOME_ICON_ARROW_DOWN, ui_bx(b.x + 16 * u->em - is / 2, ui_round(b.y + (b.h - is) / 2), is, is), cc, k))
        ui_chevron(u, b.x + 16 * u->em, b.y + b.h / 2, k * 1.5707963267948966, 4.5 * u->em, cc);
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
    if (u->sized) bh = b.h;
    uint32_t id = ui_auto(u);
    int fresh;
    ui_st *s = ui_get(u, id, &fresh);
    ui_hover(u, id, b);
    ui_ax_progress(u, b, frac, text);
    double r = ui_min(th->radius, bh / 2);
    goome_radii rr = ui_trc(u, th->radius, bh / 2);
    ui_rrect(u, b, rr, th->input_bg);
    ui_box fill;
    if (frac < 0) {
        double t = (double)(u->now % 1600) / 1600, seg = b.w * 0.3;
        double x = b.x - seg + (b.w + seg) * ui_ez(u, t);
        fill = ui_cut(ui_bx(x, b.y, seg, bh), b);
        u->anim_next = 1;
    } else {
        ui_follow(u, &s->v[0], ui_clamp(frac, 0, 1), fresh);
        fill = ui_bx(b.x, b.y, ui_round(b.w * s->v[0]), bh);
        if (fill.w > 0 && fill.w < 2 * r) fill.w = ui_min(b.w, 2 * r);
    }
    if (fill.w > 0) ui_rrect(u, fill, rr, th->accent);
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
    ui_grabby(u, id);
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
    ui_nav(u, id, b, ui_tr(u, th->radius - 2), UN_PLAIN, NULL, 0);
    if (ui_button_logic(u, id, b, &hov, &held)) {
        if (u->popup == id) u->popup = 0;
        else if (u->pop_closed != id) u->popup = id, u->pop_kind = UI_POP_COLOR, s->v[1] = 0;
    }
    int open = u->popup == id;
    if (ui_ax_color(u, id, b, label, n, argb, open)) ch = 1;
    double h = ui_anim(u, s, 0, hov || held || open), k = ui_anim_ms(u, s, 1, open, ui_ms(u, th->popup_ms));
    if (h > 0) ui_rrect(u, b, ui_tr(u, th->radius - 2), ui_fade(th->hover, h * 0.7));
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
    ui_popup_bg(u, p, &th->skin_popup);
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
    if (!ui_icon(u, GOOME_ICON_PICKER, ui_bx(mx - 9, my - 9, 18, 18), 0xFFFFFFFF)) {
        ui_ring(u, mx, my, 8, 1, 0x50000000);
        ui_ring(u, mx, my, 7, 2.5, 0xFFFFFFFF);
    }
    ui_circle(u, mx, my, 4.8, rgb);
    for (int i = 0; i < 6; i++) {
        goome_radii r = goome_radius(0);
        if (i == 0) r.tl = r.bl = 6;
        if (i == 5) r.tr = r.br = 6;
        double x0 = hb.x + hb.w * i / 6, x1 = hb.x + hb.w * (i + 1) / 6;
        ui_grad(u, ui_bx(x0, hb.y, x1 - x0 + (i < 5 ? 1 : 0), hb.h), r, ui_hsv(i / 6.0, 1, 1), ui_hsv((i + 1) / 6.0 - 1e-9, 1, 1), 1);
    }
    double hx = hb.x + s->v[2] * hb.w;
    if (!ui_icon(u, GOOME_ICON_SLIDER_KNOB, ui_bx(hx - 8, hb.y + hb.h / 2 - 8, 16, 16), 0xFFFFFFFF)) {
        ui_circle(u, hx, hb.y + hb.h / 2, 8, 0x50000000);
        ui_circle(u, hx, hb.y + hb.h / 2, 7.5, 0xFFFFFFFF);
    }
    ui_circle(u, hx, hb.y + hb.h / 2, 5, hue);
    ui_checker(u, ab, 6);
    ui_grad(u, ab, goome_radius(6), rgb & 0xFFFFFF, rgb, 1);
    double axx = ab.x + a * ab.w;
    if (!ui_icon(u, GOOME_ICON_SLIDER_KNOB, ui_bx(axx - 8, ab.y + ab.h / 2 - 8, 16, 16), 0xFFFFFFFF)) {
        ui_circle(u, axx, ab.y + ab.h / 2, 8, 0x50000000);
        ui_circle(u, axx, ab.y + ab.h / 2, 7.5, 0xFFFFFFFF);
    }
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
    double wait = u->cfg.tip_delay_ms, ms = ui_ms(u, th->popup_ms);
    if (u->tip_ms < wait) {
        if (u->tip_ms > -1000) u->anim_next = 1;
        return;
    }
    double k = ms > 0 ? ui_ez(u, (u->tip_ms - wait) / ms) : 1;
    if (k < 1) u->anim_next = 1;
    int st[32], ln[32];
    int n = ui_wrap(u, text, 260, st, ln, 32);
    double w = 0;
    for (int i = 0; i < n; i++) w = ui_max(w, ui_tw(u, text + st[i], ln[i], 0));
    double px = th->tip_pad_x, py = th->tip_pad_y, bw = ui_round(w + 2 * px), bh = ui_round(n * u->lh + 2 * py), tr = th->tip_radius;
    goome_rect a = u->area;
    double x = ui_min(u->mx + th->tip_x, a.x + a.w - bw - 4), y = u->my + th->tip_y;
    if (y + bh > a.y + a.h - 4) y = u->my - bh - 8;
    ui_box b = ui_bx(ui_round(x), ui_round(y + (1 - k) * 4), bw, bh);
    ui_save sv;
    ui_enter(u, &sv, &u->tip, UI_TIP, k);
    ui_shadow(u, ui_shb(u, b, 3), tr, 12, th->shadow);
    if (!ui_skin(u, &th->skin_tooltip, b, 0, 0)) {
        ui_rect(u, b, tr, ui_c(th->tooltip, ui_mix(th->bg, th->hover, 0.35) | 0xFF000000u));
        ui_outline(u, b, tr, 1, th->border);
    }
    for (int i = 0; i < n; i++) ui_text_mid(u, b.x + px, b.y + py + u->lh * (i + 0.5), text + st[i], ln[i], 0, th->text);
    ui_leave(u, &sv);
}

int goome_ui_hovered(goome_ui *u) { return u && u->c ? u->last_hover : 0; }

static ui_pop *ui_pop_at(goome_ui *u, uint32_t id) { return u->npop > u->pdepth && u->ps[u->pdepth].id == id ? &u->ps[u->pdepth] : NULL; }
static ui_pop *ui_pop_cur(goome_ui *u) { return u->pdepth > 0 ? &u->ps[u->pdepth - 1] : NULL; }
static int ui_pop_alive(goome_ui *u) { return u->pdepth > 0 && u->pdepth <= u->npop; }

static ui_pop *ui_pop_push(goome_ui *u, uint32_t id, int kind, int side, ui_box anchor)
{
    if (u->pdepth >= UI_MAXPOP) return NULL;
    ui_pop_cut(u, u->pdepth);
    ui_pop *p = &u->ps[u->npop++];
    memset(p, 0, sizeof(*p));
    p->id = id, p->kind = kind, p->side = side, p->anchor = anchor;
    p->sel = p->fire = p->sub = -1;
    p->born = p->seen = u->frame;
    u->anim_next = 1;
    return p;
}

static ui_box ui_pop_place(goome_ui *u, ui_pop *p, double w, double h)
{
    goome_rect a = u->area;
    ui_box an = p->anchor;
    double x = an.x, y = an.y, m = u->th.pop_gap, g = u->th.menu_gap;
    if (p->side == UA_BELOW) {
        y = an.y + an.h + g;
        if (y + h > a.y + a.h - m && an.y - h - g >= a.y) y = an.y - h - g;
    } else if (p->side == UA_RIGHT) {
        x = an.x + an.w - 3, y = an.y - 5;
        if (x + w > a.x + a.w - m) x = an.x - w + 3;
    } else if (p->side == UA_POINT) {
        x = an.x + 2, y = an.y + 2;
        if (x + w > a.x + a.w - m) x = an.x - w - 2;
        if (y + h > a.y + a.h - m) y = an.y - h - 2;
    } else {
        x = a.x + (a.w - w) / 2, y = a.y + (a.h - h) * 0.42;
    }
    x = ui_max(a.x + m, ui_min(x, a.x + a.w - w - m));
    y = ui_max(a.y + m, ui_min(y, a.y + a.h - h - m));
    return ui_bx(ui_round(x), ui_round(y), ui_round(w), ui_round(h));
}

static void ui_pop_bg(goome_ui *u, ui_box b, goome_radii r, const goome_skin *sk)
{
    const goome_theme *th = &u->th;
    ui_shadow(u, ui_shb(u, b, 8), ui_rmid(r), th->shadow_blur + 4, th->shadow);
    ui_shadow(u, ui_shb(u, b, 1), ui_rmid(r), 3, ui_fade(th->shadow, 0.5));
    if (ui_skin(u, sk, b, 0, 0)) return;
    ui_rrect(u, b, r, ui_mix(th->panel | 0xFF000000u, th->hover | 0xFF000000u, 0.22));
    ui_rline(u, b, r, th->border_width, th->border);
}

static int ui_pop_begin(goome_ui *u, ui_pop *p, double minw, double pad)
{
    if (u->nfr >= UI_MAXFR) return 0;
    const goome_theme *th = &u->th;
    int k = (int)(p - u->ps);
    p->seen = u->frame, p->items = 0, p->chk = 0;
    ui_step(u, &p->open, 1, ui_ms(u, th->popup_ms));
    double w = ui_max(p->w, minw), h = p->h > 0 ? p->h : 2 * pad + u->fh;
    ui_box b = ui_pop_place(u, p, w, h);
    p->box = b;
    double e = p->w > 0 ? ui_ez(u, p->open) : 0;
    if (p->w <= 0) u->anim_next = 1;
    double slide = ui_round((1 - e) * 6);
    ui_box d = b;
    if (p->side == UA_BELOW) d.y += b.y < p->anchor.y ? slide : -slide;
    else if (p->side == UA_RIGHT) d.x += d.x < p->anchor.x ? slide : -slide;
    else if (p->side == UA_CENTER) d.y += slide * 1.5;
    else d.y -= slide;
    ui_frame *f = &u->fr[u->nfr++];
    memset(f, 0, sizeof(*f));
    f->pop = k + 1, f->nscr0 = u->nscr, f->nclip0 = u->nclip, f->box = d, f->minw = minw, f->pad = pad;
    ui_enter(u, &f->sv, &u->plist[k], p->id, e);
    f->nid_out = u->nid;
    if (u->nid < UI_MAXID) u->ids[u->nid++] = p->id;
    f->nid0 = u->nid;
    u->pdepth = k + 1;
    double top = 0;
    goome_radii r = ui_tr(u, p->kind == UP_MENU ? ui_max(0, th->radius - 2) : th->radius + 2);
    if (p->kind == UP_MODAL) {
        goome_rect a = u->area;
        ui_rect(u, ui_bx(a.x, a.y, a.w, a.h), 0, ui_c(th->dim, 0x73000000));
        ui_shadow(u, ui_shb(u, d, 14), ui_rmid(r), 46, ui_fade(th->shadow, 1.4));
        ui_shadow(u, ui_shb(u, d, 2), ui_rmid(r), 6, ui_fade(th->shadow, 0.5));
        if (!ui_skin(u, &th->skin_popup, d, 0, 0)) {
            ui_rrect(u, d, r, ui_mix(th->panel | 0xFF000000u, th->hover | 0xFF000000u, 0.12));
            ui_rline(u, d, r, th->border_width, th->border);
        }
    } else
        ui_pop_bg(u, d, r, p->kind == UP_MENU ? &th->skin_menu : &th->skin_popup);
    ui_lay_init(&f->lay, d.x + pad, d.y + pad + top, d.w - 2 * pad);
    if (p->kind == UP_MENU) f->lay.tight = 1, f->lay.gap = 0;
    return 1;
}

static void ui_pop_finish(goome_ui *u)
{
    if (!u || !u->c || u->nfr <= 1) return;
    ui_frame *f = &u->fr[u->nfr - 1];
    if (!f->pop) return;
    while (u->nscr > f->nscr0) ui_scroll_close(u);
    ui_pop *p = &u->ps[f->pop - 1];
    double pad = f->pad;
    double nw = ui_round(ui_max(f->minw, f->lay.max_x + pad - f->box.x)), nh = ui_round(f->lay.max_y + pad - f->box.y);
    if (p->kind == UP_MENU && nh < 2 * pad + 4) nh = 2 * pad + 4;
    if (nw != p->w || nh != p->h) u->anim_next = 1;
    p->w = nw, p->h = nh;
    if (p->kind == UP_MENU) {
        p->nitems = p->items, p->has_chk = p->chk;
        if (p->sel >= p->nitems) p->sel = p->nitems - 1;
        if (p->sub != -1 && p->bar) u->mb_shift = 1;
        p->sub = p->fire = -1;
        int k = f->pop - 1;
        if (u->moved && !ui_in(p->box, u->mx, u->my) && u->npop <= k + 1) p->sel = -1;
    }
    ui_leave(u, &f->sv);
    u->nid = f->nid_out;
    u->nfr--;
    u->pdepth = f->pop - 1;
    ui_ax_pop(u, UI_AXP, NULL);
}

static int ui_row_hover(goome_ui *u, ui_pop *p, uint32_t id, int idx, ui_box b)
{
    int h = u->hover_layer == u->layer && ui_in(b, u->mx, u->my) && (!u->nclip || ui_in(u->clip[u->nclip - 1], u->mx, u->my));
    if (h) {
        u->mh_seen = 1;
        if (u->mh_id != id) u->mh_id = id, u->mh_ms = 0;
        else u->mh_ms += u->dt;
        if (p && (u->moved || u->pressed)) p->sel = idx;
        if (u->tip_id != id) u->tip_id = id, u->tip_ms = 0;
        u->tip_seen = 1;
    }
    u->last_id = id, u->last_hover = h, u->last_box = b;
    return h;
}

static void ui_menu_close(goome_ui *u)
{
    int j = u->pdepth - 1;
    while (j > 0 && u->ps[j - 1].kind == UP_MENU) j--;
    ui_pop_cut(u, j);
}

static ui_box ui_menu_row(goome_ui *u, double want)
{
    ui_lay *l = ui_lay_cur(u);
    ui_box b = ui_item(u, want, u->th.menu_item_h > 0 ? ui_round(u->th.menu_item_h) : ui_round(u->lh + 9));
    b.w = ui_round(ui_max(b.w, l->x0 + l->w - b.x));
    u->last_box = b;
    return b;
}

static void ui_menu_hl(goome_ui *u, uint32_t id, ui_box b, int on)
{
    ui_st *s = ui_get(u, id, NULL);
    s->v[0] = on ? 1 : s->v[0];
    double k = ui_anim(u, s, 0, on);
    if (k > 0) ui_rrect(u, b, ui_tr(u, ui_max(0, u->th.radius - 5)), ui_fade(u->th.hover, 0.4 + 0.6 * k));
}

int goome_ui_menu_item(goome_ui *u, const char *label, const char *shortcut, int *checked)
{
    if (!u || !u->c) return 0;
    const goome_theme *th = &u->th;
    ui_pop *p = ui_pop_cur(u);
    if (p && p->kind != UP_MENU) p = NULL;
    int n = ui_shown(label), sn = shortcut ? (int)strlen(shortcut) : 0;
    double tw = ui_tw(u, label, n, 0), sw = sn ? ui_tw(u, shortcut, sn, 0) : 0;
    double cw = p && p->has_chk ? 22 : 0;
    if (checked && p) p->chk = 1;
    ui_box b = ui_menu_row(u, ui_round(12 + cw + tw + (sn ? 40 + sw : 0) + 14));
    uint32_t id = ui_id(u, label);
    int idx = p ? p->items++ : -1;
    int hov = ui_row_hover(u, p, id, idx, b), alive = !p || ui_pop_alive(u);
    if (p && hov && u->npop > u->pdepth) {
        if (u->mh_ms >= u->cfg.menu_delay_ms * 4.0 / 3) ui_pop_cut(u, u->pdepth);
        else u->anim_next = 1;
    }
    int click = alive && ((hov && u->released) || (p && p->fire == idx) || ui_ax_do(u, id, 'p'));
    if (p && p->fire == idx) p->fire = -1;
    ui_menu_hl(u, id, b, p ? p->sel == idx : hov);
    if (click && checked) *checked = !*checked;
    ui_ax_menuitem(u, id, b, label, n, checked);
    double cy = b.y + b.h / 2, x = b.x + 12;
    if (checked && *checked) {
        double q = u->em;
        if (!ui_icon(u, GOOME_ICON_CHECK, ui_bx(x, ui_round(cy - 6 * q), ui_round(12 * q), ui_round(12 * q)), th->accent)) {
            ui_seg(u, x + 1 * q, cy, x + 4.5 * q, cy + 3.5 * q, 1.8 * q, th->accent);
            ui_seg(u, x + 4.5 * q, cy + 3.5 * q, x + 11 * q, cy - 3.5 * q, 1.8 * q, th->accent);
        }
    }
    ui_text_mid(u, x + cw, cy, label, n, 0, th->text);
    if (sn) ui_text_mid(u, ui_round(b.x + b.w - 12 - sw), cy, shortcut, sn, 0, th->text_dim);
    if (click && p) ui_menu_close(u);
    return click;
}

void goome_ui_menu_separator(goome_ui *u)
{
    if (!u || !u->c) return;
    ui_lay *l = ui_lay_cur(u);
    ui_box b = ui_item(u, 0, 9);
    double x0 = ui_pop_cur(u) ? b.x + 8 : b.x, x1 = l->x0 + l->w - (ui_pop_cur(u) ? 8 : 0);
    ui_rect(u, ui_bx(x0, b.y + (9 - u->th.sep_w) / 2, ui_max(0, x1 - x0), u->th.sep_w), 0, u->th.border);
}

static int ui_menu_head(goome_ui *u, const char *label, uint32_t id, ui_box hb, ui_box an, int bar_i)
{
    const goome_theme *th = &u->th;
    int n = ui_shown(label), base = u->pdepth;
    uint32_t pid = ui_hashi(id, 0x4D454E55u), bar = bar_i >= 0 ? u->mb_id : 0;
    ui_pop *p = ui_pop_at(u, pid);
    int any = bar && u->npop > base && u->ps[base].bar == bar;
    int hov = u->hover_layer == u->layer && ui_in(hb, u->mx, u->my) && (!u->nclip || ui_in(u->clip[u->nclip - 1], u->mx, u->my));
    u->last_id = id, u->last_hover = hov, u->last_box = hb;
    if ((hov && u->pressed) || ui_ax_do(u, id, 'p')) {
        u->active = id, u->mb_hit = 1;
        if (p) ui_pop_cut(u, base), p = NULL;
        else p = ui_pop_push(u, pid, UP_MENU, UA_BELOW, an);
    } else if (hov && any && !p)
        p = ui_pop_push(u, pid, UP_MENU, UA_BELOW, an);
    if (bar_i >= 0 && u->mb_want == bar_i && !p && (p = ui_pop_push(u, pid, UP_MENU, UA_BELOW, an))) p->sel = 0;
    if (p) p->bar = bar, p->anchor = an;
    if (p && bar_i >= 0) u->mb_open_i = bar_i;
    ui_st *s = ui_get(u, id, NULL);
    double k = ui_anim(u, s, 0, hov || p);
    goome_radii r = ui_tr(u, ui_max(0, th->radius - 4)), rb = ui_tr(u, th->radius);
    if (bar_i < 0) {
        ui_rrect(u, hb, rb, ui_mix(ui_mix(th->input_bg, th->hover, k), th->active, p ? 1 : 0));
        ui_rline(u, hb, rb, th->border_width, ui_mix(th->border, th->accent, p ? 0.9 : 0));
        double tw = ui_tw(u, label, n, 0), ax = hb.x + hb.w - 16 * u->em, ay = hb.y + hb.h / 2, d = 4 * u->em, fl = p ? -1 : 1;
        ui_text_mid(u, ui_round(hb.x + (hb.w - 22 * u->em - tw) / 2), ay, label, n, 0, th->text);
        if (!ui_icon(u, p ? GOOME_ICON_ARROW_UP : GOOME_ICON_ARROW_DOWN, ui_bx(ax - d - 1, ay - d - 1, 2 * d + 2, 2 * d + 2), th->text_dim)) {
            ui_seg(u, ax - d, ay - d * 0.5 * fl, ax, ay + d * 0.5 * fl, 1.6, th->text_dim);
            ui_seg(u, ax, ay + d * 0.5 * fl, ax + d, ay - d * 0.5 * fl, 1.6, th->text_dim);
        }
    } else {
        if (p) ui_rrect(u, hb, r, th->active);
        else if (k > 0) ui_rrect(u, hb, r, ui_fade(th->hover, k));
        ui_text_mid(u, hb.x + 10, hb.y + hb.h / 2, label, n, 0, p || hov ? th->text : ui_mix(th->text_dim, th->text, 0.7));
    }
    int on = p && ui_pop_begin(u, p, bar_i >= 0 ? 220 : hb.w, 5);
    ui_ax_menu(u, id, hb, label, n, p != NULL, on);
    return on;
}

int goome_ui_menu_begin(goome_ui *u, const char *label)
{
    if (!u || !u->c) return 0;
    const goome_theme *th = &u->th;
    int n = ui_shown(label);
    uint32_t id = ui_id(u, label);
    if (u->mb_on && u->pdepth == u->mb_base) {
        double bw = ui_round(ui_tw(u, label, n, 0) + 20);
        ui_box hb = ui_bx(u->mb_x, u->mb_box.y + 4, bw, u->mb_box.h - 8);
        u->mb_x += bw + 2;
        return ui_menu_head(u, label, id, hb, ui_bx(hb.x, u->mb_box.y, hb.w, u->mb_box.h - 2), u->mb_i++);
    }
    ui_pop *par = ui_pop_cur(u);
    if (!par || par->kind != UP_MENU) {
        ui_box hb = ui_item(u, ui_round(ui_tw(u, label, n, 0) + th->pad * 2.6 + 22 * u->em), u->fh);
        ui_nav(u, id, hb, ui_tr(u, th->radius), UN_PLAIN, NULL, 0);
        int act = u->nav_act && u->focus == id;
        if (act) u->nav_act = 0, u->pressed = 1, u->mx = hb.x + 1, u->my = hb.y + 1;
        int r = ui_menu_head(u, label, id, hb, hb, -1);
        if (act) {
            u->pressed = 0;
            if (r) u->ps[u->pdepth - 1].sel = 0;
        }
        return r;
    }
    uint32_t pid = ui_hashi(id, 0x5355424Du);
    int base = u->pdepth;
    ui_pop *p = ui_pop_at(u, pid);
    double tw = ui_tw(u, label, n, 0), cw = par->has_chk ? 22 : 0;
    ui_box b = ui_menu_row(u, ui_round(12 + cw + tw + 50));
    int idx = par->items++;
    int hov = ui_row_hover(u, par, id, idx, b);
    int want = (hov && (u->npop <= base || u->mh_ms >= u->cfg.menu_delay_ms || u->pressed)) || par->sub == idx || par->fire == idx || ui_ax_do(u, id, 'p');
    if (want && !p && ui_pop_alive(u)) {
        p = ui_pop_push(u, pid, UP_MENU, UA_RIGHT, b);
        if (p && (par->sub == idx || par->fire == idx)) p->sel = 0;
    } else if (hov && !p && u->npop > base)
        u->anim_next = 1;
    if (par->sub == idx) par->sub = -1;
    if (par->fire == idx) par->fire = -1;
    ui_menu_hl(u, id, b, par->sel == idx || p);
    double cy = b.y + b.h / 2, ax = b.x + b.w - 14, d = 3.5 * u->em;
    ui_text_mid(u, b.x + 12 + cw, cy, label, n, 0, th->text);
    if (!ui_icon(u, GOOME_ICON_ARROW_RIGHT, ui_bx(ax - d * 1.1 - 1, cy - d - 1, 2 * d + 2, 2 * d + 2), p ? th->text : th->text_dim)) {
        ui_seg(u, ax - d * 0.6, cy - d, ax + d * 0.4, cy, 1.6 * u->em, p ? th->text : th->text_dim);
        ui_seg(u, ax + d * 0.4, cy, ax - d * 0.6, cy + d, 1.6 * u->em, p ? th->text : th->text_dim);
    }
    if (p) p->anchor = ui_bx(par->box.x, b.y, par->box.w, b.h);
    int r = p && ui_pop_begin(u, p, 180, 5);
    ui_ax_menu(u, id, b, label, n, p != NULL, r);
    return r;
}

void goome_ui_menu_end(goome_ui *u) { ui_pop_finish(u); }

int goome_ui_menubar_begin(goome_ui *u)
{
    if (!u || !u->c) return 0;
    const goome_theme *th = &u->th;
    double h = th->menubar_h > 0 ? ui_round(th->menubar_h) : ui_round(u->lh + 14);
    ui_box b;
    if (u->app_on && (u->app_flags & GOOME_APP_MENUBAR) && !u->app_mb_used && u->nfr == 1 && !u->pdepth) b = u->app_mb, u->app_mb_used = 1;
    else {
        ui_lay *l = ui_lay_cur(u);
        b = ui_item(u, 0, h);
        b.w = ui_round(l->x0 + l->w - b.x);
        ui_rrect(u, b, ui_tr(u, ui_max(0, th->radius - 2)), ui_fade(th->input_bg, 0.6));
    }
    u->mb_on = 1, u->mb_box = b, u->mb_x = b.x + 6, u->mb_base = u->pdepth, u->mb_i = 0, u->mb_open_i = -1, u->mb_hit = 0;
    u->mb_id = ui_hashi(ui_id(u, "##menubar"), (uint32_t)u->nmbs + 1);
    if (u->nmbs < 4) u->mbs[u->nmbs++] = b;
    ui_ax_push(u, ui_ax_add(u, u->mb_id, b, GOOME_ROLE_GROUP, "Menu bar", 8, 0, 0), UI_AXM);
    return 1;
}

void goome_ui_menubar_end(goome_ui *u)
{
    if (!u || !u->c || !u->mb_on) return;
    ui_ax_pop(u, UI_AXM, NULL);
    u->mb_on = 0;
    int n = u->mb_i;
    u->mb_want = -1;
    if (u->mb_shift && u->mb_open_i >= 0 && n > 0) u->mb_want = (u->mb_open_i + u->mb_shift + n) % n, u->anim_next = 1;
    u->mb_shift = 0;
    if (u->pressed && !u->mb_hit && ui_in(u->mb_box, u->mx, u->my) && u->npop > u->mb_base && u->ps[u->mb_base].bar == u->mb_id) ui_pop_cut(u, u->mb_base);
}

int goome_ui_context_begin(goome_ui *u, const char *id)
{
    if (!u || !u->c) return 0;
    uint32_t pid = ui_hashi(ui_id(u, id), 0x43545854u);
    ui_lay *l = ui_lay_cur(u);
    ui_box t = l->any ? u->last_box : ui_top(u)->box;
    int over = u->hover_layer == u->layer && ui_in(t, u->pmx, u->pmy) && (!u->nclip || ui_in(u->clip[u->nclip - 1], u->pmx, u->pmy));
    ui_pop *p = ui_pop_at(u, pid);
    if (u->rpressed && over) {
        p = ui_pop_push(u, pid, UP_MENU, UA_POINT, ui_bx(u->pmx, u->pmy, 0, 0));
        if (l->any) u->rpressed = 0;
    }
    int r = p && ui_pop_begin(u, p, 190, 5);
    ui_ax_pop_node(u, pid, GOOME_ROLE_MENU, NULL, 0, r);
    return r;
}

void goome_ui_context_end(goome_ui *u) { ui_pop_finish(u); }

void goome_ui_popup_open(goome_ui *u, const char *id)
{
    if (!u) return;
    uint32_t pid = ui_hashi(ui_id(u, id), 0x504F5055u);
    int any = u->c && ui_lay_cur(u)->any;
    ui_pop_push(u, pid, UP_POPUP, any ? UA_BELOW : UA_POINT, any ? u->last_box : ui_bx(u->mx, u->my, 0, 0));
}

int goome_ui_popup_begin(goome_ui *u, const char *id)
{
    if (!u || !u->c) return 0;
    ui_pop *p = ui_pop_at(u, ui_hashi(ui_id(u, id), 0x504F5055u));
    if (!p) return 0;
    if (p->close_req) {
        ui_pop_cut(u, u->pdepth);
        return 0;
    }
    int r = ui_pop_begin(u, p, 160, u->th.pad);
    ui_ax_pop_node(u, p->id, GOOME_ROLE_GROUP, id, id ? ui_shown(id) : 0, r);
    return r;
}

void goome_ui_popup_end(goome_ui *u) { ui_pop_finish(u); }

void goome_ui_popup_close(goome_ui *u)
{
    if (!u) return;
    if (!u->pdepth && u->popup) {
        u->popup = 0;
        return;
    }
    int k = u->pdepth > 0 ? u->pdepth - 1 : u->npop - 1;
    if (k < 0 || k >= u->npop) return;
    if (u->ps[k].kind == UP_MODAL) u->ps[k].close_req = 1;
    else ui_pop_cut(u, k);
}

int goome_ui_modal_begin(goome_ui *u, const char *title, int *open)
{
    if (!u || !u->c) return 0;
    const goome_theme *th = &u->th;
    const char *t = title ? title : "";
    uint32_t pid = ui_hashi(ui_id(u, t), 0x4D4F4441u);
    ui_pop *p = ui_pop_at(u, pid);
    if (open && !*open) {
        if (p) ui_pop_cut(u, u->pdepth);
        return 0;
    }
    if (p && p->close_req) {
        p->close_req = 0;
        if (open) {
            *open = 0;
            ui_pop_cut(u, u->pdepth);
            return 0;
        }
    }
    if (!p && !(p = ui_pop_push(u, pid, UP_MODAL, UA_CENTER, ui_bx(0, 0, 0, 0)))) return 0;
    double pad = ui_round(th->pad * 2);
    if (!ui_pop_begin(u, p, ui_min(380, u->area.w - 24), pad)) return 0;
    ui_frame *f = &u->fr[u->nfr - 1];
    f->flag = open;
    ui_box d = f->box;
    int n = ui_shown(t);
    ui_ax_pop_node(u, pid, GOOME_ROLE_DIALOG, t, n, 1);
    double sz = ui_round(u->size * 1.2), lh = goome_text_line_height(th->font, sz);
    double ty = d.y + pad - 2;
    ui_text_mid(u, d.x + pad, ty + lh / 2, t, n, sz, th->text);
    if (open) {
        uint32_t xid = ui_hashi(pid, 3);
        double xs = ui_round(26 * u->em);
        ui_box xb = ui_bx(d.x + d.w - pad + 6 - xs, ui_round(ty + lh / 2 - xs / 2), xs, xs);
        int hov, held;
        if (ui_button_logic(u, xid, xb, &hov, &held)) p->close_req = 1;
        ui_ax_btn(u, xid, xb, "Close", 5);
        ui_st *s = ui_get(u, xid, NULL);
        double k = ui_anim(u, s, 0, hov || held), cx = xb.x + xs / 2, cy = xb.y + xs / 2, dd = 4 * u->em;
        if (k > 0) ui_rect(u, xb, 7, ui_fade(ui_mix(th->hover, th->active, held), k));
        uint32_t xc = ui_mix(th->text_dim, th->text, k);
        double is = ui_round(11 * u->em);
        if (!ui_icon(u, GOOME_ICON_CLOSE, ui_bx(xb.x + (xs - is) / 2, xb.y + (xs - is) / 2, is, is), xc)) {
            ui_seg(u, cx - dd, cy - dd, cx + dd, cy + dd, 1.6 * u->em, xc);
            ui_seg(u, cx + dd, cy - dd, cx - dd, cy + dd, 1.6 * u->em, xc);
        }
    }
    ui_lay_init(&f->lay, d.x + pad, ui_round(ty + lh + 14), d.w - 2 * pad);
    f->lay.max_x = d.x + pad + ui_tw(u, t, n, sz) + (open ? 40 : 0);
    return 1;
}

void goome_ui_modal_end(goome_ui *u)
{
    if (!u || !u->c || u->nfr <= 1) return;
    ui_frame *f = &u->fr[u->nfr - 1];
    if (!f->pop) return;
    int k = f->pop - 1, *open = f->flag;
    ui_pop_finish(u);
    if (k < u->npop && u->ps[k].close_req && open) *open = 0, ui_pop_cut(u, k);
}

void goome_ui_toast(goome_ui *u, const char *text, int ms)
{
    if (!u || !text) return;
    if (u->ntoast == UI_MAXTOAST) memmove(u->toasts, u->toasts + 1, sizeof(ui_toast) * (UI_MAXTOAST - 1)), u->ntoast--;
    ui_toast *t = &u->toasts[u->ntoast++];
    memset(t, 0, sizeof(*t));
    snprintf(t->text, sizeof(t->text), "%s", text);
    t->ms = ms > 0 ? ms : u->cfg.toast_ms > 0 ? u->cfg.toast_ms : 3000;
    if (t->ms < 200) t->ms = 200;
    t->fresh = 1;
    u->anim = 1;
}

static void ui_toasts_draw(goome_ui *u)
{
    if (!u->ntoast) return;
    const goome_theme *th = &u->th;
    ui_save sv;
    ui_enter(u, &sv, &u->tl, UI_TOASTL, 1);
    goome_rect a = u->area;
    double bottom = (u->app_on || u->app_was) && (u->app_flags & GOOME_APP_STATUSBAR) ? u->app_sb.y - 14 : a.y + a.h - 18, right = a.x + a.w - 18;
    for (int i = u->ntoast - 1; i >= 0; i--) {
        ui_toast *t = &u->toasts[i];
        int st[8], ln[8];
        int n = ui_wrap(u, t->text, 300, st, ln, 8);
        double w = 0;
        for (int j = 0; j < n; j++) w = ui_max(w, ui_tw(u, t->text + st[j], ln[j], 0));
        double bw = ui_round(w + 48), bh = ui_round(ui_max(1, n) * u->lh + 22), target = bottom - bh;
        bottom = target - 10;
        ui_follow(u, &t->y, target, t->fresh);
        t->fresh = 0;
        double fd = th->toast_fade_ms, fin = ui_min(fd < 0 ? 180 : fd, t->ms / 3.0), fout = ui_min(fd < 0 ? 260 : fd, t->ms / 3.0);
        double kin = fin > 0 ? ui_ez(u, t->age / fin) : 1, kout = fout > 0 ? ui_ez(u, (t->ms - t->age) / fout) : 1, k = ui_min(kin, kout);
        u->alpha = k;
        ui_box b = ui_bx(ui_round(right - bw + (1 - kin) * 36), ui_round(t->y + (1 - kout) * 8), bw, bh);
        t->box = k > 0.05 ? b : ui_bx(0, 0, 0, 0);
        goome_radii r = ui_tr(u, th->radius);
        ui_shadow(u, ui_shb(u, b, 8), ui_rmid(r), th->shadow_blur, th->shadow);
        ui_rrect(u, b, r, ui_c(th->toast, ui_mix(th->panel | 0xFF000000u, th->hover | 0xFF000000u, 0.3)));
        ui_rline(u, b, r, th->border_width, th->border);
        double cy = b.y + 11 + u->lh / 2;
        if (!ui_icon(u, GOOME_ICON_TOAST, ui_bx(b.x + 9, cy - 9, 18, 18), th->accent)) {
            ui_circle(u, b.x + 18, cy, 4, th->accent);
            ui_circle(u, b.x + 18, cy, 8, ui_fade(th->accent, 0.18));
        }
        for (int j = 0; j < n; j++) ui_text_mid(u, b.x + 34, b.y + 11 + u->lh * (j + 0.5), t->text + st[j], ln[j], 0, th->text);
    }
    u->anim_next = 1;
    ui_leave(u, &sv);
}

void goome_ui_statusbar(goome_ui *u, const char *text)
{
    if (u) snprintf(u->status, sizeof(u->status), "%s", text ? text : "");
}

static int ui_capbtn(goome_ui *u, uint32_t id, ui_box b, int kind, int max, goome_radii r)
{
    const goome_theme *th = &u->th;
    int hov, held, click = ui_button_logic(u, id, b, &hov, &held);
    ui_ax_btn(u, id, b, kind == 0 ? "Minimize" : kind == 2 ? "Close" : max ? "Restore" : "Maximize", -1);
    ui_st *s = ui_get(u, id, NULL);
    double k = ui_anim(u, s, 0, hov || held);
    uint32_t bg = kind == 2 ? ui_red(u, held, 0xFFE5484D, 0xFFB8342F) : ui_mix(th->hover, th->active, held);
    if (k > 0) ui_rrect(u, b, r, ui_fade(bg, k));
    uint32_t ic = kind == 2 ? ui_mix(th->text_dim, 0xFFFFFFFF, k) : ui_mix(th->text_dim, th->text, k);
    if (u->unfocused && k <= 0) ic = ui_fade(ic, 0.6);
    double cx = ui_round(b.x + b.w / 2), cy = ui_round(b.y + b.h / 2), d = ui_round(5 * u->em), lw = 1.2 * u->em;
    int ik = kind == 0 ? GOOME_ICON_MINIMIZE : kind == 1 ? (max ? GOOME_ICON_RESTORE : GOOME_ICON_MAXIMIZE) : GOOME_ICON_CLOSE;
    if (ui_icon(u, ik, ui_bx(cx - d - 1, cy - d - 1, 2 * d + 2, 2 * d + 2), ic)) return click;
    if (kind == 0) ui_rect(u, ui_bx(cx - d, cy - lw / 2, 2 * d, lw), 0, ic);
    else if (kind == 1 && max) {
        ui_outline(u, ui_bx(cx - d, cy - d + 2.5, 2 * d - 2.5, 2 * d - 2.5), 1.5, lw, ic);
        ui_seg(u, cx - d + 2.5, cy - d, cx + d - 1.5, cy - d, lw, ic);
        ui_seg(u, cx + d, cy - d + 1.5, cx + d, cy + d - 2.5, lw, ic);
    } else if (kind == 1)
        ui_outline(u, ui_bx(cx - d, cy - d, 2 * d, 2 * d), 2, lw, ic);
    else {
        ui_seg(u, cx - d, cy - d, cx + d, cy + d, lw * 1.1, ic);
        ui_seg(u, cx + d, cy - d, cx - d, cy + d, lw * 1.1, ic);
    }
    return click;
}

int goome_ui_app(goome_ui *u, const char *title, int flags)
{
    if (!u || !u->c) return 0;
    const goome_theme *th = &u->th;
    goome *g = u->g;
    goome_rect a = u->area;
    ui_box box = ui_bx(a.x, a.y, a.w, a.h);
    int win = goome_mode(g) == GOOME_MODE_WINDOW, wf = g->opts.window_flags;
    if (wf & GOOME_WINDOW_NO_TITLEBAR) flags &= ~GOOME_APP_TITLEBAR;
    int st = win ? goome_window_get_state(g) : GOOME_STATE_NORMAL;
    if (st < 0) st = GOOME_STATE_NORMAL;
    int max = st == GOOME_STATE_MAXIMIZED || st == GOOME_STATE_FULLSCREEN;
    int bl = win && (wf & GOOME_WINDOW_BORDERLESS);
    double r = bl && (wf & GOOME_WINDOW_TRANSPARENT) && !max ? th->radius : 0;
    u->app_on = 1, u->app_flags = flags, u->app_box = box, u->app_r = r;
    uint32_t bar = th->panel | 0xFF000000u, id = ui_hash(0x41505030u, "##app", 5);
    ui_rect(u, box, r, th->bg | 0xFF000000u);
    ui_ax_push(u, ui_ax_add(u, id, box, GOOME_ROLE_WINDOW, title, title ? ui_shown(title) : 0, 0, 0), UI_AXA);
    double y = a.y;
    if (flags & GOOME_APP_TITLEBAR) {
        double h = th->app_title_h > 0 ? ui_round(th->app_title_h) : ui_round(u->lh + 20), bw = ui_round(46 * u->em);
        ui_box tb = ui_bx(a.x, y, a.w, h);
        goome_radii tr = goome_radius(r);
        tr.bl = tr.br = 0;
        if (!ui_skin(u, &th->skin_titlebar, tb, 0, 0)) {
            ui_rrect(u, tb, tr, bar);
            if (!(flags & GOOME_APP_MENUBAR)) ui_rect(u, ui_bx(a.x, y + h - 1, a.w, 1), 0, th->border);
        }
        double bx = a.x + a.w - 3 * bw;
        goome_radii cr = goome_radius(0);
        cr.tr = r;
        if (ui_capbtn(u, ui_hashi(id, 1), ui_bx(bx, y, bw, h), 0, 0, goome_radius(0))) goome_window_state(g, GOOME_STATE_MINIMIZED);
        if (ui_capbtn(u, ui_hashi(id, 2), ui_bx(bx + bw, y, bw, h), 1, max, goome_radius(0))) goome_window_state(g, max ? GOOME_STATE_NORMAL : GOOME_STATE_MAXIMIZED);
        if (ui_capbtn(u, ui_hashi(id, 3), ui_bx(bx + 2 * bw, y, a.w - (bx + 2 * bw - a.x), h), 2, 0, cr)) {
            goome_event e;
            memset(&e, 0, sizeof(e));
            e.type = GOOME_CLOSE;
            goome_push(g, &e);
        }
        const char *t = title ? title : "";
        int n = ui_shown(t);
        double tw = ui_tw(u, t, n, 0), tx = ui_round(a.x + (a.w - tw) / 2);
        if (tx + tw > bx - 12) tx = ui_round(ui_max(a.x + 14, bx - 12 - tw));
        ui_clip_push(u, ui_bx(a.x + 10, y, ui_max(0, bx - 10 - a.x - 10), h));
        ui_text_mid(u, tx, y + h / 2, t, n, 0, u->unfocused ? th->text_dim : th->text);
        ui_clip_pop(u);
        uint32_t did = ui_hashi(id, 4);
        if (u->pressed && !u->active && u->hover_layer == UI_ROOT && ui_in(ui_bx(a.x, y, bx - a.x, h), u->mx, u->my)) {
            u->active = did;
            if (u->clicks == 2) goome_window_state(g, max ? GOOME_STATE_NORMAL : GOOME_STATE_MAXIMIZED);
            else goome_window_drag(g, 0);
        }
        y += h;
    }
    if (flags & GOOME_APP_MENUBAR) {
        double h = th->menubar_h > 0 ? ui_round(th->menubar_h) : ui_round(u->lh + 14);
        u->app_mb = ui_bx(a.x, y, a.w, h);
        goome_radii mr = goome_radius(0);
        if (!(flags & GOOME_APP_TITLEBAR)) mr.tl = mr.tr = r;
        ui_rrect(u, u->app_mb, mr, bar);
        ui_rect(u, ui_bx(a.x, y + h - 1, a.w, 1), 0, th->border);
        y += h;
    }
    double bottom = a.y + a.h;
    if (flags & GOOME_APP_STATUSBAR) {
        double h = th->statusbar_h > 0 ? ui_round(th->statusbar_h) : ui_round(u->lh + 10);
        u->app_sb = ui_bx(a.x, bottom - h, a.w, h);
        bottom -= h;
    }
    if (bl && (wf & GOOME_WINDOW_RESIZABLE) && !max && u->hover_layer == UI_ROOT && !u->active) {
        double e = th->grip / 3;
        int ed = (u->mx >= a.x && u->mx < a.x + e ? GOOME_EDGE_LEFT : 0) | (u->mx < a.x + a.w && u->mx >= a.x + a.w - e ? GOOME_EDGE_RIGHT : 0) |
                 (u->my >= a.y && u->my < a.y + e ? GOOME_EDGE_TOP : 0) | (u->my < a.y + a.h && u->my >= a.y + a.h - e ? GOOME_EDGE_BOTTOM : 0);
        if (!ui_in(box, u->mx, u->my)) ed = 0;
        if (ed) {
            int lr = ed & 3, tb = ed & 12;
            u->cur_want = lr && tb ? (((ed & GOOME_EDGE_LEFT) != 0) == ((ed & GOOME_EDGE_TOP) != 0) ? GOOME_CURSOR_RESIZE_NWSE : GOOME_CURSOR_RESIZE_NESW)
                          : lr ? GOOME_CURSOR_RESIZE_EW : GOOME_CURSOR_RESIZE_NS;
            if (u->pressed) {
                goome_window_drag(g, ed);
                u->active = ui_hashi(id, 5), u->pressed = 0, u->released = 0;
            }
        }
    }
    ui_box body = ui_bx(a.x, y, a.w, ui_max(0, bottom - y));
    ui_frame *f = &u->fr[0];
    double pad = (flags & GOOME_APP_NO_PAD) ? 0 : th->pad + 4;
    ui_lay_init(&f->lay, body.x + pad, body.y + pad, body.w - 2 * pad);
    f->box = body;
    u->page_bottom = body.y + body.h + th->pad - pad;
    ui_hit_add(u, box);
    return 1;
}

void goome_ui_app_end(goome_ui *u)
{
    if (!u || !u->c || !u->app_on) return;
    const goome_theme *th = &u->th;
    ui_list *cl = u->cl;
    u->cl = &u->root;
    if (u->app_flags & GOOME_APP_STATUSBAR) {
        ui_box b = u->app_sb;
        goome_radii r = goome_radius(0);
        r.bl = r.br = u->app_r;
        ui_rrect(u, b, r, th->panel | 0xFF000000u);
        ui_rect(u, ui_bx(b.x, b.y, b.w, 1), 0, th->border);
        int n = (int)strlen(u->status);
        goome_font *keep = u->th.font;
        if (th->font_small) u->th.font = th->font_small;
        ui_clip_push(u, ui_bx(b.x, b.y, b.w - 10, b.h));
        ui_text_mid(u, b.x + th->pad + 4, b.y + b.h / 2 + 0.5, u->status, n, th->small_size > 0 ? th->small_size : ui_round(u->size * 0.93), th->text_dim);
        ui_ax_add(u, ui_hashi(ui_hash(0x41505030u, "##app", 5), 6), b, GOOME_ROLE_TEXT, u->status, n, 0, 0);
        ui_clip_pop(u);
        u->th.font = keep;
    }
    if (u->app_r > 0) ui_outline(u, u->app_box, u->app_r, 1, ui_mix(th->border, 0x40FFFFFFu, 0.3));
    ui_ax_pop(u, UI_AXA, NULL);
    u->cl = cl;
}

int goome_ui_focused(goome_ui *u) { return u && u->c && u->focus && u->last_id == u->focus; }
void goome_ui_set_focus(goome_ui *u)
{
    if (u) u->want_focus = 1;
}

static void ui_arc(goome_ui *u, double cx, double cy, double r, double w, double a0, double a1, uint32_t col)
{
    if (!ui_vis(u, cx - r - w, cy - r - w, 2 * (r + w), 2 * (r + w))) return;
    ui_cmd *c = ui_cmd_new(u, UC_ARC);
    if (!c) return;
    c->a[0] = cx, c->a[1] = cy, c->a[2] = r, c->a[3] = w, c->a[4] = a0, c->a[5] = a1;
    c->c0 = ui_col(u, col);
}

static void ui_poly(goome_ui *u, const double *xy, int n, uint32_t col)
{
    if (n < 3) return;
    double x0 = xy[0], y0 = xy[1], x1 = xy[0], y1 = xy[1];
    for (int i = 1; i < n; i++) x0 = ui_min(x0, xy[2 * i]), x1 = ui_max(x1, xy[2 * i]), y0 = ui_min(y0, xy[2 * i + 1]), y1 = ui_max(y1, xy[2 * i + 1]);
    if (!ui_vis(u, x0, y0, x1 - x0, y1 - y0)) return;
    if (u->lay_list) ui_lay_flush(u);
    ui_list *l = u->cl;
    int need = (int)sizeof(double) * 2 * n;
    if (l->tn + need > l->tcap) {
        int cap = l->tcap ? l->tcap : 4096;
        while (cap < l->tn + need) cap *= 2;
        char *t = realloc(l->txt, (size_t)cap);
        if (!t) return;
        l->txt = t, l->tcap = cap;
    }
    ui_cmd *c = ui_cmd_new(u, UC_POLY);
    if (!c) return;
    memcpy(l->txt + l->tn, xy, (size_t)need);
    c->text = l->tn, c->flag = n;
    l->tn += need;
    c->c0 = ui_col(u, col);
}

static void ui_nine(goome_ui *u, const goome_skin *k, ui_box b)
{
    ui_put(u, UC_NINE, b.x, b.y, b.w, b.h, goome_radius(0), 0, 0, 0, 0);
    if (u->cl->n && u->cl->cmd[u->cl->n - 1].type == UC_NINE) {
        ui_cmd *c = &u->cl->cmd[u->cl->n - 1];
        c->bm = k->bm, c->r.tl = k->l, c->r.tr = k->t, c->r.br = k->r, c->r.bl = k->b;
    }
}

static double ui_skin_r(const goome_skin *k, ui_box b)
{
    double fx = k->l + k->r > b.w ? b.w / (k->l + k->r) : 1, fy = k->t + k->b > b.h ? b.h / (k->t + k->b) : 1;
    return ui_min(k->l * fx, k->t * fy);
}

static int ui_skin(goome_ui *u, const goome_skin *k, ui_box b, double hov, double held)
{
    if (!k->bm || !k->bm->px) return 0;
    ui_nine(u, k, b);
    double r = ui_skin_r(k, b);
    if (hov > 0) ui_rect(u, b, r, ui_fade(0xFFFFFFFF, 0.07 * hov));
    if (held > 0) ui_rect(u, b, r, ui_fade(0xFF000000, 0.12 * held));
    return 1;
}

static int ui_has_icon(goome_ui *u, int kind)
{
    const goome_bitmap *b = kind >= 0 && kind < GOOME_ICON_COUNT ? u->th.icon[kind] : NULL;
    return b && b->px && b->w > 0 && b->h > 0;
}

static int ui_icon(goome_ui *u, int kind, ui_box b, uint32_t col)
{
    if (!ui_has_icon(u, kind)) return 0;
    const goome_bitmap *bm = u->th.icon[kind];
    double s = u->c->scale > 0 ? u->c->scale : 1, k = ui_min(b.w / bm->w, b.h / bm->h);
    if (!(k > 0)) return 1;
    double w = ui_max(1, ui_round(bm->w * k * s)) / s, h = ui_max(1, ui_round(bm->h * k * s)) / s;
    double x = ui_round((b.x + (b.w - w) / 2) * s) / s, y = ui_round((b.y + (b.h - h) / 2) * s) / s;
    ui_bitmap(u, bm, ui_bx(x, y, w, h), goome_radius(0), u->th.icon_tint == 1 ? (col | 0xFFFFFFu) : col);
    return 1;
}

static int ui_icon2(goome_ui *u, int a, int b, ui_box box, uint32_t col, double t)
{
    int ha = ui_has_icon(u, a), hb = ui_has_icon(u, b);
    if (!ha || !hb) return ui_icon(u, t < 0.5 ? a : b, box, col);
    if (t < 1) ui_icon(u, a, box, ui_fade(col, 1 - t));
    if (t > 0) ui_icon(u, b, box, ui_fade(col, t));
    return 1;
}

static double ui_fill_h(goome_ui *u)
{
    ui_lay *l = ui_lay_cur(u);
    double bottom = u->page_bottom - u->th.pad;
    if (u->nscr > ui_top(u)->nscr0 || (u->nscr && ui_top(u)->w)) {
        ui_scr *r = &u->scr[u->nscr - 1];
        bottom = r->view.y + r->view.h - r->pad - r->off;
    }
    return ui_max(60, bottom - (l->same && l->any ? l->line_y : l->ny));
}

static int ui_cont(char c) { return ((unsigned char)c & 0xC0) == 0x80; }

static int ui_parse(const char *s, double *out)
{
    while (*s == ' ') s++;
    int neg = 0, any = 0;
    if (*s == '-' || *s == '+') neg = *s++ == '-';
    double v = 0, f = 1;
    while (*s >= '0' && *s <= '9') v = v * 10 + (*s++ - '0'), any = 1;
    if (*s == '.' || *s == ',') {
        s++;
        while (*s >= '0' && *s <= '9') f /= 10, v += (*s++ - '0') * f, any = 1;
    }
    if (!any) return 0;
    if (*s == 'e' || *s == 'E') {
        const char *p = s + 1;
        int en = 0, ev = 0, ok = 0;
        if (*p == '-' || *p == '+') en = *p++ == '-';
        while (*p >= '0' && *p <= '9') ev = ev < 400 ? ev * 10 + (*p++ - '0') : ev, ok = 1;
        if (ok)
            for (int i = 0; i < ev && v != 0; i++) v = en ? v / 10 : v * 10;
    }
    *out = neg ? -v : v;
    return 1;
}

typedef struct {
    uint32_t h;
    float w, rw;
    int rows;
} ui_lc;

typedef struct {
    int pos, dn, in, c0, a0, c1, kind;
    char *d;
} ui_op;

typedef struct ui_ed_s {
    uint32_t id;
    int seen, cur, anc, len, mode, ma, mb, brk, dirty, vov, hov_bar, nl, lcap, tcap, tn, nops, opcap, oppos, rcap, last_kind;
    const char *buf;
    goome_font *font;
    double sx, sy, tx, ty, want, maxw, wrap_at, grab, fsize;
    long blink;
    int *ls, *lr, *cr, *rb;
    uint32_t *lh;
    float *lw;
    ui_lc *tab;
    ui_op *ops;
    size_t bytes;
} ui_ed;

static void ui_ed_ops_cut(ui_ed *e, int k)
{
    for (int i = k; i < e->nops; i++) e->bytes -= (size_t)(e->ops[i].dn + e->ops[i].in), free(e->ops[i].d);
    if (k < e->nops) e->nops = k;
    if (e->oppos > e->nops) e->oppos = e->nops;
}

static void ui_ed_drop(ui_ed *e)
{
    if (!e) return;
    ui_ed_ops_cut(e, 0);
    free(e->ls), free(e->lr), free(e->cr), free(e->rb), free(e->lh), free(e->lw), free(e->tab), free(e->ops);
    free(e);
}

static void ui_ed_free_all(goome_ui *u)
{
    for (int i = 0; i < UI_MAXED; i++) ui_ed_drop(u->eds[i]), u->eds[i] = NULL;
}

static ui_ed *ui_ed_get(goome_ui *u, uint32_t id)
{
    int spare = -1;
    for (int i = 0; i < UI_MAXED; i++) {
        if (u->eds[i] && u->eds[i]->id == id) {
            u->eds[i]->seen = u->frame;
            return u->eds[i];
        }
        if (!u->eds[i] && spare < 0) spare = i;
    }
    if (spare < 0) {
        spare = 0;
        for (int i = 1; i < UI_MAXED; i++)
            if (u->eds[i]->seen < u->eds[spare]->seen) spare = i;
        ui_ed_drop(u->eds[spare]);
        u->eds[spare] = NULL;
    }
    ui_ed *e = calloc(1, sizeof(*e));
    if (!e) return NULL;
    e->id = id, e->seen = u->frame, e->want = -1, e->dirty = 1, e->wrap_at = -1, e->blink = u->now;
    u->eds[spare] = e;
    return e;
}

static double ui_ew(goome_ui *u, char *s, int a, int b)
{
    if (b <= a) return 0;
    double w = 0;
    while (b - a > 384) {
        int m = a + 256;
        while (m > a + 1 && ui_cont(s[m])) m--;
        char k = s[m];
        s[m] = 0;
        w += goome_text_measure(u->th.font, u->size, s + a, NULL);
        s[m] = k;
        a = m;
    }
    char k = s[b];
    s[b] = 0;
    w += goome_text_measure(u->th.font, u->size, s + a, NULL);
    s[b] = k;
    return w;
}

static int ui_ehit(goome_ui *u, char *s, int a, int b, double x)
{
    if (x <= 0) return a;
    int lo = a, hi = b;
    while (lo < hi) {
        int mid = lo + (hi - lo + 1) / 2;
        while (mid < hi && ui_cont(s[mid])) mid++;
        if (ui_ew(u, s, a, mid) <= x) lo = mid;
        else hi = ui_u8_prev(s, mid);
        if (hi < lo) hi = lo;
    }
    if (lo < b) {
        int nx = ui_u8_next(s, lo, b);
        double w0 = ui_ew(u, s, a, lo), w1 = ui_ew(u, s, a, nx);
        if (x - w0 > w1 - x) lo = nx;
    }
    return lo;
}

static int ui_erows(goome_ui *u, ui_ed *e, char *s, int a, int b, double ww, int fill)
{
    int n = 1;
    if (fill) e->rb[0] = a;
    if (ww <= 0 || b <= a) return 1;
    double x = 0;
    int i = a;
    while (i < b) {
        int j = i, k;
        while (j < b && s[j] != ' ') j++;
        k = j;
        while (k < b && s[k] == ' ') k++;
        double w = ui_ew(u, s, i, j);
        if (x > 0 && x + w > ww) {
            if (fill && n < e->rcap) e->rb[n] = i;
            n++, x = 0;
        }
        if (w > ww) {
            int p = i;
            while (p < j) {
                int q = ui_u8_next(s, p, j);
                double cw = ui_ew(u, s, p, q);
                if (x > 0 && x + cw > ww) {
                    if (fill && n < e->rcap) e->rb[n] = p;
                    n++, x = 0;
                }
                x += cw, p = q;
            }
        } else
            x += w;
        x += ui_ew(u, s, j, k);
        i = k;
    }
    return n;
}

static uint32_t ui_lhash(const char *s, int n)
{
    uint32_t h = 2166136261u ^ (uint32_t)n;
    for (int i = 0; i < n; i++) h = (h ^ (unsigned char)s[i]) * 16777619u;
    return h ? h : 1;
}

static ui_lc *ui_lc_find(ui_ed *e, uint32_t h, int add)
{
    if (!e->tab) return NULL;
    uint32_t m = (uint32_t)e->tcap - 1, p = (h * 2654435761u) & m;
    while (e->tab[p].h) {
        if (e->tab[p].h == h) return &e->tab[p];
        p = (p + 1) & m;
    }
    if (!add || e->tn * 2 >= e->tcap) return NULL;
    e->tab[p].h = h, e->tab[p].rw = -1, e->tn++;
    return &e->tab[p];
}

static int ui_ed_grow(ui_ed *e, int n)
{
    if (n < e->lcap) return 1;
    int cap = e->lcap ? e->lcap : 256;
    while (cap <= n) cap *= 2;
    int *a = realloc(e->ls, sizeof(int) * (size_t)cap);
    if (a) e->ls = a;
    int *b = realloc(e->lr, sizeof(int) * (size_t)cap);
    if (b) e->lr = b;
    int *c = realloc(e->cr, sizeof(int) * (size_t)(cap + 1));
    if (c) e->cr = c;
    uint32_t *d = realloc(e->lh, sizeof(uint32_t) * (size_t)cap);
    if (d) e->lh = d;
    float *f = realloc(e->lw, sizeof(float) * (size_t)cap);
    if (f) e->lw = f;
    if (!a || !b || !c || !d || !f) return 0;
    for (int i = e->lcap; i < cap; i++) e->lh[i] = 0, e->lw[i] = 0, e->lr[i] = 1;
    e->lcap = cap;
    return 1;
}

static void ui_ed_scan(goome_ui *u, ui_ed *e, char *s, int len, double ww)
{
    int old = e->nl, same_w = ww == e->wrap_at;
    if (!same_w) {
        for (int i = 0; i < e->tcap; i++) e->tab[i].rw = -1;
        e->wrap_at = ww;
    }
    int n = 0, i = 0;
    double mw = 0;
    for (;;) {
        if (!ui_ed_grow(e, n + 1)) break;
        const char *p = memchr(s + i, '\n', (size_t)(len - i));
        int end = p ? (int)(p - s) : len;
        uint32_t h = ui_lhash(s + i, end - i);
        e->ls[n] = i;
        if (!(n < old && e->lh[n] == h && same_w)) {
            if (e->tn * 2 >= e->tcap) {
                int cap = 1024;
                while (cap < e->lcap * 4) cap *= 2;
                ui_lc *t = cap == e->tcap ? e->tab : calloc((size_t)cap, sizeof(ui_lc));
                if (t == e->tab && t) memset(t, 0, sizeof(ui_lc) * (size_t)cap);
                else if (t) free(e->tab), e->tab = t, e->tcap = cap;
                e->tn = 0;
            }
            ui_lc *c = ui_lc_find(e, h, 0);
            if (!c) {
                c = ui_lc_find(e, h, 1);
                if (c) c->w = (float)ui_ew(u, s, i, end);
            }
            float w = c ? c->w : (float)ui_ew(u, s, i, end);
            int rows = 1;
            if (ww > 0) {
                if (c && c->rw == (float)ww) rows = c->rows;
                else {
                    rows = w <= ww ? 1 : ui_erows(u, e, s, i, end, ww, 0);
                    if (c) c->rw = (float)ww, c->rows = rows;
                }
            }
            e->lh[n] = h, e->lw[n] = w, e->lr[n] = rows;
        }
        if (e->lw[n] > mw) mw = e->lw[n];
        n++;
        if (!p) break;
        i = end + 1;
    }
    if (n == 0) e->ls[0] = 0, e->lr[0] = 1, e->lw[0] = 0, n = 1;
    e->nl = n, e->maxw = mw;
    e->cr[0] = 0;
    for (int k = 0; k < n; k++) e->cr[k + 1] = e->cr[k] + e->lr[k];
    e->dirty = 0;
}

static int ui_ed_same(const ui_ed *e, const char *s, int len)
{
    if (e->nl <= 0) return 0;
    for (int i = 0; i < e->nl; i++) {
        int a = e->ls[i];
        if (a > len) return 0;
        const char *p = memchr(s + a, '\n', (size_t)(len - a));
        if (i + 1 < e->nl ? !p || (int)(p - s) != e->ls[i + 1] - 1 : p != NULL) return 0;
    }
    return 1;
}

static int ui_ed_line(const ui_ed *e, int pos)
{
    int lo = 0, hi = e->nl - 1;
    while (lo < hi) {
        int m = (lo + hi + 1) / 2;
        if (e->ls[m] <= pos) lo = m;
        else hi = m - 1;
    }
    return lo;
}
static int ui_ed_lend(const ui_ed *e, int i, int len) { return i + 1 < e->nl ? e->ls[i + 1] - 1 : len; }

static int ui_ed_rows_of(goome_ui *u, ui_ed *e, char *s, int i, int len)
{
    int a = e->ls[i], b = ui_ed_lend(e, i, len), want = e->lr[i] + 1;
    if (want > e->rcap) {
        int cap = e->rcap ? e->rcap : 64;
        while (cap < want) cap *= 2;
        int *r = realloc(e->rb, sizeof(int) * (size_t)cap);
        if (!r) {
            if (!e->rb) return 0;
            want = e->rcap;
        } else
            e->rb = r, e->rcap = cap;
    }
    if (e->wrap_at <= 0 || e->lr[i] <= 1) {
        e->rb[0] = a;
        return 1;
    }
    int n = ui_erows(u, e, s, a, b, e->wrap_at, 1);
    return n < e->rcap ? n : e->rcap;
}

static void ui_ed_row_span(goome_ui *u, ui_ed *e, char *s, int len, int g, int *rs, int *re, int *last, int *line)
{
    if (g < 0) g = 0;
    if (g >= e->cr[e->nl]) g = e->cr[e->nl] - 1;
    int lo = 0, hi = e->nl - 1;
    while (lo < hi) {
        int m = (lo + hi + 1) / 2;
        if (e->cr[m] <= g) lo = m;
        else hi = m - 1;
    }
    int n = ui_ed_rows_of(u, e, s, lo, len), r = g - e->cr[lo];
    if (r >= n) r = n - 1;
    if (r < 0) r = 0;
    *rs = n ? e->rb[r] : e->ls[lo];
    *re = r + 1 < n ? e->rb[r + 1] : ui_ed_lend(e, lo, len);
    *last = r + 1 >= n;
    if (line) *line = lo;
}

static int ui_ed_locate(goome_ui *u, ui_ed *e, char *s, int len, int pos, double *x)
{
    int i = ui_ed_line(e, pos), n = ui_ed_rows_of(u, e, s, i, len), r = 0;
    while (r + 1 < n && e->rb[r + 1] <= pos) r++;
    int rs = n ? e->rb[r] : e->ls[i], re = r + 1 < n ? e->rb[r + 1] : ui_ed_lend(e, i, len);
    if (x && ui_bd(u, s + rs, re - rs)) {
        gm_tc *b;
        int nb = ui_bd_lay(u, s + rs, re - rs, &b);
        *x = ui_bd_x(b, nb, pos - rs);
        free(b);
    } else if (x)
        *x = ui_ew(u, s, rs, pos);
    return e->cr[i] + r;
}

static int ui_ed_hit_row(goome_ui *u, ui_ed *e, char *s, int len, int g, double x)
{
    int rs, re, last;
    ui_ed_row_span(u, e, s, len, g, &rs, &re, &last, NULL);
    if (!last && re > rs) re = s[re - 1] == ' ' ? re - 1 : ui_u8_prev(s, re);
    if (re < rs) re = rs;
    if (ui_bd(u, s + rs, re - rs)) {
        gm_tc *b;
        int nb = ui_bd_lay(u, s + rs, re - rs, &b), at = ui_bd_hit(b, nb, x);
        free(b);
        return rs + at;
    }
    return ui_ehit(u, s, rs, re, x);
}

static int ui_ed_step(goome_ui *u, ui_ed *e, char *s, int len, int c, int right)
{
    int rs, re, last, line, g = ui_ed_locate(u, e, s, len, c, NULL);
    ui_ed_row_span(u, e, s, len, g, &rs, &re, &last, &line);
    int bd = ui_bd(u, s + rs, re - rs);
    if (!bd) return right ? gm_text_next(s, c, len) : gm_text_prev(s, c);
    gm_tc *b;
    int nb = ui_bd_lay(u, s + rs, re - rs, &b), to = ui_bd_move(b, nb, c - rs, right);
    free(b);
    if (to >= 0) return rs + to;
    if (right != (bd == 2)) return re < len && s[re] == '\n' ? re + 1 : re;
    return rs > 0 && s[rs - 1] == '\n' ? rs - 1 : gm_text_prev(s, rs);
}

static void ui_ed_record(ui_ed *e, int pos, const char *del, int dn, const char *ins, int in, int c0, int a0, int c1, int kind)
{
    ui_ed_ops_cut(e, e->oppos);
    if (kind && !e->brk && e->oppos > 0) {
        ui_op *o = &e->ops[e->oppos - 1];
        int ok = 0;
        if (kind == 1 && o->kind == 1 && dn == 0 && o->pos + o->in == pos) ok = 1;
        if (kind == 2 && o->kind == 2 && in == 0 && o->in == 0 && pos + dn == o->pos) ok = 2;
        if (kind == 3 && o->kind == 3 && in == 0 && o->in == 0 && pos == o->pos) ok = 3;
        if (ok) {
            char *d = realloc(o->d, (size_t)(o->dn + o->in + dn + in) + 1);
            if (d) {
                o->d = d;
                if (ok == 1) memcpy(d + o->dn + o->in, ins, (size_t)in), o->in += in;
                else if (ok == 2) memmove(d + dn, d, (size_t)o->dn), memcpy(d, del, (size_t)dn), o->dn += dn, o->pos = pos;
                else memcpy(d + o->dn, del, (size_t)dn), o->dn += dn;
                o->c1 = c1;
                e->bytes += (size_t)(dn + in);
                return;
            }
        }
    }
    while (e->nops > 0 && (e->nops >= 2000 || e->bytes > ((size_t)8 << 20))) {
        e->bytes -= (size_t)(e->ops[0].dn + e->ops[0].in);
        free(e->ops[0].d);
        memmove(e->ops, e->ops + 1, sizeof(ui_op) * (size_t)(e->nops - 1));
        e->nops--;
    }
    if (e->nops == e->opcap) {
        int cap = e->opcap ? e->opcap * 2 : 64;
        ui_op *n = realloc(e->ops, sizeof(ui_op) * (size_t)cap);
        if (!n) return;
        e->ops = n, e->opcap = cap;
    }
    char *d = malloc((size_t)(dn + in) + 1);
    if (!d) return;
    if (dn) memcpy(d, del, (size_t)dn);
    if (in) memcpy(d + dn, ins, (size_t)in);
    ui_op *o = &e->ops[e->nops++];
    o->pos = pos, o->dn = dn, o->in = in, o->c0 = c0, o->a0 = a0, o->c1 = c1, o->kind = kind, o->d = d;
    e->oppos = e->nops;
    e->bytes += (size_t)(dn + in);
    e->brk = 0;
}

static int ui_ed_edit(ui_ed *e, char *s, int *len, int cap, int a, int b, const char *ins, int n, int kind)
{
    if (a > b) {
        int t = a;
        a = b, b = t;
    }
    int dn = b - a, room = cap - 1 - (*len - dn);
    if (n > room) {
        n = room < 0 ? 0 : room;
        while (n > 0 && ui_cont(ins[n])) n--;
    }
    if (dn == 0 && n <= 0) return 0;
    ui_ed_record(e, a, s + a, dn, ins, n, e->cur, e->anc, a + n, kind);
    memmove(s + a + n, s + b, (size_t)(*len - b + 1));
    if (n) memcpy(s + a, ins, (size_t)n);
    *len += n - dn;
    e->cur = e->anc = a + n;
    e->dirty = 1, e->len = *len, e->want = -1;
    return 1;
}

static int ui_ed_undo(ui_ed *e, char *s, int *len, int cap, int redo)
{
    if (redo ? e->oppos >= e->nops : e->oppos <= 0) return 0;
    ui_op *o = &e->ops[redo ? e->oppos : e->oppos - 1];
    const char *want = redo ? o->d : o->d + o->dn, *put = redo ? o->d + o->dn : o->d;
    int wn = redo ? o->dn : o->in, pn = redo ? o->in : o->dn;
    if (o->pos < 0 || o->pos + wn > *len || memcmp(s + o->pos, want, (size_t)wn) || *len - wn + pn > cap - 1) {
        ui_ed_ops_cut(e, 0);
        return 0;
    }
    memmove(s + o->pos + pn, s + o->pos + wn, (size_t)(*len - o->pos - wn + 1));
    if (pn) memcpy(s + o->pos, put, (size_t)pn);
    *len += pn - wn;
    if (redo) e->cur = e->anc = o->c1, e->oppos++;
    else e->cur = o->c0, e->anc = o->a0, e->oppos--;
    if (e->cur > *len) e->cur = *len;
    if (e->anc > *len) e->anc = *len;
    e->dirty = 1, e->len = *len, e->brk = 1, e->want = -1;
    return 1;
}

static void ui_ed_copy(goome_ui *u, const char *s, int a, int b, int nl)
{
    char *t = malloc((size_t)(b - a) + 2);
    if (!t) return;
    memcpy(t, s + a, (size_t)(b - a));
    t[b - a] = '\n';
    t[b - a + nl] = 0;
    goome_clipboard_set(u->g, t);
    free(t);
}

static char *ui_ed_paste(goome_ui *u, int *n)
{
    char small[512];
    int got = goome_clipboard_get(u->g, small, sizeof(small));
    *n = 0;
    if (got <= 0) return NULL;
    char *t = malloc((size_t)got * 4 + 1);
    if (!t) return NULL;
    if (got < (int)sizeof(small)) memcpy(t, small, (size_t)got + 1);
    else {
        got = goome_clipboard_get(u->g, t, got + 1);
        if (got < 0) got = 0;
    }
    char *src = malloc((size_t)got + 1);
    if (!src) {
        free(t);
        return NULL;
    }
    memcpy(src, t, (size_t)got);
    int w = 0;
    for (int i = 0; i < got && src[i]; i++) {
        unsigned char c = (unsigned char)src[i];
        if (c == '\r') {
            t[w++] = '\n';
            if (i + 1 < got && src[i + 1] == '\n') i++;
        } else if (c == '\t')
            memcpy(t + w, "    ", 4), w += 4;
        else if (c == '\n' || (c >= 32 && c != 127))
            t[w++] = (char)c;
    }
    free(src);
    t[w] = 0;
    *n = w;
    return t;
}

static int ui_ed_indent(ui_ed *e, char *s, int *len, int cap, int lo, int hi, int out)
{
    int a = lo;
    while (a > 0 && s[a - 1] != '\n') a--;
    int b = hi;
    if (b > lo && s[b - 1] == '\n') b--;
    while (b < *len && s[b] != '\n') b++;
    int lines = 1;
    for (int i = a; i < b; i++) lines += s[i] == '\n';
    char *t = malloc((size_t)(b - a) + (size_t)lines * 4 + 1);
    if (!t) return 0;
    int w = 0, i = a;
    while (i <= b) {
        if (out) {
            int k = 0;
            while (k < 4 && i + k < b && s[i + k] == ' ') k++;
            i += k;
        } else if (i < b && s[i] != '\n')
            memcpy(t + w, "    ", 4), w += 4;
        while (i < b && s[i] != '\n') t[w++] = s[i++];
        if (i < b) t[w++] = s[i];
        i++;
    }
    int c0 = e->cur, ch = 0;
    if (*len - (b - a) + w <= cap - 1 && (w != b - a || memcmp(t, s + a, (size_t)w))) {
        ch = ui_ed_edit(e, s, len, cap, a, b, t, w, 0);
        if (ch && lo == hi) {
            int nc = c0 + w - (b - a);
            e->cur = e->anc = nc < a ? a : nc > a + w ? a + w : nc;
        } else if (ch)
            e->anc = a, e->cur = a + w;
    }
    free(t);
    return ch;
}

static goome_font *ui_mono(goome_ui *u)
{
    if (u->th.font_mono) return u->th.font_mono;
    if (!u->th.font) return NULL;
    if (!u->mono_tried) {
        static const char *const names[] = {"monospace", "dejavusansmono", "consolas", "menlo", "liberationmono", "cascadiamono", "couriernew", NULL};
        u->mono_tried = 1;
        for (int i = 0; names[i] && !u->mono; i++) u->mono = goome_font_system(names[i]);
    }
    return u->mono ? u->mono : u->th.font;
}

static int ui_bar(goome_ui *u, uint32_t id, ui_box track, int vert, double view, double content, double *target, double *drawn)
{
    double maxo = content - view;
    if (maxo <= 0.5) return 0;
    double len = vert ? track.h : track.w;
    double th = ui_clamp(len * view / content, ui_min(u->th.scroll_min, len), len), sw = u->th.scroll_w;
    double pos = (len - th) * ui_clamp(*drawn / maxo, 0, 1);
    ui_st *s = ui_get(u, id, NULL);
    int hov = ui_hover(u, id, track);
    if (hov && u->pressed && !u->active) {
        u->active = id;
        double m = vert ? u->my - track.y : u->mx - track.x;
        s->v[3] = m >= pos && m < pos + th ? m - pos : th / 2;
    }
    int held = u->active == id && (u->down || u->pressed);
    ui_grabby(u, id);
    if (held && len > th) {
        double m = (vert ? u->my - track.y : u->mx - track.x) - s->v[3];
        *target = *drawn = ui_clamp(m / (len - th), 0, 1) * maxo;
        pos = (len - th) * ui_clamp(*drawn / maxo, 0, 1);
    }
    double a = ui_anim(u, s, 0, hov || held), bw = sw + 2 * a;
    ui_box k = vert ? ui_bx(track.x + track.w - bw - 1, track.y + pos, bw, th) : ui_bx(track.x + pos, track.y + track.h - bw - 1, th, bw);
    if (!ui_skin(u, &u->th.skin_scrollbar, k, a, held)) ui_rect(u, k, bw / 2, ui_fade(u->th.text, 0.2 + 0.25 * a));
    return hov || held;
}

int goome_ui_text_edit(goome_ui *u, const char *id, char *buf, int cap, double h, int flags)
{
    if (!u || !u->c || !buf || cap <= 0) return 0;
    const goome_theme *th = &u->th;
    int ro = flags & GOOME_EDIT_READONLY, wrap = flags & GOOME_EDIT_WRAP, nums = flags & GOOME_EDIT_LINE_NUMBERS, ch = 0;
    if (h <= 0) h = ui_fill_h(u);
    ui_box b = ui_item(u, 0, h);
    uint32_t eid = ui_id(u, id);
    ui_ed *e = ui_ed_get(u, eid);
    if (!e) return 0;
    if (!ro && ui_ax_edit(u, eid, buf, cap)) ch = 1;
    ui_st *st = ui_get(u, eid, NULL);
    goome_font *keep = u->th.font;
    if (flags & GOOME_EDIT_MONO) u->th.font = ui_mono(u);
    double lh = goome_text_line_height(u->th.font, u->size), rh = ui_round(lh + 3);
    if (e->font != u->th.font || e->fsize != u->size) {
        if (e->tab) memset(e->tab, 0, sizeof(ui_lc) * (size_t)e->tcap);
        e->font = u->th.font, e->fsize = u->size, e->tn = 0, e->nl = 0, e->dirty = 1, e->wrap_at = -1;
    }
    buf[cap - 1] = 0;
    int len = (int)strlen(buf);
    if (e->buf != buf) e->buf = buf, e->cur = e->anc = 0, e->sx = e->sy = e->tx = e->ty = 0, ui_ed_ops_cut(e, 0), e->dirty = 1, e->len = len;
    else if (e->len != len) ui_ed_ops_cut(e, 0), e->len = len, e->dirty = 1;
    else if (!e->dirty && !ui_ed_same(e, buf, len)) ui_ed_ops_cut(e, 0), e->dirty = 1;
    e->cur = e->cur < 0 ? 0 : e->cur > len ? len : e->cur;
    e->anc = e->anc < 0 ? 0 : e->anc > len ? len : e->anc;
    while (e->cur > 0 && ui_cont(buf[e->cur])) e->cur--;
    while (e->anc > 0 && ui_cont(buf[e->anc])) e->anc--;
    e->ma = e->ma < 0 ? 0 : e->ma > len ? len : e->ma;
    e->mb = e->mb < e->ma ? e->ma : e->mb > len ? len : e->mb;

    double gw = 0;
    if (nums) {
        int d = 1;
        for (int n = e->nl > 0 ? e->nl : 1; n >= 10; n /= 10) d++;
        if (d < 2) d = 2;
        gw = ui_round(ui_tw(u, "0", 1, 0) * d + 22);
    }
    double px = 8, py = 6, sw = th->scroll_w;
    ui_box ta = ui_bx(b.x + gw + px, b.y + py, b.w - gw - 2 * px - (e->vov ? sw + 4 : 0), b.h - 2 * py);
    if (ta.w < 10) ta.w = 10;
    double ww = wrap ? ui_max(20, ta.w - 4) : 0;
    if (e->dirty || ww != e->wrap_at) ui_ed_scan(u, e, buf, len, ww);
    int total = e->cr[e->nl];
    int vov = total * rh > ta.h + 0.5;
    if (vov != e->vov) {
        e->vov = vov;
        ta.w = ui_max(10, b.w - gw - 2 * px - (vov ? sw + 4 : 0));
        if (wrap) ww = ui_max(20, ta.w - 4), ui_ed_scan(u, e, buf, len, ww), total = e->cr[e->nl];
    }
    double cw = wrap ? ta.w : e->maxw + 6, chh = total * rh;
    int hov_x = !wrap && cw > ta.w + 0.5;
    if (hov_x) ta.h = ui_max(rh, ta.h - (sw + 4));

    int focused0 = u->focus == eid;
    ui_nav(u, eid, b, ui_tr(u, th->radius), ro ? UN_INPUT : UN_EDIT, NULL, 0);
    ui_box vt = ui_bx(b.x + b.w - (sw + 8), b.y + 4, sw + 6, b.h - 8 - (hov_x ? sw + 6 : 0));
    ui_box ht = ui_bx(b.x + gw + 4, b.y + b.h - (sw + 8), b.w - gw - 8 - (vov ? sw + 6 : 0), sw + 6);
    uint32_t vid = ui_hashi(eid, 1), hid = ui_hashi(eid, 2);
    int on_bar = (vov && ui_in(vt, u->mx, u->my)) || (hov_x && ui_in(ht, u->mx, u->my)) || u->active == vid || u->active == hid;
    int hov = ui_hover(u, eid, b);
    int in_gut = nums && u->mx < b.x + gw;
    if ((hov && !on_bar && !in_gut) || (u->active == eid && e->mode >= 0)) u->cur_want = u->cfg.cursor[GOOME_CUR_INPUT];
    double ox = ta.x - e->sx, oy = ta.y - e->sy;
    int moved = 0;
    if (hov && !on_bar && u->pressed && !u->active) {
        u->active = eid;
        if (u->focus != eid) {
            ui_focus_to(u, eid, 0, ro ? UN_INPUT : UN_EDIT);
            u->nkeys = 0;
        }
        int g = (int)ui_floor((u->my - oy) / rh);
        int p = ui_ed_hit_row(u, e, buf, len, g, u->mx - ox), sh = u->pmods & GOOME_SHIFT;
        if (in_gut || u->clicks >= 3) {
            int i = ui_ed_line(e, p);
            e->ma = e->ls[i], e->mb = i + 1 < e->nl ? e->ls[i + 1] : len, e->mode = 2;
            e->anc = e->ma, e->cur = e->mb;
        } else if (u->clicks == 2) {
            int w = (p < len && ui_word_ch((unsigned char)buf[p])) || (p > 0 && ui_word_ch((unsigned char)buf[p - 1]));
            int a = p, z = p;
            if (buf[p] != '\n' || w)
                while (a > 0 && buf[a - 1] != '\n' && ui_word_ch((unsigned char)buf[a - 1]) == w) a--;
            while (z < len && buf[z] != '\n' && ui_word_ch((unsigned char)buf[z]) == w) z++;
            e->ma = a, e->mb = z, e->mode = 1;
            e->anc = a, e->cur = z;
        } else {
            e->mode = 0, e->cur = p;
            if (!sh) e->anc = p;
        }
        e->want = -1, e->brk = 1, moved = 1;
    } else if (u->active == eid && u->down) {
        if (u->my < ta.y) e->ty -= ui_min(rh * 2, (ta.y - u->my) * u->dt / 60), u->anim_next = 1;
        else if (u->my > ta.y + ta.h) e->ty += ui_min(rh * 2, (u->my - ta.y - ta.h) * u->dt / 60), u->anim_next = 1;
        if (!wrap && u->mx < ta.x) e->tx -= ui_min(40, (ta.x - u->mx) * u->dt / 30), u->anim_next = 1;
        else if (!wrap && u->mx > ta.x + ta.w) e->tx += ui_min(40, (u->mx - ta.x - ta.w) * u->dt / 30), u->anim_next = 1;
        int g = (int)ui_floor((u->my - oy) / rh);
        int p = ui_ed_hit_row(u, e, buf, len, g, u->mx - ox);
        if (e->mode == 1) {
            int a = p, z = p;
            while (a > 0 && ui_word_ch((unsigned char)buf[a - 1]) && buf[a - 1] != '\n') a--;
            while (z < len && ui_word_ch((unsigned char)buf[z])) z++;
            if (p < e->ma) e->anc = e->mb, e->cur = a;
            else e->anc = e->ma, e->cur = z > e->mb ? z : e->mb;
        } else if (e->mode == 2) {
            int i = ui_ed_line(e, p), a = e->ls[i], z = i + 1 < e->nl ? e->ls[i + 1] : len;
            if (a < e->ma) e->anc = e->mb, e->cur = a;
            else e->anc = e->ma, e->cur = z;
        } else if (p != e->cur)
            e->cur = p;
        if (u->moved) moved = 1, e->blink = u->now;
    } else if (u->focus == eid && u->pressed && !hov)
        ui_unfocus(u);

    e->cur = e->cur < 0 ? 0 : e->cur > len ? len : e->cur;
    e->anc = e->anc < 0 ? 0 : e->anc > len ? len : e->anc;
    if (u->focus == eid) {
        u->focus_seen = 1;
        for (int k = 0; k < u->nkeys && u->focus == eid; k++) {
            ui_key *ky = &u->keys[k];
            int key = ky->key, m = ky->mods, sh = m & GOOME_SHIFT, cmd = m & (GOOME_CTRL | GOOME_SUPER), act = ui_act(u, key, m);
            if (!key && !ky->text[0]) continue;
            if (e->dirty) ui_ed_scan(u, e, buf, len, ww);
            int lo = e->anc < e->cur ? e->anc : e->cur, hi = e->anc < e->cur ? e->cur : e->anc, has = lo != hi;
            int c = e->cur, keepx = 0, nav = 1, kind = -1;
            e->blink = u->now;
            if (key == UI_IME_KEY) {
                int n;
                char *t = ro ? NULL : ui_ime_take(u, k, 1, 0, &n);
                if (t && n) e->brk = 1, ch |= ui_ed_edit(e, buf, &len, cap, lo, hi, t, n, 0), e->brk = 1, moved = 1;
                free(t);
                continue;
            }
            if (act == GOOME_ACT_SELECT_ALL) e->anc = 0, c = len, nav = 0;
            else if (act == GOOME_ACT_COPY || act == GOOME_ACT_CUT) {
                nav = 0;
                if (has) ui_ed_copy(u, buf, lo, hi, 0);
                else {
                    int i = ui_ed_line(e, c);
                    lo = e->ls[i], hi = ui_ed_lend(e, i, len);
                    ui_ed_copy(u, buf, lo, hi, 1);
                    if (hi < len) hi++;
                    else if (lo > 0) lo--;
                }
                if (act == GOOME_ACT_CUT && !ro) e->brk = 1, ch |= ui_ed_edit(e, buf, &len, cap, lo, hi, "", 0, 0), c = e->cur;
            } else if (act == GOOME_ACT_PASTE && !ro) {
                int n;
                char *t = ui_ed_paste(u, &n);
                if (t && n) e->brk = 1, ch |= ui_ed_edit(e, buf, &len, cap, lo, hi, t, n, 0), c = e->cur;
                free(t);
                nav = 0;
            } else if ((act == GOOME_ACT_UNDO || act == GOOME_ACT_REDO) && !ro) {
                int redo = act == GOOME_ACT_REDO;
                ch |= ui_ed_undo(e, buf, &len, cap, redo);
                c = e->cur, nav = 0;
                if (ch) moved = 1;
                continue;
            } else if ((act == GOOME_ACT_DEL_WORD_LEFT || act == GOOME_ACT_DEL_WORD_RIGHT || (act < 0 && (key == GOOME_K_BACKSPACE || key == GOOME_K_DELETE))) && !ro) {
                int a0 = lo, b0 = hi, wd = act >= 0, fw = act == GOOME_ACT_DEL_WORD_RIGHT || (act < 0 && key == GOOME_K_DELETE);
                if (!has && !fw) a0 = wd ? ui_word_left(buf, c) : ui_u8_prev(buf, c), kind = 2;
                if (!has && fw) b0 = wd ? ui_word_right(buf, c, len) : ui_u8_next(buf, c, len), kind = 3;
                if (has) e->brk = 1, kind = 0;
                if (b0 > a0) ch |= ui_ed_edit(e, buf, &len, cap, a0, b0, "", 0, kind);
                c = e->cur, nav = 0;
            } else if (act == GOOME_ACT_WORD_LEFT || act == GOOME_ACT_WORD_RIGHT || (act < 0 && (key == GOOME_K_LEFT || key == GOOME_K_RIGHT))) {
                int r = act == GOOME_ACT_WORD_RIGHT || (act < 0 && key == GOOME_K_RIGHT), wd = act >= 0;
                if (has && !sh) c = r ? hi : lo;
                else if (r) c = wd ? ui_word_right(buf, c, len) : ui_ed_step(u, e, buf, len, c, 1);
                else c = wd ? ui_word_left(buf, c) : ui_ed_step(u, e, buf, len, c, 0);
            } else if ((key == GOOME_K_UP || key == GOOME_K_DOWN) && cmd && !sh) {
                e->ty += (key == GOOME_K_UP ? -rh : rh);
                continue;
            } else if (key == GOOME_K_UP || key == GOOME_K_DOWN || key == GOOME_K_PGUP || key == GOOME_K_PGDN) {
                double x;
                int g = ui_ed_locate(u, e, buf, len, c, &x), d = key == GOOME_K_UP ? -1 : key == GOOME_K_DOWN ? 1 : 0;
                int page = (int)(ta.h / rh) - 1;
                if (page < 1) page = 1;
                if (!d) d = key == GOOME_K_PGUP ? -page : page, e->ty += d * rh;
                if (e->want < 0) e->want = x;
                if (g + d < 0) c = 0;
                else if (g + d >= total) c = len;
                else c = ui_ed_hit_row(u, e, buf, len, g + d, e->want);
                keepx = 1;
            } else if (act == GOOME_ACT_DOC_START || act == GOOME_ACT_DOC_END || key == GOOME_K_HOME || key == GOOME_K_END) {
                if (act == GOOME_ACT_DOC_START || act == GOOME_ACT_DOC_END) c = act == GOOME_ACT_DOC_START ? 0 : len;
                else {
                    int g = ui_ed_locate(u, e, buf, len, c, NULL), rs, re, last, line;
                    ui_ed_row_span(u, e, buf, len, g, &rs, &re, &last, &line);
                    if (key == GOOME_K_END) c = last ? re : (re > rs && buf[re - 1] == ' ' ? re - 1 : ui_u8_prev(buf, re));
                    else if (rs == e->ls[line]) {
                        int f = rs;
                        while (f < re && buf[f] == ' ') f++;
                        c = c == f ? rs : f;
                    } else
                        c = rs;
                }
            } else if (key == GOOME_K_ESC) {
                ui_unfocus(u);
                break;
            } else if (key == GOOME_K_ENTER && !ro) {
                int i = ui_ed_line(e, lo), a = e->ls[i], z = a;
                while (z < lo && buf[z] == ' ') z++;
                char t[130];
                int n = z - a > 128 ? 128 : z - a;
                t[0] = '\n';
                memset(t + 1, ' ', (size_t)n);
                e->brk = 1;
                ch |= ui_ed_edit(e, buf, &len, cap, lo, hi, t, n + 1, 1);
                e->brk = 1, c = e->cur, nav = 0;
            } else if (key == GOOME_K_TAB && !ro) {
                int multi = 0;
                for (int i = lo; i < hi; i++) multi |= buf[i] == '\n';
                if (multi || sh) ch |= ui_ed_indent(e, buf, &len, cap, lo, hi, sh);
                else {
                    int i = ui_ed_line(e, lo), col = lo - e->ls[i];
                    e->brk = 1;
                    ch |= ui_ed_edit(e, buf, &len, cap, lo, hi, "    ", 4 - col % 4, 1);
                    e->brk = 1;
                }
                c = e->cur, nav = 0;
                if (multi || sh) {
                    moved = 1;
                    continue;
                }
            } else if ((unsigned char)ky->text[0] >= 32 && ky->text[0] != 127 && !(m & (GOOME_CTRL | GOOME_ALT | GOOME_SUPER)) && !ro) {
                int n = (int)strlen(ky->text);
                if (has) e->brk = 1;
                if (ky->text[0] == ' ' && e->last_kind != ' ') e->brk = 1;
                e->last_kind = ky->text[0] == ' ' ? ' ' : 'a';
                ch |= ui_ed_edit(e, buf, &len, cap, lo, hi, ky->text, n, has ? 0 : 1);
                if (has) e->brk = 1;
                c = e->cur, nav = 0;
            } else
                continue;
            if (nav) {
                e->brk = 1;
                if (!keepx) e->want = -1;
                e->cur = c;
                if (!sh) e->anc = c;
            } else
                e->cur = c;
            moved = 1;
        }
        u->nkeys = 0;
    }
    if (e->dirty) ui_ed_scan(u, e, buf, len, ww), total = e->cr[e->nl], chh = total * rh, cw = wrap ? ta.w : e->maxw + 6;

    if (hov && u->wheel && chh > ta.h) e->ty += u->wheel * u->cfg.scroll_lines * rh, u->wheel = 0;
    double cx;
    int cg = ui_ed_locate(u, e, buf, len, e->cur, &cx);
    if (moved || (focused0 != (u->focus == eid) && u->focus == eid)) {
        double cy = cg * rh;
        if (cy < e->ty) e->ty = cy;
        if (cy + rh > e->ty + ta.h) e->ty = cy + rh - ta.h;
        if (!wrap) {
            if (cx < e->tx + 4) e->tx = ui_max(0, cx - 40);
            if (cx > e->tx + ta.w - 6) e->tx = cx - ta.w + 40;
        }
    }
    double maxy = ui_max(0, chh - ta.h), maxx = ui_max(0, cw - ta.w);
    e->ty = ui_clamp(e->ty, 0, maxy), e->tx = ui_clamp(e->tx, 0, maxx);
    ui_follow(u, &e->sy, e->ty, st->born == u->frame || u->active == vid);
    ui_follow(u, &e->sx, e->tx, st->born == u->frame || u->active == hid);
    e->sy = ui_clamp(e->sy, 0, maxy), e->sx = ui_clamp(e->sx, 0, maxx);
    ox = ui_round(ta.x - e->sx), oy = ui_round(ta.y - e->sy);

    int focused = u->focus == eid;
    double hv = ui_anim(u, st, 0, hov), fk = ui_anim_ms(u, st, 1, focused, th->anim_ms), fg = th->focus_gap;
    goome_radii rr = ui_tr(u, th->radius);
    if (fk > 0) ui_rline(u, ui_bx(b.x - fg, b.y - fg, b.w + 2 * fg, b.h + 2 * fg), ui_rgrow(rr, fg), th->focus_w + 1, ui_fade(ui_c(th->focus, th->accent), 0.28 * fk));
    if (!ui_skin(u, &th->skin_input, b, hv * (1 - fk), 0)) ui_rrect(u, b, rr, ui_mix(th->input_bg, ui_mix(th->input_bg, th->hover, 0.35), hv * (1 - fk)));
    if (nums) {
        goome_radii gr = goome_radius(0);
        gr.tl = rr.tl, gr.bl = rr.bl;
        ui_rrect(u, ui_bx(b.x, b.y, gw, b.h), gr, ui_fade(0xFF000000, 0.12));
        ui_rect(u, ui_bx(b.x + gw - 1, b.y, 1, b.h), 0, th->border);
    }
    ui_box inner = ui_bx(b.x + gw, b.y + 1, b.w - gw - 1, b.h - 2);
    ui_clip_push(u, inner);
    int lo = e->anc < e->cur ? e->anc : e->cur, hi = e->anc < e->cur ? e->cur : e->anc;
    int g0 = (int)ui_floor((inner.y - oy) / rh), g1 = (int)ui_floor((inner.y + inner.h - oy) / rh) + 1;
    if (g0 < 0) g0 = 0;
    if (g1 > total) g1 = total;
    double spw = ui_max(4, ui_tw(u, " ", 1, 0));
    int nl_row[128], nl_line[128], nn = 0;
    uint32_t selc = focused ? ui_c(th->select, ui_fade(th->accent, 0.3)) : ui_fade(th->text_dim, 0.22);
    if (focused && lo == hi) ui_rect(u, ui_bx(inner.x, oy + cg * rh, inner.w, rh), 0, ui_fade(th->text, 0.04));
    int curline = ui_ed_line(e, e->cur), line = 0;
    if (g0 < g1) {
        int rs, re, last;
        ui_ed_row_span(u, e, buf, len, g0, &rs, &re, &last, &line);
    }
    for (int g = g0; g < g1 && line < e->nl; line++) {
        int n = ui_ed_rows_of(u, e, buf, line, len), lend = ui_ed_lend(e, line, len);
        for (int r = g - e->cr[line]; r < n && g < g1; r++, g++) {
            int rs = e->rb[r], re = r + 1 < n ? e->rb[r + 1] : lend, last = r + 1 >= n;
            double y = oy + g * rh, ty = ui_round(y + (rh - lh) / 2);
            if (ui_bd(u, buf + rs, re - rs)) {
                gm_tc *bx;
                int nb = ui_bd_lay(u, buf + rs, re - rs, &bx), nl = last && lo <= re && hi > re && re < len;
                if (hi > lo && (hi > rs || nl) && lo <= re) ui_bd_sel(u, bx, nb, lo - rs, hi - rs, ox, y, rh, 2, selc, nl ? spw : 0);
                free(bx);
                int z = re;
                while (z > rs && buf[z - 1] == '\n') z--;
                if (z > rs) ui_text(u, ox, ty, buf + rs, z - rs, 0, th->text);
                if (nums && r == 0 && nn < 128) nl_row[nn] = g, nl_line[nn++] = line;
                continue;
            }
            if (hi > lo) {
                int s0 = lo > rs ? lo : rs, s1 = hi < re ? hi : re, nl = last && lo <= re && hi > re && re < len;
                if (s1 > s0 || nl) {
                    double x0 = ui_ew(u, buf, rs, s0 < re ? s0 : re), x1 = s1 > s0 ? ui_ew(u, buf, rs, s1) : x0;
                    if (nl) x1 = ui_ew(u, buf, rs, re) + spw;
                    ui_rect(u, ui_bx(ox + x0, y, ui_max(1, x1 - x0), rh), 2, selc);
                }
            }
            int a = rs, z = re;
            double x = 0;
            while (z > a && (buf[z - 1] == ' ' || buf[z - 1] == '\n')) z--;
            while (a < z && ox + x <= inner.x + inner.w) {
                int m = z - a > 384 ? a + 256 : z;
                while (m < z && m > a + 1 && ui_cont(buf[m])) m--;
                double w = m == z ? 0 : ui_ew(u, buf, a, m);
                if (m == z || ox + x + w >= inner.x) ui_text(u, ox + x, ty, buf + a, m - a, 0, th->text);
                x += w, a = m;
            }
            if (nums && r == 0 && nn < 128) nl_row[nn] = g, nl_line[nn++] = line;
        }
    }
    if (focused) {
        long t = u->now - e->blink, bl = u->cfg.blink_ms;
        double cwd = th->caret_w > 0 ? th->caret_w : 2;
        if (bl <= 0 || (t / bl) % 2 == 0) ui_rect(u, ui_bx(ox + cx - (th->caret_w > 0 ? 0 : 0.5), oy + cg * rh + 1, cwd, rh - 2), 1, ui_c(th->caret, th->accent));
        if (!ro) {
            double ix = cx;
            int ig = lo < e->cur ? ui_ed_locate(u, e, buf, len, lo, &ix) : cg;
            ui_ime_show(u, eid, ox + ix, oy + ig * rh, rh);
        }
    }
    ui_clip_pop(u);
    if (nn) {
        ui_clip_push(u, ui_bx(b.x, b.y + 1, gw - 1, b.h - 2));
        for (int k = 0; k < nn; k++) {
            char t[16];
            snprintf(t, sizeof(t), "%d", nl_line[k] + 1);
            int tn = (int)strlen(t);
            double tw = ui_tw(u, t, tn, 0);
            ui_text(u, b.x + gw - 10 - tw, ui_round(oy + nl_row[k] * rh + (rh - lh) / 2), t, tn, 0, nl_line[k] == curline && focused ? th->text : ui_fade(th->text_dim, 0.7));
        }
        ui_clip_pop(u);
    }
    if (vov) ui_bar(u, vid, vt, 1, ta.h, chh, &e->ty, &e->sy);
    if (hov_x) ui_bar(u, hid, ht, 0, ta.w, cw, &e->tx, &e->sx);
    ui_rline(u, b, rr, th->border_width, ui_mix(ui_mix(th->border, ui_fade(th->text_dim, 0.5), hv), th->accent, fk));
    u->th.font = keep;
    u->last_id = eid, u->last_box = b, u->last_hover = hov;
    if (!u->down && u->active != eid) e->mode = 0;
    ui_ax_field(u, eid, b, id, ui_shown(id), NULL, buf, len, e->cur, (ro ? GOOME_INPUT_READONLY : 0) | 0x10000);
    return ch;
}

static ui_tbl *ui_tbl_cur(goome_ui *u) { return u->ntb > 0 && u->ntb <= UI_MAXTBL && u->tbs[u->ntb - 1].live ? &u->tbs[u->ntb - 1] : NULL; }

int goome_ui_table_begin(goome_ui *u, const char *id, int cols, double h, int flags)
{
    if (!u || !u->c) return 0;
    if (u->ntb >= UI_MAXTBL) {
        u->ntb++;
        return 0;
    }
    ui_tbl *t = &u->tbs[u->ntb++];
    memset(t, 0, sizeof(*t));
    t->id = ui_id(u, id);
    t->cols = cols < 1 ? 1 : cols > UI_MAXCOL ? UI_MAXCOL : cols;
    t->flags = flags, t->cell = -1;
    t->hh = ui_round(u->lh + 12);
    t->min_row = ui_round(u->lh + 10);
    t->box = ui_item(u, 0, h > 0 ? h : t->hh);
    t->lay = ui_lay_cur(u);
    t->saved = *t->lay;
    t->scroll = h > 0;
    t->nclip0 = u->nclip;
    t->bg_cmd = ui_n(u);
    ui_rrect(u, t->box, ui_tr(u, ui_max(0, u->th.radius - 2)), ui_fade(u->th.input_bg, 0.55));
    if (u->cl->n == t->bg_cmd) t->bg_cmd = -1;
    t->nid_out = u->nid;
    if (u->nid < UI_MAXID) u->ids[u->nid++] = t->id;
    t->live = 1;
    ui_ax_push(u, ui_ax_add(u, t->id, t->box, GOOME_ROLE_TABLE, id, id ? ui_shown(id) : 0, 0, 0), UI_AXB);
    if (h > 0 && !ui_vis(u, t->box.x, t->box.y, t->box.w, t->box.h)) {
        t->live = 2;
        return 0;
    }
    return 1;
}

void goome_ui_table_column(goome_ui *u, const char *name, double width)
{
    ui_tbl *t = u ? ui_tbl_cur(u) : NULL;
    if (!t || t->head || t->ncol >= t->cols) return;
    int n = ui_shown(name);
    if (n > 47) n = 47;
    if (n > 0) memcpy(t->names[t->ncol], name, (size_t)n);
    t->names[t->ncol][n > 0 ? n : 0] = 0;
    t->want[t->ncol++] = width;
}

static void ui_tbl_head(goome_ui *u, ui_tbl *t)
{
    if (t->head) return;
    t->head = 1;
    const goome_theme *th = &u->th;
    while (t->ncol < t->cols) t->want[t->ncol++] = 0;
    ui_box b = t->box;
    uint32_t sid = ui_hashi(t->id, 7);
    ui_box body = ui_bx(b.x, b.y + t->hh, b.w, ui_max(0, b.h - t->hh));
    double avail = b.w;
    if (t->scroll) {
        ui_st *ss = ui_get(u, sid, NULL);
        if (ss->v[2] - body.h > 0.5) avail -= th->scroll_w + 6;
    }
    double fixed = 0;
    int share = 0;
    for (int i = 0; i < t->ncol; i++) {
        ui_st *cs = ui_get(u, ui_hashi(t->id, 100 + (uint32_t)i), NULL);
        cs->keep = 1;
        double w = cs->v[0] > 0 ? cs->v[0] : t->want[i];
        t->w[i] = w;
        if (w > 0) fixed += w;
        else share++;
    }
    double sw = share ? ui_max(36, ui_floor((avail - fixed) / share)) : 0;
    double x = b.x;
    for (int i = 0; i < t->ncol; i++) {
        if (t->w[i] <= 0) t->w[i] = sw;
        t->x[i] = x, x += t->w[i];
    }
    if (share) t->w[t->ncol - 1] += ui_max(0, b.x + avail - x);
    goome_radii hr = ui_tr(u, ui_max(0, th->radius - 2));
    hr.bl = hr.br = 0;
    ui_rrect(u, ui_bx(b.x, b.y, b.w, t->hh), hr, ui_fade(th->text, 0.05));
    ui_rect(u, ui_bx(b.x, b.y + t->hh - 1, b.w, 1), 0, th->border);
    ui_st *so = ui_get(u, t->sort_id = ui_hashi(t->id, 99), NULL);
    so->keep = 1;
    for (int i = 0; i < t->ncol; i++) {
        ui_box c = ui_bx(t->x[i], b.y, t->w[i], t->hh - 1);
        uint32_t cid = ui_hashi(t->id, 200 + (uint32_t)i), did = ui_hashi(t->id, 300 + (uint32_t)i);
        int hov = 0, held = 0;
        ui_box dz = ui_bx(c.x + c.w - 5, b.y, 10, t->hh);
        int on_div = (t->flags & GOOME_TABLE_RESIZABLE) && i < t->ncol - 1;
        if (on_div) {
            int dh = ui_hover(u, did, dz);
            if (dh && u->pressed && !u->active) u->active = did, u->grab_x = c.x + c.w - u->mx;
            if (dh || u->active == did) u->cur_want = u->cfg.cursor[GOOME_CUR_COLUMN];
            ui_grabby(u, did);
            if (u->active == did && u->down) {
                ui_st *cs = ui_get(u, ui_hashi(t->id, 100 + (uint32_t)i), NULL);
                cs->v[0] = ui_round(ui_max(28, u->mx + u->grab_x - c.x));
            }
        }
        if (t->flags & GOOME_TABLE_SORTABLE) {
            ui_box hb = on_div ? ui_bx(c.x, c.y, c.w - 5, c.h) : c;
            if (ui_button_logic(u, cid, hb, &hov, &held)) {
                if (so->i[0] == i + 1) so->i[1] = !so->i[1];
                else so->i[0] = i + 1, so->i[1] = 0;
                so->i[2] = u->frame;
            }
            ui_st *hs = ui_get(u, cid, NULL);
            double k = ui_anim(u, hs, 0, hov || held);
            if (k > 0) {
                goome_radii r = goome_radius(0);
                if (i == 0) r.tl = hr.tl;
                if (i == t->ncol - 1) r.tr = hr.tr;
                ui_rrect(u, c, r, ui_fade(th->hover, 0.55 * k));
            }
        }
        ui_clip_push(u, ui_bx(c.x + 1, c.y, ui_max(0, c.w - 2), c.h));
        int n = (int)strlen(t->names[i]), on = so->i[0] == i + 1;
        double tx = c.x + 10, cy = c.y + c.h / 2;
        ui_text_mid(u, tx, cy, t->names[i], n, 0, on ? th->text : th->text_dim);
        if (on) {
            double ax = tx + ui_tw(u, t->names[i], n, 0) + 10, d = 3.5 * u->em, f = so->i[1] ? 1 : -1;
            if (!ui_icon(u, so->i[1] ? GOOME_ICON_SORT_DOWN : GOOME_ICON_SORT_UP, ui_bx(ax - d - 1, cy - d - 1, 2 * d + 2, 2 * d + 2), th->accent)) {
                ui_seg(u, ax - d, cy - d * 0.5 * f, ax, cy + d * 0.5 * f, 1.6 * u->em, th->accent);
                ui_seg(u, ax, cy + d * 0.5 * f, ax + d, cy - d * 0.5 * f, 1.6 * u->em, th->accent);
            }
        }
        ui_clip_pop(u);
        if (i < t->ncol - 1 && (t->flags & (GOOME_TABLE_RESIZABLE | GOOME_TABLE_BORDERS))) {
            int act = u->active == did || (on_div && ui_in(dz, u->mx, u->my) && u->hover_layer == u->layer);
            ui_rect(u, ui_bx(c.x + c.w - 1, b.y + 6, 1, t->hh - 12), 0, act ? th->accent : th->border);
        }
    }
    if (t->scroll) {
        ui_scroll_open(u, sid, body, 0);
        t->y = t->lay->ny;
    } else
        t->y = b.y + t->hh;
}

int goome_ui_table_sort(goome_ui *u, int *col, int *desc)
{
    ui_tbl *t = u ? ui_tbl_cur(u) : NULL;
    if (!t) return 0;
    ui_st *so = ui_get(u, ui_hashi(t->id, 99), NULL);
    if (!so->i[3]) {
        so->i[3] = 1, so->keep = 1;
        so->i[0] = col && *col >= 0 ? *col + 1 : 0, so->i[1] = desc ? *desc != 0 : 0;
    }
    ui_tbl_head(u, t);
    if (col) *col = so->i[0] - 1;
    if (desc) *desc = so->i[1];
    return so->i[2] == u->frame;
}

static void ui_cmds_shift(ui_list *l, int a, int b, double dy)
{
    for (int i = a; i < b && i < l->n; i++) {
        ui_cmd *c = &l->cmd[i];
        if (c->type == UC_UNCLIP) continue;
        c->a[1] += dy;
        if (c->type == UC_SEG || c->type == UC_TRI) c->a[3] += dy;
        if (c->type == UC_TRI) c->a[5] += dy;
        if (c->type == UC_POLY)
            for (int k = 0; k < c->flag; k++) {
                double v;
                memcpy(&v, l->txt + c->text + (size_t)(2 * k + 1) * sizeof(double), sizeof(double));
                v += dy;
                memcpy(l->txt + c->text + (size_t)(2 * k + 1) * sizeof(double), &v, sizeof(double));
            }
    }
}

static void ui_tbl_cell_end(goome_ui *u, ui_tbl *t)
{
    if (t->cell < 0) return;
    ui_lay *l = t->lay;
    if (l->max_y > t->cell_max) t->cell_max = l->max_y;
    t->cell_h[t->cell] = l->any ? l->max_y - (t->row_y + 5) : -1;
    t->cmd1[t->cell] = ui_n(u);
    ui_clip_pop(u);
}

static void ui_tbl_row_end(goome_ui *u, ui_tbl *t)
{
    if (!t->in_row) return;
    ui_tbl_cell_end(u, t);
    const goome_theme *th = &u->th;
    double rh = ui_max(t->min_row, ui_round(t->cell_max + 5 - t->row_y));
    ui_box rb = ui_bx(t->box.x, t->row_y, t->box.w, rh);
    int hov = u->hover_layer == u->layer && ui_in(rb, u->mx, u->my) && (!u->nclip || ui_in(u->clip[u->nclip - 1], u->mx, u->my));
    uint32_t col = 0;
    if ((t->flags & GOOME_TABLE_STRIPES) && (t->row & 1)) col = ui_fade(th->text, 0.035);
    if (hov) col = ui_mix(col, ui_fade(th->hover, 0.6), 1);
    if (t->row_cmd >= 0 && t->row_cmd < u->cl->n) {
        ui_cmd *c = &u->cl->cmd[t->row_cmd];
        c->a[0] = rb.x, c->a[1] = rb.y, c->a[2] = rb.w, c->a[3] = rb.h, c->c0 = ui_col(u, col);
    }
    for (int i = 0; i <= t->cell && i < UI_MAXCOL; i++) {
        double dy = t->cell_h[i] >= 0 ? ui_round((rh - 10 - t->cell_h[i]) / 2) : 0;
        if (dy > 0) ui_cmds_shift(u->cl, t->cmd0[i], t->cmd1[i], dy);
    }
    for (int i = 0; i <= t->cell && i < UI_MAXCOL; i++) {
        int k = t->clip_cmd[i];
        if (k < 0 || k >= u->cl->n || u->cl->cmd[k].type != UC_CLIP) continue;
        double *a = u->cl->cmd[k].a;
        a[3] = ui_max(0, ui_min(a[1] + a[3], rb.y + rb.h) - a[1]);
    }
    if (t->flags & GOOME_TABLE_BORDERS) ui_rect(u, ui_bx(rb.x, rb.y + rb.h - 1, rb.w, 1), 0, ui_fade(th->border, 0.7));
    t->y = rb.y + rb.h;
    t->row++, t->in_row = 0, t->cell = -1;
}

static void ui_tbl_cell(goome_ui *u, ui_tbl *t, int c)
{
    ui_tbl_cell_end(u, t);
    t->cell = c;
    double x = t->x[c] + 10, w = ui_max(4, t->w[c] - 20);
    ui_lay_init(t->lay, x, t->row_y + 5, w);
    t->clip_cmd[c] = ui_n(u);
    ui_clip_push(u, ui_bx(t->x[c] + 1, t->row_y, ui_max(0, t->w[c] - 2), 1e6));
    if (u->cl->n == t->clip_cmd[c]) t->clip_cmd[c] = -1;
    t->cmd0[c] = t->cmd1[c] = u->cl->n, t->cell_h[c] = -1;
}

void goome_ui_table_row(goome_ui *u)
{
    ui_tbl *t = u ? ui_tbl_cur(u) : NULL;
    if (!t || t->live != 1) return;
    ui_tbl_head(u, t);
    ui_tbl_row_end(u, t);
    t->in_row = 1, t->row_y = t->y, t->cell_max = t->y;
    ui_cmd *c = ui_cmd_new(u, UC_RECT);
    t->row_cmd = c ? u->cl->n - 1 : -1;
    ui_tbl_cell(u, t, 0);
}

void goome_ui_table_next(goome_ui *u)
{
    ui_tbl *t = u ? ui_tbl_cur(u) : NULL;
    if (!t || t->live != 1) return;
    if (!t->in_row) {
        goome_ui_table_row(u);
        return;
    }
    if (t->cell == 0 && !t->lay->any) return;
    if (t->cell + 1 >= t->ncol) {
        goome_ui_table_row(u);
        return;
    }
    ui_tbl_cell(u, t, t->cell + 1);
}

void goome_ui_table_end(goome_ui *u)
{
    if (!u || !u->c || u->ntb <= 0) return;
    if (u->ntb > UI_MAXTBL) {
        u->ntb--;
        return;
    }
    ui_tbl *t = &u->tbs[u->ntb - 1];
    const goome_theme *th = &u->th;
    if (t->live == 1) {
        ui_tbl_head(u, t);
        ui_tbl_row_end(u, t);
    }
    while (u->nclip > t->nclip0 + t->scroll && u->nclip > 0) ui_clip_pop(u);
    double bottom = ui_max(t->y, t->clip_end);
    ui_box b = t->box;
    if (t->scroll && t->head) {
        t->lay->max_y = bottom;
        ui_scroll_close(u);
    } else if (!t->scroll) {
        double total = ui_round(ui_max(t->hh, bottom - b.y));
        *t->lay = t->saved;
        ui_lay *l = t->lay;
        if (total > l->line_h) l->line_h = total;
        l->ny = l->line_y + l->line_h + (l->tight ? l->gap : th->gap);
        if (l->line_y + l->line_h > l->max_y) l->max_y = l->line_y + l->line_h;
        b.h = total;
        if (t->bg_cmd >= 0 && t->bg_cmd < u->cl->n) u->cl->cmd[t->bg_cmd].a[3] = total;
    }
    if (t->head && (t->flags & GOOME_TABLE_BORDERS)) {
        for (int i = 0; i + 1 < t->ncol; i++) ui_rect(u, ui_bx(t->x[i] + t->w[i] - 1, b.y + t->hh, 1, b.h - t->hh), 0, ui_fade(th->border, 0.7));
        ui_rline(u, b, ui_tr(u, ui_max(0, th->radius - 2)), th->border_width, th->border);
    }
    u->nid = t->nid_out;
    u->last_box = b, u->last_id = t->id, u->last_hover = 0;
    ui_ax_pop(u, UI_AXB, &b);
    u->ntb--;
}

int goome_ui_tree(goome_ui *u, const char *label, int *open)
{
    if (!u || !u->c) return 0;
    const goome_theme *th = &u->th;
    int n = ui_shown(label);
    ui_box b = ui_item(u, 0, u->fh - 6);
    uint32_t id = ui_id(u, label);
    int fresh;
    ui_st *s = ui_get(u, id, &fresh);
    s->keep = 1;
    int *o = open ? open : &s->i[1];
    int hov, held;
    if ((ui_nav(u, id, b, ui_tr(u, ui_max(0, th->radius - 4)), UN_PLAIN, NULL, 0) & 1) && u->nav_dx) *o = u->nav_dx > 0;
    if (ui_button_logic(u, id, b, &hov, &held)) *o = !*o;
    if (fresh) s->v[2] = *o ? 1 : 0;
    double h = ui_anim(u, s, 0, hov || held), k = ui_anim_ms(u, s, 2, *o, ui_ms(u, th->collapse_ms));
    if (h > 0) ui_rrect(u, b, ui_tr(u, ui_max(0, th->radius - 4)), ui_fade(th->hover, 0.7 * h));
    double cx = b.x + ui_round(10 * u->em), cy = b.y + b.h / 2;
    uint32_t cc = ui_mix(th->text_dim, th->text, h);
    double is = ui_round(10 * u->em);
    if (!ui_icon2(u, GOOME_ICON_ARROW_RIGHT, GOOME_ICON_ARROW_DOWN, ui_bx(cx - is / 2, ui_round(cy - is / 2), is, is), cc, k))
        ui_chevron(u, cx, cy, k * 1.5707963267948966, 3.8 * u->em, cc);
    ui_clip_push(u, b);
    ui_text_mid(u, b.x + ui_round(22 * u->em), cy, label, n, 0, th->text);
    ui_clip_pop(u);
    ui_ax_expander(u, id, b, label, n, GOOME_ROLE_TREE, *o, 1);
    if (!*o) return 0;
    if (u->ntree < UI_MAXTREE) {
        ui_lay *l = ui_lay_cur(u);
        ui_tree *t = &u->trees[u->ntree++];
        t->lay = l, t->ind = ui_round(th->indent * u->em), t->x = cx, t->y = b.y + b.h + 1;
        l->x0 += t->ind, l->w -= t->ind;
    } else
        u->ntree++;
    return 1;
}

void goome_ui_tree_end(goome_ui *u)
{
    if (!u || !u->c || u->ntree <= 0) return;
    if (u->ntree-- > UI_MAXTREE) return;
    ui_ax_pop(u, UI_AXT, NULL);
    ui_tree *t = &u->trees[u->ntree];
    ui_lay *l = t->lay;
    l->x0 -= t->ind, l->w += t->ind, l->same = 0;
    double y1 = l->max_y - 3;
    if (y1 > t->y + 2) ui_rect(u, ui_bx(t->x - 0.5, t->y, 1, y1 - t->y), 0.5, ui_fade(u->th.text_dim, 0.28));
}

int goome_ui_list_clip(goome_ui *u, int count, double row_h, int *first, int *last)
{
    if (first) *first = 0;
    if (last) *last = 0;
    if (!u || !u->c || count <= 0) return 0;
    ui_tbl *t = ui_tbl_cur(u);
    int in_tbl = t && t->live == 1 && t->lay == ui_lay_cur(u);
    if (row_h <= 0) row_h = in_tbl ? t->min_row : u->fh - 4 + u->th.gap;
    ui_lay *l = ui_lay_cur(u);
    if (in_tbl) ui_tbl_head(u, t), ui_tbl_row_end(u, t);
    double y0 = in_tbl ? t->y : (l->same && l->any ? l->line_y : l->ny);
    double top = -1e9, bot = 1e9;
    if (u->nclip) top = u->clip[u->nclip - 1].y, bot = top + u->clip[u->nclip - 1].h;
    else top = u->area.y, bot = u->area.y + u->area.h;
    int f = (int)ui_floor((top - y0) / row_h), z = (int)ui_floor((bot - y0) / row_h) + 1;
    f = f < 0 ? 0 : f > count ? count : f;
    z = z < f ? f : z > count ? count : z;
    double end = y0 + count * row_h;
    if (in_tbl) {
        t->y = y0 + f * row_h;
        t->clip_end = ui_max(t->clip_end, end);
    } else {
        l->ny = y0 + f * row_h, l->same = 0;
        if (end - (l->tight ? l->gap : u->th.gap) > l->max_y) l->max_y = end - (l->tight ? l->gap : u->th.gap);
    }
    if (first) *first = f;
    if (last) *last = z;
    return z > f;
}

static int ui_num_text(goome_ui *u, const char *label, uint32_t id, ui_box box, double *v, double lo, double hi, int whole)
{
    int ch = 0;
    ui_lay *l = ui_lay_cur(u), keep = *l;
    goome_ui_push_id(u, label);
    goome_ui_push_id(u, "##num");
    uint32_t iid = ui_id(u, "##t");
    if (u->num_id == id && u->focus != iid && !u->want_focus) {
        double nv;
        if (!u->num_cancel && ui_parse(u->num_buf, &nv)) {
            if (lo < hi) nv = ui_clamp(nv, lo, hi);
            if (whole) nv = ui_floor(nv + 0.5);
            if (nv != *v) *v = nv, ch = 1;
        }
        u->num_id = 0, u->num_cancel = 0;
        goome_ui_pop_id(u);
        goome_ui_pop_id(u);
        return ch | 2;
    }
    for (int k = 0; k < u->nkeys && u->focus == iid; k++)
        if (u->keys[k].key == GOOME_K_ESC) u->num_cancel = 1;
    ui_lay_init(l, box.x, box.y, box.w);
    goome_ui_input(u, "##t", u->num_buf, sizeof(u->num_buf));
    *l = keep;
    goome_ui_pop_id(u);
    goome_ui_pop_id(u);
    return ch;
}

static void ui_num_fmt(char *out, int cap, double v, const char *fmt, int whole)
{
    if (whole) snprintf(out, (size_t)cap, "%d", (int)v);
    else snprintf(out, (size_t)cap, fmt ? fmt : "%.3f", v);
}

static void ui_num_start(goome_ui *u, uint32_t id, double v, const char *fmt, int whole)
{
    u->num_id = id, u->num_cancel = 0, u->want_focus = 1, u->pressed = 0;
    ui_num_fmt(u->num_buf, sizeof(u->num_buf), v, fmt, whole);
}

static int ui_dragv(goome_ui *u, const char *label, double *v, double speed, double lo, double hi, const char *fmt, int whole)
{
    const goome_theme *th = &u->th;
    int n, ch = 0;
    ui_box b = ui_field_label(u, label, &n);
    uint32_t id = ui_id(u, label);
    ui_st *s = ui_get(u, id, NULL);
    ch = ui_ax_spin(u, id, b, label, n, v, lo, hi, fmt, whole);
    if (speed == 0) speed = whole ? 0.2 : 0.01;
    if (u->num_id == id) {
        int r = ui_num_text(u, label, id, b, v, lo, hi, whole);
        if (!(r & 2)) {
            u->last_id = id, u->last_box = b;
            return r & 1;
        }
        ch = r & 1;
    }
    int nav = ui_nav(u, id, b, ui_tr(u, th->radius), UN_PLAIN, NULL, 0) & 1;
    int hov = ui_hover(u, id, b);
    if (hov || u->active == id) u->cur_want = u->cfg.cursor[GOOME_CUR_DRAG_VALUE];
    if (nav && u->nav_act) {
        u->nav_act = 0;
        ui_num_start(u, id, *v, fmt, whole);
        ui_num_text(u, label, id, b, v, lo, hi, whole);
        return ch;
    }
    if (hov && u->pressed && !u->active) {
        u->active = id, s->v[3] = *v, s->v[4] = u->mx, s->i[0] = 0;
        if (u->clicks == 2 || (u->pmods & GOOME_CTRL)) {
            ui_num_start(u, id, *v, fmt, whole);
            ui_num_text(u, label, id, b, v, lo, hi, whole);
            return ch;
        }
    }
    int held = u->active == id && (u->down || u->pressed);
    ui_grabby(u, id);
    double old = *v;
    if (held && u->mx != s->v[4]) {
        double nv = s->v[3] + (u->mx - s->v[4]) * speed;
        if (lo < hi) nv = ui_clamp(nv, lo, hi);
        *v = nv;
    }
    if (nav && u->nav_dx) *v += u->nav_dx * (whole ? (speed >= 1 ? speed : 1) : speed * 10);
    if (nav && u->nav_home && lo < hi) *v = u->nav_home < 0 ? lo : hi;
    if (lo < hi) *v = ui_clamp(*v, lo, hi);
    if (whole) *v = ui_floor(*v + 0.5);
    if (*v != old) ch = 1;
    double h = ui_anim(u, s, 0, hov || held), p = ui_anim(u, s, 1, held);
    goome_radii rr = ui_tr(u, th->radius);
    if (!ui_skin(u, &th->skin_input, b, h, p)) ui_rrect(u, b, rr, ui_mix(ui_mix(th->input_bg, th->hover, h * 0.7), th->active, p));
    if (lo < hi) {
        double f = ui_clamp((*v - lo) / (hi - lo), 0, 1);
        if (f > 0) {
            ui_clip_push(u, ui_bx(b.x, b.y, ui_round(b.w * f), b.h));
            ui_rrect(u, b, rr, ui_fade(th->accent, 0.18 + 0.1 * p));
            ui_clip_pop(u);
        }
    }
    ui_rline(u, b, rr, th->border_width, ui_mix(th->border, th->accent, p * 0.8));
    char t[64];
    ui_num_fmt(t, sizeof(t), *v, fmt, whole);
    int tn = (int)strlen(t);
    double tw = ui_tw(u, t, tn, 0), cy = b.y + b.h / 2;
    ui_clip_push(u, ui_bx(b.x + 4, b.y, ui_max(0, b.w - 8), b.h));
    ui_text_mid(u, ui_round(b.x + (b.w - tw) / 2), cy, t, tn, 0, th->text);
    ui_clip_pop(u);
    if (h > 0) {
        double d = 3.2 * u->em, ax = b.x + 12, bx = b.x + b.w - 12;
        uint32_t ac = ui_fade(th->text_dim, h);
        if (!ui_icon(u, GOOME_ICON_ARROW_LEFT, ui_bx(ax - d - 1, cy - d - 1, 2 * d + 2, 2 * d + 2), ac)) {
            ui_seg(u, ax + d * 0.5, cy - d, ax - d * 0.5, cy, 1.5 * u->em, ac);
            ui_seg(u, ax - d * 0.5, cy, ax + d * 0.5, cy + d, 1.5 * u->em, ac);
        }
        if (!ui_icon(u, GOOME_ICON_ARROW_RIGHT, ui_bx(bx - d - 1, cy - d - 1, 2 * d + 2, 2 * d + 2), ac)) {
            ui_seg(u, bx - d * 0.5, cy - d, bx + d * 0.5, cy, 1.5 * u->em, ac);
            ui_seg(u, bx + d * 0.5, cy, bx - d * 0.5, cy + d, 1.5 * u->em, ac);
        }
    }
    u->last_id = id, u->last_box = b, u->last_hover = hov;
    return ch;
}

int goome_ui_drag_float(goome_ui *u, const char *label, double *v, double speed, double min, double max, const char *fmt)
{
    if (!u || !u->c || !v) return 0;
    return ui_dragv(u, label, v, speed, min, max, fmt, 0);
}

int goome_ui_drag_int(goome_ui *u, const char *label, int *v, double speed, int min, int max)
{
    if (!u || !u->c || !v) return 0;
    double d = *v;
    ui_dragv(u, label, &d, speed, min, max, NULL, 1);
    int k = (int)d;
    if (k == *v) return 0;
    *v = k;
    return 1;
}

static int ui_step_btn(goome_ui *u, uint32_t id, ui_box b, int plus, goome_radii r)
{
    const goome_theme *th = &u->th;
    int hov, held;
    ui_button_logic(u, id, b, &hov, &held);
    ui_st *s = ui_get(u, id, NULL);
    int fire = 0;
    if (held && u->pressed) fire = 1, s->v[3] = (double)u->now + u->cfg.repeat_delay_ms;
    else if (held && u->down && (double)u->now >= s->v[3]) fire = 1, s->v[3] = (double)u->now + u->cfg.repeat_ms;
    if (held) u->anim_next = 1;
    double h = ui_anim(u, s, 0, hov || held), p = ui_anim(u, s, 1, held);
    ui_rrect(u, b, r, ui_mix(ui_mix(th->input_bg, th->hover, h), th->active, p));
    double cx = ui_round(b.x + b.w / 2), cy = ui_round(b.y + b.h / 2), d = ui_round(5 * u->em), lw = ui_round(1.6 * u->em * 2) / 2;
    uint32_t c = ui_mix(th->text_dim, th->text, h);
    if (ui_icon(u, plus ? GOOME_ICON_PLUS : GOOME_ICON_MINUS, ui_bx(cx - d - 1, cy - d - 1, 2 * d + 2, 2 * d + 2), c)) return fire;
    ui_rect(u, ui_bx(cx - d, cy - lw / 2, 2 * d, lw), lw / 2, c);
    if (plus) ui_rect(u, ui_bx(cx - lw / 2, cy - d, lw, 2 * d), lw / 2, c);
    return fire;
}

int goome_ui_number(goome_ui *u, const char *label, double *v, double step, const char *fmt)
{
    if (!u || !u->c || !v) return 0;
    const goome_theme *th = &u->th;
    int n, ch = 0;
    ui_box b = ui_field_label(u, label, &n);
    uint32_t id = ui_id(u, label);
    ch = ui_ax_spin(u, id, b, label, n, v, 0, 0, fmt, 0);
    if (step == 0) step = 1;
    double bw = ui_round(u->fh * 1.05);
    goome_radii rl = ui_tr(u, th->radius), rr = rl;
    rl.tr = rl.br = 0, rr.tl = rr.bl = 0;
    ui_box mb = ui_bx(b.x, b.y, bw, b.h), pb = ui_bx(b.x + b.w - bw, b.y, bw, b.h), tb = ui_bx(b.x + bw + 1, b.y, ui_max(10, b.w - 2 * bw - 2), b.h);
    double old = *v;
    if (ui_step_btn(u, ui_hashi(id, 1), mb, 0, rl)) *v -= step;
    if (ui_step_btn(u, ui_hashi(id, 2), pb, 1, rr)) *v += step;
    goome_ui_push_id(u, label);
    goome_ui_push_id(u, "##num");
    uint32_t iid = ui_id(u, "##t");
    goome_ui_pop_id(u);
    goome_ui_pop_id(u);
    int foc = u->focus == iid;
    if (foc && u->num_id != id) u->num_id = id, ui_num_fmt(u->num_buf, sizeof(u->num_buf), *v, fmt, 0);
    if (foc)
        for (int k = 0; k < u->nkeys; k++) {
            int key = u->keys[k].key;
            if (key == GOOME_K_UP || key == GOOME_K_DOWN || key == GOOME_K_PGUP || key == GOOME_K_PGDN) {
                double cur = *v;
                ui_parse(u->num_buf, &cur);
                cur += (key == GOOME_K_UP ? 1 : key == GOOME_K_DOWN ? -1 : key == GOOME_K_PGUP ? 10 : -10) * step;
                *v = cur;
                ui_num_fmt(u->num_buf, sizeof(u->num_buf), cur, fmt, 0);
                u->keys[k].key = 0, u->keys[k].text[0] = 0;
            }
        }
    if (*v != old && u->num_id == id) ui_num_fmt(u->num_buf, sizeof(u->num_buf), *v, fmt, 0);
    ui_lay *l = ui_lay_cur(u), keep = *l;
    goome_ui_push_id(u, label);
    goome_ui_push_id(u, "##num");
    char tmp[64];
    char *tb2 = u->num_id == id ? u->num_buf : tmp;
    if (tb2 == tmp) ui_num_fmt(tmp, sizeof(tmp), *v, fmt, 0);
    ui_lay_init(l, tb.x, tb.y, tb.w);
    double r0 = u->th.radius;
    int rad0 = u->rad_on;
    u->th.radius = 0, u->rad_on = 0;
    int typed = goome_ui_input(u, "##t", tb2, sizeof(u->num_buf));
    u->th.radius = r0, u->rad_on = rad0;
    *l = keep;
    goome_ui_pop_id(u);
    goome_ui_pop_id(u);
    if (typed && u->num_id == id) {
        double nv;
        if (ui_parse(u->num_buf, &nv)) *v = nv;
    }
    if (u->num_id == id && u->focus != iid) u->num_id = 0;
    if (*v != old) ch = 1;
    u->last_id = id, u->last_box = b;
    u->last_hover = u->hover_layer == u->layer && ui_in(b, u->mx, u->my);
    return ch;
}

static void ui_plot_range(const float *v, int n, double *lo, double *hi)
{
    if (*lo < *hi) return;
    double a = 0, z = 0;
    int any = 0;
    for (int i = 0; i < n; i++) {
        double x = v[i];
        if (x != x) continue;
        if (!any || x < a) a = x;
        if (!any || x > z) z = x;
        any = 1;
    }
    if (z - a < 1e-9) a -= 1, z += 1;
    double pad = (z - a) * 0.08;
    *lo = a - pad, *hi = z + pad;
}

static ui_box ui_plot_box(goome_ui *u, const char *label, const float *v, int n, double h, double *ty)
{
    int ln = ui_shown(label);
    double top = ln ? u->lh + 4 : 0;
    if (h <= 0) h = ui_round(u->fh * 3);
    ui_box b = ui_item(u, 0, top + h);
    if (ln) ui_text_mid(u, b.x, b.y + u->lh / 2, label, ln, 0, u->th.text_dim);
    if (ln && n > 0 && v[n - 1] == v[n - 1]) {
        char t[32];
        snprintf(t, sizeof(t), "%.4g", (double)v[n - 1]);
        int tn = (int)strlen(t);
        ui_text_mid(u, b.x + b.w - ui_tw(u, t, tn, 0), b.y + u->lh / 2, t, tn, 0, u->th.text);
    }
    *ty = b.y;
    return ui_bx(b.x, b.y + top, b.w, h);
}

static void ui_plot_tip(goome_ui *u, ui_box p, double x, double y, double val, int idx)
{
    const goome_theme *th = &u->th;
    char t[48];
    snprintf(t, sizeof(t), "%d: %.4g", idx, val);
    int tn = (int)strlen(t);
    double w = ui_round(ui_tw(u, t, tn, 0) + 14), h = ui_round(u->lh + 6);
    double bx = ui_clamp(x - w / 2, p.x + 2, p.x + p.w - w - 2), by = y - h - 8;
    if (by < p.y + 2) by = y + 8;
    ui_box b = ui_bx(ui_round(bx), ui_round(by), w, h);
    ui_shadow(u, ui_bx(b.x, b.y + 2, b.w, b.h), 6, 8, ui_fade(th->shadow, 0.6));
    if (!ui_skin(u, &th->skin_tooltip, b, 0, 0)) ui_rect(u, b, 6, ui_mix(th->bg, th->hover, 0.5) | 0xFF000000u);
    ui_text_mid(u, b.x + 7, b.y + b.h / 2, t, tn, 0, th->text);
}

void goome_ui_plot_lines(goome_ui *u, const char *label, const float *v, int n, double min, double max, double h)
{
    if (!u || !u->c) return;
    const goome_theme *th = &u->th;
    double ty;
    if (!v) n = 0;
    ui_box p = ui_plot_box(u, label, v, n, h, &ty);
    uint32_t id = ui_id(u, label);
    int hov = ui_hover(u, id, p);
    ui_ax_text(u, p, label, label ? ui_shown(label) : 0, GOOME_ROLE_IMAGE);
    ui_rrect(u, p, ui_tr(u, ui_max(0, th->radius - 2)), ui_fade(th->input_bg, 0.8));
    for (int i = 1; i < 4; i++) ui_rect(u, ui_bx(p.x + 6, ui_round(p.y + p.h * i / 4), p.w - 12, 1), 0, ui_fade(th->text, 0.05));
    if (n < 1) return;
    ui_plot_range(v, n, &min, &max);
    double x0 = p.x + 6, x1 = p.x + p.w - 6, y0 = p.y + 6, y1 = p.y + p.h - 6, span = max - min;
    int step = n > (int)(x1 - x0) ? n / (int)ui_max(1, x1 - x0) + 1 : 1, m = 0;
    int cap = n / step + 3;
    double tmp[512], *xy = cap * 2 + 4 <= 512 ? tmp : malloc(sizeof(double) * (size_t)(cap * 2 + 4));
    if (!xy) return;
    for (int i = 0; i < n; i += step) {
        double val = v[i] == v[i] ? v[i] : min;
        xy[2 * m] = n > 1 ? x0 + (x1 - x0) * i / (n - 1) : (x0 + x1) / 2;
        xy[2 * m + 1] = y1 - (y1 - y0) * ui_clamp((val - min) / span, 0, 1);
        m++;
    }
    if (m > 1 && (n - 1) % step) {
        double val = v[n - 1] == v[n - 1] ? v[n - 1] : min;
        xy[2 * m] = x1, xy[2 * m + 1] = y1 - (y1 - y0) * ui_clamp((val - min) / span, 0, 1), m++;
    }
    ui_clip_push(u, p);
    if (m > 1) {
        xy[2 * m] = xy[2 * (m - 1)], xy[2 * m + 1] = p.y + p.h;
        xy[2 * m + 2] = xy[0], xy[2 * m + 3] = p.y + p.h;
        ui_poly(u, xy, m + 2, ui_fade(th->accent, 0.16));
        for (int i = 0; i + 1 < m; i++) ui_seg(u, xy[2 * i], xy[2 * i + 1], xy[2 * i + 2], xy[2 * i + 3], th->plot_w * u->em, th->accent);
    } else
        ui_circle(u, xy[0], xy[1], 3, th->accent);
    ui_clip_pop(u);
    if (hov) {
        int i = n > 1 ? (int)ui_floor((u->mx - x0) / (x1 - x0) * (n - 1) + 0.5) : 0;
        i = i < 0 ? 0 : i >= n ? n - 1 : i;
        double val = v[i] == v[i] ? v[i] : min;
        double hx = n > 1 ? x0 + (x1 - x0) * i / (n - 1) : (x0 + x1) / 2, hy = y1 - (y1 - y0) * ui_clamp((val - min) / span, 0, 1);
        ui_rect(u, ui_bx(ui_round(hx) - 0.5, p.y + 4, 1, p.h - 8), 0, ui_fade(th->text, 0.18));
        ui_circle(u, hx, hy, 5.5 * u->em, ui_fade(th->accent, 0.3));
        ui_circle(u, hx, hy, 3.2 * u->em, th->accent);
        ui_plot_tip(u, p, hx, hy, val, i);
    }
    if (xy != tmp) free(xy);
}

void goome_ui_plot_bars(goome_ui *u, const char *label, const float *v, int n, double min, double max, double h)
{
    if (!u || !u->c) return;
    const goome_theme *th = &u->th;
    double ty;
    if (!v) n = 0;
    ui_box p = ui_plot_box(u, label, v, n, h, &ty);
    uint32_t id = ui_id(u, label);
    int hov = ui_hover(u, id, p);
    ui_ax_text(u, p, label, label ? ui_shown(label) : 0, GOOME_ROLE_IMAGE);
    ui_rrect(u, p, ui_tr(u, ui_max(0, th->radius - 2)), ui_fade(th->input_bg, 0.8));
    if (n < 1) return;
    if (min >= max) {
        ui_plot_range(v, n, &min, &max);
        if (min > 0) min = 0;
        if (max < 0) max = 0;
    }
    double x0 = p.x + 6, x1 = p.x + p.w - 6, y0 = p.y + 6, y1 = p.y + p.h - 6, span = max - min;
    double zero = y1 - (y1 - y0) * ui_clamp((0 - min) / span, 0, 1), slot = (x1 - x0) / n, gap = slot > 6 ? ui_min(slot * 0.25, 8) : slot > 2 ? 1 : 0;
    int hi = -1;
    if (hov) hi = (int)ui_floor((u->mx - x0) / slot), hi = hi < 0 ? 0 : hi >= n ? n - 1 : hi;
    if (zero > y0 && zero < y1) ui_rect(u, ui_bx(p.x + 4, ui_round(zero), p.w - 8, 1), 0, ui_fade(th->text, 0.12));
    ui_clip_push(u, p);
    for (int i = 0; i < n; i++) {
        double val = v[i] == v[i] ? v[i] : 0;
        double y = y1 - (y1 - y0) * ui_clamp((val - min) / span, 0, 1);
        double bx = x0 + i * slot + gap / 2, bw = ui_max(1, slot - gap), top = ui_min(y, zero), bh = ui_abs(zero - y);
        if (bh < 1) bh = 1;
        goome_radii r = goome_radius(0);
        double rr = ui_min(4 * u->em, bw / 2);
        if (y <= zero) r.tl = r.tr = rr;
        else r.bl = r.br = rr;
        uint32_t c = i == hi ? ui_mix(th->accent, 0xFFFFFFFF, 0.25) : hi >= 0 ? ui_fade(th->accent, 0.75) : th->accent;
        ui_rrect(u, ui_bx(bx, top, bw, bh), r, c);
    }
    ui_clip_pop(u);
    if (hi >= 0) {
        double val = v[hi] == v[hi] ? v[hi] : 0;
        double y = y1 - (y1 - y0) * ui_clamp((val - min) / span, 0, 1);
        ui_plot_tip(u, p, x0 + (hi + 0.5) * slot, ui_min(y, zero), val, hi);
    }
}

int goome_ui_split_begin(goome_ui *u, const char *id, int vertical, double *frac)
{
    if (!u || !u->c) return 0;
    if (u->nsp >= UI_MAXSPL) {
        u->nsp++;
        return 0;
    }
    const goome_theme *th = &u->th;
    ui_split *s = &u->sps[u->nsp++];
    memset(s, 0, sizeof(*s));
    s->id = ui_id(u, id), s->vertical = vertical != 0;
    int root = u->app_on && u->nfr == 1 && u->nscr == 0;
    double h = ui_fill_h(u);
    if (root) {
        ui_lay *l = ui_lay_cur(u);
        h = ui_max(60, u->fr[0].box.y + u->fr[0].box.h - (l->same && l->any ? l->line_y : l->ny) - (l->x0 - u->fr[0].box.x));
    }
    ui_box b = ui_item(u, 0, h);
    s->box = b;
    if (root && b.x > u->fr[0].box.x + 0.5) root = 0;
    int fresh;
    ui_st *st = ui_get(u, s->id, &fresh);
    st->keep = 1;
    double f = frac ? *frac : (fresh ? 0.3 : st->v[0]);
    if (!(f > 0 && f < 1)) f = 0.3;
    double full = vertical ? b.h : b.w, minp = ui_min(48, full / 2);
    double at = ui_round(ui_clamp(full * f, minp, full - minp));
    uint32_t bid = ui_hashi(s->id, 1);
    ui_box bar = vertical ? ui_bx(b.x, b.y + at - 4, b.w, 8) : ui_bx(b.x + at - 4, b.y, 8, b.h);
    int hov = ui_hover(u, bid, bar);
    if (hov && u->pressed && !u->active) u->active = bid, u->grab_x = (vertical ? u->my - b.y : u->mx - b.x) - at;
    int held = u->active == bid && (u->down || u->pressed);
    ui_grabby(u, bid);
    if (hov || held) u->cur_want = u->cfg.cursor[vertical ? GOOME_CUR_SPLIT_Y : GOOME_CUR_SPLIT_X];
    if (held && u->down) {
        at = ui_round(ui_clamp((vertical ? u->my - b.y : u->mx - b.x) - u->grab_x, minp, full - minp));
        f = at / full;
    }
    if (frac) *frac = f;
    st->v[0] = f;
    s->bar = bar;
    s->a = vertical ? ui_bx(b.x, b.y, b.w, at) : ui_bx(b.x, b.y, at, b.h);
    s->b = vertical ? ui_bx(b.x, b.y + at + 1, b.w, b.h - at - 1) : ui_bx(b.x + at + 1, b.y, b.w - at - 1, b.h);
    double k = ui_anim_ms(u, st, 1, hov || held, ui_ms(u, th->hover_ms));
    st->v[2] = k;
    goome_radii r = root ? goome_radius(0) : ui_tr(u, ui_max(0, th->radius - 2));
    if (vertical) r.bl = r.br = 0;
    else r.tr = r.br = 0;
    if (root && !(u->app_flags & GOOME_APP_STATUSBAR) && !vertical) r.bl = u->app_r;
    ui_rrect(u, s->a, r, root ? th->panel | 0xFF000000u : ui_fade(th->input_bg, 0.45));
    s->nid_out = u->nid;
    if (u->nid < UI_MAXID) u->ids[u->nid++] = s->id;
    s->live = 1, s->pane = 0;
    ui_scroll_open(u, ui_hashi(s->id, 2), s->a, th->pad);
    return 1;
}

void goome_ui_split_next(goome_ui *u)
{
    if (!u || !u->c || u->nsp <= 0 || u->nsp > UI_MAXSPL) return;
    ui_split *s = &u->sps[u->nsp - 1];
    if (!s->live || s->pane) return;
    ui_scroll_close(u);
    s->pane = 1;
    ui_scroll_open(u, ui_hashi(s->id, 3), s->b, u->th.pad);
}

void goome_ui_split_end(goome_ui *u)
{
    if (!u || !u->c || u->nsp <= 0) return;
    if (u->nsp-- > UI_MAXSPL) return;
    ui_split *s = &u->sps[u->nsp];
    if (!s->live) return;
    const goome_theme *th = &u->th;
    ui_scroll_close(u);
    if (!s->pane) ui_scroll_open(u, ui_hashi(s->id, 3), s->b, th->pad), ui_scroll_close(u);
    ui_st *st = ui_get(u, s->id, NULL);
    double k = st->v[2];
    if (s->vertical) ui_rect(u, ui_bx(s->box.x, s->a.y + s->a.h, s->box.w, 1), 0, th->border);
    else ui_rect(u, ui_bx(s->a.x + s->a.w, s->box.y, 1, s->box.h), 0, th->border);
    if (k > 0) {
        ui_box l = s->vertical ? ui_bx(s->box.x, s->a.y + s->a.h - 1, s->box.w, 3) : ui_bx(s->a.x + s->a.w - 1, s->box.y, 3, s->box.h);
        ui_rect(u, l, 1.5, ui_fade(th->accent, 0.85 * k));
    }
    if (ui_has_icon(u, GOOME_ICON_DRAG_HANDLE)) {
        double cx = s->vertical ? s->box.x + s->box.w / 2 : s->a.x + s->a.w + 0.5, cy = s->vertical ? s->a.y + s->a.h + 0.5 : s->box.y + s->box.h / 2;
        ui_box hb = s->vertical ? ui_bx(ui_round(cx - 12), ui_round(cy - 6), 24, 12) : ui_bx(ui_round(cx - 6), ui_round(cy - 12), 12, 24);
        ui_icon(u, GOOME_ICON_DRAG_HANDLE, hb, ui_mix(th->text_dim, th->accent, k));
    }
    u->nid = s->nid_out;
    u->last_box = s->box, u->last_id = s->id, u->last_hover = 0;
}

static void ui_dd_set(goome_ui *u, const char *type, const void *data, int len)
{
    snprintf(u->dd_type, sizeof(u->dd_type), "%s", type ? type : "");
    if (len < 0 || !data) len = 0;
    if (len > u->dd_cap) {
        unsigned char *n = realloc(u->dd_data, (size_t)len);
        if (!n) len = u->dd_cap;
        else u->dd_data = n, u->dd_cap = len;
    }
    if (len) memcpy(u->dd_data, data, (size_t)len);
    u->dd_len = len;
}

int goome_ui_drag_source(goome_ui *u, const char *type, const void *data, int len)
{
    if (!u || !u->c) return 0;
    ui_box b = u->last_box;
    uint32_t key = u->last_id ? u->last_id : ui_hashi(ui_seed(u), (uint32_t)(b.x * 7 + b.y * 131));
    int over = u->hover_layer == u->layer && ui_in(b, u->mx, u->my) && (!u->nclip || ui_in(u->clip[u->nclip - 1], u->mx, u->my));
    if (over && u->pressed && !u->dd_on) {
        u->dd_src = key, u->dd_px = u->mx, u->dd_py = u->my, u->dd_on = 0;
        u->dd_ox = b.x - u->mx, u->dd_oy = b.y - u->my, u->dd_box = b;
    }
    if (u->dd_src != key) return 0;
    if (!u->down && !u->released) {
        if (!u->dd_on) u->dd_src = 0;
        return 0;
    }
    if (!u->dd_on && u->down && (ui_abs(u->mx - u->dd_px) > u->cfg.drag_px || ui_abs(u->my - u->dd_py) > u->cfg.drag_px)) u->dd_on = 1;
    if (u->dd_on) {
        ui_dd_set(u, type, data, len);
        u->dd_box.w = b.w, u->dd_box.h = b.h;
        u->dd_seen = u->frame, u->grab_frame = u->frame;
        u->cur_want = u->cfg.cursor[GOOME_CUR_DRAG_DROP];
        u->anim_next = 1;
    }
    return u->dd_on;
}

int goome_ui_drop_target(goome_ui *u, const char *type, void *buf, int cap)
{
    if (!u || !u->c || !u->dd_on) return 0;
    if (type && strcmp(type, u->dd_type)) return 0;
    ui_box b = u->last_box;
    const goome_theme *th = &u->th;
    int over = u->hover_layer == u->layer && ui_in(b, u->mx, u->my) && (!u->nclip || ui_in(u->clip[u->nclip - 1], u->mx, u->my));
    ui_box g = ui_bx(b.x - 2, b.y - 2, b.w + 4, b.h + 4);
    goome_radii r = ui_tr(u, ui_max(0, th->radius - 1));
    if (over) ui_rrect(u, g, r, ui_fade(th->accent, 0.14));
    ui_rline(u, g, r, over ? 2 : 1.5, ui_fade(th->accent, over ? 0.95 : 0.4));
    if (!over || !u->released) return 0;
    int n = u->dd_len < cap ? u->dd_len : cap;
    if (buf && n > 0) memcpy(buf, u->dd_data, (size_t)n);
    u->dd_on = 0, u->dd_src = 0;
    return n > 0 ? n : 0;
}

static void ui_dd_end(goome_ui *u)
{
    if (u->dd_on) {
        const goome_theme *th = &u->th;
        ui_save sv;
        ui_enter(u, &sv, &u->tip, UI_TIP, 1);
        double w = ui_min(u->dd_box.w, 280), h = ui_min(u->dd_box.h, 80);
        ui_box b = ui_bx(ui_round(u->mx + ui_max(u->dd_ox, -w + 12)), ui_round(u->my + ui_max(u->dd_oy, -h + 8)), w, h);
        ui_shadow(u, ui_shb(u, b, 6), th->radius, 16, ui_fade(th->shadow, 0.9));
        ui_rect(u, b, th->radius, ui_fade(ui_mix(th->panel | 0xFF000000u, th->accent, 0.3), 0.4));
        ui_outline(u, b, th->radius, 1.5, ui_fade(th->accent, 0.9));
        ui_leave(u, &sv);
        u->cur_want = u->cfg.cursor[GOOME_CUR_DRAG_DROP];
    }
    if (!u->down) u->dd_on = 0, u->dd_src = 0;
}

void goome_ui_spinner(goome_ui *u, double size)
{
    if (!u || !u->c) return;
    if (size <= 0) size = u->th.spinner > 0 ? u->th.spinner : ui_round(u->lh);
    ui_box b = ui_item(u, size, size);
    ui_hover(u, ui_auto(u), b);
    ui_ax_progress(u, b, -1, NULL);
    double cx = b.x + size / 2, cy = b.y + size / 2, w = ui_max(2, size / 8), r = size / 2 - w / 2 - 0.5;
    double t = (double)(u->now % 1400) / 1400, pi2 = 6.283185307179586;
    double head = t * pi2 * 1.25, k = (double)(u->now % 2800) / 2800;
    double grow = k < 0.5 ? ui_ez(u, k * 2) : 1 - ui_ez(u, k * 2 - 1);
    double sweep = 0.6 + 3.4 * grow;
    ui_ring(u, cx, cy, r, w, ui_fade(u->th.text, 0.1));
    ui_arc(u, cx, cy, r, w, head + k * pi2 - sweep, head + k * pi2, u->th.accent);
    u->anim_next = 1;
}

int goome_ui_link(goome_ui *u, const char *text)
{
    if (!u || !u->c) return 0;
    const goome_theme *th = &u->th;
    int n = ui_shown(text);
    double tw = ui_tw(u, text, n, 0);
    ui_box b = ui_item(u, tw, u->lh);
    uint32_t id = ui_id(u, text);
    ui_nav(u, id, ui_bx(b.x - 2, b.y, b.w + 4, b.h), goome_radius(4), UN_PLAIN, NULL, 0);
    int hov, held, click = ui_button_logic(u, id, b, &hov, &held);
    ui_ax_btn(u, id, b, text, n);
    if (hov || held) u->cur_want = u->cfg.cursor[GOOME_CUR_LINK];
    ui_st *s = ui_get(u, id, NULL);
    double h = ui_anim(u, s, 0, hov || held);
    uint32_t lc = ui_c(th->link, th->accent), c = ui_mix(lc, ui_mix(lc, th->text, 0.35), h);
    if (held) c = ui_mix(c, th->text, 0.3);
    ui_text_mid(u, b.x, b.y + b.h / 2, text, n, 0, c);
    double uw = ui_round(tw * (0.35 + 0.65 * h));
    ui_rect(u, ui_bx(b.x, ui_round(b.y + b.h / 2 + u->size * 0.55), uw, ui_max(1, ui_round(u->em))), 0, ui_fade(c, 0.4 + 0.6 * h));
    return click;
}

void goome_ui_badge(goome_ui *u, const char *text, uint32_t argb)
{
    if (!u || !u->c) return;
    const goome_theme *th = &u->th;
    int n = ui_shown(text);
    goome_font *keep = u->th.font;
    if (th->font_small) u->th.font = th->font_small;
    double sz = th->small_size > 0 ? th->small_size : ui_round(u->size * 0.82), tw = ui_tw(u, text, n, sz), lh = goome_text_line_height(u->th.font, sz);
    double h = ui_round(lh + 4), w = ui_round(ui_max(h, tw + h * 0.9));
    ui_box b = ui_item(u, w, h);
    ui_hover(u, ui_auto(u), b);
    ui_ax_text(u, b, text, n, GOOME_ROLE_TEXT);
    uint32_t bg = argb ? argb : ui_c(th->badge, th->accent);
    int lum = (int)(((bg >> 16) & 255) * 299 + ((bg >> 8) & 255) * 587 + (bg & 255) * 114) / 1000;
    uint32_t fg = (bg >> 24) < 140 ? th->text : lum > 150 ? 0xFF16181C : 0xFFFFFFFF;
    ui_rect(u, b, h / 2, bg);
    ui_text_mid(u, ui_round(b.x + (w - tw) / 2), b.y + h / 2, text, n, sz, fg);
    u->th.font = keep;
}
#define UI_AXMAX 4096
#define UI_AXDEEP 64
#define UI_AXACT 16

typedef struct {
    int verb;
    uint32_t id;
    char text[64];
} ui_axact;

typedef struct ui_ax_s {
    goome_a11y_node *n;
    gm_ax_x *x;
    char *text;
    int count, cap, ntext, tcap, depth, nact, npend, combo, has_role, has_val, role;
    int stack[UI_AXDEEP], kind[UI_AXDEEP];
    ui_axact act[UI_AXACT], pend[UI_AXACT];
    char label[96], value[64];
} ui_axs;

static ui_axs *ui_ax_on(goome_ui *u) { return u->ax_on && u->ax && u->c && u->mon == 0 ? u->ax : NULL; }

static void ui_ax_cp(char *d, int cap, const char *s, int n)
{
    if (!s || n < 0) n = 0;
    if (n > cap - 1) {
        n = cap - 1;
        while (n > 0 && ((unsigned char)s[n] & 0xC0) == 0x80) n--;
    }
    if (n) memcpy(d, s, (size_t)n);
    d[n] = 0;
}

static int ui_ax_add(goome_ui *u, uint32_t id, ui_box b, int role, const char *s, int n, int states, int kind)
{
    ui_axs *a = ui_ax_on(u);
    if (!a || !id) return -1;
    if (a->count >= a->cap) {
        if (a->cap >= UI_AXMAX) return -1;
        int nc = a->cap ? a->cap * 2 : 128;
        goome_a11y_node *nn = realloc(a->n, sizeof(*nn) * (size_t)nc);
        if (!nn) return -1;
        a->n = nn;
        gm_ax_x *nx = realloc(a->x, sizeof(*nx) * (size_t)nc);
        if (!nx) return -1;
        a->x = nx, a->cap = nc;
    }
    int i = a->count++;
    goome_a11y_node *d = &a->n[i];
    gm_ax_x *x = &a->x[i];
    memset(d, 0, sizeof(*d));
    memset(x, 0, sizeof(*x));
    d->role = role, d->parent = a->depth ? a->stack[a->depth - 1] : -1, d->states = states, d->id = id;
    d->x = b.x, d->y = b.y, d->w = b.w, d->h = b.h;
    ui_ax_cp(d->label, (int)sizeof(d->label), s, s && n < 0 ? (int)strlen(s) : n);
    x->press = id, x->kind = kind;
    if (u->nclip) {
        ui_box c = u->clip[u->nclip - 1];
        if (b.x >= c.x + c.w || b.y >= c.y + c.h || b.x + b.w <= c.x || b.y + b.h <= c.y) x->kind |= GM_AXK_HIDDEN;
    }
    return i;
}

static void ui_ax_val(goome_ui *u, int i, const char *s, int n)
{
    ui_axs *a = u->ax;
    if (!a || i < 0 || i >= a->count) return;
    if (!s) s = "", n = 0;
    if (n < 0) n = (int)strlen(s);
    ui_ax_cp(a->n[i].value, (int)sizeof(a->n[i].value), s, n);
    if (n < (int)sizeof(a->n[i].value)) return;
    if (a->ntext + n + 1 > a->tcap) {
        int nc = a->tcap ? a->tcap : 4096;
        while (nc < a->ntext + n + 1) nc *= 2;
        char *t = realloc(a->text, (size_t)nc);
        if (!t) return;
        a->text = t, a->tcap = nc;
    }
    memcpy(a->text + a->ntext, s, (size_t)n);
    a->x[i].text = a->ntext, a->x[i].ntext = n;
    a->ntext += n;
    a->text[a->ntext++] = 0;
}

static void ui_ax_push(goome_ui *u, int i, int kind)
{
    ui_axs *a = ui_ax_on(u);
    if (!a || i < 0 || a->depth >= UI_AXDEEP) return;
    a->stack[a->depth] = i, a->kind[a->depth] = kind;
    a->depth++;
}

static void ui_ax_pop(goome_ui *u, int kind, const ui_box *b)
{
    ui_axs *a = ui_ax_on(u);
    if (!a) return;
    for (int k = a->depth - 1; k >= 0; k--)
        if (a->kind[k] == kind) {
            if (b) {
                goome_a11y_node *d = &a->n[a->stack[k]];
                d->x = b->x, d->y = b->y, d->w = b->w, d->h = b->h;
            }
            a->depth = k;
            return;
        }
}

static ui_axact *ui_ax_get(goome_ui *u, uint32_t id, int verb)
{
    ui_axs *a = u->ax_on ? u->ax : NULL;
    if (!a || !a->nact || !id) return NULL;
    for (int i = 0; i < a->nact; i++)
        if (a->act[i].id == id && a->act[i].verb == verb) return &a->act[i];
    return NULL;
}

static void ui_ax_drop(goome_ui *u, ui_axact *t)
{
    ui_axs *a = u->ax;
    int k = (int)(t - a->act);
    memmove(t, t + 1, sizeof(*t) * (size_t)(a->nact - k - 1));
    a->nact--;
    u->anim_next = 1;
}

static int ui_ax_do(goome_ui *u, uint32_t id, int verb)
{
    ui_axact *t = ui_ax_get(u, id, verb);
    if (!t) return 0;
    ui_ax_drop(u, t);
    return 1;
}

static const char *ui_ax_take(goome_ui *u, uint32_t id, char *out, int cap)
{
    ui_axact *t = ui_ax_get(u, id, 's');
    if (!t) return NULL;
    const char *l = gm_ax_long(u->g, id);
    snprintf(out, (size_t)cap, "%s", l ? l : t->text);
    ui_ax_drop(u, t);
    return out;
}

static int ui_ax_feed(goome_ui *u, const goome_event *e)
{
    ui_axs *a = u->ax;
    if (!a || !u->ax_on || strncmp(e->msg, "a11y ", 5)) return 0;
    const char *p = e->msg + 5;
    int verb = !strncmp(p, "press ", 6) ? 'p' : !strncmp(p, "focus ", 6) ? 'f' : !strncmp(p, "set ", 4) ? 's' : !strncmp(p, "expand ", 7) ? 'e' : !strncmp(p, "collapse ", 9) ? 'c' : 0;
    if (!verb) return 0;
    while (*p && *p != ' ') p++;
    while (*p == ' ') p++;
    uint32_t id = 0;
    while (*p >= '0' && *p <= '9') id = id * 10 + (uint32_t)(*p++ - '0');
    if (*p == ' ') p++;
    if (verb != 'f' && verb != 's') {
        int k = -1;
        for (int i = 0; i < a->count && k < 0; i++)
            if (a->n[i].id == id) k = i;
        if (k < 0) return 1;
        int open = (a->n[k].states & GOOME_A11Y_EXPANDED) != 0;
        if (verb != 'p' && (!(a->x[k].kind & GM_AXK_EXPANDABLE) || open == (verb == 'e'))) return 1;
        id = a->x[k].press, verb = 'p';
    }
    if (a->npend >= UI_AXACT) return 1;
    ui_axact *t = &a->pend[a->npend++];
    t->verb = verb, t->id = id;
    snprintf(t->text, sizeof(t->text), "%s", p);
    return 1;
}

static void ui_ax_begin(goome_ui *u)
{
    ui_axs *a = ui_ax_on(u);
    if (!a) return;
    memcpy(a->act, a->pend, sizeof(a->act[0]) * (size_t)a->npend);
    a->nact = a->npend, a->npend = 0;
    a->count = 0, a->ntext = 0, a->depth = 0, a->combo = -1;
}

static void ui_ax_end(goome_ui *u)
{
    ui_axs *a = ui_ax_on(u);
    if (!a) return;
    a->nact = 0;
    a->has_role = a->has_val = 0;
    for (int i = 0; i < a->count; i++) {
        if (u->focus && a->n[i].id == u->focus) a->n[i].states |= GOOME_A11Y_FOCUSED;
        for (int j = i - 1; j >= 0 && j >= i - 64; j--)
            if (a->n[j].id == a->n[i].id) {
                a->n[i].id = ui_hashi(a->n[i].id, (uint32_t)i);
                break;
            }
    }
    gm_ax_sync(u->g, a->n, a->x, a->count, a->text, a->ntext, !u->unfocused);
}

static void ui_ax_free(goome_ui *u)
{
    if (!u->ax) return;
    free(u->ax->n), free(u->ax->x), free(u->ax->text);
    free(u->ax);
    u->ax = NULL;
}

void goome_ui_a11y(goome_ui *u, int on)
{
    if (!u) return;
    if (on && !u->ax) u->ax = calloc(1, sizeof(ui_axs));
    u->ax_on = on && u->ax;
}

void goome_ui_a11y_role(goome_ui *u, int role, const char *label)
{
    if (!u || !u->ax) return;
    u->ax->role = role, u->ax->has_role = 1;
    ui_ax_cp(u->ax->label, (int)sizeof(u->ax->label), label, label ? (int)strlen(label) : 0);
}

void goome_ui_a11y_value(goome_ui *u, const char *value)
{
    if (!u || !u->ax) return;
    u->ax->has_val = 1;
    ui_ax_cp(u->ax->value, (int)sizeof(u->ax->value), value, value ? (int)strlen(value) : 0);
}

int goome_a11y_tree(goome_ui *u, goome_a11y_node *out, int max)
{
    if (!u || !u->ax || !u->ax_on) return 0;
    for (int i = 0; out && i < u->ax->count && i < max; i++) out[i] = u->ax->n[i];
    return u->ax->count;
}

static void ui_ax_text(goome_ui *u, ui_box b, const char *s, int n, int role) { ui_ax_add(u, u->last_id, b, role, s, n, 0, 0); }

static void ui_ax_btn(goome_ui *u, uint32_t id, ui_box b, const char *s, int n) { ui_ax_add(u, id, b, GOOME_ROLE_BUTTON, s, n, 0, GM_AXK_PRESS | GM_AXK_FOCUSABLE); }

static void ui_ax_check(goome_ui *u, uint32_t id, ui_box b, const char *s, int n, int role, int on)
{
    ui_ax_add(u, id, b, role, s, n, on ? GOOME_A11Y_CHECKED : 0, GM_AXK_PRESS | GM_AXK_CHECKABLE | GM_AXK_FOCUSABLE);
}

static int ui_ax_num_in(goome_ui *u, uint32_t id, double *v, double lo, double hi)
{
    char t[256];
    double nv;
    if (!ui_ax_take(u, id, t, (int)sizeof(t)) || !ui_parse(t, &nv)) return 0;
    if (lo < hi) nv = ui_clamp(nv, lo, hi);
    else if (hi < lo) nv = ui_clamp(nv, hi, lo);
    if (nv == *v) return 0;
    *v = nv;
    return 1;
}

static int ui_ax_slider(goome_ui *u, uint32_t id, ui_box b, const char *s, int n, double *v, double lo, double hi, const char *val)
{
    int ch = ui_ax_num_in(u, id, v, lo, hi);
    int i = ui_ax_add(u, id, b, GOOME_ROLE_SLIDER, s, n, 0, GM_AXK_FOCUSABLE | GM_AXK_RANGE | GM_AXK_EDITABLE);
    if (i >= 0) u->ax->n[i].num = *v, u->ax->n[i].min = lo, u->ax->n[i].max = hi;
    ui_ax_val(u, i, val, -1);
    return ch;
}

static int ui_ax_input(goome_ui *u, uint32_t id, char *buf, int cap, int *len, int flags)
{
    if (!ui_ax_get(u, id, 's')) return 0;
    char *t = malloc((size_t)cap + 1);
    if (!t) return 0;
    if (!ui_ax_take(u, id, t, cap) || (flags & GOOME_INPUT_READONLY)) {
        free(t);
        return 0;
    }
    int n = (int)strlen(t);
    while (n > 0 && ((unsigned char)t[n] & 0xC0) == 0x80) n--;
    memcpy(buf, t, (size_t)n);
    buf[n] = 0;
    free(t);
    *len = n;
    return 1;
}

static void ui_ax_field(goome_ui *u, uint32_t id, ui_box b, const char *label, int n, const char *hint, const char *shown, int len, int cur, int flags)
{
    if (label && !strcmp(label, "##t")) return;
    if (!n && hint) label = hint, n = (int)strlen(hint);
    int k = GM_AXK_FOCUSABLE | ((flags & GOOME_INPUT_READONLY) ? 0 : GM_AXK_EDITABLE) | ((flags & GOOME_INPUT_PASSWORD) ? GM_AXK_PASSWORD : 0) | ((flags & 0x10000) ? GM_AXK_MULTILINE : 0);
    int i = ui_ax_add(u, id, b, GOOME_ROLE_INPUT, label, n, 0, k);
    if (i < 0) return;
    ui_ax_val(u, i, shown, len);
    u->ax->n[i].caret = gm_ax_chars(shown, cur);
}

static int ui_ax_combo(goome_ui *u, uint32_t id, ui_box b, const char *s, int n, int *sel, const char *const *items, int count, int open)
{
    char t[256];
    int ch = 0;
    if (items && ui_ax_take(u, id, t, (int)sizeof(t))) {
        int k = -1;
        for (int i = 0; i < count && k < 0; i++)
            if (items[i] && !strcmp(items[i], t)) k = i;
        if (k < 0 && t[0] >= '0' && t[0] <= '9') {
            const char *p = t;
            k = gm_num(&p);
        }
        if (k >= 0 && k < count && k != *sel) *sel = k, ch = 1;
    }
    const char *cur = items && *sel >= 0 && *sel < count && items[*sel] ? items[*sel] : "";
    int i = ui_ax_add(u, id, b, GOOME_ROLE_COMBO, s, n, open ? GOOME_A11Y_EXPANDED : 0, GM_AXK_PRESS | GM_AXK_EXPANDABLE | GM_AXK_FOCUSABLE | GM_AXK_EDITABLE);
    ui_ax_val(u, i, cur, -1);
    if (u->ax) u->ax->combo = i;
    return ch;
}

static void ui_ax_list(goome_ui *u, uint32_t id, ui_box b, int on)
{
    if (!ui_ax_on(u)) return;
    if (!on) {
        ui_ax_pop(u, UI_AXL, NULL);
        ui_ax_pop(u, UI_AXL, NULL);
        return;
    }
    ui_ax_push(u, u->ax->combo, UI_AXL);
    ui_ax_push(u, ui_ax_add(u, ui_hashi(id, 99), b, GOOME_ROLE_LIST, NULL, 0, 0, 0), UI_AXL);
}

static void ui_ax_item(goome_ui *u, uint32_t id, ui_box b, const char *s, int sel, int role)
{
    ui_ax_add(u, id, b, role, s, s ? ui_shown(s) : 0, sel ? GOOME_A11Y_SELECTED : 0, GM_AXK_PRESS | GM_AXK_SELECTABLE | (role == GOOME_ROLE_ITEM && u->popup ? 0 : GM_AXK_FOCUSABLE));
}

static void ui_ax_tabs(goome_ui *u, uint32_t id, ui_box b, const char *s, int on)
{
    if (on) ui_ax_push(u, ui_ax_add(u, id, b, GOOME_ROLE_GROUP, s, s ? ui_shown(s) : 0, 0, GM_AXK_FOCUSABLE), UI_AXG);
    else ui_ax_pop(u, UI_AXG, NULL);
}

static void ui_ax_expander(goome_ui *u, uint32_t id, ui_box b, const char *s, int n, int role, int open, int push)
{
    int i = ui_ax_add(u, id, b, role, s, n, open ? GOOME_A11Y_EXPANDED : 0, GM_AXK_PRESS | GM_AXK_EXPANDABLE | GM_AXK_FOCUSABLE);
    if (push && open && u->ntree < UI_MAXTREE) ui_ax_push(u, i, UI_AXT);
}

static void ui_ax_progress(goome_ui *u, ui_box b, double frac, const char *s)
{
    int i = ui_ax_add(u, u->last_id, b, GOOME_ROLE_PROGRESS, NULL, 0, 0, GM_AXK_RANGE);
    if (i < 0) return;
    char t[32];
    goome_a11y_node *d = &u->ax->n[i];
    d->min = 0, d->max = 1, d->num = frac < 0 ? -1 : ui_clamp(frac, 0, 1);
    if (frac >= 0) snprintf(t, sizeof(t), "%d%%", (int)(ui_clamp(frac, 0, 1) * 100 + 0.5));
    ui_ax_val(u, i, s ? s : frac < 0 ? "" : t, -1);
}

static void ui_ax_window(goome_ui *u, uint32_t id, ui_box b, const char *t, int flags, int collapsed)
{
    int col = flags & GOOME_WIN_COLLAPSE;
    int i = ui_ax_add(u, id, b, GOOME_ROLE_WINDOW, t, ui_shown(t), col && !collapsed ? GOOME_A11Y_EXPANDED : 0, col ? GM_AXK_EXPANDABLE : 0);
    if (i >= 0 && col) u->ax->x[i].press = ui_hashi(id, 4);
    ui_ax_push(u, i, UI_AXW);
}

static void ui_ax_menu(goome_ui *u, uint32_t id, ui_box b, const char *s, int n, int open, int push)
{
    int i = ui_ax_add(u, id, b, GOOME_ROLE_MENU, s, n, open ? GOOME_A11Y_EXPANDED : 0, GM_AXK_PRESS | GM_AXK_EXPANDABLE);
    if (push) ui_ax_push(u, i, UI_AXP);
}

static void ui_ax_menuitem(goome_ui *u, uint32_t id, ui_box b, const char *s, int n, const int *checked)
{
    ui_ax_add(u, id, b, GOOME_ROLE_MENUITEM, s, n, checked && *checked ? GOOME_A11Y_CHECKED : 0, GM_AXK_PRESS | (checked ? GM_AXK_CHECKABLE : 0));
}

static void ui_ax_pop_node(goome_ui *u, uint32_t id, int role, const char *s, int n, int ok)
{
    if (!ok || !ui_ax_on(u)) return;
    ui_ax_push(u, ui_ax_add(u, id, u->fr[u->nfr - 1].box, role, s, n, 0, 0), UI_AXP);
}

static int ui_ax_spin(goome_ui *u, uint32_t id, ui_box b, const char *s, int n, double *v, double lo, double hi, const char *fmt, int whole)
{
    if (!ui_ax_on(u)) return 0;
    int ch = ui_ax_num_in(u, id, v, lo, hi);
    if (ch && whole) *v = ui_floor(*v + 0.5);
    char t[64];
    ui_num_fmt(t, (int)sizeof(t), *v, fmt, whole);
    int i = ui_ax_add(u, id, b, GOOME_ROLE_SLIDER, s, n, 0, GM_AXK_FOCUSABLE | GM_AXK_RANGE | GM_AXK_EDITABLE);
    if (i >= 0) u->ax->n[i].num = *v, u->ax->n[i].min = lo, u->ax->n[i].max = hi;
    ui_ax_val(u, i, t, -1);
    return ch;
}

static int ui_ax_color(goome_ui *u, uint32_t id, ui_box b, const char *s, int n, uint32_t *argb, int open)
{
    if (!ui_ax_on(u)) return 0;
    char t[64];
    int ch = 0;
    if (ui_ax_take(u, id, t, (int)sizeof(t))) {
        const char *p = t[0] == '#' ? t + 1 : t;
        uint32_t c = 0;
        int k = 0;
        for (; p[k] && k < 8; k++) {
            int d = p[k] >= '0' && p[k] <= '9' ? p[k] - '0' : (p[k] | 32) >= 'a' && (p[k] | 32) <= 'f' ? (p[k] | 32) - 'a' + 10 : -1;
            if (d < 0) break;
            c = c << 4 | (uint32_t)d;
        }
        if (k == 6) c |= 0xFF000000u;
        if ((k == 6 || k == 8) && c != *argb) *argb = c, ch = 1;
    }
    snprintf(t, sizeof(t), "#%08X", (unsigned)*argb);
    int i = ui_ax_add(u, id, b, GOOME_ROLE_BUTTON, s, n, open ? GOOME_A11Y_EXPANDED : 0, GM_AXK_PRESS | GM_AXK_EXPANDABLE | GM_AXK_FOCUSABLE | GM_AXK_EDITABLE);
    ui_ax_val(u, i, t, -1);
    return ch;
}

static int ui_ax_edit(goome_ui *u, uint32_t id, char *buf, int cap)
{
    int len;
    return ui_ax_on(u) && ui_ax_input(u, id, buf, cap, &len, 0);
}

static void ui_ax_custom(goome_ui *u, uint32_t id, ui_box b, int focusable, int *click)
{
    ui_axs *a = u->ax;
    if (!a) return;
    if (a->has_role && ui_ax_on(u)) {
        int i = ui_ax_add(u, id, b, a->role, a->label, (int)strlen(a->label), 0, GM_AXK_PRESS | (focusable ? GM_AXK_FOCUSABLE : 0));
        if (a->has_val) ui_ax_val(u, i, a->value, -1);
        if (ui_ax_do(u, id, 'p')) *click = 1;
    }
    a->has_role = a->has_val = 0;
}

void goome_ui_pos(goome_ui *u, double x, double y)
{
    if (!u || !u->c) return;
    u->pos_on = 1, u->pos_x = x, u->pos_y = y;
}

void goome_ui_size(goome_ui *u, double w, double h)
{
    if (!u || !u->c) return;
    u->size_w = w > 0 ? w : 0, u->size_h = h > 0 ? h : 0;
}

static ui_sty *ui_sty_push(goome_ui *u)
{
    if (!u || !u->c) return NULL;
    if (u->nsty == u->sty_cap) {
        int cap = u->sty_cap ? u->sty_cap * 2 : 16;
        ui_sty *n = cap <= 4096 ? realloc(u->sty, sizeof(ui_sty) * (size_t)cap) : NULL;
        if (!n) return NULL;
        u->sty = n, u->sty_cap = cap;
    }
    ui_sty *s = &u->sty[u->nsty++];
    s->th = u->th, s->rad = u->rad, s->rad_on = u->rad_on;
    return s;
}

static const size_t ui_col_at[] = {
    offsetof(goome_theme, focus), offsetof(goome_theme, select), offsetof(goome_theme, caret), offsetof(goome_theme, link), offsetof(goome_theme, badge),
    offsetof(goome_theme, toast), offsetof(goome_theme, tooltip), offsetof(goome_theme, danger), offsetof(goome_theme, knob), offsetof(goome_theme, dim),
};
static const size_t ui_num_at[] = {
    offsetof(goome_theme, check_size), offsetof(goome_theme, check_radius), offsetof(goome_theme, toggle_w), offsetof(goome_theme, toggle_h),
    offsetof(goome_theme, slider_h), offsetof(goome_theme, slider_track), offsetof(goome_theme, knob_size), offsetof(goome_theme, scroll_w),
    offsetof(goome_theme, scroll_min), offsetof(goome_theme, item_h), offsetof(goome_theme, menu_item_h), offsetof(goome_theme, tab_h),
    offsetof(goome_theme, win_title_h), offsetof(goome_theme, app_title_h), offsetof(goome_theme, menubar_h), offsetof(goome_theme, statusbar_h),
    offsetof(goome_theme, focus_w), offsetof(goome_theme, focus_gap), offsetof(goome_theme, sep_w), offsetof(goome_theme, grip),
    offsetof(goome_theme, indent), offsetof(goome_theme, plot_w), offsetof(goome_theme, spinner), offsetof(goome_theme, caret_w),
    offsetof(goome_theme, win_min_w), offsetof(goome_theme, win_min_h), offsetof(goome_theme, pop_gap), offsetof(goome_theme, menu_gap),
    offsetof(goome_theme, tip_pad_x), offsetof(goome_theme, tip_pad_y), offsetof(goome_theme, tip_x), offsetof(goome_theme, tip_y),
    offsetof(goome_theme, tip_radius), offsetof(goome_theme, shadow_x), offsetof(goome_theme, shadow_y), offsetof(goome_theme, align),
    offsetof(goome_theme, heading_size), offsetof(goome_theme, small_size), offsetof(goome_theme, ease), offsetof(goome_theme, hover_ms),
    offsetof(goome_theme, press_ms), offsetof(goome_theme, popup_ms), offsetof(goome_theme, collapse_ms), offsetof(goome_theme, toast_fade_ms),
};
static const signed char ui_em_at[] = {1, 0, 1, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, -1, -1, -1, -1, -1, -1};
typedef char ui_num_check[sizeof(ui_em_at) == sizeof(ui_num_at) / sizeof(ui_num_at[0]) && sizeof(ui_num_at) / sizeof(ui_num_at[0]) == GOOME_S_COUNT - GOOME_S_CHECK_SIZE && sizeof(ui_col_at) / sizeof(ui_col_at[0]) == GOOME_S_DIM - GOOME_S_FOCUS + 1 ? 1 : -1];

static uint32_t *ui_sty_col(goome_theme *t, int what)
{
    if (what >= GOOME_S_FOCUS && what <= GOOME_S_DIM) return (uint32_t *)(void *)((char *)t + ui_col_at[what - GOOME_S_FOCUS]);
    switch (what) {
    case GOOME_S_BG: return &t->bg;
    case GOOME_S_PANEL: return &t->panel;
    case GOOME_S_BORDER: return &t->border;
    case GOOME_S_TEXT: return &t->text;
    case GOOME_S_TEXT_DIM: return &t->text_dim;
    case GOOME_S_ACCENT: return &t->accent;
    case GOOME_S_ACCENT_TEXT: return &t->accent_text;
    case GOOME_S_HOVER: return &t->hover;
    case GOOME_S_ACTIVE: return &t->active;
    case GOOME_S_INPUT_BG: return &t->input_bg;
    case GOOME_S_SHADOW: return &t->shadow;
    }
    return NULL;
}

static double *ui_sty_num(goome_theme *t, int what)
{
    if (what >= GOOME_S_CHECK_SIZE && what < GOOME_S_COUNT) return (double *)(void *)((char *)t + ui_num_at[what - GOOME_S_CHECK_SIZE]);
    switch (what) {
    case GOOME_S_RADIUS: return &t->radius;
    case GOOME_S_PAD: return &t->pad;
    case GOOME_S_GAP: return &t->gap;
    case GOOME_S_BORDER_WIDTH: return &t->border_width;
    case GOOME_S_FONT_SIZE: return &t->font_size;
    case GOOME_S_SHADOW_BLUR: return &t->shadow_blur;
    case GOOME_S_ANIM_MS: return &t->anim_ms;
    }
    return NULL;
}

void goome_ui_push_color(goome_ui *u, int what, uint32_t argb)
{
    uint32_t *f = ui_sty_push(u) ? ui_sty_col(&u->th, what) : NULL;
    if (f) *f = argb;
}

void goome_ui_push_num(goome_ui *u, int what, double v)
{
    double *f = ui_sty_push(u) ? ui_sty_num(&u->th, what) : NULL;
    if (!f || v != v) return;
    *f = what == GOOME_S_FONT_SIZE ? ui_clamp(v, 4, 400) : v;
    ui_sty_apply(u);
}

void goome_ui_push_radii(goome_ui *u, goome_radii r)
{
    if (!ui_sty_push(u)) return;
    u->rad = ui_rgrow(r, 0), u->rad_on = 1;
}

void goome_ui_push_font(goome_ui *u, goome_font *f)
{
    if (!ui_sty_push(u)) return;
    u->th.font = f;
    ui_sty_apply(u);
}

static goome_skin *ui_skin_of(goome_theme *t, int kind)
{
    switch (kind) {
    case GOOME_SKIN_BUTTON: return &t->skin_button;
    case GOOME_SKIN_PANEL: return &t->skin_panel;
    case GOOME_SKIN_INPUT: return &t->skin_input;
    case GOOME_SKIN_CHECKBOX: return &t->skin_checkbox;
    case GOOME_SKIN_RADIO: return &t->skin_radio;
    case GOOME_SKIN_TOGGLE: return &t->skin_toggle;
    case GOOME_SKIN_SLIDER_TRACK: return &t->skin_slider;
    case GOOME_SKIN_SLIDER_FILL: return &t->skin_slider_fill;
    case GOOME_SKIN_SCROLLBAR: return &t->skin_scrollbar;
    case GOOME_SKIN_TAB: return &t->skin_tab;
    case GOOME_SKIN_TITLEBAR: return &t->skin_titlebar;
    case GOOME_SKIN_TOOLTIP: return &t->skin_tooltip;
    case GOOME_SKIN_MENU: return &t->skin_menu;
    case GOOME_SKIN_POPUP: return &t->skin_popup;
    }
    return NULL;
}

void goome_ui_push_skin(goome_ui *u, int kind, const goome_skin *s)
{
    if (!ui_sty_push(u)) return;
    goome_skin *k = ui_skin_of(&u->th, kind);
    if (!k) return;
    if (s) *k = *s;
    else memset(k, 0, sizeof(*k));
}

void goome_ui_push_icon(goome_ui *u, int icon, const goome_bitmap *b)
{
    if (!ui_sty_push(u) || icon < 0 || icon >= GOOME_ICON_COUNT) return;
    u->th.icon[icon] = b;
}

void goome_ui_push_theme(goome_ui *u, const goome_theme *t)
{
    if (!ui_sty_push(u) || !t) return;
    u->th = *t, u->rad_on = 0;
    ui_sty_apply(u);
}

void goome_ui_pop(goome_ui *u, int n)
{
    if (!u || n <= 0) return;
    if (n > u->nsty) {
        if (!u->sty_warn) gm_fail(u->g, GOOME_EARG, "ui pop of %d but only %d pushed", n, u->nsty), u->sty_warn = 1;
        n = u->nsty;
    }
    if (!n) return;
    u->nsty -= n;
    ui_sty *s = &u->sty[u->nsty];
    u->th = s->th, u->rad = s->rad, u->rad_on = s->rad_on;
    if (u->c) ui_sty_apply(u);
}

int goome_ui_custom(goome_ui *u, const char *id, double w, double h, int flags, goome_ui_item *it)
{
    goome_ui_item spare;
    if (!it) it = &spare;
    memset(it, 0, sizeof(*it));
    if (!u || !u->c) return 0;
    ui_box b = ui_item(u, w > 0 ? w : 0, h > 0 ? h : u->fh);
    uint32_t id2 = ui_id(u, id);
    ui_st *s = ui_get(u, id2, NULL);
    int hit = !(flags & GOOME_ITEM_NO_HIT), ax = flags & GOOME_ITEM_DRAG_X, ay = flags & GOOME_ITEM_DRAG_Y, fo = flags & GOOME_ITEM_FOCUSABLE;
    if (fo) ui_nav(u, id2, b, ui_tr(u, u->th.radius), UN_CUSTOM, NULL, 0);
    int hov = hit && ui_hover(u, id2, b), press = 0, key = 0;
    if (!hit) u->last_id = id2, u->last_hover = 0;
    if (hit && u->layer == UI_ROOT && !u->app_on) ui_hit_add(u, u->nclip ? ui_cut(b, u->clip[u->nclip - 1]) : b);
    if (hov && u->pressed && !u->active) {
        u->active = id2, press = 1;
        s->v[0] = s->v[2] = u->mx, s->v[1] = s->v[3] = u->my, s->v[4] = (double)u->now + u->cfg.repeat_delay_ms;
        if (fo && u->focus != id2) ui_focus_to(u, id2, u->cfg.focus_on_mouse, UN_CUSTOM);
    }
    int mine = u->active == id2, held = mine && (u->down || u->pressed);
    if (ax || ay) {
        ui_grabby(u, id2);
        if (held || (hov && !u->active)) u->cur_want = u->cfg.cursor[ax && ay ? GOOME_CUR_CUSTOM_XY : ax ? GOOME_CUR_CUSTOM_X : GOOME_CUR_CUSTOM_Y];
    }
    double dx = 0, dy = 0, tx = mine ? u->mx - s->v[0] : 0, ty = mine ? u->my - s->v[1] : 0;
    if (held) dx = u->mx - s->v[2], dy = u->my - s->v[3], s->v[2] = u->mx, s->v[3] = u->my;
    if (ax && !ay) dy = ty = 0;
    if (ay && !ax) dx = tx = 0;
    int rel = mine && u->released, click = rel && hov;
    if (flags & GOOME_ITEM_REPEAT) {
        click = press;
        if (held && !press && (double)u->now >= s->v[4]) click = 1, s->v[4] = (double)u->now + u->cfg.repeat_ms;
        if (held) u->anim_next = 1;
    }
    if (fo && u->focus == id2) {
        u->focus_seen = 1;
        for (int k = 0; k < u->nkeys && !key; k++)
            if (u->keys[k].key) key = u->keys[k].key | u->keys[k].mods, u->keys[k].key = 0, u->keys[k].text[0] = 0;
        if (key == GOOME_K_ENTER || key == GOOME_K_SPACE) click = 1;
        if (key == GOOME_K_ESC) ui_unfocus(u);
    }
    int right = hit && u->rpressed && u->hover_layer == u->layer && ui_in(b, u->pmx, u->pmy) && (!u->nclip || ui_in(u->clip[u->nclip - 1], u->pmx, u->pmy));
    ui_ax_custom(u, id2, b, fo, &click);
    it->x = b.x, it->y = b.y, it->w = b.w, it->h = b.h;
    it->hovered = hov, it->pressed = press, it->held = held, it->released = rel, it->clicked = click;
    it->double_clicked = press && u->clicks == 2, it->right_clicked = right, it->focused = fo && u->focus == id2;
    it->mx = u->mx, it->my = u->my, it->dx = dx, it->dy = dy, it->drag_x = tx, it->drag_y = ty, it->key = key;
    u->last_box = b;
    return press || rel || click || right || key || dx != 0 || dy != 0;
}

goome_canvas *goome_ui_canvas(goome_ui *u)
{
    if (!u || !u->c) return NULL;
    goome_canvas *c = u->c, *k = &u->lay;
    if (u->lay_list) ui_lay_flush(u);
    size_t need = (size_t)c->w * (size_t)c->h;
    if (!need) return NULL;
    if (need > u->lay_cap) {
        uint32_t *n = calloc(need, sizeof(uint32_t));
        if (!n) return NULL;
        free(u->lay_px);
        u->lay_px = n, u->lay_cap = need;
    }
    memset(k, 0, sizeof(*k));
    k->px = u->lay_px, k->w = k->stride = c->w, k->h = c->h, k->ox = c->ox, k->oy = c->oy, k->scale = c->scale, k->mon = c->mon;
    k->kx1 = c->w, k->ky1 = c->h;
    if (u->nclip) {
        ui_box b = u->clip[u->nclip - 1];
        goome_clip_push(k, b.x, b.y, b.w, b.h);
    }
    u->lay_list = u->cl, u->lay_alpha = u->alpha;
    return k;
}

double *goome_ui_store(goome_ui *u, const char *id, int n)
{
    if (!u || n <= 0 || n > (1 << 24)) return NULL;
    uint32_t h = ui_hashi(ui_id(u, id), 0x53544F52u);
    ui_st *s = ui_get(u, h, NULL);
    s->keep = 1;
    ui_keep *k = s->i[0] > 0 && s->i[0] <= u->nkeep && u->keeps[s->i[0] - 1].id == h ? &u->keeps[s->i[0] - 1] : NULL;
    for (int i = 0; !k && i < u->nkeep; i++)
        if (u->keeps[i].id == h) k = &u->keeps[i], s->i[0] = i + 1;
    if (!k) {
        if (u->nkeep == u->keep_cap) {
            int cap = u->keep_cap ? u->keep_cap * 2 : 16;
            ui_keep *m = realloc(u->keeps, sizeof(ui_keep) * (size_t)cap);
            if (!m) return NULL;
            u->keeps = m, u->keep_cap = cap;
        }
        k = &u->keeps[u->nkeep++];
        memset(k, 0, sizeof(*k));
        k->id = h, s->i[0] = u->nkeep;
    }
    if (k->n < n) {
        double *v = realloc(k->v, sizeof(double) * (size_t)n);
        if (!v) return NULL;
        memset(v + k->n, 0, sizeof(double) * (size_t)(n - k->n));
        k->v = v, k->n = n;
    }
    k->seen = u->frame;
    return k->v;
}

static ui_st *ui_tween(goome_ui *u, const char *id, uint32_t tag, double target, double ms, int ease)
{
    int fresh;
    ui_st *s = ui_get(u, ui_hashi(ui_id(u, id), tag), &fresh);
    if (s == &u->dummy) return NULL;
    ms = ui_ms(u, ms);
    if (fresh || ms <= 0) {
        s->v[0] = s->v[1] = target, s->v[2] = s->v[3] = 0, s->i[0] = ease, s->i[1] = u->frame;
        return s;
    }
    if (s->i[1] != u->frame) s->v[2] += u->dt, s->i[1] = u->frame;
    s->i[0] = ease;
    if (target != s->v[1]) s->v[0] = s->v[4], s->v[1] = target, s->v[2] = 0, s->v[3] = ms;
    if (s->v[2] < s->v[3]) u->anim_next = 1;
    return s;
}
static int ui_tween_done(ui_st *s) { return s->v[3] <= 0 || s->v[2] >= s->v[3]; }

double goome_ui_anim(goome_ui *u, const char *id, double target, double ms, int ease)
{
    if (!u) return target;
    ui_st *s = ui_tween(u, id, 0x414E494Du, target, ms, ease);
    if (!s) return target;
    s->v[4] = ui_tween_done(s) ? s->v[1] : s->v[0] + (s->v[1] - s->v[0]) * ui_ease(u, s->i[0], s->v[2] / s->v[3]);
    return s->v[4];
}

static uint32_t ui_mix_lin(uint32_t a, uint32_t b, double t)
{
    double aa = (double)(a >> 24) / 255, ba = (double)(b >> 24) / 255, oa = ui_clamp(aa + (ba - aa) * t, 0, 1);
    uint32_t out = (uint32_t)(oa * 255 + 0.5) << 24;
    if (oa <= 0) return 0;
    for (int sh = 0; sh < 24; sh += 8) {
        double x = (double)((a >> sh) & 255) / 255, y = (double)((b >> sh) & 255) / 255;
        double lx = x * x * aa, ly = y * y * ba, l = lx + (ly - lx) * t;
        out |= (uint32_t)(ui_sqrt(ui_clamp(l / oa, 0, 1)) * 255 + 0.5) << sh;
    }
    return out;
}

uint32_t goome_ui_anim_color(goome_ui *u, const char *id, uint32_t target, double ms, int ease)
{
    if (!u) return target;
    ui_st *s = ui_tween(u, id, 0x414E4943u, target, ms, ease);
    if (!s) return target;
    uint32_t c = ui_tween_done(s) ? (uint32_t)s->v[1] : ui_mix_lin((uint32_t)s->v[0], (uint32_t)s->v[1], ui_ease(u, s->i[0], s->v[2] / s->v[3]));
    s->v[4] = c;
    return c;
}

goome_ui_config *goome_ui_cfg(goome_ui *u) { return u ? &u->cfg : NULL; }

static void ui_bind_raw(goome_ui_config *c, int act, const char *combos)
{
    memset(c->keys[act], 0, sizeof(c->keys[act]));
    int n = 0;
    for (const char *p = combos; p && *p && n < 4;) {
        while (*p == ' ' || *p == ',') p++;
        char one[48];
        int l = 0, k, m;
        while (p[l] && p[l] != ' ' && p[l] != ',' && l < 47) one[l] = p[l], l++;
        one[l] = 0;
        if (!l) break;
        if (goome_parse_combo(one, &k, &m) == GOOME_OK) c->keys[act][n].key = k, c->keys[act][n].mods = m, n++;
        p += l;
        while (*p && *p != ' ' && *p != ',') p++;
    }
}

void goome_ui_config_default(goome_ui_config *c)
{
    static const char *const keys[GOOME_ACT_COUNT] = {
        "ctrl+c super+c", "ctrl+x super+x", "ctrl+v super+v", "ctrl+z super+z", "ctrl+y ctrl+shift+z super+y super+shift+z", "ctrl+a super+a",
        "ctrl+left super+left", "ctrl+right super+right", "ctrl+backspace super+backspace", "ctrl+delete super+delete", "ctrl+home super+home", "ctrl+end super+end",
    };
    static const int cur[GOOME_CUR_COUNT] = {
        GOOME_CURSOR_ARROW, GOOME_CURSOR_IBEAM, GOOME_CURSOR_HAND, GOOME_CURSOR_RESIZE_EW, GOOME_CURSOR_RESIZE_EW, GOOME_CURSOR_RESIZE_NS, GOOME_CURSOR_RESIZE_NWSE,
        GOOME_CURSOR_RESIZE_EW, GOOME_CURSOR_MOVE, GOOME_CURSOR_RESIZE_EW, GOOME_CURSOR_RESIZE_NS, GOOME_CURSOR_MOVE,
    };
    if (!c) return;
    memset(c, 0, sizeof(*c));
    c->tip_delay_ms = 500, c->dbl_click_ms = 450, c->dbl_click_px = 5, c->drag_px = 4, c->repeat_delay_ms = 380, c->repeat_ms = 55;
    c->blink_ms = 530, c->menu_delay_ms = 120, c->toast_ms = 3000, c->scroll_lines = 3;
    c->raise_on_click = c->esc_closes = c->tab_nav = 1;
    memcpy(c->cursor, cur, sizeof(cur));
    for (int i = 0; i < GOOME_ACT_COUNT; i++) ui_bind_raw(c, i, keys[i]);
}

int goome_ui_bind(goome_ui *u, int act, const char *combos)
{
    if (!u || act < 0 || act >= GOOME_ACT_COUNT) return GOOME_EARG;
    ui_bind_raw(&u->cfg, act, combos);
    return combos && *combos && !u->cfg.keys[act][0].key ? GOOME_EARG : GOOME_OK;
}

static uint32_t ui_tint(uint32_t c, uint32_t to, double k) { return ui_mix(c, (c & 0xFF000000u) | (to & 0xFFFFFFu), k); }

void goome_ui_theme_make(goome_theme *t, uint32_t accent, int dark, double radius)
{
    if (!t) return;
    if (dark) goome_ui_theme_dark(t);
    else goome_ui_theme_light(t);
    accent |= 0xFF000000u;
    int lum = (int)(((accent >> 16) & 255) * 299 + ((accent >> 8) & 255) * 587 + (accent & 255) * 114) / 1000;
    t->accent = accent;
    t->accent_text = lum > 150 ? ui_mix(accent, 0xFF000000u, 0.86) : 0xFFFFFFFFu;
    double k = dark ? 1 : 0.6;
    t->bg = ui_tint(t->bg, accent, 0.03 * k), t->panel = ui_tint(t->panel, accent, 0.04 * k), t->input_bg = ui_tint(t->input_bg, accent, 0.05 * k);
    t->hover = ui_tint(t->hover, accent, 0.08 * k), t->active = ui_tint(t->active, accent, 0.12 * k), t->text_dim = ui_tint(t->text_dim, accent, 0.1 * k);
    if (!(radius >= 0)) radius = 10;
    t->radius = radius;
    t->check_radius = ui_min(ui_round(radius * 0.5), 9), t->tip_radius = ui_min(ui_round(radius * 0.8), 12);
}

int goome_ui_theme_preset(goome_theme *t, int preset)
{
    if (!t || preset < 0 || preset >= GOOME_PRESET_COUNT) return GOOME_EARG;
    if (preset == GOOME_PRESET_LIGHT || preset == GOOME_PRESET_PAPER) goome_ui_theme_light(t);
    else goome_ui_theme_dark(t);
    switch (preset) {
    case GOOME_PRESET_MIDNIGHT:
        t->bg = 0xFF0E1320, t->panel = 0xF0151B2B, t->border = 0x22FFFFFF, t->text = 0xFFE6EAF2, t->text_dim = 0xFF8A94A8, t->accent = 0xFF6EA8FF;
        t->accent_text = 0xFF0B1424, t->hover = 0xFF243049, t->active = 0xFF2E3B58, t->input_bg = 0xFF1C2438, t->shadow = 0x99000000, t->radius = 8;
        break;
    case GOOME_PRESET_PAPER:
        t->bg = 0xFFF3EFE6, t->panel = 0xFAFBF8F1, t->border = 0x2A3B2F1E, t->text = 0xFF2B2620, t->text_dim = 0xFF7A6F60, t->accent = 0xFFC0572B;
        t->accent_text = 0xFFFFFFFF, t->hover = 0xFFE7E0D2, t->active = 0xFFDCD3C1, t->input_bg = 0xFFEDE7DA, t->shadow = 0x30402A10, t->radius = 6, t->shadow_blur = 14;
        break;
    case GOOME_PRESET_MINT:
        t->bg = 0xFF101815, t->panel = 0xEE16211C, t->border = 0x1E9BFFCB, t->text = 0xFFE3F2EA, t->text_dim = 0xFF86A397, t->accent = 0xFF5FE0A8;
        t->accent_text = 0xFF06231A, t->hover = 0xFF22332B, t->active = 0xFF2B4036, t->input_bg = 0xFF1B2923, t->radius = 12;
        break;
    case GOOME_PRESET_HIGH_CONTRAST:
        t->bg = 0xFF000000, t->panel = 0xFF000000, t->border = 0xFFFFFFFF, t->text = 0xFFFFFFFF, t->text_dim = 0xFFD0D0D0, t->accent = 0xFFFFE600;
        t->accent_text = 0xFF000000, t->hover = 0xFF333333, t->active = 0xFF555555, t->input_bg = 0xFF1A1A1A, t->shadow = 0x00000000, t->radius = 4, t->border_width = 2, t->shadow_blur = 0;
        break;
    case GOOME_PRESET_ROSE:
        t->bg = 0xFF1C1418, t->panel = 0xEE241A1F, t->border = 0x22FFD6E4, t->text = 0xFFF5E8EE, t->text_dim = 0xFFB09AA5, t->accent = 0xFFF27BA6;
        t->accent_text = 0xFF2A0D18, t->hover = 0xFF3A2A31, t->active = 0xFF46333C, t->input_bg = 0xFF2E2228, t->radius = 14;
        break;
    }
    return GOOME_OK;
}

void goome_ui_scale(goome_ui *u, double s)
{
    if (!u || !(s > 0) || s > 64) return;
    goome_theme *t = &u->th;
    t->font_size *= s, t->radius *= s, t->pad *= s, t->gap *= s, t->border_width *= s, t->shadow_blur *= s;
    for (int i = 0; i < (int)(sizeof(ui_num_at) / sizeof(ui_num_at[0])); i++)
        if (!ui_em_at[i]) *(double *)(void *)((char *)t + ui_num_at[i]) *= s;
    if (u->c) ui_sty_apply(u);
}
#endif
