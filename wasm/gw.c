#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "goome.h"
#include "gw.h"

#ifdef __wasm__
void lc_set_time(double ms);
#endif

enum { GJ_NULL, GJ_BOOL, GJ_NUM, GJ_STR, GJ_ARR, GJ_OBJ };

typedef struct gw_j gw_j;
struct gw_j {
    int t, n;
    double v;
    char *s;
    const char *k;
    gw_j *kid, *next;
    void *aux;
};

typedef struct {
    int type, cap, nitems;
    double d;
    int i;
    uint32_t c;
    char *s;
    const char **items;
    const char *key;
} gw_var;

enum { GV_NUM, GV_INT, GV_BOOL, GV_COLOR, GV_STR, GV_LIST };

typedef struct {
    double *s;
    gw_j *params;
    int fl[3];
} gw_env;

typedef struct {
    char *src;
    gw_j *pool;
    int npool, used;
    gw_j *root;
    gw_var *vars;
    int nvars;
    goome_theme *themes;
    int nthemes;
    gw_j **defs;
    int ndefs;
    goome_bitmap *bms;
    int *bm_ok;
    int nbms;
    int app;
    gw_j *app_title, *app_status, *nodes, *hotkeys;
    int app_flags;
} gw_prog;

static goome *gw_g;
static goome_ui *gw_u;
static goome_font *gw_font;
static gw_prog gw_p;
static goome_bitmap gw_ph;
static int gw_ph_ok;
static uint8_t *gw_px;
static size_t gw_px_cap;
static char gw_logbuf[8192];
static int gw_loglen;
static char gw_tbuf[1 << 16];
static size_t gw_tpos;
static char gw_clip[4096];
static double gw_gsx[64], gw_gsy[64], gw_gax[64], gw_gay[64];
static char gw_err[128];

static const size_t gw_off[GOOME_S_COUNT] = {
    offsetof(goome_theme, bg), offsetof(goome_theme, panel), offsetof(goome_theme, border), offsetof(goome_theme, text), offsetof(goome_theme, text_dim),
    offsetof(goome_theme, accent), offsetof(goome_theme, accent_text), offsetof(goome_theme, hover), offsetof(goome_theme, active), offsetof(goome_theme, input_bg),
    offsetof(goome_theme, shadow), offsetof(goome_theme, radius), offsetof(goome_theme, pad), offsetof(goome_theme, gap), offsetof(goome_theme, border_width),
    offsetof(goome_theme, font_size), offsetof(goome_theme, shadow_blur), offsetof(goome_theme, anim_ms), offsetof(goome_theme, focus), offsetof(goome_theme, select),
    offsetof(goome_theme, caret), offsetof(goome_theme, link), offsetof(goome_theme, badge), offsetof(goome_theme, toast), offsetof(goome_theme, tooltip),
    offsetof(goome_theme, danger), offsetof(goome_theme, knob), offsetof(goome_theme, dim), offsetof(goome_theme, check_size), offsetof(goome_theme, check_radius),
    offsetof(goome_theme, toggle_w), offsetof(goome_theme, toggle_h), offsetof(goome_theme, slider_h), offsetof(goome_theme, slider_track), offsetof(goome_theme, knob_size),
    offsetof(goome_theme, scroll_w), offsetof(goome_theme, scroll_min), offsetof(goome_theme, item_h), offsetof(goome_theme, menu_item_h), offsetof(goome_theme, tab_h),
    offsetof(goome_theme, win_title_h), offsetof(goome_theme, app_title_h), offsetof(goome_theme, menubar_h), offsetof(goome_theme, statusbar_h), offsetof(goome_theme, focus_w),
    offsetof(goome_theme, focus_gap), offsetof(goome_theme, sep_w), offsetof(goome_theme, grip), offsetof(goome_theme, indent), offsetof(goome_theme, plot_w),
    offsetof(goome_theme, spinner), offsetof(goome_theme, caret_w), offsetof(goome_theme, win_min_w), offsetof(goome_theme, win_min_h), offsetof(goome_theme, pop_gap),
    offsetof(goome_theme, menu_gap), offsetof(goome_theme, tip_pad_x), offsetof(goome_theme, tip_pad_y), offsetof(goome_theme, tip_x), offsetof(goome_theme, tip_y),
    offsetof(goome_theme, tip_radius), offsetof(goome_theme, shadow_x), offsetof(goome_theme, shadow_y), offsetof(goome_theme, align), offsetof(goome_theme, heading_size),
    offsetof(goome_theme, small_size), offsetof(goome_theme, ease), offsetof(goome_theme, hover_ms), offsetof(goome_theme, press_ms), offsetof(goome_theme, popup_ms),
    offsetof(goome_theme, collapse_ms), offsetof(goome_theme, toast_fade_ms),
};

static int gw_is_color(int i) { return i <= GOOME_S_SHADOW || (i >= GOOME_S_FOCUS && i <= GOOME_S_DIM); }

static double gw_th_get(const goome_theme *t, int i)
{
    if (i < 0 || i >= GOOME_S_COUNT) return 0;
    const char *p = (const char *)t + gw_off[i];
    if (gw_is_color(i)) return (double)*(const uint32_t *)(const void *)p;
    return *(const double *)(const void *)p;
}

static void gw_th_set(goome_theme *t, int i, double v)
{
    if (i < 0 || i >= GOOME_S_COUNT) return;
    char *p = (char *)t + gw_off[i];
    if (gw_is_color(i)) *(uint32_t *)(void *)p = (uint32_t)(int64_t)v;
    else *(double *)(void *)p = v;
}

static void gw_log(const char *s)
{
    int n = (int)strlen(s);
    if (gw_loglen + n + 2 >= (int)sizeof(gw_logbuf)) return;
    memcpy(gw_logbuf + gw_loglen, s, (size_t)n);
    gw_loglen += n;
    gw_logbuf[gw_loglen++] = '\n';
    gw_logbuf[gw_loglen] = 0;
}

static gw_j *gw_new(int t)
{
    if (gw_p.used >= gw_p.npool) return NULL;
    gw_j *j = &gw_p.pool[gw_p.used++];
    memset(j, 0, sizeof(*j));
    j->t = t;
    return j;
}

static void gw_ws(const char **p)
{
    while (**p == ' ' || **p == '\n' || **p == '\r' || **p == '\t') (*p)++;
}

static double gw_atof(const char **p)
{
    static const double pw[23] = {1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9, 1e10, 1e11, 1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22};
    const char *s = *p;
    int neg = *s == '-', dig = 0, ex = 0;
    if (neg || *s == '+') s++;
    uint64_t m = 0;
    for (; *s >= '0' && *s <= '9'; s++)
        if (dig < 19) m = m * 10 + (uint64_t)(*s - '0'), dig += m > 0;
        else ex++;
    if (*s == '.')
        for (s++; *s >= '0' && *s <= '9'; s++)
            if (dig < 19) m = m * 10 + (uint64_t)(*s - '0'), dig += m > 0, ex--;
    if (*s == 'e' || *s == 'E') {
        s++;
        int en = *s == '-', e = 0;
        if (*s == '-' || *s == '+') s++;
        for (; *s >= '0' && *s <= '9'; s++)
            if (e < 10000) e = e * 10 + *s - '0';
        ex += en ? -e : e;
    }
    *p = s;
    double v = (double)m;
    if (m <= ((uint64_t)1 << 53) && ex >= -22 && ex <= 22) v = ex < 0 ? v / pw[-ex] : v * pw[ex];
    else {
        while (ex > 22) v *= 1e22, ex -= 22;
        while (ex < -22) v /= 1e22, ex += 22;
        v = ex < 0 ? v / pw[-ex] : v * pw[ex];
    }
    return neg ? -v : v;
}

static int gw_hex(char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; }

static char *gw_utf8(char *o, uint32_t c)
{
    if (c < 0x80) *o++ = (char)c;
    else if (c < 0x800) *o++ = (char)(0xC0 | c >> 6), *o++ = (char)(0x80 | (c & 63));
    else if (c < 0x10000) *o++ = (char)(0xE0 | c >> 12), *o++ = (char)(0x80 | ((c >> 6) & 63)), *o++ = (char)(0x80 | (c & 63));
    else *o++ = (char)(0xF0 | c >> 18), *o++ = (char)(0x80 | ((c >> 12) & 63)), *o++ = (char)(0x80 | ((c >> 6) & 63)), *o++ = (char)(0x80 | (c & 63));
    return o;
}

static uint32_t gw_u4(const char *s)
{
    uint32_t v = 0;
    for (int i = 0; i < 4; i++) {
        int h = gw_hex(s[i]);
        if (h < 0) return 0xFFFD;
        v = v << 4 | (uint32_t)h;
    }
    return v;
}

static char *gw_str(char **p)
{
    char *s = *p + 1, *o = s, *start = s;
    while (*s && *s != '"') {
        if (*s != '\\') {
            *o++ = *s++;
            continue;
        }
        s++;
        char c = *s ? *s++ : 0;
        switch (c) {
        case 'n': *o++ = '\n'; break;
        case 't': *o++ = '\t'; break;
        case 'r': *o++ = '\r'; break;
        case 'b': *o++ = '\b'; break;
        case 'f': *o++ = '\f'; break;
        case 'u': {
            if (strlen(s) < 4) return NULL;
            uint32_t u = gw_u4(s);
            s += 4;
            if (u >= 0xD800 && u < 0xDC00 && s[0] == '\\' && s[1] == 'u' && strlen(s) >= 6) {
                uint32_t lo = gw_u4(s + 2);
                if (lo >= 0xDC00 && lo < 0xE000) u = 0x10000 + ((u - 0xD800) << 10) + (lo - 0xDC00), s += 6;
            }
            o = gw_utf8(o, u);
            break;
        }
        case 0: return NULL;
        default: *o++ = c;
        }
    }
    if (*s != '"') return NULL;
    *p = s + 1;
    *o = 0;
    return start;
}

