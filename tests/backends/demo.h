#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "goome.h"

enum { DEMO_W = 360, DEMO_H = 330, CLEAR_R = 0x30, CLEAR_G = 0x40, CLEAR_B = 0x50 };

typedef struct {
    goome *g;
    goome_ui *u;
    double x, y, w, h, volume;
    int check, clicks, fails;
    char name[64];
} demo;

static int demo_open(demo *d)
{
    int err;
    memset(d, 0, sizeof(*d));
    d->g = goome_open(&(goome_opts){.mode = GOOME_MODE_OFFSCREEN, .width = DEMO_W, .height = DEMO_H}, &err);
    if (!d->g) return printf("cant open offscreen goome: %s\n", goome_strerror(err)), 0;
    d->u = goome_ui_new(d->g);
    if (!d->u) return 0;
    goome_ui_theme(d->u)->anim_ms = 0;
    goome_ui_theme(d->u)->font = NULL;
    d->x = 20, d->y = 20, d->w = 320, d->volume = 0.25;
    snprintf(d->name, sizeof(d->name), "goome");
    return 1;
}

static void demo_frame(demo *d)
{
    goome_event ev;
    while (goome_poll(d->g, &ev, 0) > 0) goome_ui_feed(d->u, &ev);
    if (!goome_ui_begin(d->u, 0)) return;
    if (goome_ui_window(d->u, "backend", &d->x, &d->y, &d->w, &d->h, GOOME_WIN_MOVE, NULL)) {
        goome_ui_label(d->u, "drawn by a renderer backend");
        goome_ui_checkbox(d->u, "Pushed a click", &d->check);
        goome_ui_slider(d->u, "Volume", &d->volume, 0, 1, "%.2f");
        goome_ui_input(d->u, "Name", d->name, sizeof(d->name));
        goome_ui_progress(d->u, d->volume, NULL);
        if (goome_ui_button(d->u, "Hover me")) d->clicks++;
    }
    goome_ui_window_end(d->u);
    goome_ui_end(d->u);
}

static void demo_close(demo *d)
{
    goome_ui_free(d->u);
    goome_close(d->g);
}

static void demo_expect(demo *d, const char *what, int got, int want)
{
    printf("  %-34s %s\n", what, got == want ? "ok" : "WRONG");
    if (got != want) d->fails++;
}

static int demo_compare(demo *d, const char *what, const unsigned char *rgba, int w, int h)
{
    goome_image f;
    if (goome_frame(d->g, &f) != GOOME_OK || f.w != w || f.h != h) {
        printf("  %-34s WRONG (frame %dx%d, gl %dx%d)\n", what, f.w, f.h, w, h);
        return d->fails++, 0;
    }
    int bad = 0, worst = 0, ink = 0;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            uint32_t s = f.px[(size_t)y * f.stride + x], a = s >> 24;
            const unsigned char *p = rgba + ((size_t)(h - 1 - y) * w + x) * 4;
            int want[3] = {(int)((s >> 16) & 255) + CLEAR_R * (255 - (int)a) / 255, (int)((s >> 8) & 255) + CLEAR_G * (255 - (int)a) / 255,
                           (int)(s & 255) + CLEAR_B * (255 - (int)a) / 255};
            if (a) ink++;
            for (int k = 0; k < 3; k++) {
                int diff = abs(p[k] - want[k]);
                if (diff > worst) worst = diff;
                if (diff > 2) bad++;
            }
        }
    int ok = bad == 0 && ink > 1000;
    printf("  %-34s %s (ink %d px, worst diff %d)\n", what, ok ? "ok" : "WRONG", ink, worst);
    if (!ok) d->fails++;
    return ok;
}

enum { EV_MOVE, EV_DOWN, EV_UP, EV_KEY };

typedef int (*demo_send)(void *ctx, int type, double x, double y, int ch);
typedef void (*demo_tick)(void *ctx);

static void demo_input(demo *d, demo_send send, demo_tick tick, void *ctx)
{
    tick(ctx);
    tick(ctx);
    demo_expect(d, "move over the window eaten", send(ctx, EV_MOVE, 60, 163, 0), 1);
    demo_expect(d, "slider press eaten", send(ctx, EV_DOWN, 60, 163, 0), 1);
    tick(ctx);
    demo_expect(d, "slider drag eaten", send(ctx, EV_MOVE, 250, 163, 0), 1);
    tick(ctx);
    demo_expect(d, "drag past the window still eaten", send(ctx, EV_MOVE, 355, 163, 0), 1);
    tick(ctx);
    demo_expect(d, "slider release eaten", send(ctx, EV_UP, 355, 163, 0), 1);
    tick(ctx);
    demo_expect(d, "slider moved", d->volume > 0.9, 1);
    demo_expect(d, "move to empty space not eaten", send(ctx, EV_MOVE, 352, 322, 0), 0);
    tick(ctx);
    demo_expect(d, "click on empty space not eaten", send(ctx, EV_DOWN, 352, 322, 0), 0);
    demo_expect(d, "release on empty space not eaten", send(ctx, EV_UP, 352, 322, 0), 0);
    tick(ctx);
    demo_expect(d, "key with nothing focused not eaten", send(ctx, EV_KEY, 0, 0, 'z'), 0);
    tick(ctx);
    send(ctx, EV_MOVE, 150, 223, 0);
    demo_expect(d, "click on the input eaten", send(ctx, EV_DOWN, 150, 223, 0), 1);
    demo_expect(d, "input release eaten", send(ctx, EV_UP, 150, 223, 0), 1);
    tick(ctx);
    tick(ctx);
    demo_expect(d, "typing in the input eaten", send(ctx, EV_KEY, 0, 0, 'x'), 1);
    tick(ctx);
    demo_expect(d, "the input got the text", (int)strlen(d->name) > 5 && d->name[strlen(d->name) - 1] == 'x', 1);
    send(ctx, EV_MOVE, 352, 322, 0);
    send(ctx, EV_DOWN, 352, 322, 0);
    send(ctx, EV_UP, 352, 322, 0);
    tick(ctx);
    tick(ctx);
    demo_expect(d, "key after unfocus not eaten", send(ctx, EV_KEY, 0, 0, 'q'), 0);
    tick(ctx);
}
