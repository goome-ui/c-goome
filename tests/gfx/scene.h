#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "goome.h"

#define SCENE_CLEAR 0xFF336699u
#define SCENE_W 480
#define SCENE_H 420

typedef struct {
    goome *g;
    goome_ui *u;
    double wx, wy, ww, wh, volume;
    int check, clicks, bad;
    char name[64];
} scene;

static void scene_mouse(goome *g, int type, double x, double y)
{
    goome_event e;
    memset(&e, 0, sizeof(e));
    e.type = type, e.x = x, e.y = y, e.button = 1;
    goome_push(g, &e);
}

static int scene_open(scene *s)
{
    int err;
    memset(s, 0, sizeof(*s));
    s->g = goome_open(&(goome_opts){.mode = GOOME_MODE_OFFSCREEN, .width = 360, .height = 330, .scale = 1}, &err);
    if (!s->g) {
        fprintf(stderr, "cant open offscreen: %s\n", goome_strerror(err));
        return 1;
    }
    s->u = goome_ui_new(s->g);
    if (!s->u) return 1;
    goome_ui_theme(s->u)->anim_ms = 0;
    s->wx = 20, s->wy = 20, s->ww = 320, s->volume = 0.25;
    strcpy(s->name, "goome");
    return 0;
}

static void scene_frame(scene *s, int frame)
{
    goome *g = s->g;
    if (frame == 2) scene_mouse(g, GOOME_DOWN, 40, 109), scene_mouse(g, GOOME_UP, 40, 109);
    if (frame == 3) scene_mouse(g, GOOME_DOWN, 60, 163);
    if (frame == 4) scene_mouse(g, GOOME_MOTION, 250, 163);
    if (frame == 5) scene_mouse(g, GOOME_UP, 250, 163);
    if (frame == 6) scene_mouse(g, GOOME_MOTION, 80, 279);
    if (frame == 10) scene_mouse(g, GOOME_DOWN, 120, 30);
    if (frame == 11) scene_mouse(g, GOOME_MOTION, 60, 60);
    if (frame == 12) scene_mouse(g, GOOME_UP, 60, 60);
    goome_event ev;
    while (goome_poll(g, &ev, 0) > 0) goome_ui_feed(s->u, &ev);
    if (!goome_ui_begin(s->u, 0)) return;
    if (goome_ui_window(s->u, "renderer", &s->wx, &s->wy, &s->ww, &s->wh, GOOME_WIN_MOVE, NULL)) {
        goome_ui_label(s->u, "drawn by the host's own gpu api");
        goome_ui_checkbox(s->u, "Pushed a click", &s->check);
        goome_ui_slider(s->u, "Volume", &s->volume, 0, 1, "%.2f");
        goome_ui_input(s->u, "Name", s->name, sizeof(s->name));
        goome_ui_progress(s->u, s->volume, NULL);
        if (goome_ui_button(s->u, "Hover me")) s->clicks++;
    }
    goome_ui_window_end(s->u);
    goome_ui_end(s->u);
}

static int scene_check(scene *s, const unsigned char *px, int pitch, int rgba, const char *tag)
{
    goome_image f;
    if (goome_frame(s->g, &f) != GOOME_OK) {
        printf("%s: no frame\n", tag);
        return s->bad = 1;
    }
    int worst = 0, wx = 0, wy = 0, set = 0;
    for (int y = 0; y < SCENE_H; y++)
        for (int x = 0; x < SCENE_W; x++) {
            uint32_t p = x < f.w && y < f.h ? f.px[(size_t)y * f.stride + x] : 0, a = p >> 24;
            const unsigned char *q = px + (size_t)y * pitch + x * 4;
            for (int k = 0; k < 4; k++) {
                int sh = k == 3 ? 24 : rgba ? 16 - k * 8 : k * 8;
                uint32_t sv = (p >> sh) & 255, dv = (SCENE_CLEAR >> sh) & 255;
                int want = (int)(sv + (dv * (255 - a) + 127) / 255), diff = abs(want - q[k]);
                if (diff > worst) worst = diff, wx = x, wy = y;
            }
            if (p >> 24) set++;
        }
    int bad = worst > 2 || set < 1000;
    printf("%s: %dx%d frame, %d covered px, worst diff %d at %d,%d %s\n", tag, f.w, f.h, set, worst, wx, wy, bad ? "FAIL" : "ok");
    if (bad) s->bad = 1;
    return bad;
}

static void scene_close(scene *s)
{
    if (s->u) goome_ui_free(s->u);
    if (s->g) goome_close(s->g);
}