static gw_j *gw_parse(char **p, int depth)
{
    const char *q = *p;
    gw_ws(&q);
    *p = (char *)q;
    if (depth > 200) return NULL;
    char c = **p;
    gw_j *j = NULL;
    if (c == '{' || c == '[') {
        j = gw_new(c == '{' ? GJ_OBJ : GJ_ARR);
        if (!j) return NULL;
        (*p)++;
        gw_j **tail = &j->kid;
        for (;;) {
            q = *p;
            gw_ws(&q);
            *p = (char *)q;
            if (**p == (c == '{' ? '}' : ']')) {
                (*p)++;
                break;
            }
            if (j->n) {
                if (**p != ',') return NULL;
                (*p)++;
            }
            const char *key = NULL;
            if (c == '{') {
                q = *p;
                gw_ws(&q);
                *p = (char *)q;
                if (**p != '"' || !(key = gw_str(p))) return NULL;
                q = *p;
                gw_ws(&q);
                *p = (char *)q;
                if (**p != ':') return NULL;
                (*p)++;
            }
            gw_j *v = gw_parse(p, depth + 1);
            if (!v) return NULL;
            v->k = key;
            *tail = v;
            tail = &v->next;
            j->n++;
        }
        return j;
    }
    if (c == '"') {
        j = gw_new(GJ_STR);
        if (!j || !(j->s = gw_str(p))) return NULL;
        return j;
    }
    if (!strncmp(*p, "true", 4) || !strncmp(*p, "false", 5)) {
        j = gw_new(GJ_BOOL);
        if (!j) return NULL;
        j->v = **p == 't';
        *p += **p == 't' ? 4 : 5;
        return j;
    }
    if (!strncmp(*p, "null", 4)) {
        *p += 4;
        return gw_new(GJ_NULL);
    }
    if (c == '-' || (c >= '0' && c <= '9')) {
        j = gw_new(GJ_NUM);
        if (!j) return NULL;
        q = *p;
        j->v = gw_atof(&q);
        *p = (char *)q;
        return j;
    }
    return NULL;
}

static gw_j *gw_key(const gw_j *o, const char *k)
{
    if (!o || o->t != GJ_OBJ) return NULL;
    for (gw_j *j = o->kid; j; j = j->next)
        if (!strcmp(j->k, k)) return j;
    return NULL;
}

static gw_j *gw_at(const gw_j *a, int i)
{
    if (!a || (a->t != GJ_ARR && a->t != GJ_OBJ) || i < 0) return NULL;
    gw_j *j = a->kid;
    while (j && i--) j = j->next;
    return j;
}

static double gw_num(const gw_j *j, double d) { return j && (j->t == GJ_NUM || j->t == GJ_BOOL) ? j->v : d; }
static double gw_kn(const gw_j *o, const char *k, double d) { return gw_num(gw_key(o, k), d); }
static int gw_ki(const gw_j *o, const char *k, int d) { return (int)gw_kn(o, k, d); }
static const char *gw_ks(const gw_j *o, const char *k)
{
    gw_j *j = gw_key(o, k);
    return j && j->t == GJ_STR ? j->s : NULL;
}

static gw_var *gw_var_at(int i) { return i >= 0 && i < gw_p.nvars ? &gw_p.vars[i] : NULL; }

static double gw_var_num(const gw_var *v)
{
    if (!v) return 0;
    switch (v->type) {
    case GV_NUM: return v->d;
    case GV_INT: case GV_BOOL: return v->i;
    case GV_COLOR: return v->c;
    }
    return 0;
}

static void gw_var_put(gw_var *v, double x)
{
    if (!v) return;
    switch (v->type) {
    case GV_NUM: v->d = x; break;
    case GV_INT: v->i = (int)x; break;
    case GV_BOOL: v->i = x != 0; break;
    case GV_COLOR: v->c = (uint32_t)(int64_t)x; break;
    }
}

static char *gw_talloc(size_t n)
{
    if (gw_tpos + n > sizeof(gw_tbuf)) return NULL;
    char *p = gw_tbuf + gw_tpos;
    gw_tpos += n;
    return p;
}

static const char *gw_text(const gw_j *t)
{
    if (!t) return "";
    if (t->t == GJ_STR) return t->s;
    if (t->t != GJ_ARR) return "";
    char tmp[1024];
    size_t n = 0;
    tmp[0] = 0;
    for (gw_j *k = t->kid; k; k = k->next) {
        char part[512];
        const char *s = part;
        part[0] = 0;
        if (k->t == GJ_STR) s = k->s;
        else if (k->t == GJ_NUM) {
            gw_var *v = gw_var_at((int)k->v);
            if (!v) continue;
            switch (v->type) {
            case GV_NUM: snprintf(part, sizeof(part), "%g", v->d); break;
            case GV_INT: case GV_BOOL: snprintf(part, sizeof(part), "%d", v->i); break;
            case GV_COLOR: snprintf(part, sizeof(part), "#%08X", (unsigned)v->c); break;
            case GV_STR: s = v->s ? v->s : ""; break;
            case GV_LIST: s = v->nitems ? v->items[0] : ""; break;
            }
        }
        size_t l = strlen(s);
        if (n + l >= sizeof(tmp)) l = sizeof(tmp) - 1 - n;
        memcpy(tmp + n, s, l);
        n += l;
        tmp[n] = 0;
    }
    char *o = gw_talloc(n + 1);
    if (!o) return "";
    memcpy(o, tmp, n + 1);
    return o;
}

static double gw_eval(const gw_j *e, const gw_env *env)
{
    if (!e) return 0;
    if (e->t == GJ_NUM || e->t == GJ_BOOL) return e->v;
    if (e->t != GJ_ARR || !e->kid) return 0;
    int op = (int)e->kid->v;
    gw_j *a = e->kid->next, *b = a ? a->next : NULL, *c = b ? b->next : NULL;
    switch (op) {
    case 0: return gw_num(a, 0);
    case 1: return gw_var_num(gw_var_at((int)gw_num(a, -1)));
    case 2: return env && env->s ? env->s[(int)gw_num(a, 0)] : 0;
    case 3: {
        gw_j *p = env ? gw_at(env->params, (int)gw_num(a, 0)) : NULL;
        return gw_num(p, 0);
    }
    case 4: {
        int i = (int)gw_num(a, 0);
        return env && i >= 0 && i < 3 ? env->fl[i] : 0;
    }
    case 5: return gw_th_get(goome_ui_theme(gw_u), (int)gw_num(a, 0));
    case 6: {
        int fn = (int)gw_num(a, 0);
        double x = gw_eval(b, env), y = c ? gw_eval(c, env) : 0;
        return fn == 0 ? (x < y ? x : y) : fn == 1 ? (x > y ? x : y) : x < 0 ? -x : x;
    }
    case 7: {
        double x = gw_eval(b, env);
        return gw_num(a, 0) == 0 ? -x : !x;
    }
    case 8: return gw_eval(a, env) != 0 ? gw_eval(b, env) : gw_eval(c, env);
    case 9: {
        int o = (int)gw_num(a, 0);
        double x = gw_eval(b, env);
        if (o == 0) return x != 0 || gw_eval(c, env) != 0;
        if (o == 1) return x != 0 && gw_eval(c, env) != 0;
        double y = gw_eval(c, env);
        switch (o) {
        case 2: return x == y;
        case 3: return x != y;
        case 4: return x < y;
        case 5: return x > y;
        case 6: return x <= y;
        case 7: return x >= y;
        case 8: return x + y;
        case 9: return x - y;
        case 10: return x * y;
        case 11: return y == 0 ? 0 : x / y;
        }
    }
    }
    return 0;
}

static double gw_clamp(double v, double lo, double hi) { return v < lo ? lo : v > hi ? hi : v; }
static double gw_floor(double a)
{
    double t = (double)(long long)a;
    return t > a ? t - 1 : t;
}

static void gw_acts(const gw_j *list)
{
    if (!list || list->t != GJ_ARR) return;
    for (gw_j *a = list->kid; a; a = a->next) {
        int op = (int)gw_num(gw_at(a, 0), -1);
        gw_j *x = gw_at(a, 1), *y = gw_at(a, 2);
        gw_var *v = gw_var_at((int)gw_num(x, -1));
        switch (op) {
        case 0: {
            double e = gw_eval(y, NULL);
            if (gw_num(gw_at(a, 3), 0)) e = (int)e;
            gw_var_put(v, e);
            break;
        }
        case 1:
            if (v && v->s) snprintf(v->s, (size_t)v->cap, "%s", gw_text(y));
            break;
        case 2:
            if (v) v->i = !v->i;
            break;
        case 3: {
            double e = gw_var_num(v) + gw_num(gw_at(a, 3), 1) * gw_eval(y, NULL);
            gw_j *lo = gw_at(a, 4), *hi = gw_at(a, 5);
            if (lo && hi && lo->t == GJ_NUM && hi->t == GJ_NUM) e = gw_clamp(e, lo->v, hi->v);
            if (gw_num(gw_at(a, 6), 0)) e = (int)e;
            gw_var_put(v, e);
            break;
        }
        case 4:
            if (v) v->i = gw_num(y, 0) != 0;
            break;
        case 5: goome_ui_popup_open(gw_u, x && x->t == GJ_STR ? x->s : "popup"); break;
        case 6: goome_ui_popup_close(gw_u); break;
        case 7: goome_ui_toast(gw_u, gw_text(x), (int)gw_num(y, 2500)); break;
        case 8: goome_clipboard_set(gw_g, gw_text(x)); break;
        case 9: {
            goome_event e;
            memset(&e, 0, sizeof(e));
            e.type = GOOME_CLOSE;
            goome_push(gw_g, &e);
            gw_log("quit");
            break;
        }
        case 10: gw_log(gw_text(x)); break;
        }
    }
}

