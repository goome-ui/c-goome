#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "goome.h"

int goome_web_init(void);
int goome_web_frame(void);
void goome_web_done(void) __attribute__((weak));
void lc_set_time(double ms);

static goome *gmw_g;
static int gmw_w = 960, gmw_h = 640, gmw_chrome, gmw_over;
static double gmw_scale = 1;
static uint8_t *gmw_px;
static size_t gmw_px_cap;
static char gmw_clip[65536];

void gmw_claim(goome_opts *o)
{
    o->backend = GOOME_OFFSCREEN;
    o->width = gmw_w, o->height = gmw_h, o->scale = gmw_scale;
    o->min_width = o->min_height = 0;
    if (!gmw_chrome) o->window_flags |= GOOME_WINDOW_NO_TITLEBAR;
}

void gmw_opened(goome *g) { gmw_g = g; }

int gmw_init(int w, int h, double scale, int chrome)
{
    gmw_w = w, gmw_h = h, gmw_scale = scale > 0 ? scale : 1, gmw_chrome = chrome;
    gmw_over = 0;
    if (goome_web_init() != 0) return -1;
    return gmw_g ? 0 : -2;
}

int gmw_frame(double ms)
{
    if (!gmw_g || gmw_over) return 0;
    lc_set_time(ms);
    int alive = goome_web_frame();
    if (!alive) {
        gmw_over = 1;
        if (goome_web_done) goome_web_done();
    }
    goome_rect r;
    return goome_frame_dirty(gmw_g, &r) | (alive ? 2 : 0);
}

int gmw_resize(int w, int h, double scale)
{
    gmw_w = w, gmw_h = h, gmw_scale = scale > 0 ? scale : 1;
    return gmw_g ? goome_resize(gmw_g, w, h, gmw_scale) : -1;
}

int gmw_width(void)
{
    goome_monitor m;
    return gmw_g && goome_monitors(gmw_g, &m, 1) > 0 ? m.pw : 0;
}

int gmw_height(void)
{
    goome_monitor m;
    return gmw_g && goome_monitors(gmw_g, &m, 1) > 0 ? m.ph : 0;
}

uint8_t *gmw_pixels(void)
{
    int w = gmw_width(), h = gmw_height();
    size_t need = (size_t)w * (size_t)h * 4;
    if (!need) return NULL;
    if (need > gmw_px_cap) {
        uint8_t *p = realloc(gmw_px, need);
        if (!p) return NULL;
        gmw_px = p, gmw_px_cap = need;
    }
    if (goome_frame_copy(gmw_g, gmw_px, w * 4, GOOME_FMT_RGBA, NULL) != GOOME_OK) memset(gmw_px, 0, need);
    return gmw_px;
}

int gmw_push(int type, double x, double y, int button, int dy, int key, int mods, const char *text)
{
    if (!gmw_g) return -1;
    goome_event e;
    memset(&e, 0, sizeof(e));
    e.type = type, e.x = x, e.y = y, e.button = button, e.dy = dy, e.key = key, e.mods = mods;
    if (type == GOOME_FOCUS || type == GOOME_PREEDIT) e.id = button;
    if (text && (type == GOOME_TEXT || type == GOOME_PREEDIT)) snprintf(e.msg, sizeof(e.msg), "%s", text);
    else if (text) snprintf(e.text, sizeof(e.text), "%s", text);
    return goome_push(gmw_g, &e);
}

int gmw_cursor(void) { return gmw_g ? goome_cursor_shape(gmw_g) : 0; }

const char *gmw_title(void)
{
    static goome_monitor m;
    if (!gmw_g || goome_monitors(gmw_g, &m, 1) < 1) return "goome";
    return m.name;
}

const char *gmw_clip_get(void)
{
    gmw_clip[0] = 0;
    if (gmw_g) goome_clipboard_get(gmw_g, gmw_clip, sizeof(gmw_clip));
    return gmw_clip;
}

int gmw_clip_set(const char *s) { return gmw_g ? goome_clipboard_set(gmw_g, s ? s : "") : -1; }

void *gmw_alloc(int n) { return malloc(n > 0 ? (size_t)n : 1); }
void gmw_free(void *p) { free(p); }
