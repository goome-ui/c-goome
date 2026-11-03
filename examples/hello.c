// draws a box you can click, everything else on screen still works under it
// alt+h hides it, click "type" to type into it, esc or the x quits
#include <stdio.h>
#include <string.h>

#include "goome.h"

int main(void)
{
    int err;
    goome *g = goome_open(&(goome_opts){.title = "goome hello"}, &err);
    if (!g) {
        fprintf(stderr, "cant open an overlay: %s\n", goome_strerror(err));
        return 1;
    }
    goome_monitor mons[16];
    int n = goome_monitors(g, mons, 16);
    printf("backend: %s\n", goome_backend_name(g));
    for (int i = 0; i < n; i++)
        printf("monitor %d: %s %dx%d at %d,%d scale %.2f\n", i, mons[i].name, mons[i].area.w, mons[i].area.h, mons[i].area.x, mons[i].area.y, mons[i].scale);
    if (goome_hotkey(g, "alt+h", 1) != GOOME_OK) printf("no global hotkeys here: %s\n", goome_error(g));
    fflush(stdout);

    double bx = mons[0].area.x + 80, by = mons[0].area.y + 80;
    int clicks = 0, hidden = 0, typing = 0, running = 1;
    char typed[40] = "";
    goome_pacer pace = {0};
    while (running) {
        goome_event ev;
        int wait = goome_pace(&pace, 30);
        while (goome_poll(g, &ev, wait) > 0) {
            if (ev.type == GOOME_QUIT) running = 0;
            if (ev.type == GOOME_DOWN) {
                printf("click %.0f,%.0f\n", ev.x, ev.y);
                if (ev.x > bx + 250 && ev.y < by + 30) running = 0;
                else if (ev.y > by + 80) typing = !typing, goome_keyboard(g, typing);
                else clicks++;
            }
            if (ev.type == GOOME_KEY) {
                printf("key %d mods %x text '%s'\n", ev.key, ev.mods, ev.text);
                size_t l = strlen(typed);
                if (ev.key == GOOME_K_ESC) running = 0;
                else if (ev.key == GOOME_K_BACKSPACE && l) typed[l - 1] = 0;
                else if (ev.text[0]) snprintf(typed + l, sizeof(typed) - l, "%s", ev.text);
            }
            if (ev.type == GOOME_HOTKEY || (ev.type == GOOME_CONTROL && !strcmp(ev.msg, "hide"))) hidden = !hidden;
            if (ev.type == GOOME_SCROLL) by += ev.dy * 10;
            fflush(stdout);
            wait = goome_pace(&pace, 30);
        }
        goome_canvas *c = goome_begin(g, 0);
        if (!c) continue;
        goome_rect hit = {(int)bx, (int)by, 280, 120};
        if (!hidden) {
            char line[64];
            goome_draw_round_rect(c, bx, by, 280, 120, 14, 0xE0202838);
            goome_draw_frame(c, bx, by, 280, 120, 2, clicks & 1 ? 0xFF6FDC5F : 0xFF8090A0);
            goome_draw_text(c, bx + 16, by + 12, "hello from goome", 1, 0xFFFFFFFF);
            goome_draw_line(c, bx + 256, by + 10, bx + 268, by + 22, 2, 0xFFFF6B8B);
            goome_draw_line(c, bx + 268, by + 10, bx + 256, by + 22, 2, 0xFFFF6B8B);
            snprintf(line, sizeof(line), "clicks: %d", clicks);
            goome_draw_text(c, bx + 16, by + 46, line, 0, 0xFFC0C8D0);
            goome_draw_round_rect(c, bx + 12, by + 80, 256, 28, 8, typing ? 0xFF2E4A30 : 0xFF303848);
            goome_draw_text(c, bx + 22, by + 86, typing ? (typed[0] ? typed : "type something...") : "click here to type", 0, 0xFFFFFFFF);
        }
        goome_end(g, c, &hit, hidden ? 0 : 1);
    }
    goome_close(g);
    return 0;
}
