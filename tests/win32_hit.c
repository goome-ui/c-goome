// hit rect bigger than what is drawn: clicks on the empty part have to come here. prints clicks for a few seconds
#include <stdio.h>

#include "goome.h"

int main(int argc, char **argv)
{
    int err, secs = argc > 1 ? argv[1][0] - '0' : 8;
    goome *g = goome_open(&(goome_opts){.title = "goome hit"}, &err);
    if (!g) {
        printf("cant open: %s\n", goome_strerror(err));
        return 1;
    }
    goome_monitor m;
    goome_monitors(g, &m, 1);
    double x = m.area.x + 80, y = m.area.y + 80;
    goome_pacer p = {0};
    long end = goome_now_ms() + secs * 1000L;
    int said = 0;
    while (goome_now_ms() < end) {
        goome_event ev;
        int wait = goome_pace(&p, 30);
        while (wait > 0 && goome_poll(g, &ev, wait) > 0) {
            if (ev.type == GOOME_DOWN) printf("click %.0f,%.0f\n", ev.x - m.area.x, ev.y - m.area.y), fflush(stdout);
            wait = goome_pace(&p, 30);
        }
        goome_canvas *c = goome_begin(g, 0);
        if (!c) continue;
        goome_draw_rect(c, x, y, 40, 40, 0xFF20C040);
        goome_rect hit = {(int)x, (int)y, 300, 200};
        goome_end(g, c, &hit, 1);
        if (!said++) printf("alpha at the empty spot %d, on the box %d\n", goome_alpha_at(g, x + 220, y + 120), goome_alpha_at(g, x + 20, y + 20)), fflush(stdout);
    }
    goome_close(g);
    return 0;
}
