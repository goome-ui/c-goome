#include <stdio.h>
#include <string.h>

#include "goome.h"

#define MAX_DOTS 4096

static goome *g;
static float dots[MAX_DOTS][3];
static int ndots, down, width = 6, pasted;
static double mx = -1, my = -1;
static char note[96] = "draw with the mouse, wheel for size, c clears, ctrl+c copies the dot count";

int goome_web_init(void)
{
    int err;
    g = goome_open(&(goome_opts){.title = "Scribble", .mode = GOOME_MODE_WINDOW, .width = 640, .height = 420, .window_flags = GOOME_WINDOW_RESIZABLE}, &err);
    if (!g) {
        fprintf(stderr, "cant open a window: %s\n", goome_strerror(err));
        return 1;
    }
    return 0;
}

void goome_web_done(void) { goome_close(g); }

static void dot(double x, double y)
{
    if (ndots == MAX_DOTS) memmove(dots, dots + 1, sizeof(dots[0]) * (MAX_DOTS - 1)), ndots--;
    dots[ndots][0] = (float)x, dots[ndots][1] = (float)y, dots[ndots][2] = (float)width;
    ndots++;
}

int goome_web_frame(void)
{
    goome_event ev;
    while (goome_poll(g, &ev, 16) > 0) {
        if (ev.type == GOOME_QUIT || ev.type == GOOME_CLOSE) return 0;
        if (ev.type == GOOME_MOTION) {
            mx = ev.x, my = ev.y;
            if (down) dot(mx, my);
        }
        if (ev.type == GOOME_DOWN) down = 1, dot(ev.x, ev.y);
        if (ev.type == GOOME_UP) down = 0;
        if (ev.type == GOOME_SCROLL) width += ev.dy > 0 ? -1 : 1, width = width < 2 ? 2 : width > 40 ? 40 : width;
        if (ev.type == GOOME_TEXT) snprintf(note, sizeof(note), "text: %s", goome_event_text(g));
        if (ev.type == GOOME_PREEDIT) snprintf(note, sizeof(note), "composing: %s", goome_event_text(g));
        if (ev.type == GOOME_KEY && ev.key == 'c' && !ev.mods) ndots = 0;
        if (ev.type == GOOME_KEY && ev.key == 'c' && ev.mods == GOOME_CTRL) {
            snprintf(note, sizeof(note), "%d dots", ndots);
            goome_clipboard_set(g, note);
        }
        if (ev.type == GOOME_KEY && ev.key == 'v' && ev.mods == GOOME_CTRL) {
            char t[64];
            if (goome_clipboard_get(g, t, sizeof(t)) >= 0) snprintf(note, sizeof(note), "pasted: %s", t), pasted++;
        }
    }
    goome_cursor(g, down ? GOOME_CURSOR_HAND : GOOME_CURSOR_ARROW);
    goome_monitor m;
    goome_monitors(g, &m, 1);
    goome_canvas *c = goome_begin(g, 0);
    if (!c) return 1;
    goome_draw_rect(c, 0, 0, m.area.w, m.area.h, 0xFF181A20);
    for (int i = 0; i < ndots; i++) goome_draw_round_rect(c, dots[i][0] - dots[i][2] / 2, dots[i][1] - dots[i][2] / 2, dots[i][2], dots[i][2], dots[i][2] / 2, 0xFF6FDC5F);
    goome_draw_text(c, 12, 10, note, 0, 0xFFE8EAF0);
    goome_end(g, c, NULL, 0);
    return 1;
}

int main(void)
{
    if (goome_web_init()) return 1;
    while (goome_web_frame()) {
    }
    goome_web_done();
    return 0;
}
