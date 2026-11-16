#include "internal.h"

#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <time.h>
#endif

typedef struct {
    int tick, type, x, y, a, b; // a b = hold end tick / scroll amount
    char s[256];
} tb_step;

enum { TB_CLICK = 1, TB_HOVER, TB_HOLD, TB_SCROLL, TB_KEY, TB_HOTKEY, TB_SEND, TB_RESIZE, TB_CLOSE, TB_FOCUS, TB_DROP, TB_GRABBED, TB_TEXT, TB_PREEDIT, TB_IME };

typedef struct {
    char frames[1024], shots[1024];
    uint32_t *frame, *cap;
    int fw, fh, single, nframes;
    int w, h, cw, ch;
    double scale;
    int win, clear, focused, shown, state, wx, wy;
    uint32_t *px;
    goome_canvas cv;
    int tick, done_tick, fired, end, ended;
    tb_step *steps;
    int nsteps;
    goome_rect hits[512];
    int nhits;
    int hover_on, held, grab;
    double hx, hy;
} tb_state;

static tb_state tb;

static void tb_sleep(int ms)
{
    if (ms <= 0) return;
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    struct timespec t = {ms / 1000, (long)(ms % 1000) * 1000000L};
    nanosleep(&t, NULL);
#endif
}

static uint32_t *tb_load_ppm(const char *path, int *w, int *h)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    int vals[3], nv = 0, c;
    if (fgetc(f) != 'P' || fgetc(f) != '6') {
        fclose(f);
        return NULL;
    }
    while (nv < 3 && (c = fgetc(f)) != EOF) { // gimp puts # comments in the header
        if (c == '#')
            while ((c = fgetc(f)) != EOF && c != '\n') {}
        else if (c >= '0' && c <= '9') {
            int v = c - '0';
            while ((c = fgetc(f)) >= '0' && c <= '9' && v < 100000) v = v * 10 + c - '0';
            vals[nv++] = v;
        }
    }
    if (nv < 3 || vals[0] <= 0 || vals[1] <= 0 || vals[0] > 16384 || vals[1] > 16384 || vals[2] != 255) {
        fclose(f);
        return NULL;
    }
    size_t n = (size_t)vals[0] * vals[1];
    unsigned char *rgb = malloc(n * 3);
    uint32_t *out = malloc(n * 4);
    if (!rgb || !out || fread(rgb, 3, n, f) != n) {
        free(rgb), free(out), fclose(f);
        return NULL;
    }
    fclose(f);
    for (size_t i = 0; i < n; i++) out[i] = 0xFF000000u | (uint32_t)rgb[i * 3] << 16 | (uint32_t)rgb[i * 3 + 1] << 8 | rgb[i * 3 + 2];
    free(rgb);
    *w = vals[0], *h = vals[1];
    return out;
}

static void tb_fake_frame(void)
{
    free(tb.frame);
    tb.fw = (int)(tb.w * tb.scale + 0.5), tb.fh = (int)(tb.h * tb.scale + 0.5);
    tb.frame = malloc((size_t)tb.fw * tb.fh * 4);
    if (!tb.frame) return;
    for (int y = 0; y < tb.fh; y++)
        for (int x = 0; x < tb.fw; x++) {
            int grid = (x % 64 == 0 || y % 64 == 0) ? 24 : 0;
            uint32_t r = (uint32_t)(40 + x * 60 / tb.fw + grid), gg = (uint32_t)(44 + y * 50 / tb.fh + grid), b = (uint32_t)(70 + grid);
            tb.frame[(size_t)y * tb.fw + x] = 0xFF000000u | r << 16 | gg << 8 | b;
        }
}

static int tb_load_frame(int n)
{
    char path[1100];
    if (tb.single) {
        if (tb.frame) return 1;
        snprintf(path, sizeof(path), "%s", tb.frames);
    } else
        snprintf(path, sizeof(path), "%s/%06d.ppm", tb.frames, n);
    int w, h;
    uint32_t *px = tb_load_ppm(path, &w, &h);
    if (!px) return 0;
    if (tb.frame && (w != tb.fw || h != tb.fh)) { // frames have to stay one size, the monitor doesnt change
        free(px);
        return 0;
    }
    free(tb.frame);
    tb.frame = px, tb.fw = w, tb.fh = h;
    return 1;
}

