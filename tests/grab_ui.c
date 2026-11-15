#include <stdio.h>
#include <string.h>

#include "goome.h"

int main(int argc, char **argv)
{
    int secs = 0, win = argc > 2 && strstr(argv[2], "win"), self = argc > 2 && strstr(argv[2], "self"), err;
    for (const char *p = argc > 1 ? argv[1] : "12"; *p >= '0' && *p <= '9'; p++) secs = secs * 10 + *p - '0';
    goome *g = win ? goome_open_app("goome grab", 400, 300, &err) : goome_open(&(goome_opts){.title = "goome grab"}, &err);
    if (!g) {
        printf("cant open: %s\n", goome_strerror(err));
        return 1;
    }
    goome_ui *u = goome_ui_new(g);
    goome_ui_theme(u)->font = NULL, goome_ui_theme(u)->font_size = 14;
    goome_monitor m;
    goome_monitors(g, &m, 1);
    double x = m.area.x + (win ? 20 : 100), y = m.area.y + (win ? 20 : 100), w = 300, h = 0, val = 0;
    int grabbed = 0, wants = 0, shown = -1, quit = 0;
    printf("caps %x origin %d,%d\n", goome_caps(g), m.area.x, m.area.y);
    goome_pacer pc = {0};
    long end = goome_now_ms() + secs * 1000L;
    while (!quit && goome_now_ms() < end) {
        goome_event ev;
        int wait = goome_pace(&pc, 60);
        while (goome_poll(g, &ev, wait) > 0) {
            if (ev.type == GOOME_QUIT || ev.type == GOOME_CLOSE) quit = 1;
            goome_ui_feed(u, &ev);
            wait = 0;
        }
        if (!goome_ui_begin(u, 0)) continue;
        if (goome_ui_window(u, "grab", &x, &y, &w, &h, 0, NULL)) goome_ui_slider(u, "Val", &val, 0, 100, NULL);
        goome_ui_window_end(u);
        goome_ui_end(u);
        if (shown < 0) printf("ready\n");
        if (shown < 0 && self) {
            int e = goome_grab(g, 1);
            printf("self grab %d %s\n", goome_grabbed(g), goome_strerror(e));
            e = goome_grab(g, 0);
            printf("self ungrab %d %s\n", goome_grabbed(g), goome_strerror(e));
        }
        if (goome_grabbed(g) != grabbed) printf("grab %d\n", grabbed = goome_grabbed(g));
        if (goome_ui_wants(u) != wants) printf("wants %d\n", wants = goome_ui_wants(u));
        if ((int)(val + 0.5) != shown) printf("val %d\n", shown = (int)(val + 0.5));
        fflush(stdout);
    }
    goome_ui_free(u);
    printf("bye grab %d\n", goome_grabbed(g));
    goome_close(g);
    return 0;
}
