// tests/x11.sh runs this, box on every monitor and it prints whatever it gets
#include <stdio.h>
#include <string.h>

#include "goome.h"

static goome_monitor mons[16];
static int nmon;

static void list(goome *g)
{
    nmon = goome_monitors(g, mons, 16);
    printf("monitors %d\n", nmon);
    for (int i = 0; i < nmon; i++)
        printf("mon %d %s %d,%d %dx%d scale %.2f px %dx%d\n", i, mons[i].name, mons[i].area.x, mons[i].area.y, mons[i].area.w, mons[i].area.h,
               mons[i].scale, mons[i].pw, mons[i].ph);
}

int main(void)
{
    int err;
    goome *g = goome_open(&(goome_opts){.title = "goome probe"}, &err);
    if (!g) {
        printf("open failed: %s\n", goome_strerror(err));
        return 1;
    }
    printf("backend %s caps %x\n", goome_backend_name(g), goome_caps(g));
    list(g);
    printf("hotkey %d\n", goome_hotkey(g, "ctrl+shift+f5", 7));
    fflush(stdout);
    double lx = -1, ly = -1;
    int lb = -1, running = 1;
    goome_pacer pace = {0};
    while (running) {
        goome_event ev;
        int wait = goome_pace(&pace, 30);
        while (goome_poll(g, &ev, wait) > 0) {
            switch (ev.type) {
            case GOOME_QUIT: running = 0; break;
            case GOOME_DOWN: printf("down %.0f,%.0f mon %d button %d\n", ev.x, ev.y, ev.mon, ev.button); break;
            case GOOME_UP: printf("up %.0f,%.0f button %d\n", ev.x, ev.y, ev.button); break;
            case GOOME_MOTION: printf("motion %.0f,%.0f\n", ev.x, ev.y); break;
            case GOOME_SCROLL: printf("scroll %d at %.0f,%.0f\n", ev.dy, ev.x, ev.y); break;
            case GOOME_KEY: printf("key %d mods %x text '%s'\n", ev.key, ev.mods, ev.text); break;
            case GOOME_HOTKEY: printf("hotkey id %d\n", ev.id); break;
            case GOOME_MONITORS: list(g); break;
            }
            if (ev.type == GOOME_DOWN && ev.button == 3) printf("keyboard %d\n", goome_keyboard(g, 1));
            if (ev.type == GOOME_KEY && ev.key == GOOME_K_ESC) printf("keyboard %d\n", goome_keyboard(g, 0));
            if (ev.type == GOOME_KEY && ev.key == 'q') running = 0;
            fflush(stdout);
            wait = goome_pace(&pace, 30);
        }
        double x, y;
        int b;
        if (goome_pointer(g, &x, &y, &b) && (x != lx || y != ly || b != lb)) {
            printf("ptr %.0f,%.0f %d\n", x, y, b);
            fflush(stdout);
            lx = x, ly = y, lb = b;
        }
        for (int m = 0; m < nmon; m++) {
            goome_canvas *c = goome_begin(g, m);
            if (!c) continue;
            goome_rect hit = {mons[m].area.x + 80, mons[m].area.y + 80, 200, 100};
            goome_draw_rect(c, hit.x, hit.y, hit.w, hit.h, 0xFF00C000);
            goome_end(g, c, &hit, 1);
        }
    }
    goome_close(g);
    return 0;
}