static const char **gw_list(gw_j *n, int *cnt)
{
    gw_var *lv = gw_var_at(gw_ki(n, "iv", -1));
    if (lv && lv->type == GV_LIST) {
        *cnt = lv->nitems;
        return lv->items;
    }
    gw_j *it = gw_key(n, "it");
    *cnt = 0;
    if (!it || it->t != GJ_ARR) return NULL;
    if (!it->aux) {
        const char **a = calloc((size_t)(it->n ? it->n : 1), sizeof(*a));
        if (!a) return NULL;
        int i = 0;
        for (gw_j *k = it->kid; k; k = k->next) a[i++] = k->t == GJ_STR ? k->s : "";
        it->aux = (void *)a;
    }
    *cnt = it->n;
    return (const char **)it->aux;
}

static int gw_styles(const gw_j *st)
{
    if (!st || st->t != GJ_ARR) return 0;
    int k = 0;
    for (gw_j *s = st->kid; s; s = s->next) {
        int kind = (int)gw_num(gw_at(s, 0), -1), i = (int)gw_num(gw_at(s, 1), 0);
        gw_j *v = gw_at(s, 2);
        if (kind == 0) goome_ui_push_color(gw_u, i, (uint32_t)(int64_t)gw_num(v, 0)), k++;
        else if (kind == 1) goome_ui_push_num(gw_u, i, gw_num(v, 0)), k++;
        else if (kind == 2) {
            goome_radii r = {gw_num(gw_at(v, 0), 0), gw_num(gw_at(v, 1), 0), gw_num(gw_at(v, 2), 0), gw_num(gw_at(v, 3), 0)};
            goome_ui_push_radii(gw_u, r), k++;
        } else if (kind == 3 && i >= 0 && i < gw_p.nthemes) goome_ui_push_theme(gw_u, &gw_p.themes[i]), k++;
    }
    return k;
}

static void gw_list_nodes(const gw_j *list, int ptype);
static void gw_inst(gw_j *n, int ptype);

enum { GP_OVERLAY, GP_APP, GP_IN, GP_ELEMENT };

typedef struct {
    gw_j *n, *on;
    int en;
} gw_ctx;

static gw_j *gw_ev(const gw_ctx *c, const char *e)
{
    gw_j *l = gw_key(c->on, e);
    return l && l->t == GJ_ARR && l->n ? l : NULL;
}

static int gw_change_evs(const gw_ctx *c) { return gw_ev(c, "change") || gw_ev(c, "on") || gw_ev(c, "off"); }

static void gw_fire(const gw_ctx *c, const gw_var *v)
{
    gw_acts(gw_ev(c, "change"));
    if (v && gw_ev(c, "on") && gw_var_num(v) != 0) gw_acts(gw_ev(c, "on"));
    if (v && gw_ev(c, "off") && gw_var_num(v) == 0) gw_acts(gw_ev(c, "off"));
}

static void gw_changed(const gw_ctx *c, int changed, const gw_var *v)
{
    if (changed && c->en && gw_change_evs(c)) gw_fire(c, v);
}

static void gw_hover(const gw_ctx *c)
{
    gw_j *l = gw_ev(c, "hover");
    if (!l) return;
    gw_var *hv = gw_var_at(gw_ki(c->n, "hv", -1));
    int h = goome_ui_hovered(gw_u);
    if (h && hv && !hv->i) gw_acts(l);
    if (hv) hv->i = h;
}

static void gw_slot(gw_j *n, int s, int ptype)
{
    gw_j *ss = gw_at(gw_key(n, "ss"), s);
    int k = gw_styles(ss);
    gw_list_nodes(gw_at(gw_key(n, "k"), s), ptype);
    if (k) goome_ui_pop(gw_u, k);
}

