#include "internal.h"

#include <stdio.h>
#include <stdlib.h>

typedef struct {
    uint32_t *px;
    int w, h, pw, ph, shown, state;
    double scale;
    goome_canvas cv;
} of_state;

static void of_monitor(goome *g, of_state *s)
{
    goome_monitor m;
    memset(&m, 0, sizeof(m));
    snprintf(m.name, sizeof(m.name), "%s", g->title);
    m.area = (goome_rect){0, 0, s->w, s->h};
    m.scale = s->scale, m.pw = s->pw, m.ph = s->ph;
    gm_set_monitors(g, &m, 1);
}

static int of_size(goome *g, of_state *s, int w, int h, double scale)
{
    if (w <= 0 || h <= 0 || w > 16384 || h > 16384 || !(scale >= 0 && scale <= 8)) return gm_fail(g, GOOME_EARG, "offscreen: bad size %dx%d at %g", w, h, scale);
    if (scale == 0) scale = 1;
    int pw = (int)(w * scale + 0.5), ph = (int)(h * scale + 0.5);
    if (pw < 1) pw = 1;
    if (ph < 1) ph = 1;
    if (pw > 16384 || ph > 16384) return gm_fail(g, GOOME_EARG, "offscreen: %dx%d px is too big", pw, ph);
    if (!s->px || pw != s->pw || ph != s->ph) {
        uint32_t *px = calloc((size_t)pw * ph, 4);
        if (!px) return gm_fail(g, GOOME_ENOMEM, "offscreen: no memory for a %dx%d frame", pw, ph);
        free(s->px);
        s->px = px;
    } else
        memset(s->px, 0, (size_t)pw * ph * 4);
    s->w = w, s->h = h, s->pw = pw, s->ph = ph, s->scale = scale;
    gm_canvas_reset(&s->cv, s->px, pw, ph, pw, 0, 0, scale, 0);
    of_monitor(g, s);
    return GOOME_OK;
}

static int of_open(goome *g)
{
    of_state *s = calloc(1, sizeof(*s));
    if (!s) return gm_fail(g, GOOME_ENOMEM, "offscreen: no memory");
    g->ov_priv = s;
    g->opts.mode = GOOME_MODE_OFFSCREEN;
    s->shown = !(g->opts.window_flags & GOOME_WINDOW_HIDDEN);
    int e = of_size(g, s, g->opts.width, g->opts.height, g->opts.scale);
    if (e != GOOME_OK) {
        free(s);
        g->ov_priv = NULL;
    }
    return e;
}

static void of_close(goome *g)
{
    of_state *s = g->ov_priv;
    if (s) free(s->px);
    free(s);
    g->ov_priv = NULL;
}

int gm_of_resize(goome *g, int w, int h, double scale)
{
    of_state *s = g->ov_priv;
    return of_size(g, s, w, h, scale > 0 ? scale : s->scale);
}

static goome_canvas *of_canvas(goome *g, int mon)
{
    of_state *s = g->ov_priv;
    return mon == 0 ? &s->cv : NULL;
}

static int of_commit(goome *g, int mon, goome_canvas *c, const goome_rect *hits, int n) { return GOOME_OK; }
static int of_keyboard(goome *g, int on) { return GOOME_OK; }
static int of_cursor(goome *g, int shape) { return GOOME_OK; }
static int of_clip_set(goome *g, const char *s) { return gm_clip_keep(g, s); }
static int of_clip_get(goome *g, char **out) { return gm_clip_copy(g, out); }

static int of_win(goome *g, int op, int a, int b, int *ra, int *rb)
{
    of_state *s = g->ov_priv;
    switch (op) {
    case GM_WIN_TITLE: of_monitor(g, s); return GOOME_OK;
    case GM_WIN_SIZE: return of_size(g, s, a, b, s->scale);
    case GM_WIN_STATE: s->state = a; return GOOME_OK;
    case GM_WIN_GET_STATE: *ra = s->state; return GOOME_OK;
    case GM_WIN_SHOW: s->shown = a; return GOOME_OK;
    }
    return gm_fail(g, GOOME_ENOTSUP, "offscreen: no window, nothing to move");
}

static const gm_ov of_ov = {"offscreen", of_open, of_canvas, of_commit, NULL, NULL, of_keyboard, NULL, of_close,
                            GOOME_CAP_KEYBOARD | GOOME_CAP_CLIPBOARD | GOOME_CAP_CURSOR, of_win, of_cursor, of_clip_set, of_clip_get};

const gm_ov *gm_ov_offscreen(void) { return &of_ov; }
