#include <stdio.h>
#include <string.h>

#include "goome.h"

int main(void)
{
    int err;
    setvbuf(stdout, NULL, _IOLBF, 0);
    goome *g = goome_open_app("goome ime", 360, 160, &err);
    if (!g) {
        printf("open failed: %s\n", goome_strerror(err));
        return 1;
    }
    goome_ui *u = goome_ui_new(g);
    char buf[128] = "", was[128] = "";
    int frame = 0;
    for (;;) {
        goome_event ev;
        while (goome_poll(g, &ev, frame < 3 ? 0 : 30) > 0) {
            if (ev.type == GOOME_QUIT || ev.type == GOOME_CLOSE) {
                goome_ui_free(u);
                goome_close(g);
                return 0;
            }
            if (ev.type == GOOME_TEXT) printf("text %s\n", goome_event_text(g));
            if (ev.type == GOOME_PREEDIT) printf("preedit '%s' %d\n", goome_event_text(g), goome_preedit_cursor(g));
            if (ev.type == GOOME_KEY && ev.text[0]) printf("key %s\n", ev.text);
            goome_ui_feed(u, &ev);
        }
        if (!goome_ui_begin(u, 0)) continue;
        if (!frame) goome_ui_set_focus(u);
        goome_ui_input(u, "type here", buf, sizeof(buf));
        goome_ui_end(u);
        if (strcmp(buf, was)) printf("buf %s\n", buf), snprintf(was, sizeof(was), "%s", buf);
        if (++frame == 3) printf("ime rc %d %s\n", goome_text_input(g, 1, &(goome_rect){20, 40, 1, 18}), goome_error(g));
    }
}