static void gw_bare(gw_j *n, int ptype, int en)
{
    int op = gw_ki(n, "t", -1);
    gw_ctx cx = {n, gw_key(n, "on"), en};
    gw_var *v = gw_var_at(gw_ki(n, "v", -1));
    gw_j *lab = gw_key(n, "l");
    if (ptype == GP_OVERLAY && op != 0 && op != 35) return;
    gw_j *wd = gw_key(n, "wd");
    if (wd) goome_ui_width(gw_u, gw_num(wd, 0));
    gw_j *ab = gw_key(n, "ab");
    if (ab) {
        goome_ui_pos(gw_u, gw_num(gw_at(ab, 0), 0), gw_num(gw_at(ab, 1), 0));
        double w = gw_num(gw_at(ab, 2), 0), h = gw_num(gw_at(ab, 3), 0);
        if (w > 0 || h > 0) goome_ui_size(gw_u, w > 0 ? w : 0, h > 0 ? h : 0);
    }
    int changed = 0, leaf = 0;
    switch (op) {
    case 0: {
        gw_var *x = gw_var_at(gw_ki(n, "vx", -1));
        if (!x || x + 3 >= gw_p.vars + gw_p.nvars) return;
        gw_var *o = gw_var_at(gw_ki(n, "o", -1));
        if (goome_ui_window(gw_u, gw_text(lab), &x[0].d, &x[1].d, &x[2].d, &x[3].d, gw_ki(n, "f", 0), o ? &o->i : NULL)) gw_list_nodes(gw_at(gw_key(n, "k"), 0), GP_IN);
        goome_ui_window_end(gw_u);
        return;
    }
    case 1: {
        int k = gw_ki(n, "c", 0);
        if (k >= 2) goome_ui_row(gw_u, k);
        gw_list_nodes(gw_at(gw_key(n, "k"), 0), GP_IN);
        return;
    }
    case 2: goome_ui_same_line(gw_u); return;
    case 3: goome_ui_space(gw_u, gw_kn(n, "px", 0)); return;
    case 4: goome_ui_separator(gw_u); return;
    case 5:
        goome_ui_scroll_begin(gw_u, gw_ks(n, "id"), gw_kn(n, "h", 0));
        gw_list_nodes(gw_at(gw_key(n, "k"), 0), GP_IN);
        goome_ui_scroll_end(gw_u);
        return;
    case 6:
        if (!v) return;
        goome_ui_split_begin(gw_u, gw_ks(n, "id"), gw_ki(n, "vert", 0), &v->d);
        gw_slot(n, 0, GP_IN);
        goome_ui_split_next(gw_u);
        gw_slot(n, 1, GP_IN);
        goome_ui_split_end(gw_u);
        return;
    case 7: {
        int cnt;
        const char **items = gw_list(n, &cnt);
        if (!v || !items) return;
        changed = goome_ui_tabs(gw_u, gw_ks(n, "id"), items, cnt, &v->i);
        gw_changed(&cx, changed, v);
        gw_j *k = gw_key(n, "k");
        if (v->i >= 0 && v->i < (k ? k->n : 0)) gw_slot(n, v->i, GP_IN);
        return;
    }
    case 8: {
        if (!v) return;
        int was = v->i, open = goome_ui_collapsing(gw_u, gw_text(lab), &v->i);
        if (gw_ev(&cx, "change") && en && v->i != was) gw_acts(gw_ev(&cx, "change"));
        if (open) gw_list_nodes(gw_at(gw_key(n, "k"), 0), GP_IN);
        return;
    }
    case 9: {
        if (!v) return;
        int was = v->i, open = goome_ui_tree(gw_u, gw_text(lab), &v->i);
        if (gw_ev(&cx, "change") && en && v->i != was) gw_acts(gw_ev(&cx, "change"));
        if (open) {
            gw_list_nodes(gw_at(gw_key(n, "k"), 0), GP_IN);
            goome_ui_tree_end(gw_u);
        }
        return;
    }
    case 10: {
        if (!v) return;
        gw_j *btn = gw_key(n, "btn");
        if (btn && goome_ui_button(gw_u, gw_text(btn))) v->i = 1;
        if (goome_ui_modal_begin(gw_u, gw_text(lab), &v->i)) {
            gw_list_nodes(gw_at(gw_key(n, "k"), 0), GP_IN);
            goome_ui_modal_end(gw_u);
        }
        return;
    }
    case 11: {
        const char *id = gw_ks(n, "id");
        if (!id) id = "popup";
        if (v && v->i) goome_ui_popup_open(gw_u, id), v->i = 0;
        if (goome_ui_popup_begin(gw_u, id)) {
            gw_list_nodes(gw_at(gw_key(n, "k"), 0), GP_IN);
            goome_ui_popup_end(gw_u);
        }
        return;
    }
    case 12:
        if (goome_ui_menubar_begin(gw_u)) {
            gw_list_nodes(gw_at(gw_key(n, "k"), 0), GP_IN);
            goome_ui_menubar_end(gw_u);
        }
        return;
    case 13:
        if (goome_ui_menu_begin(gw_u, gw_text(lab))) {
            gw_list_nodes(gw_at(gw_key(n, "k"), 0), GP_IN);
            goome_ui_menu_end(gw_u);
        }
        return;
    case 14: {
        const char *sc = gw_ks(n, "sc");
        int hit = goome_ui_menu_item(gw_u, gw_text(lab), sc, v ? &v->i : NULL);
        if (hit && en) {
            if (gw_ev(&cx, "click")) gw_acts(gw_ev(&cx, "click"));
            else if (!v) {
                char m[300];
                snprintf(m, sizeof(m), "%s clicked", gw_text(lab));
                gw_log(m);
            }
        }
        return;
    }
    case 15: goome_ui_menu_separator(gw_u); return;
    case 16: goome_ui_label(gw_u, "%s", gw_text(lab)); gw_hover(&cx); leaf = 1; break;
    case 17: goome_ui_heading(gw_u, gw_text(lab)); gw_hover(&cx); leaf = 1; break;
    case 18: goome_ui_wrapped(gw_u, gw_text(lab)); leaf = 1; break;
    case 19: {
        if (goome_ui_button(gw_u, gw_text(lab)) && en) {
            if (gw_ev(&cx, "click")) gw_acts(gw_ev(&cx, "click"));
            else {
                int act = gw_ki(n, "act", 0);
                if (act == 1) goome_ui_toast(gw_u, gw_text(gw_key(n, "toast")), gw_ki(n, "ms", 2500));
                else if (act == 2) goome_ui_popup_close(gw_u);
                else {
                    char m[300];
                    snprintf(m, sizeof(m), "%s clicked", gw_text(lab));
                    gw_log(m);
                }
            }
        }
        gw_hover(&cx);
        leaf = 1;
        break;
    }
    case 20:
    case 21:
        if (!v) return;
        changed = op == 20 ? goome_ui_checkbox(gw_u, gw_text(lab), &v->i) : goome_ui_toggle(gw_u, gw_text(lab), &v->i);
        gw_changed(&cx, changed, v);
        gw_hover(&cx);
        leaf = 1;
        break;
    case 22: {
        int cnt, any = 0;
        const char **items = gw_list(n, &cnt);
        if (!v || !items) return;
        for (int i = 0; i < cnt; i++) {
            if (i && gw_ki(n, "inline", 0)) goome_ui_same_line(gw_u);
            any |= goome_ui_radio(gw_u, items[i], &v->i, i);
        }
        gw_changed(&cx, any, v);
        leaf = 1;
        break;
    }
    case 23:
        if (!v) return;
        if (v->type == GV_INT) changed = goome_ui_slider_int(gw_u, gw_text(lab), &v->i, (int)gw_kn(n, "min", 0), (int)gw_kn(n, "max", 1));
        else changed = goome_ui_slider(gw_u, gw_text(lab), &v->d, gw_kn(n, "min", 0), gw_kn(n, "max", 1), gw_ks(n, "fmt"));
        gw_changed(&cx, changed, v);
        gw_hover(&cx);
        leaf = 1;
        break;
    case 24:
        if (!v) return;
        changed = goome_ui_slider_int(gw_u, gw_text(lab), &v->i, (int)gw_kn(n, "min", 0), (int)gw_kn(n, "max", 100));
        gw_changed(&cx, changed, v);
        gw_hover(&cx);
        leaf = 1;
        break;
    case 25: {
        if (!v || !v->s) return;
        int fl = gw_ki(n, "f", 0);
        const char *hint = gw_ks(n, "hint");
        int r = fl || hint ? goome_ui_input_ex(gw_u, gw_text(lab), v->s, v->cap, fl, hint) : goome_ui_input(gw_u, gw_text(lab), v->s, v->cap);
        if (gw_ev(&cx, "enter")) {
            gw_changed(&cx, r == 1, v);
            if (r == 2 && en) gw_acts(gw_ev(&cx, "enter"));
        } else gw_changed(&cx, (fl & GOOME_INPUT_ENTER_RETURNS) ? r == 1 : r, v);
        gw_hover(&cx);
        leaf = 1;
        break;
    }
    case 26:
        if (!v || !v->s) return;
        changed = goome_ui_text_edit(gw_u, gw_ks(n, "id"), v->s, v->cap, gw_kn(n, "h", 100), gw_ki(n, "f", 0));
        gw_changed(&cx, changed, v);
        leaf = 1;
        break;
    case 27: {
        int cnt;
        const char **items = gw_list(n, &cnt);
        if (!v || !items) return;
        changed = goome_ui_combo(gw_u, gw_text(lab), &v->i, items, cnt);
        gw_changed(&cx, changed, v);
        gw_hover(&cx);
        leaf = 1;
        break;
    }
    case 28: {
        int cnt;
        const char **items = gw_list(n, &cnt);
        if (!v || !items) return;
        for (int i = 0; i < cnt; i++)
            if (goome_ui_selectable(gw_u, items[i], v->i == i)) {
                v->i = i;
                if (gw_change_evs(&cx) && en) gw_fire(&cx, v);
            }
        leaf = 1;
        break;
    }
    case 29:
        if (!v) return;
        changed = goome_ui_color(gw_u, gw_text(lab), &v->c);
        gw_changed(&cx, changed, v);
        gw_hover(&cx);
        leaf = 1;
        break;
    case 30: {
        double val = gw_kn(n, "val", 0);
        gw_var *pv = gw_var_at(gw_ki(n, "pv", -1));
        if (pv) {
            double lo = gw_kn(n, "lo", 0), hi = gw_kn(n, "hi", 1);
            val = gw_var_num(pv);
            if (hi > lo && !(lo == 0 && hi == 1)) val = (val - lo) / (hi - lo);
        }
        goome_ui_progress(gw_u, val, lab ? gw_text(lab) : NULL);
        gw_hover(&cx);
        leaf = 1;
        break;
    }
    case 31: {
        gw_j *cols = gw_key(n, "cols"), *rows = gw_key(n, "rows");
        int k = cols ? cols->n : 0;
        if (k < 1) return;
        if (goome_ui_table_begin(gw_u, gw_ks(n, "id"), k, gw_kn(n, "h", 0), gw_ki(n, "f", 0))) {
            for (gw_j *c = cols->kid; c; c = c->next) {
                gw_j *nm = gw_at(c, 0);
                goome_ui_table_column(gw_u, nm && nm->t == GJ_STR ? nm->s : "", gw_num(gw_at(c, 1), 0));
            }
            for (gw_j *r = rows ? rows->kid : NULL; r; r = r->next) {
                goome_ui_table_row(gw_u);
                int j = 0;
                for (gw_j *c = r->kid; c && j < k; c = c->next, j++) {
                    if (j) goome_ui_table_next(gw_u);
                    goome_ui_label(gw_u, "%s", c->t == GJ_STR ? c->s : "");
                }
            }
        }
        goome_ui_table_end(gw_u);
        leaf = 1;
        break;
    }
    case 32:
    case 33: {
        gw_j *vals = gw_key(n, "vals");
        if (!vals || vals->t != GJ_ARR) return;
        if (!vals->aux) {
            float *f = calloc((size_t)(vals->n ? vals->n : 1), sizeof(float));
            if (!f) return;
            int i = 0;
            for (gw_j *x = vals->kid; x; x = x->next) f[i++] = (float)gw_num(x, 0);
            vals->aux = f;
        }
        int cnt = vals->n ? vals->n : 1;
        const char *pl = gw_ks(n, "l");
        if (op == 32) goome_ui_plot_lines(gw_u, pl ? pl : "", (const float *)vals->aux, cnt, gw_kn(n, "min", 0), gw_kn(n, "max", 0), gw_kn(n, "h", 90));
        else goome_ui_plot_bars(gw_u, pl ? pl : "", (const float *)vals->aux, cnt, gw_kn(n, "min", 0), gw_kn(n, "max", 0), gw_kn(n, "h", 90));
        leaf = 1;
        break;
    }
    case 34:
        goome_ui_image(gw_u, gw_ph_ok ? &gw_ph : NULL, gw_kn(n, "w", 120), gw_kn(n, "h", 80), gw_kn(n, "r", 0));
        gw_hover(&cx);
        leaf = 1;
        break;
    case 35: gw_inst(n, ptype); return;
    default: return;
    }
    const char *tp = gw_ks(n, "tp");
    if (leaf && tp && *tp) goome_ui_tooltip(gw_u, tp);
}

static void gw_node(gw_j *n, int ptype)
{
    gw_j *sh = gw_key(n, "sh");
    if (sh && gw_eval(sh, NULL) == 0) return;
    int k = gw_styles(gw_key(n, "st"));
    gw_j *ej = gw_key(n, "en");
    int en = !ej || gw_eval(ej, NULL) != 0;
    if (!en) {
        goome_theme *t = goome_ui_theme(gw_u);
        goome_ui_push_color(gw_u, GOOME_S_TEXT, t->text_dim);
        goome_ui_push_color(gw_u, GOOME_S_ACCENT, (t->accent & 0xFFFFFFu) | (t->accent >> 25) << 24);
    }
    gw_var *v = en ? NULL : gw_var_at(gw_ki(n, "v", -1)), saved;
    char *sbuf = NULL;
    memset(&saved, 0, sizeof(saved));
    if (v) {
        saved = *v;
        if (v->s && (sbuf = malloc((size_t)v->cap))) memcpy(sbuf, v->s, (size_t)v->cap);
    }
    gw_bare(n, ptype, en);
    if (v) {
        v->d = saved.d, v->i = saved.i, v->c = saved.c;
        if (sbuf) memcpy(v->s, sbuf, (size_t)v->cap);
        free(sbuf);
    }
    if (!en) goome_ui_pop(gw_u, 2);
    if (k) goome_ui_pop(gw_u, k);
}

static void gw_list_nodes(const gw_j *list, int ptype)
{
    if (!list || list->t != GJ_ARR) return;
    for (gw_j *n = list->kid; n; n = n->next) gw_node(n, ptype);
}