static char *tb_read_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    char *buf = malloc(65537);
    size_t n = buf ? fread(buf, 1, 65536, f) : 0;
    fclose(f);
    if (buf) buf[n] = 0;
    return buf;
}

static int tb_word(const char **p, const char *w)
{
    size_t l = strlen(w);
    if (strncmp(*p, w, l) || ((*p)[l] != ' ' && (*p)[l] != 0)) return 0;
    *p += l;
    while (**p == ' ') (*p)++;
    return 1;
}

static void tb_xy(const char **p, int *x, int *y)
{
    *x = gm_num(p);
    if (**p == ',') (*p)++;
    *y = gm_num(p);
    while (**p == ' ') (*p)++;
}

static void tb_ime_parse(const char *q, tb_step *s)
{
    snprintf(s->s, sizeof(s->s), "%s", q);
    int n = (int)strlen(s->s), sp = n;
    while (sp > 0 && s->s[sp - 1] != ' ') sp--;
    s->a = -1;
    if (sp > 0 && sp < n) {
        const char *d = s->s + sp;
        int ok = 1;
        for (const char *c = d; *c; c++) ok &= *c >= '0' && *c <= '9';
        if (ok) {
            s->a = gm_num(&d);
            s->s[sp - 1] = 0;
        }
    }
    if (s->a < 0) s->a = (int)strlen(s->s);
}

static void tb_ime_fire(goome *g, const tb_step *s)
{
    if (s->type != TB_IME && !(tb.win ? tb.focused : g->kb_on)) return;
    if (s->type == TB_TEXT) gm_ime_push(g, GOOME_TEXT, s->s, -1, 0);
    else if (s->type == TB_PREEDIT) gm_ime_push(g, GOOME_PREEDIT, s->s, -1, s->a);
    else {
        goome_event e;
        memset(&e, 0, sizeof(e));
        e.type = GOOME_CONTROL;
        snprintf(e.msg, sizeof(e.msg), "ime %d %d,%d %dx%d", g->ime_on, g->ime_caret.x, g->ime_caret.y, g->ime_caret.w, g->ime_caret.h);
        gm_push(g, &e);
    }
}

// "<tick> click x,y" etc, one per line (or split with ;). the readme has the full list
static void tb_parse(const char *text)
{
    int cap = 64;
    tb.steps = malloc(sizeof(tb_step) * (size_t)cap);
    const char *p = text;
    while (p && *p && tb.steps) {
        char line[256];
        int l = 0;
        while (*p && *p != '\n' && *p != ';' && l < 255) line[l++] = *p++;
        while (*p && *p != '\n' && *p != ';') p++;
        if (*p) p++;
        line[l] = 0;
        for (int i = 0; i < l; i++)
            if (line[i] == '#' && (i == 0 || line[i - 1] == ' ')) line[i] = 0, l = i;
        while (l && (line[l - 1] == ' ' || line[l - 1] == '\r' || line[l - 1] == '\t')) line[--l] = 0;
        const char *q = line;
        while (*q == ' ' || *q == '\t') q++;
        if (!*q) continue;
        if (tb_word(&q, "screen")) {
            tb.w = gm_num(&q);
            while (*q == ' ' || *q == 'x') q++;
            tb.h = gm_num(&q);
            while (*q == ' ') q++;
            int s100 = gm_num(&q) * 100, frac = 0, digits = 0; // "2" or "1.5"
            if (*q == '.') {
                q++;
                while (*q >= '0' && *q <= '9' && digits < 2) frac = frac * 10 + *q++ - '0', digits++;
                if (digits == 1) frac *= 10;
            }
            if (s100 + frac > 0) tb.scale = (s100 + frac) / 100.0;
            continue;
        }
        if (tb_word(&q, "end")) {
            tb.end = gm_num(&q);
            continue;
        }
        if (*q < '0' || *q > '9') continue;
        tb_step s;
        memset(&s, 0, sizeof(s));
        s.tick = gm_num(&q);
        while (*q == ' ') q++;
        if (tb_word(&q, "click")) s.type = TB_CLICK, tb_xy(&q, &s.x, &s.y), s.a = *q ? gm_num(&q) : 1;
        else if (tb_word(&q, "hover")) s.type = TB_HOVER, tb_xy(&q, &s.x, &s.y);
        else if (tb_word(&q, "hold")) s.type = TB_HOLD, tb_xy(&q, &s.x, &s.y), s.a = gm_num(&q);
        else if (tb_word(&q, "scroll")) s.type = TB_SCROLL, tb_xy(&q, &s.x, &s.y), s.b = *q ? gm_num(&q) : 1;
        else if (tb_word(&q, "key")) s.type = TB_KEY, snprintf(s.s, sizeof(s.s), "%s", q);
        else if (tb_word(&q, "hotkey")) s.type = TB_HOTKEY, snprintf(s.s, sizeof(s.s), "%s", q);
        else if (tb_word(&q, "send")) s.type = TB_SEND, snprintf(s.s, sizeof(s.s), "%s", q);
        else if (tb_word(&q, "resize")) {
            s.type = TB_RESIZE, s.a = gm_num(&q);
            while (*q == ' ' || *q == 'x') q++;
            s.b = gm_num(&q);
        } else if (tb_word(&q, "close")) s.type = TB_CLOSE;
        else if (tb_word(&q, "focus")) s.type = TB_FOCUS, s.a = gm_num(&q);
        else if (tb_word(&q, "drop")) s.type = TB_DROP, tb_xy(&q, &s.x, &s.y), snprintf(s.s, sizeof(s.s), "%s", q);
        else if (tb_word(&q, "grabbed")) s.type = TB_GRABBED;
        else if (tb_word(&q, "text")) s.type = TB_TEXT, snprintf(s.s, sizeof(s.s), "%s", q);
        else if (tb_word(&q, "preedit")) s.type = TB_PREEDIT, tb_ime_parse(q, &s);
        else if (tb_word(&q, "ime")) s.type = TB_IME;
        else continue;
        if (tb.nsteps == cap) {
            tb_step *n = realloc(tb.steps, sizeof(tb_step) * (size_t)(cap *= 2));
            if (!n) break;
            tb.steps = n;
        }
        tb.steps[tb.nsteps++] = s;
        int last = s.type == TB_HOLD && s.a > s.tick ? s.a : s.tick;
        if (last + 2 > tb.done_tick) tb.done_tick = last + 2;
    }
}

