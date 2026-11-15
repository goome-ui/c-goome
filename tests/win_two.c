// two windows in one process, each its own goome. prints what each gets, quits when both got closed
#include <stdio.h>

#include "goome.h"

int main(void)
{
    goome *g[2];
    const char *names[2] = {"goome one", "goome two"};
    uint32_t col[2] = {0xFFC03030, 0xFF30A040};
    for (int i = 0; i < 2; i++) {
        int err;
        g[i] = goome_open(&(goome_opts){.title = names[i], .mode = GOOME_MODE_WINDOW, .width = 300 + i * 40, .height = 200}, &err);
        if (!g[i]) {
            printf("open %d failed: %s\n", i, goome_strerror(err));
            return 1;
        }
        goome_monitor m;
        goome_monitors(g[i], &m, 1);
        printf("window %d %s %dx%d\n", i, m.name, m.area.w, m.area.h);
    }
    fflush(stdout);
    int open[2] = {1, 1}, dirty[2] = {1, 1};
    while (open[0] || open[1]) {
        for (int i = 0; i < 2; i++) {
            if (!open[i]) continue;
            goome_event ev;
            while (goome_poll(g[i], &ev, 10) > 0) {
                if (ev.type == GOOME_CLOSE || ev.type == GOOME_QUIT) {
                    printf("close %d\n", i);
                    open[i] = 0;
                    goome_close(g[i]);
                    break;
                }
                if (ev.type == GOOME_MONITORS) dirty[i] = 1;
                if (ev.type == GOOME_DOWN) printf("down %d %.0f,%.0f\n", i, ev.x, ev.y);
            }
            fflush(stdout);
            if (!open[i] || !dirty[i]) continue;
            goome_canvas *c = goome_begin(g[i], 0);
            if (!c) continue;
            goome_draw_rect(c, 0, 0, 400, 400, col[i]);
            goome_end(g[i], c, NULL, 0);
            dirty[i] = 0;
        }
    }
    return 0;
}