static void gw_bases(gw_j *parts, double bx, double by, const gw_env *env)
{
    for (gw_j *pt = parts ? parts->kid : NULL; pt; pt = pt->next) {
        if (gw_ki(pt, "k", -1) != 13) continue;
        int gb = gw_ki(pt, "gb", -1);
        gw_j *f = gw_key(pt, "f");
        double sx = bx + gw_eval(gw_key(f, "x"), env), sy = by + gw_eval(gw_key(f, "y"), env);
        int d = gw_ki(pt, "dx", -1);
        double ax = d >= 0 ? sx + env->s[d] : sx, ay = d >= 0 ? sy + env->s[d + 1] : sy;
        if (gb >= 0 && gb < 64) gw_gsx[gb] = sx, gw_gsy[gb] = sy, gw_gax[gb] = ax, gw_gay[gb] = ay;
        gw_bases(gw_key(pt, "kids"), ax, ay, env);
    }
}

static gw_j *gw_find_part(gw_j *parts, int id)
{
    for (gw_j *pt = parts ? parts->kid : NULL; pt; pt = pt->next) {
        if (gw_ki(pt, "id", -1) == id) return pt;
        gw_j *r = gw_find_part(gw_key(pt, "kids"), id);
        if (r) return r;
    }
    return NULL;
}

static int gw_bbox(gw_j *pt, double bx, double by, const gw_env *env, double *r)
{
    gw_j *f = gw_key(pt, "f");
#define GW_F(k) gw_eval(gw_key(f, k), env)
    switch (gw_ki(pt, "k", -1)) {
    case 0: case 1: case 2: case 8: case 9: case 11: case 12: case 13:
        r[0] = bx + GW_F("x"), r[1] = by + GW_F("y"), r[2] = GW_F("w"), r[3] = GW_F("h");
        return 1;
    case 4: case 5: case 6: {
        double rr = GW_F("r");
        r[0] = bx + (GW_F("cx") - rr), r[1] = by + (GW_F("cy") - rr), r[2] = 2 * rr, r[3] = 2 * rr;
        return 1;
    }
    case 3: {
        double x0 = GW_F("x0"), x1 = GW_F("x1"), y0 = GW_F("y0"), y1 = GW_F("y1");
        double dx = x1 - x0, dy = y1 - y0;
        dx = dx < 0 ? -dx : dx, dy = dy < 0 ? -dy : dy;
        r[0] = bx + (x0 < x1 ? x0 : x1), r[1] = by + (y0 < y1 ? y0 : y1), r[2] = dx > 1 ? dx : 1, r[3] = dy > 1 ? dy : 1;
        return 1;
    }
    case 10: {
        double len = 0, sz = GW_F("size");
        gw_j *tp = gw_key(pt, "tp");
        if (tp) {
            gw_j *p = gw_at(env->params, (int)gw_num(tp, 0));
            len = p && p->t == GJ_STR ? (double)strlen(p->s) : 0;
        } else {
            const char *s = gw_ks(pt, "tx");
            for (; s && *s; s++) len += ((unsigned char)*s & 0xC0) != 0x80;
        }
        r[0] = bx + GW_F("x"), r[1] = by + GW_F("y"), r[2] = len * sz * 0.55, r[3] = sz * 1.25;
        return 1;
    }
    }
#undef GW_F
    return 0;
}

static void gw_bounds(gw_j *g, gw_j *def, const gw_env *env, double *bd, int *has)
{
    int gb = gw_ki(g, "gb", 0), mode = gw_ki(g, "bd", 0);
    gw_j *f = gw_key(gw_find_part(gw_key(def, "parts"), gw_ki(g, "pid", -1)), "f");
    double sx = gw_gsx[gb], sy = gw_gsy[gb], gw = gw_eval(gw_key(f, "w"), env), gh = gw_eval(gw_key(f, "h"), env);
    *has = 0;
    if (mode == 1) {
        bd[0] = -sx, bd[1] = (env->s[1] - gw) - sx, bd[2] = -sy, bd[3] = (env->s[2] - gh) - sy, *has = 1;
    } else if (mode == 2) {
        gw_j *rc = gw_key(g, "rc");
        double x = gw_num(gw_at(rc, 0), 0), y = gw_num(gw_at(rc, 1), 0), w = gw_num(gw_at(rc, 2), 0), h = gw_num(gw_at(rc, 3), 0);
        bd[0] = x - sx, bd[1] = ((x + w) - gw) - sx, bd[2] = y - sy, bd[3] = ((y + h) - gh) - sy, *has = 1;
    }
}

typedef struct {
    int hovered, pressed, dragging, pressed_now, clicked, released, dragend, changed;
} gw_fl;

static void gw_acc(gw_fl *f, const goome_ui_item *it, int click)
{
    f->hovered |= it->hovered || it->held;
    f->pressed |= it->held;
    f->pressed_now |= it->pressed;
    if (click) f->clicked |= it->clicked;
    f->released |= it->released;
    if (!click) f->dragend |= it->released;
}

static void gw_hits(gw_j *parts, double bx, double by, const gw_env *env, double ox, double oy, int fo, int *k, gw_fl *fl)
{
    for (gw_j *pt = parts ? parts->kid : NULL; pt; pt = pt->next) {
        int kind = gw_ki(pt, "k", -1);
        double r[4];
        int one = 0;
        if (kind == 13) one = gw_ki(pt, "hit", 0) && gw_ki(pt, "dx", -1) < 0;
        else one = kind == 12 || gw_ki(pt, "hit", 0);
        if (one && gw_bbox(pt, bx, by, env, r)) {
            char id[24];
            goome_ui_item it;
            snprintf(id, sizeof(id), "##hit%d", (*k)++);
            goome_ui_pos(gw_u, ox + r[0], oy + r[1]);
            goome_ui_custom(gw_u, id, r[2], r[3], fo ? GOOME_ITEM_FOCUSABLE : 0, &it);
            gw_acc(fl, &it, 1);
        }
        if (kind == 13) {
            int gb = gw_ki(pt, "gb", 0);
            gw_hits(gw_key(pt, "kids"), gw_gax[gb], gw_gay[gb], env, ox, oy, fo, k, fl);
        }
    }
}

static uint32_t gw_col(const gw_j *e, const gw_env *env) { return (uint32_t)(int64_t)gw_eval(e, env); }

static void gw_draw(goome_canvas *c, gw_j *parts, double bx, double by, const gw_env *env, double elx, double ely)
{
    for (gw_j *pt = parts ? parts->kid : NULL; pt; pt = pt->next) {
        int kind = gw_ki(pt, "k", -1);
        if (kind == 14 || kind == 12) continue;
        gw_j *sh = gw_key(pt, "sh");
        if (sh && gw_eval(sh, env) == 0) continue;
        gw_j *f = gw_key(pt, "f");
#define GW_N(k) gw_eval(gw_key(f, k), env)
#define GW_C(k) gw_col(gw_key(f, k), env)
        switch (kind) {
        case 0: goome_draw_rect(c, bx + GW_N("x"), by + GW_N("y"), GW_N("w"), GW_N("h"), GW_C("color")); break;
        case 1: {
            goome_radii r = {GW_N("tl"), GW_N("tr"), GW_N("br"), GW_N("bl")};
            goome_draw_rrect(c, bx + GW_N("x"), by + GW_N("y"), GW_N("w"), GW_N("h"), r, GW_C("color"));
            break;
        }
        case 2: goome_draw_frame(c, bx + GW_N("x"), by + GW_N("y"), GW_N("w"), GW_N("h"), GW_N("width"), GW_C("color")); break;
        case 3: goome_draw_line(c, bx + GW_N("x0"), by + GW_N("y0"), bx + GW_N("x1"), by + GW_N("y1"), GW_N("width"), GW_C("color")); break;
        case 4: goome_draw_circle(c, bx + GW_N("cx"), by + GW_N("cy"), GW_N("r"), GW_C("color")); break;
        case 5: goome_draw_ring(c, bx + GW_N("cx"), by + GW_N("cy"), GW_N("r"), GW_N("width"), GW_C("color")); break;
        case 6: goome_draw_arc(c, bx + GW_N("cx"), by + GW_N("cy"), GW_N("r"), GW_N("width"), GW_N("a0"), GW_N("a1"), GW_C("color")); break;
        case 7: {
            gw_j *pts = gw_key(pt, "pts");
            double xy[128];
            int n = 0;
            for (gw_j *q = pts ? pts->kid : NULL; q && n < 64; q = q->next, n++) xy[n * 2] = bx + gw_eval(gw_at(q, 0), env), xy[n * 2 + 1] = by + gw_eval(gw_at(q, 1), env);
            if (n >= 3) goome_draw_poly(c, xy, n, GW_C("color"));
            break;
        }
        case 8: goome_draw_rrect_grad(c, bx + GW_N("x"), by + GW_N("y"), GW_N("w"), GW_N("h"), goome_radius(GW_N("radius")), GW_C("from"), GW_C("to"), gw_ki(pt, "side", 0)); break;
        case 9: goome_draw_shadow(c, bx + GW_N("x"), by + GW_N("y"), GW_N("w"), GW_N("h"), GW_N("radius"), GW_N("blur"), GW_C("color")); break;
        case 10: {
            const char *s = gw_ks(pt, "tx");
            gw_j *tp = gw_key(pt, "tp");
            if (tp) {
                gw_j *p = gw_at(env->params, (int)gw_num(tp, 0));
                s = p && p->t == GJ_STR ? p->s : "";
            }
            goome_text(c, NULL, GW_N("size"), bx + GW_N("x"), by + GW_N("y"), s ? s : "", GW_C("color"));
            break;
        }
        case 11: {
            int b = gw_ki(pt, "bm", -1);
            gw_j *tint = gw_key(f, "tint");
            if (b >= 0 && b < gw_p.nbms && gw_p.bm_ok[b])
                goome_draw_bitmap(c, &gw_p.bms[b], bx + GW_N("x"), by + GW_N("y"), GW_N("w"), GW_N("h"), goome_radius(GW_N("radius")), tint ? GW_C("tint") : 0xFFFFFFFFu);
            break;
        }
        case 13: {
            int gb = gw_ki(pt, "gb", 0);
            double ax = elx + gw_gax[gb], ay = ely + gw_gay[gb];
            int clip = gw_ki(pt, "clip", 0);
            if (clip) goome_clip_push(c, ax, ay, GW_N("w"), GW_N("h"));
            gw_draw(c, gw_key(pt, "kids"), ax, ay, env, elx, ely);
            if (clip) goome_clip_pop(c);
            break;
        }
        }
#undef GW_N
#undef GW_C
    }
}