static void tb_monitor(goome *g)
{
    goome_monitor m;
    memset(&m, 0, sizeof(m));
    snprintf(m.name, sizeof(m.name), "%s", tb.win ? g->title : "test");
    m.area = (goome_rect){0, 0, tb.w, tb.h};
    m.scale = tb.scale, m.pw = tb.cw, m.ph = tb.ch;
    gm_set_monitors(g, &m, 1);
}

static int tb_size(goome *g, int w, int h)
{
    if (tb.win && w < g->opts.min_width) w = g->opts.min_width;
    if (tb.win && h < g->opts.min_height) h = g->opts.min_height;
    if (w < 1 || h < 1 || w > 16384 || h > 16384) return gm_fail(g, GOOME_EARG, "test backend: bad size %dx%d", w, h);
    int same = !tb.win && !tb.px, cw = same ? tb.fw : (int)(w * tb.scale + 0.5), ch = same ? tb.fh : (int)(h * tb.scale + 0.5);
    uint32_t *px = calloc((size_t)cw * ch, 4);
    if (!px) return gm_fail(g, GOOME_ENOMEM, "test backend: no memory for the canvas");
    free(tb.px);
    tb.px = px, tb.w = w, tb.h = h, tb.cw = cw, tb.ch = ch;
    gm_canvas_reset(&tb.cv, tb.px, cw, ch, cw, 0, 0, tb.scale, 0);
    tb_monitor(g);
    return GOOME_OK;
}

