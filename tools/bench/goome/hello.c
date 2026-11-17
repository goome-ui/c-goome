#define GOOME_IMPLEMENTATION
#include "goome.h"
#include "../bench_util.h"

int main(int argc, char **argv)
{
    const char *mode = argc > 1 ? argv[1] : "window";
    int off = !strcmp(mode, "offscreen");
    goome *g = off ? goome_open(&(goome_opts){.mode = GOOME_MODE_OFFSCREEN, .width = 640, .height = 400}, NULL)
                   : goome_open_app("hello", 640, 400, NULL);
    if (!g) return 1;
    goome_ui *u = goome_ui_new(g);
    int clicks = 0, ready = 0, spins = 0;
    double x = 40, y = 40, w = 300, h = 160;
    for (;;) {
        goome_event ev;
        while (goome_poll(g, &ev, 16) > 0) {
            if (ev.type == GOOME_QUIT || ev.type == GOOME_CLOSE) return 0;
            goome_ui_feed(u, &ev);
        }
        if (goome_ui_begin(u, 0)) {
            goome_ui_window(u, "Hello", &x, &y, &w, &h, GOOME_WIN_MOVE, NULL);
            goome_ui_label(u, "hello world");
            if (goome_ui_button(u, "Click me")) clicks++;
            goome_ui_label(u, "clicks: %d", clicks);
            goome_ui_window_end(u);
            goome_ui_end(u);
            if (!ready) {
                ready = 1;
                bench_ready();
            }
        }
        if (!ready && ++spins > 600) return 2;
        if (off && ready) bench_sleep_ms(50);
    }
}