static void gw_el_acts(gw_j *def, int on, double *s, const gw_env *env, gw_fl *fl)
{
    for (gw_j *e = gw_key(def, "ev") ? gw_key(def, "ev")->kid : NULL; e; e = e->next) {
        if ((int)gw_num(gw_at(e, 0), -1) != on) continue;
        int act = (int)gw_num(gw_at(e, 1), 0), t = (int)gw_num(gw_at(e, 2), -1);
        if (act == 0) {
            gw_j *tx = gw_at(e, 5);
            gw_log(tx && tx->t == GJ_STR ? tx->s : "");
            continue;
        }
        if (t < 0) continue;
        if (act == 1) s[t] = !s[t];
        else if (act == 2) s[t] += gw_eval(gw_at(e, 3), env);
        else if (act == 3) s[t] = gw_num(gw_at(e, 4), 0) ? gw_eval(gw_at(e, 3), env) != 0 : gw_eval(gw_at(e, 3), env);
        fl->changed = 1;
    }
}

static int gw_element(gw_j *def, const char *id, gw_j *params, gw_j *init)
{
    int N = gw_ki(def, "n", 5);
    double *s = goome_ui_store(gw_u, id, N);
    if (!s) return 0;
    gw_env env = {s, params, {0, 0, 0}};
    gw_j *parts = gw_key(def, "parts"), *groups = gw_key(def, "g"), *val = gw_key(def, "val");
    gw_j *ws = gw_key(def, "ws");
    if (!s[0]) {
        s[0] = 1;
        s[1] = init ? gw_num(gw_at(init, 0), 0) : gw_kn(def, "w", 0);
        s[2] = init ? gw_num(gw_at(init, 1), 0) : gw_kn(def, "h", 0);
        int i = 0;
        for (gw_j *st = gw_key(def, "st") ? gw_key(def, "st")->kid : NULL; st; st = st->next, i++) s[(int)gw_num(gw_at(st, 0), 0)] = init ? gw_num(gw_at(init, i + 2), 0) : gw_num(gw_at(st, 1), 0);
        for (gw_j *w = ws ? ws->kid : NULL; w; w = w->next) s[(int)gw_num(gw_at(w, 0), 0)] = gw_num(gw_at(w, 2), 0);
        if (val && gw_ki(val, "src", 0) == 1) {
            int vs = gw_ki(val, "vs", -1);
            double mn = gw_kn(val, "min", 0), mx = gw_kn(val, "max", 0), span = mx - mn;
            s[3] = vs >= 0 ? s[vs] : mn;
            gw_j *g = gw_at(groups, gw_ki(val, "g", -1));
            if (g) {
                gw_bases(parts, 0, 0, &env);
                double bd[4];
                int has;
                gw_bounds(g, def, &env, bd, &has);
                if (has) {
                    int ay = gw_ki(val, "ax", 0) == 1;
                    double t = span ? gw_clamp((s[3] - mn) / span, 0, 1) : 0;
                    s[gw_ki(g, "dx", 0) + ay] = bd[ay * 2] + t * (bd[ay * 2 + 1] - bd[ay * 2]);
                }
            }
        }
    }
    gw_fl fl;
    memset(&fl, 0, sizeof(fl));
    goome_ui_push_id(gw_u, id);
    goome_ui_item el, o, it;
    goome_ui_custom(gw_u, "##el", s[1], s[2], GOOME_ITEM_NO_HIT, &el);
    goome_ui_pos(gw_u, 0, 0);
    goome_ui_custom(gw_u, "##o", 1, 1, GOOME_ITEM_NO_HIT, &o);
    double ox = el.x - o.x, oy = el.y - o.y;
    gw_j *rs = gw_key(def, "rs");
    if (rs) {
        goome_ui_pos(gw_u, ox + (s[1] - 14), oy + (s[2] - 14));
        goome_ui_custom(gw_u, "##rs", 14, 14, GOOME_ITEM_DRAG_X | GOOME_ITEM_DRAG_Y, &it);
        int rw = gw_ki(def, "rw", N - 2);
        if (it.pressed) s[rw] = s[1], s[rw + 1] = s[2];
        if (it.held) {
            s[1] = gw_clamp(s[rw] + it.drag_x, gw_num(gw_at(rs, 0), 0), gw_num(gw_at(rs, 2), 0));
            s[2] = gw_clamp(s[rw + 1] + it.drag_y, gw_num(gw_at(rs, 1), 0), gw_num(gw_at(rs, 3), 0));
            fl.changed = 1, fl.dragging = 1;
        }
        gw_acc(&fl, &it, 0);
    }
    for (int gi = (groups ? groups->n : 0) - 1; gi >= 0; gi--) {
        gw_j *g = gw_at(groups, gi);
        env.fl[0] = fl.hovered, env.fl[1] = fl.pressed, env.fl[2] = fl.dragging;
        gw_bases(parts, 0, 0, &env);
        int gb = gw_ki(g, "gb", 0), dx = gw_ki(g, "dx", 0), axis = gw_ki(g, "ax", 0);
        gw_j *gp = gw_find_part(parts, gw_ki(g, "pid", -1)), *gf = gw_key(gp, "f");
        double r[4] = {gw_gax[gb], gw_gay[gb], gw_eval(gw_key(gf, "w"), &env), gw_eval(gw_key(gf, "h"), &env)};
        gw_j *hd = gw_key(g, "hd");
        if (hd) {
            gw_j *hp = gw_find_part(gw_key(gp, "kids"), (int)gw_num(hd, -1));
            if (hp) gw_bbox(hp, gw_gax[gb], gw_gay[gb], &env, r);
        }
        goome_ui_pos(gw_u, ox + r[0], oy + r[1]);
        int ifl = axis == 1 ? GOOME_ITEM_DRAG_X : axis == 2 ? GOOME_ITEM_DRAG_Y : GOOME_ITEM_DRAG_X | GOOME_ITEM_DRAG_Y;
        goome_ui_custom(gw_u, gw_ks(g, "id"), r[2], r[3], ifl, &it);
        if (it.pressed) s[dx + 2] = s[dx], s[dx + 3] = s[dx + 1];
        if (it.held) {
            double bd[4], nv[2] = {s[dx], s[dx + 1]}, snap = gw_kn(g, "snap", 0);
            int has, changed = 0;
            gw_bounds(g, def, &env, bd, &has);
            for (int a = 0; a < 2; a++) {
                if ((axis == 1 && a == 1) || (axis == 2 && a == 0)) continue;
                double n = s[dx + 2 + a] + (a ? it.drag_y : it.drag_x);
                if (snap > 0) n = gw_floor(n / snap + 0.5) * snap;
                if (has) n = gw_clamp(n, bd[a * 2], bd[a * 2 + 1]);
                nv[a] = n;
                if (n != s[dx + a]) changed = 1;
            }
            if (changed) {
                s[dx] = nv[0], s[dx + 1] = nv[1];
                fl.changed = 1;
                if (val && gw_ki(val, "src", 0) == 1 && gw_ki(val, "g", -1) == gi && has) {
                    int a = gw_ki(val, "ax", 0) == 1;
                    double lo = bd[a * 2], hi = bd[a * 2 + 1], mn = gw_kn(val, "min", 0), span = gw_kn(val, "max", 0) - mn;
                    double t = hi > lo ? (s[dx + a] - lo) / (hi - lo) : 0;
                    s[3] = mn + (span == 1 ? t : t * span);
                    int vs = gw_ki(val, "vs", -1);
                    if (vs >= 0) s[vs] = s[3];
                }
            }
            fl.dragging = 1;
        }
        gw_acc(&fl, &it, 0);
    }
    int fo = gw_ki(def, "fo", 0);
    env.fl[0] = fl.hovered, env.fl[1] = fl.pressed, env.fl[2] = fl.dragging;
    gw_bases(parts, 0, 0, &env);
    if (gw_ki(def, "hit", 0)) {
        int k = 0;
        gw_hits(parts, 0, 0, &env, ox, oy, fo, &k, &fl);
    } else {
        goome_ui_pos(gw_u, ox, oy);
        goome_ui_custom(gw_u, "##box", s[1], s[2], gw_ki(def, "deco", 0) ? GOOME_ITEM_NO_HIT : fo ? GOOME_ITEM_FOCUSABLE : 0, &it);
        gw_acc(&fl, &it, 1);
    }
    env.fl[0] = fl.hovered, env.fl[1] = fl.pressed, env.fl[2] = fl.dragging;
    if (gw_ki(def, "el", 0)) {
        if (fl.hovered && !s[4]) gw_el_acts(def, 0, s, &env, &fl);
        if (!fl.hovered && s[4]) gw_el_acts(def, 1, s, &env, &fl);
        s[4] = fl.hovered;
    }
    if (fl.pressed_now) gw_el_acts(def, 2, s, &env, &fl);
    if (fl.dragging) gw_el_acts(def, 3, s, &env, &fl);
    if (fl.clicked) gw_el_acts(def, 4, s, &env, &fl);
    if (fl.released) gw_el_acts(def, 5, s, &env, &fl);
    if (val && gw_ki(val, "src", 0) == 2 && gw_ki(val, "vs", -1) >= 0) s[3] = s[gw_ki(val, "vs", -1)];
    goome_canvas *c = goome_ui_canvas(gw_u);
    if (c) {
        gw_bases(parts, 0, 0, &env);
        gw_draw(c, parts, el.x, el.y, &env, el.x, el.y);
    }
    gw_j *wp = gw_key(def, "wp");
    if (wp && wp->n) {
        for (gw_j *w = ws ? ws->kid : NULL; w; w = w->next) gw_var_put(gw_var_at((int)gw_num(gw_at(w, 1), -1)), s[(int)gw_num(gw_at(w, 0), 0)]);
        gw_list_nodes(wp, GP_ELEMENT);
        for (gw_j *w = ws ? ws->kid : NULL; w; w = w->next) s[(int)gw_num(gw_at(w, 0), 0)] = gw_var_num(gw_var_at((int)gw_num(gw_at(w, 1), -1)));
    }
    goome_ui_pop_id(gw_u);
    return fl.changed | fl.clicked << 1 | fl.dragend << 2 | fl.hovered << 3;
}

