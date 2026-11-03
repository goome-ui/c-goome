// hide_self check for tests/win32.ps1: draws a magenta box, grabs it, says if the grab saw it
#include <stdio.h>

#include "goome.h"

int main(void)
{
    int err;
    goome *g = goome_open(&(goome_opts){.title = "goome hide", .hide_self = 1}, &err);
    if (!g) {
        printf("cant open: %s\n", goome_strerror(err));
        return 1;
    }
    goome_monitor m;
    goome_monitors(g, &m, 1);
    double x = m.area.x + 300, y = m.area.y + 300;
    goome_pacer p = {0};
    long end = goome_now_ms() + 800;
    while (goome_now_ms() < end) {
        goome_event ev;
        int wait = goome_pace(&p, 30);
        while (wait > 0 && goome_poll(g, &ev, wait) > 0) wait = goome_pace(&p, 30);
        goome_canvas *c = goome_begin(g, 0);
        if (!c) continue;
        goome_draw_rect(c, x, y, 100, 100, 0xFFFF00FF);
        goome_end(g, c, NULL, 0);
    }
    goome_image img;
    goome_rect r = {(int)x + 40, (int)y + 40, 20, 20};
    if (goome_capture(g, 0, &r, &img) != GOOME_OK) {
        printf("grab failed: %s\n", goome_error(g));
        goome_close(g);
        return 1;
    }
    unsigned px = img.px[(img.h / 2) * img.stride + img.w / 2];
    printf("caps %d hide %d has_self %d center %06x %s\n", goome_caps(g), (goome_caps(g) & GOOME_CAP_HIDE_SELF) != 0, img.has_self, px & 0xFFFFFF,
           (px & 0xFFFFFF) == 0xFF00FF ? "magenta" : "not magenta");
    goome_close(g);
    return 0;
}
