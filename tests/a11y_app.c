#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "goome.h"

int main(int argc, char **argv)
{
    static const char *const fruits[] = {"Mango", "Kiwi", "Fig"};
    long secs = argc > 1 ? atol(argv[1]) : 30;
    int err;
    goome *g = goome_open_app("goome a11y", 420, 360, &err);
    if (!g) {
        printf("no window: %s\n", goome_strerror(err));
        return 1;
    }
    goome_ui *u = goome_ui_new(g);
    int dark = 0, fruit = 0, presses = 0, running = 1;
    double vol = 3;
    char name[64] = "Bo";
    long end = goome_now_ms() + secs * 1000;
    printf("ready %s\n", goome_backend_name(g));
    fflush(stdout);
    while (running && goome_now_ms() < end) {
        goome_event ev;
        while (goome_poll(g, &ev, 16) > 0) {
            if (ev.type == GOOME_QUIT || ev.type == GOOME_CLOSE) running = 0;
            goome_ui_feed(u, &ev);
        }
        if (!goome_ui_begin(u, 0)) continue;
        goome_ui_app(u, "goome a11y", 0);
        goome_ui_label(u, "Screen reader test");
        if (goome_ui_button(u, "Press me")) printf("pressed %d\n", ++presses);
        if (goome_ui_checkbox(u, "Dark mode", &dark)) printf("dark %d\n", dark);
        if (goome_ui_slider(u, "Volume", &vol, 0, 10, "%.1f")) printf("volume %.1f\n", vol);
        if (goome_ui_input(u, "Name", name, sizeof(name))) printf("name %s\n", name);
        goome_ui_combo(u, "Fruit", &fruit, fruits, 3);
        if (goome_ui_button(u, "Quit")) running = 0;
        goome_ui_app_end(u);
        goome_ui_end(u);
        fflush(stdout);
    }
    printf("bye fruit %d\n", fruit);
    goome_ui_free(u);
    goome_close(g);
    return 0;
}