static int tb_open(goome *g)
{
    memset(&tb, 0, sizeof(tb));
    tb.w = 1280, tb.h = 720, tb.scale = 1, tb.end = -1, tb.fired = -1;
    const char *fr = g->opts.test_frames ? g->opts.test_frames : gm_env("GOOME_TEST_FRAMES");
    const char *sc = g->opts.test_script ? g->opts.test_script : gm_env("GOOME_TEST_SCRIPT");
    const char *sh = g->opts.test_shots ? g->opts.test_shots : gm_env("GOOME_TEST_SHOTS");
    if (sh) snprintf(tb.shots, sizeof(tb.shots), "%s", sh);
    if (sc) {
        char *text = (strchr(sc, '\n') || strchr(sc, ';')) ? NULL : tb_read_file(sc);
        tb_parse(text ? text : sc);
        free(text);
    }
    if (fr) {
        snprintf(tb.frames, sizeof(tb.frames), "%s", fr);
        size_t l = strlen(fr);
        tb.single = l > 4 && !strcmp(fr + l - 4, ".ppm");
        if (!tb_load_frame(0)) return gm_fail(g, GOOME_ENODISPLAY, "test backend: cant read frames from %s", fr);
        if (!tb.single)
            while (tb.nframes < 1000000) {
                char path[1100];
                snprintf(path, sizeof(path), "%s/%06d.ppm", tb.frames, tb.nframes);
                FILE *f = fopen(path, "rb");
                if (!f) break;
                fclose(f);
                tb.nframes++;
            }
        tb.w = (int)(tb.fw / tb.scale + 0.5), tb.h = (int)(tb.fh / tb.scale + 0.5);
    } else
        tb_fake_frame();
    if (!tb.frame) return gm_fail(g, GOOME_ENOMEM, "test backend: no memory for the frame");
    if (tb.end < 0) tb.end = tb.nframes && !tb.single ? tb.nframes : tb.done_tick > 0 ? tb.done_tick : 30;
    tb.win = g->opts.mode == GOOME_MODE_WINDOW;
    tb.clear = tb.win && !(g->opts.window_flags & GOOME_WINDOW_TRANSPARENT);
    tb.shown = tb.focused = !(tb.win && (g->opts.window_flags & GOOME_WINDOW_HIDDEN));
    return tb_size(g, tb.win ? g->opts.width : tb.w, tb.win ? g->opts.height : tb.h);
}

static void tb_close(goome *g)
{
    free(tb.frame), free(tb.cap), free(tb.px), free(tb.steps);
    memset(&tb, 0, sizeof(tb));
}

static uint32_t tb_mix(int x, int y, int self)
{
    int fx = x + (int)(tb.wx * tb.scale), fy = y + (int)(tb.wy * tb.scale);
    uint32_t u = tb.clear || fx < 0 || fy < 0 || fx >= tb.fw || fy >= tb.fh ? 0xFF000000u : tb.frame[(size_t)fy * tb.fw + fx];
    uint32_t o = self ? tb.px[(size_t)y * tb.cw + x] : 0, a = o >> 24, v = 0xFF000000u;
    for (int s = 0; s < 24; s += 8) v |= (((o >> s) & 255) + ((u >> s) & 255) * (255 - a) / 255) << s; // integers only so every os writes the same bytes
    return v;
}

static int tb_grab(goome *g, int mon, goome_rect r, goome_image *img)
{
    if (!tb.single && tb.nframes) tb_load_frame(tb.tick < tb.nframes ? tb.tick : tb.nframes - 1);
    int x0 = (int)(r.x * tb.scale), y0 = (int)(r.y * tb.scale), w = (int)(r.w * tb.scale + 0.5), h = (int)(r.h * tb.scale + 0.5);
    if (x0 + w > tb.cw) w = tb.cw - x0;
    if (y0 + h > tb.ch) h = tb.ch - y0;
    if (w < 1 || h < 1) return gm_fail(g, GOOME_EARG, "test backend: nothing to grab there");
    free(tb.cap);
    tb.cap = malloc((size_t)w * h * 4);
    if (!tb.cap) return gm_fail(g, GOOME_ENOMEM, "test backend: no memory for the grab");
    int self = !g->opts.hide_self; // a real linux grab sees our overlay too
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) tb.cap[(size_t)y * w + x] = tb_mix(x0 + x, y0 + y, self);
    img->px = tb.cap, img->w = w, img->h = h, img->stride = w, img->area = r, img->has_self = self;
    return GOOME_OK;
}

static goome_canvas *tb_canvas(goome *g, int mon) { return mon == 0 ? &tb.cv : NULL; }

static void tb_shot(void)
{
    char path[1100];
    snprintf(path, sizeof(path), "%s/%06d.ppm", tb.shots, tb.tick);
    if (!tb.single && tb.nframes) tb_load_frame(tb.tick < tb.nframes ? tb.tick : tb.nframes - 1);
    uint32_t *out = malloc((size_t)tb.cw * tb.ch * 4);
    if (!out) return;
    for (int y = 0; y < tb.ch; y++)
        for (int x = 0; x < tb.cw; x++) out[(size_t)y * tb.cw + x] = tb_mix(x, y, 1);
    goome_save_ppm(path, out, tb.cw, tb.ch, tb.cw);
    free(out);
}