static void gw_inst(gw_j *n, int ptype)
{
    int d = gw_ki(n, "d", -1);
    if (d < 0 || d >= gw_p.ndefs || ptype == GP_ELEMENT) return;
    if (ptype == GP_OVERLAY) goome_ui_pos(gw_u, gw_kn(n, "x", 0), gw_kn(n, "y", 0));
    const char *id = gw_ks(n, "id");
    gw_j *init = gw_key(n, "init");
    int m = gw_element(gw_p.defs[d], id ? id : "el", gw_key(n, "args"), init && init->t == GJ_ARR ? init : NULL);
    gw_ctx cx = {n, gw_key(n, "on"), 1};
    if (m & 1) gw_acts(gw_ev(&cx, "change"));
    if (m & 2) gw_acts(gw_ev(&cx, "click"));
    if (m & 4) gw_acts(gw_ev(&cx, "dragend"));
    gw_j *hl = gw_ev(&cx, "hover");
    if (hl) {
        gw_var *hv = gw_var_at(gw_ki(n, "hv", -1));
        if ((m & 8) && hv && !hv->i) gw_acts(hl);
        if (hv) hv->i = (m & 8) != 0;
    }
}

static int gw_b64(const char *s, uint8_t *out)
{
    int n = 0, bits = 0;
    uint32_t acc = 0;
    for (; *s; s++) {
        int v = *s >= 'A' && *s <= 'Z' ? *s - 'A' : *s >= 'a' && *s <= 'z' ? *s - 'a' + 26 : *s >= '0' && *s <= '9' ? *s - '0' + 52 : *s == '+' ? 62 : *s == '/' ? 63 : -1;
        if (v < 0) continue;
        acc = acc << 6 | (uint32_t)v, bits += 6;
        if (bits >= 8) bits -= 8, out[n++] = (uint8_t)(acc >> bits);
    }
    return n;
}

static void gw_theme_fill(goome_theme *t, const gw_j *vals)
{
    int i = 0;
    for (gw_j *v = vals ? vals->kid : NULL; v && i < GOOME_S_COUNT; v = v->next, i++) gw_th_set(t, i, gw_num(v, 0));
}

static const goome_bitmap *gw_bm(int i) { return i >= 0 && i < gw_p.nbms && gw_p.bm_ok[i] ? &gw_p.bms[i] : NULL; }

static goome_skin *gw_skin_of(goome_theme *t, int kind)
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

static void gw_free_prog(gw_prog *p)
{
    for (int i = 0; i < p->used; i++) free(p->pool[i].aux);
    for (int i = 0; i < p->nvars; i++) free(p->vars[i].s), free((void *)p->vars[i].items);
    for (int i = 0; i < p->nbms; i++)
        if (p->bm_ok[i]) goome_bitmap_free(&p->bms[i]);
    free(p->pool), free(p->src), free(p->vars), free(p->themes), free(p->defs), free(p->bms), free(p->bm_ok);
    memset(p, 0, sizeof(*p));
}

static void gw_var_init(gw_var *v, gw_j *d)
{
    gw_j *def = gw_at(d, 1);
    v->type = (int)gw_num(gw_at(d, 0), 0);
    gw_j *key = gw_at(d, 3);
    v->key = key && key->t == GJ_STR ? key->s : NULL;
    switch (v->type) {
    case GV_STR: {
        int cap = (int)gw_num(gw_at(d, 2), 256);
        if (cap < 2) cap = 2;
        v->cap = cap;
        v->s = calloc((size_t)cap, 1);
        if (v->s && def && def->t == GJ_STR) snprintf(v->s, (size_t)cap, "%s", def->s);
        break;
    }
    case GV_LIST: {
        int n = def && def->t == GJ_ARR ? def->n : 0;
        v->items = calloc((size_t)(n ? n : 1), sizeof(*v->items));
        if (!v->items) break;
        int i = 0;
        for (gw_j *k = n ? def->kid : NULL; k; k = k->next) v->items[i++] = k->t == GJ_STR ? k->s : "";
        if (!n) v->items[0] = "", n = 1;
        v->nitems = n;
        break;
    }
    default: gw_var_put(v, gw_num(def, 0));
    }
}

static int gw_same_def(const gw_var *a, const gw_var *b, gw_j *ad, gw_j *bd)
{
    gw_j *x = gw_at(ad, 1), *y = gw_at(bd, 1);
    if (!x || !y || x->t != y->t) return 0;
    if (x->t == GJ_STR) return !strcmp(x->s, y->s);
    if (x->t == GJ_NUM || x->t == GJ_BOOL) return x->v == y->v;
    return 0;
}

static void gw_apply_cfg(gw_j *cfg)
{
    if (!cfg) return;
    goome_ui_config *c = goome_ui_cfg(gw_u);
    gw_j *nums = gw_key(cfg, "n"), *tg = gw_key(cfg, "t"), *cur = gw_key(cfg, "cur"), *keys = gw_key(cfg, "k");
    int *ints[] = {&c->tip_delay_ms, &c->dbl_click_ms, &c->dbl_click_px, &c->drag_px, &c->repeat_delay_ms, &c->repeat_ms, &c->blink_ms, &c->menu_delay_ms, &c->toast_ms};
    for (int i = 0; i < 9 && nums && i < nums->n; i++) *ints[i] = (int)gw_num(gw_at(nums, i), *ints[i]);
    if (nums && nums->n > 9) c->scroll_lines = gw_num(gw_at(nums, 9), c->scroll_lines);
    int *tgs[] = {&c->raise_on_click, &c->esc_closes, &c->tab_nav, &c->focus_on_mouse};
    for (int i = 0; i < 4 && tg && i < tg->n; i++) *tgs[i] = (int)gw_num(gw_at(tg, i), *tgs[i]);
    for (int i = 0; i < GOOME_CUR_COUNT && cur && i < cur->n; i++) c->cursor[i] = (int)gw_num(gw_at(cur, i), c->cursor[i]);
    for (gw_j *k = keys ? keys->kid : NULL; k; k = k->next) {
        gw_j *cb = gw_at(k, 1);
        goome_ui_bind(gw_u, (int)gw_num(gw_at(k, 0), -1), cb && cb->t == GJ_STR ? cb->s : "");
    }
    const char *m = gw_ks(cfg, "m");
    if (m) c->mask = m;
}

int gw_init(int w, int h, double scale)
{
    if (gw_g) return 0;
    int err;
    goome_opts o;
    memset(&o, 0, sizeof(o));
    o.mode = GOOME_MODE_OFFSCREEN, o.backend = GOOME_OFFSCREEN, o.width = w, o.height = h, o.scale = scale;
    gw_g = goome_open(&o, &err);
    if (!gw_g) return err ? err : -1;
    uint32_t ph[4] = {0xFF3A3F48, 0xFF3A3F48, 0xFF3A3F48, 0xFF3A3F48};
    gw_ph_ok = goome_bitmap_from_argb(ph, 2, 2, &gw_ph) == GOOME_OK;
    return 0;
}

int gw_resize(int w, int h, double scale) { return gw_g ? goome_resize(gw_g, w, h, scale) : -1; }

int gw_font_mem(const void *data, int n)
{
    goome_font *f = n > 0 ? goome_font_load_mem(data, (size_t)n, 1) : NULL;
    if (n > 0 && !f) return -1;
    if (gw_u) goome_ui_theme(gw_u)->font = f;
    for (int i = 0; i < gw_p.nthemes; i++) gw_p.themes[i].font = f;
    goome_font_free(gw_font);
    gw_font = f;
    return 0;
}

const char *gw_error(void) { return gw_err; }

