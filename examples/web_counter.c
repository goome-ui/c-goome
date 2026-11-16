#include <stdio.h>

#include "goome.h"

static goome *g;
static goome_ui *u;
static int clicks, bold;
static double volume = 0.4;
static char name[64] = "world";

int goome_web_init(void)
{
    int err;
    g = goome_open(&(goome_opts){.title = "Counter", .mode = GOOME_MODE_WINDOW, .width = 480, .height = 360, .min_width = 320, .min_height = 240,
                                 .window_flags = GOOME_WINDOW_RESIZABLE | GOOME_WINDOW_CENTER},
                   &err);
    if (!g) {
        fprintf(stderr, "cant open a window: %s\n", goome_strerror(err));
        return 1;
    }
    u = goome_ui_new(g);
    return u ? 0 : 1;
}

void goome_web_done(void)
{
    goome_ui_free(u);
    goome_close(g);
}

int goome_web_frame(void)
{
    goome_event ev;
    while (goome_poll(g, &ev, 16) > 0) {
        if (ev.type == GOOME_QUIT || ev.type == GOOME_CLOSE) return 0;
        goome_ui_feed(u, &ev);
    }
    if (!goome_ui_begin(u, 0)) return 1;
    char line[96], status[64];
    goome_ui_app(u, "Counter", GOOME_APP_TITLEBAR | GOOME_APP_STATUSBAR);
    snprintf(line, sizeof(line), "hello %s, %d clicks", name, clicks);
    goome_ui_label(u, "%s", line);
    if (goome_ui_button(u, "Click me")) clicks++;
    if (goome_ui_button(u, "Reset")) clicks = 0;
    goome_ui_checkbox(u, "Bold", &bold);
    goome_ui_slider(u, "Volume", &volume, 0, 1, "%.2f");
    goome_ui_progress(u, volume, NULL);
    goome_ui_input(u, "Name", name, sizeof(name));
    snprintf(status, sizeof(status), "%d clicks", clicks);
    goome_ui_statusbar(u, status);
    goome_ui_app_end(u);
    goome_ui_end(u);
    return 1;
}

int main(void)
{
    if (goome_web_init()) return 1;
    while (goome_web_frame()) {
    }
    goome_web_done();
    return 0;
}