static int tb_commit(goome *g, int mon, goome_canvas *c, const goome_rect *hits, int n)
{
    tb.nhits = n < 512 ? n : 512;
    if (tb.nhits) memcpy(tb.hits, hits, sizeof(goome_rect) * (size_t)tb.nhits);
    if (tb.shots[0]) tb_shot();
    tb.tick++;
    return GOOME_OK;
}

static int tb_on_hit(double x, double y)
{
    if (tb.grab) return 1;
    if (tb.win) return tb.shown && x >= 0 && y >= 0 && x < tb.w && y < tb.h;
    for (int i = 0; i < tb.nhits; i++)
        if (x >= tb.hits[i].x && x < tb.hits[i].x + tb.hits[i].w && y >= tb.hits[i].y && y < tb.hits[i].y + tb.hits[i].h) return 1;
    return 0;
}

static void tb_ev(goome *g, int type, double x, double y)
{
    goome_event e;
    memset(&e, 0, sizeof(e));
    e.type = type, e.x = x, e.y = y, e.button = 1;
    gm_push(g, &e);
}

static void tb_keys(goome *g, const char *s, int hot)
{
    if (hot) {
        int k, m;
        if (goome_parse_combo(s, &k, &m) != GOOME_OK) return;
        for (int i = 0; i < g->nhk; i++)
            if (g->hk[i].key == k && g->hk[i].mods == m) {
                goome_event e;
                memset(&e, 0, sizeof(e));
                e.type = GOOME_HOTKEY, e.id = g->hk[i].id, e.key = k, e.mods = m;
                gm_push(g, &e);
                return;
            }
        if (!(tb.win ? tb.focused : g->kb_on)) return;
        goome_event e;
        memset(&e, 0, sizeof(e));
        e.type = GOOME_KEY, e.key = k, e.mods = m;
        if (k >= 32 && k < 127 && !(m & (GOOME_CTRL | GOOME_ALT | GOOME_SUPER))) e.text[0] = (char)k;
        gm_push(g, &e);
        return;
    }
    if (!(tb.win ? tb.focused : g->kb_on)) return; // like a real one, no keyboard = the keys go to whatever has focus
    for (; *s; s++) {
        int ch = (unsigned char)*s;
        if (ch == '\\' && s[1]) {
            s++;
            ch = *s == 'b' ? 8 : *s == 'e' ? 27 : *s == 'n' ? 13 : *s == 't' ? 9 : *s == 's' ? ' ' : (unsigned char)*s;
        }
        goome_event e;
        memset(&e, 0, sizeof(e));
        e.type = GOOME_KEY;
        if (ch >= 32 && ch < 127) e.text[0] = (char)ch;
        int c = gm_combo_key(ch, 0);
        e.key = c & 0xFF, e.mods = c & ~0xFF;
        gm_push(g, &e);
    }
}

