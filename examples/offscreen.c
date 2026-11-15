#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "goome.h"

typedef struct {
    int frame, type;
    double x, y;
    const char *text;
} step;

static void mouse(goome *g, int type, double x, double y)
{
    goome_event e;
    memset(&e, 0, sizeof(e));
    e.type = type, e.x = x, e.y = y, e.button = 1;
    goome_push(g, &e);
}

static void type_text(goome *g, const char *s)
{
    for (; *s; s++) {
        goome_event e;
        memset(&e, 0, sizeof(e));
        e.type = GOOME_KEY, e.key = (unsigned char)*s, e.text[0] = *s;
        goome_push(g, &e);
    }
}

static int save(goome *g, const char *path)
{
    goome_image f;
    if (goome_frame(g, &f) != GOOME_OK) return 1;
    uint32_t *out = malloc((size_t)f.w * f.h * 4);
    if (!out) return 1;
    for (int y = 0; y < f.h; y++)
        for (int x = 0; x < f.w; x++) {
            uint32_t s = f.px[(size_t)y * f.stride + x], a = s >> 24, v = 0xFF000000u;
            uint32_t bg = (x / 16 + y / 16) & 1 ? 0xFF3A4A5Au : 0xFF2E3C4Au;
            for (int k = 0; k < 24; k += 8) v |= (((s >> k) & 255) + ((bg >> k) & 255) * (255 - a) / 255) << k;
            out[(size_t)y * f.w + x] = v;
        }
    int e = goome_save_ppm(path, out, f.w, f.h, f.w);
    free(out);
    return e != GOOME_OK;
}

int main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "offscreen.ppm";
    int err;
    goome *g = goome_open(&(goome_opts){.mode = GOOME_MODE_OFFSCREEN, .width = 360, .height = 330, .scale = 2}, &err);
    if (!g) {
        fprintf(stderr, "cant open offscreen: %s\n", goome_strerror(err));
        return 1;
    }
    goome_ui *u = goome_ui_new(g);
    if (!u) {
        goome_close(g);
        return 1;
    }
    goome_ui_theme(u)->anim_ms = 0;
    static const step script[] = {
        {2, GOOME_DOWN, 40, 109, NULL}, {2, GOOME_UP, 40, 109, NULL},
        {3, GOOME_DOWN, 60, 163, NULL}, {4, GOOME_MOTION, 250, 163, NULL}, {5, GOOME_UP, 250, 163, NULL},
        {6, GOOME_DOWN, 150, 223, NULL}, {6, GOOME_UP, 150, 223, NULL}, {7, GOOME_KEY, 0, 0, " from an engine"},
        {8, GOOME_MOTION, 80, 279, NULL},
    };
    double wx = 20, wy = 20, ww = 320, wh = 0, volume = 0.25;
    int check = 0, clicks = 0;
    char name[64] = "goome";
    for (int frame = 0; frame < 10; frame++) {
        for (size_t i = 0; i < sizeof(script) / sizeof(script[0]); i++) {
            const step *s = &script[i];
            if (s->frame != frame) continue;
            if (s->type == GOOME_KEY) type_text(g, s->text);
            else mouse(g, s->type, s->x, s->y);
        }
        goome_event ev;
        while (goome_poll(g, &ev, 0) > 0) goome_ui_feed(u, &ev);
        if (!goome_ui_begin(u, 0)) continue;
        if (goome_ui_window(u, "offscreen", &wx, &wy, &ww, &wh, GOOME_WIN_MOVE, NULL)) {
            goome_ui_label(u, "no window, the engine owns the screen");
            goome_ui_checkbox(u, "Pushed a click", &check);
            goome_ui_slider(u, "Volume", &volume, 0, 1, "%.2f");
            goome_ui_input(u, "Name", name, sizeof(name));
            goome_ui_progress(u, volume, NULL);
            if (goome_ui_button(u, "Hover me")) clicks++;
        }
        goome_ui_window_end(u);
        goome_ui_end(u);
    }
    int bad = save(g, path);
    printf("check %d clicks %d volume %.2f name '%s' cursor %d -> %s\n", check, clicks, volume, name, goome_cursor_shape(g), bad ? "failed" : path);
    goome_ui_free(u);
    goome_close(g);
    return bad;
}
