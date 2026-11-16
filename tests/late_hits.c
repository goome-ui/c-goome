#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "goome.h"

int main(int argc, char **argv)
{
    int err;
    setvbuf(stdout, NULL, _IOLBF, 0);
    goome *g = goome_open(&(goome_opts){.title = "goome late", .no_capture = 1}, &err);
    if (!g) {
        printf("open failed: %s\n", goome_strerror(err));
        return 1;
    }
    long late = argc > 1 && strcmp(argv[1], "key") ? atol(argv[1]) : -1, t0 = goome_now_ms();
    printf("hotkey %d\n", goome_hotkey(g, argc > 2 ? argv[2] : "ctrl+shift+f6", 1));
    int menu = 0, said = 0, ready = 0;
    goome_rect box = {300, 200, 200, 120};
    for (;;) {
        goome_event ev;
        while (goome_poll(g, &ev, 30) > 0) {
            if (ev.type == GOOME_QUIT) {
                goome_close(g);
                return 0;
            }
            if (ev.type == GOOME_DOWN) printf("down %.0f,%.0f\n", ev.x, ev.y);
            if (ev.type == GOOME_HOTKEY) printf("hotkey id %d\n", ev.id), menu = !menu;
        }
        goome_canvas *c = goome_begin(g, 0);
        if (!c) continue;
        goome_clear(c);
        goome_draw_rect(c, box.x, box.y, box.w, box.h, 0xff3060c0);
        int hits = late < 0 ? menu : goome_now_ms() - t0 >= late;
        if (hits != said) printf("hits %s\n", hits ? "on" : "off"), said = hits;
        goome_end(g, c, &box, hits);
        if (!ready) ready = 1, printf("ready\n");
    }
}