static int tb_wait(goome *g, int timeout_ms)
{
    while (tb.fired < tb.tick) { // ticks fire in order, even ones the app drew past without waiting
        int t = ++tb.fired;
        for (int i = 0; i < tb.nsteps; i++) {
            tb_step *s = &tb.steps[i];
            if (s->type == TB_HOLD && s->a == t && tb.held) {
                tb.held = 0;
                tb_ev(g, GOOME_UP, s->x, s->y);
            }
            if (s->tick != t) continue;
            switch (s->type) {
            case TB_CLICK:
                if (!tb_on_hit(s->x, s->y)) break; // missed our regions, it went to whatever is under us
                for (int k = 0; k < (s->a > 0 ? s->a : 1); k++) tb_ev(g, GOOME_DOWN, s->x, s->y), tb_ev(g, GOOME_UP, s->x, s->y);
                break;
            case TB_HOVER:
                tb.hover_on = 1, tb.hx = s->x, tb.hy = s->y;
                if (tb_on_hit(s->x, s->y)) tb_ev(g, GOOME_MOTION, s->x, s->y);
                break;
            case TB_HOLD:
                tb.hover_on = 1, tb.hx = s->x, tb.hy = s->y;
                if (tb_on_hit(s->x, s->y)) tb.held = 1, tb_ev(g, GOOME_DOWN, s->x, s->y);
                break;
            case TB_SCROLL:
                if (tb_on_hit(s->x, s->y)) {
                    goome_event e;
                    memset(&e, 0, sizeof(e));
                    e.type = GOOME_SCROLL, e.x = s->x, e.y = s->y, e.dy = s->b;
                    gm_push(g, &e);
                }
                break;
            case TB_KEY: tb_keys(g, s->s, 0); break;
            case TB_HOTKEY: tb_keys(g, s->s, 1); break;
            case TB_SEND: {
                goome_event e;
                memset(&e, 0, sizeof(e));
                e.type = GOOME_CONTROL;
                snprintf(e.msg, sizeof(e.msg), "%.63s", s->s);
                gm_push(g, &e);
                break;
            }
            case TB_RESIZE: tb_size(g, s->a, s->b); break;
            case TB_CLOSE: tb_ev(g, GOOME_CLOSE, 0, 0); break;
            case TB_FOCUS: {
                goome_event e;
                memset(&e, 0, sizeof(e));
                e.type = GOOME_FOCUS, e.id = tb.focused = !!s->a;
                gm_push(g, &e);
                break;
            }
            case TB_DROP:
                if (tb_on_hit(s->x, s->y)) gm_drop(g, s->x, s->y, s->s);
                break;
            case TB_GRABBED: {
                goome_event e;
                memset(&e, 0, sizeof(e));
                e.type = GOOME_CONTROL;
                snprintf(e.msg, sizeof(e.msg), "grabbed %d", tb.grab);
                gm_push(g, &e);
                break;
            }
            case TB_TEXT:
            case TB_PREEDIT:
            case TB_IME: tb_ime_fire(g, s); break;
            }
        }
        if (t >= tb.end && !tb.ended && t == tb.tick) {
            tb.ended = 1;
            tb_ev(g, GOOME_QUIT, 0, 0);
        }
        if (g->qlen) break; // a click can turn the keyboard on, let the app see it first
    }
    if (!g->qlen) tb_sleep(timeout_ms);
    return 0;
}

static int tb_pointer(goome *g, double *x, double *y, int *buttons)
{
    if (!tb.hover_on) return 0;
    *x = tb.hx, *y = tb.hy, *buttons = tb.held;
    return 1;
}

static int tb_keyboard(goome *g, int on) { return GOOME_OK; }
static int tb_take(goome *g, int on)
{
    tb.grab = on;
    return GOOME_OK;
}
static int tb_cursor(goome *g, int shape) { return GOOME_OK; }
static int tb_clip_set(goome *g, const char *s) { return gm_clip_keep(g, s); }
static int tb_clip_get(goome *g, char **out) { return gm_clip_copy(g, out); }

static int tb_win(goome *g, int op, int a, int b, int *ra, int *rb)
{
    if (!tb.win) return gm_fail(g, GOOME_ENOTSUP, "test backend: no window in overlay mode");
    switch (op) {
    case GM_WIN_TITLE: tb_monitor(g); return GOOME_OK;
    case GM_WIN_SIZE: return tb_size(g, a, b);
    case GM_WIN_POS_GET: *ra = tb.wx, *rb = tb.wy; return GOOME_OK;
    case GM_WIN_POS_SET: tb.wx = a, tb.wy = b; return GOOME_OK;
    case GM_WIN_STATE: tb.state = a; return GOOME_OK;
    case GM_WIN_GET_STATE: *ra = tb.state; return GOOME_OK;
    case GM_WIN_DRAG: return GOOME_OK;
    case GM_WIN_SHOW: tb.shown = a; return GOOME_OK;
    }
    return GOOME_EARG;
}
static int tb_hotkeys(goome *g) { return GOOME_OK; }
static int tb_cap_open(goome *g) { return GOOME_OK; }

static const gm_cap tb_cap = {"test", NULL, tb_cap_open, tb_grab, NULL, GOOME_CAP_CAPTURE | GOOME_CAP_HIDE_SELF};
static const gm_ov tb_ov = {"test", tb_open, tb_canvas, tb_commit, tb_wait, tb_pointer, tb_keyboard, tb_hotkeys, tb_close,
                            GOOME_CAP_GLOBAL_POINTER | GOOME_CAP_KEYBOARD | GOOME_CAP_HOTKEYS | GOOME_CAP_WINDOW | GOOME_CAP_CLIPBOARD | GOOME_CAP_CURSOR |
                                GOOME_CAP_GRAB,
                            tb_win, tb_cursor, tb_clip_set, tb_clip_get, tb_take};

const gm_cap *gm_cap_test(void) { return &tb_cap; }
const gm_ov *gm_ov_test(void) { return &tb_ov; }