int gw_load(const char *json, int len)
{
    if (!gw_g) return -1;
    gw_prog np;
    memset(&np, 0, sizeof(np));
    np.src = malloc((size_t)len + 1);
    int est = 64;
    for (int i = 0; i < len; i++) est += json[i] == ',' || json[i] == '[' || json[i] == '{';
    np.pool = calloc((size_t)est, sizeof(gw_j));
    if (!np.src || !np.pool) {
        free(np.src), free(np.pool);
        snprintf(gw_err, sizeof(gw_err), "no memory");
        return -2;
    }
    memcpy(np.src, json, (size_t)len);
    np.src[len] = 0;
    np.npool = est;
    gw_prog old = gw_p;
    gw_p = np;
    char *p = gw_p.src;
    gw_j *root = gw_parse(&p, 0);
    if (!root || root->t != GJ_OBJ) {
        snprintf(gw_err, sizeof(gw_err), "bad json near byte %d", (int)(p - gw_p.src));
        np = gw_p;
        gw_p = old;
        gw_free_prog(&np);
        return -3;
    }
    gw_p.root = root;
    gw_j *vars = gw_key(root, "v");
    gw_p.nvars = vars ? vars->n : 0;
    gw_p.vars = calloc((size_t)(gw_p.nvars ? gw_p.nvars : 1), sizeof(gw_var));
    gw_j *ovars = gw_key(old.root, "v");
    int i = 0;
    for (gw_j *d = vars ? vars->kid : NULL; d; d = d->next, i++) {
        gw_var *v = &gw_p.vars[i];
        gw_var_init(v, d);
        if (!v->key) continue;
        int k = 0;
        for (gw_j *od = ovars ? ovars->kid : NULL; od; od = od->next, k++) {
            gw_var *ov = &old.vars[k];
            if (!ov->key || strcmp(ov->key, v->key) || ov->type != v->type || !gw_same_def(ov, v, od, d)) continue;
            if (v->type == GV_STR && v->s && ov->s) snprintf(v->s, (size_t)v->cap, "%s", ov->s);
            else if (v->type != GV_LIST) v->d = ov->d, v->i = ov->i, v->c = ov->c;
            break;
        }
    }
    gw_j *bms = gw_key(root, "bm");
    gw_p.nbms = bms ? bms->n : 0;
    gw_p.bms = calloc((size_t)(gw_p.nbms ? gw_p.nbms : 1), sizeof(goome_bitmap));
    gw_p.bm_ok = calloc((size_t)(gw_p.nbms ? gw_p.nbms : 1), sizeof(int));
    i = 0;
    for (gw_j *b = bms ? bms->kid : NULL; b; b = b->next, i++) {
        if (b->t != GJ_STR) continue;
        size_t n = strlen(b->s);
        uint8_t *raw = malloc(n * 3 / 4 + 4);
        if (!raw) continue;
        int k = gw_b64(b->s, raw);
        gw_p.bm_ok[i] = goome_bitmap_load_mem(raw, (size_t)k, &gw_p.bms[i]) == GOOME_OK;
        free(raw);
    }
    if (gw_u) goome_ui_free(gw_u);
    gw_u = goome_ui_new(gw_g);
    if (!gw_u) {
        snprintf(gw_err, sizeof(gw_err), "no memory for the ui");
        gw_free_prog(&old);
        return -4;
    }
    goome_theme *t = goome_ui_theme(gw_u);
    t->font = gw_font;
    gw_theme_fill(t, gw_key(root, "th"));
    for (gw_j *s = gw_key(root, "sk") ? gw_key(root, "sk")->kid : NULL; s; s = s->next) {
        goome_skin *sk = gw_skin_of(t, (int)gw_num(gw_at(s, 0), -1));
        const goome_bitmap *b = gw_bm((int)gw_num(gw_at(s, 1), -1));
        if (sk && b) sk->bm = b, sk->l = gw_num(gw_at(s, 2), 0), sk->t = gw_num(gw_at(s, 3), 0), sk->r = gw_num(gw_at(s, 4), 0), sk->b = gw_num(gw_at(s, 5), 0);
    }
    for (gw_j *s = gw_key(root, "ic") ? gw_key(root, "ic")->kid : NULL; s; s = s->next) {
        int k = (int)gw_num(gw_at(s, 0), -1);
        const goome_bitmap *b = gw_bm((int)gw_num(gw_at(s, 1), -1));
        if (k >= 0 && k < GOOME_ICON_COUNT && b) t->icon[k] = b;
    }
    if (gw_ki(root, "it", 0)) t->icon_tint = 1;
    gw_j *ths = gw_key(root, "ths");
    gw_p.nthemes = ths ? ths->n : 0;
    gw_p.themes = calloc((size_t)(gw_p.nthemes ? gw_p.nthemes : 1), sizeof(goome_theme));
    i = 0;
    for (gw_j *x = ths ? ths->kid : NULL; x; x = x->next, i++) {
        gw_p.themes[i] = *t;
        gw_theme_fill(&gw_p.themes[i], x);
    }
    gw_apply_cfg(gw_key(root, "cfg"));
    gw_j *defs = gw_key(root, "el");
    gw_p.ndefs = defs ? defs->n : 0;
    gw_p.defs = calloc((size_t)(gw_p.ndefs ? gw_p.ndefs : 1), sizeof(gw_j *));
    i = 0;
    for (gw_j *x = defs ? defs->kid : NULL; x; x = x->next) gw_p.defs[i++] = x;
    gw_j *app = gw_key(root, "app");
    gw_p.app = app && app->t == GJ_OBJ;
    gw_p.app_title = gw_key(app, "title");
    gw_p.app_status = gw_key(app, "st");
    gw_p.app_flags = gw_ki(app, "f", 0);
    gw_p.nodes = gw_key(root, "r");
    gw_p.hotkeys = gw_key(root, "hk");
    for (gw_j *h = gw_p.hotkeys ? gw_p.hotkeys->kid : NULL; h; h = h->next) {
        gw_j *c = gw_at(h, 0);
        int key = 0, mods = 0;
        if (c && c->t == GJ_STR && goome_parse_combo(c->s, &key, &mods) == GOOME_OK) c->v = key, c->n = mods;
        else if (c) c->v = -1;
    }
    gw_free_prog(&old);
    gw_err[0] = 0;
    return 0;
}

int gw_push(int type, double x, double y, int button, int dy, int key, int mods, const char *text)
{
    if (!gw_g) return -1;
    goome_event e;
    memset(&e, 0, sizeof(e));
    e.type = type, e.x = x, e.y = y, e.button = button, e.dy = dy, e.key = key, e.mods = mods;
    if (type == GOOME_FOCUS) e.id = button;
    if (text) snprintf(e.text, sizeof(e.text), "%s", text);
    return goome_push(gw_g, &e);
}

int gw_frame(double ms)
{
    if (!gw_g || !gw_u) return 0;
#ifdef __wasm__
    lc_set_time(ms);
#else
    (void)ms;
#endif
    gw_tpos = 0;
    goome_event ev;
    while (goome_poll(gw_g, &ev, 0) > 0) {
        if (ev.type == GOOME_KEY)
            for (gw_j *h = gw_p.hotkeys ? gw_p.hotkeys->kid : NULL; h; h = h->next) {
                gw_j *c = gw_at(h, 0);
                if (c && c->v == ev.key && c->n == ev.mods && ev.key) gw_acts(gw_at(h, 1));
            }
        goome_ui_feed(gw_u, &ev);
    }
    if (!goome_ui_begin(gw_u, 0)) return 0;
    if (gw_p.app) {
        goome_ui_app(gw_u, gw_text(gw_p.app_title), gw_p.app_flags);
        gw_list_nodes(gw_p.nodes, GP_APP);
        if (gw_p.app_status) goome_ui_statusbar(gw_u, gw_text(gw_p.app_status));
        goome_ui_app_end(gw_u);
    } else gw_list_nodes(gw_p.nodes, GP_OVERLAY);
    goome_ui_end(gw_u);
    return 1 | goome_ui_animating(gw_u) << 1 | goome_ui_wants(gw_u) << 2;
}

int gw_width(void)
{
    goome_monitor m;
    return gw_g && goome_monitors(gw_g, &m, 1) > 0 ? m.pw : 0;
}

int gw_height(void)
{
    goome_monitor m;
    return gw_g && goome_monitors(gw_g, &m, 1) > 0 ? m.ph : 0;
}

uint8_t *gw_pixels(void)
{
    int w = gw_width(), h = gw_height();
    size_t need = (size_t)w * (size_t)h * 4;
    if (!need) return NULL;
    if (need > gw_px_cap) {
        uint8_t *p = realloc(gw_px, need);
        if (!p) return NULL;
        gw_px = p, gw_px_cap = need;
    }
    if (goome_frame_copy(gw_g, gw_px, w * 4, GOOME_FMT_RGBA, NULL) != GOOME_OK) memset(gw_px, 0, need);
    return gw_px;
}

uint32_t gw_hash(void)
{
    uint8_t *p = gw_pixels();
    uint32_t h = 2166136261u;
    size_t n = (size_t)gw_width() * (size_t)gw_height() * 4;
    for (size_t i = 0; p && i < n; i++) h = (h ^ p[i]) * 16777619u;
    return h;
}

int gw_cursor(void) { return gw_g ? goome_cursor_shape(gw_g) : 0; }

const char *gw_log_take(void)
{
    static char out[sizeof(gw_logbuf)];
    memcpy(out, gw_logbuf, (size_t)gw_loglen + 1);
    gw_loglen = 0;
    gw_logbuf[0] = 0;
    return out;
}

const char *gw_clip_get(void)
{
    gw_clip[0] = 0;
    if (gw_g) goome_clipboard_get(gw_g, gw_clip, sizeof(gw_clip));
    return gw_clip;
}

int gw_clip_set(const char *s) { return gw_g ? goome_clipboard_set(gw_g, s ? s : "") : -1; }

void *gw_alloc(int n) { return malloc(n > 0 ? (size_t)n : 1); }
void gw_free(void *p) { free(p); }
