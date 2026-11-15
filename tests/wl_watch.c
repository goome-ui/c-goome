// test helper: a box on every monitor, prints every event. control lines (goome_send to state dir):
// grab <mon> [x y w h] <file>, empty, draw, kb 0|1, quit
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "goome.h"

static void list(goome *g)
{
    goome_monitor m[16];
    int n = goome_monitors(g, m, 16);
    printf("monitors %d\n", n);
    for (int i = 0; i < n && i < 16; i++)
        printf("mon %d id %d %s %dx%d+%d+%d scale %.2f px %dx%d rot %d\n", i, m[i].id, m[i].name, m[i].area.w, m[i].area.h, m[i].area.x,
               m[i].area.y, m[i].scale, m[i].pw, m[i].ph, m[i].rotation);
}

int main(int argc, char **argv)
{
    int err;
    goome *g = goome_open(&(goome_opts){.title = "goome watch", .state_dir = argc > 1 ? argv[1] : NULL}, &err);
    if (!g) {
        printf("open failed: %s\n", goome_strerror(err));
        return 1;
    }
    printf("backend: %s caps %x%s\n", goome_backend_name(g), goome_caps(g), goome_caps(g) & GOOME_CAP_GLOBAL_POINTER ? " global pointer" : "");
    list(g);
    fflush(stdout);
    int running = 1, empty = 0, lastb = 0, motions = 0;
    goome_pacer pace = {0};
    while (running) {
        goome_event ev;
        int wait = goome_pace(&pace, 30);
        while (goome_poll(g, &ev, wait) > 0) {
            switch (ev.type) {
            case GOOME_QUIT: running = 0; break;
            case GOOME_DOWN: printf("down %d %.0f,%.0f mon %d\n", ev.button, ev.x, ev.y, ev.mon); break;
            case GOOME_UP: printf("up %d %.0f,%.0f mon %d\n", ev.button, ev.x, ev.y, ev.mon); break;
            case GOOME_MOTION: if (motions++ < 400) printf("motion %.0f,%.0f mon %d\n", ev.x, ev.y, ev.mon); break;
            case GOOME_SCROLL: printf("scroll %d %.0f,%.0f\n", ev.dy, ev.x, ev.y); break;
            case GOOME_KEY: printf("key %d mods %x text '%s'\n", ev.key, ev.mods, ev.text); break;
            case GOOME_MONITORS: list(g); break;
            case GOOME_CONTROL: {
                printf("ctl %s\n", ev.msg);
                const char *p = ev.msg;
                if (!strcmp(p, "quit")) running = 0;
                else if (!strcmp(p, "empty")) empty = 1;
                else if (!strcmp(p, "draw")) empty = 0;
                else if (!strncmp(p, "kb ", 3)) printf("keyboard %d\n", goome_keyboard(g, p[3] == '1'));
                else if (!strncmp(p, "grab ", 5)) {
                    int v[5], n = 0;
                    p += 5;
                    while (n < 5 && *p >= '0' && *p <= '9') {
                        v[n] = 0;
                        while (*p >= '0' && *p <= '9') v[n] = v[n] * 10 + *p++ - '0';
                        n++;
                        while (*p == ' ') p++;
                    }
                    goome_rect r = {v[1], v[2], v[3], v[4]};
                    goome_image img;
                    int e = goome_capture(g, v[0], n == 5 ? &r : NULL, &img);
                    if (e == GOOME_OK) e = goome_save_ppm(p, img.px, img.w, img.h, img.stride);
                    printf("grabbed %d: %dx%d of %d,%d %dx%d self %d cursor %d (%s)\n", e, img.w, img.h, img.area.x, img.area.y, img.area.w,
                           img.area.h, img.has_self, img.has_cursor, e ? goome_error(g) : "ok");
                }
                break;
            }
            }
            fflush(stdout);
            wait = goome_pace(&pace, 30);
        }
        double px, py;
        int b = 0;
        if (goome_pointer(g, &px, &py, &b) && b != lastb) printf("held %d at %.0f,%.0f\n", b, px, py), fflush(stdout);
        lastb = b;
        goome_monitor m[16];
        int n = goome_monitors(g, m, 16);
        for (int i = 0; i < n && i < 16; i++) {
            goome_canvas *c = goome_begin(g, i);
            if (!c) continue;
            goome_rect hit = {m[i].area.x + 80, m[i].area.y + 80, 280, 120};
            if (!empty) goome_draw_rect(c, hit.x, hit.y, hit.w, hit.h, 0xFF3366CC);
            goome_end(g, c, &hit, empty ? 0 : 1);
        }
    }
    goome_close(g);
    return 0;
}
