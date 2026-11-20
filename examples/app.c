// a plain window, no ui kit, just the draw api. drag the top bar to move it, the corner to resize it
// drop files on it, ctrl+c / ctrl+v or the buttons for the clipboard, hover the boxes for cursors
#include <stdio.h>
#include <string.h>

#include "goome.h"

static char lines[8][96];
static int nlines;

static void say(const char *fmt, const char *s)
{
    char buf[96];
    snprintf(buf, sizeof(buf), fmt, s);
    printf("%s\n", buf);
    fflush(stdout);
    if (nlines == 8) memmove(lines, lines + 1, sizeof(lines[0]) * 7), nlines--;
    snprintf(lines[nlines++], sizeof(lines[0]), "%s", buf);
}

static int in(double x, double y, double rx, double ry, double rw, double rh) { return x >= rx && y >= ry && x < rx + rw && y < ry + rh; }

static const char *cur_names[] = {"ibeam", "hand", "move", "wait", "hidden"};
static const int cur_shapes[] = {GOOME_CURSOR_IBEAM, GOOME_CURSOR_HAND, GOOME_CURSOR_MOVE, GOOME_CURSOR_WAIT, GOOME_CURSOR_HIDDEN};

int main(int argc, char **argv)
{
    int flags = GOOME_WINDOW_RESIZABLE | GOOME_WINDOW_CENTER;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "borderless")) flags |= GOOME_WINDOW_BORDERLESS;
        if (!strcmp(argv[i], "--no-console")) flags |= GOOME_WINDOW_NO_CONSOLE;
        if (!strcmp(argv[i], "transparent")) flags |= GOOME_WINDOW_TRANSPARENT;
    }
    int small = argc > 1 && !strcmp(argv[argc - 1], "small");
    int err;
    goome *g = goome_open(&(goome_opts){.title = "goome app", .mode = GOOME_MODE_WINDOW, .width = small ? 480 : 640, .height = small ? 320 : 420,
                                        .min_width = 360, .min_height = 260, .window_flags = flags},
                          &err);
    if (!g) {
        fprintf(stderr, "cant open a window: %s\n", goome_strerror(err));
        return 1;
    }
    goome_font *font = goome_font_system(NULL);
    goome_monitor m;
    goome_monitors(g, &m, 1);
    printf("backend %s caps %x\n", goome_backend_name(g), goome_caps(g));
    printf("window %dx%d scale %.2f px %dx%d\n", m.area.w, m.area.h, m.scale, m.pw, m.ph);
    fflush(stdout);

    char clip[256] = "nothing pasted yet";
    int running = 1, dirty = 1, cur = GOOME_CURSOR_ARROW, focused = 1, copies = 0;
    double mx = -1, my = -1;
    while (running) {
        goome_event ev;
        int wait = dirty ? 0 : 1000;
        while (goome_poll(g, &ev, wait) > 0) {
            wait = 0, dirty = 1;
            double w = m.area.w, h = m.area.h;
            switch (ev.type) {
            case GOOME_QUIT: running = 0; break;
            case GOOME_CLOSE: say("%s", "close"), running = 0; break;
            case GOOME_MONITORS:
                goome_monitors(g, &m, 1);
                printf("resized %dx%d scale %.2f px %dx%d\n", m.area.w, m.area.h, m.scale, m.pw, m.ph);
                fflush(stdout);
                break;
            case GOOME_FOCUS: focused = ev.id, say("%s", ev.id ? "focus in" : "focus out"); break;
            case GOOME_DROP: say("dropped %s", goome_drop_path(g)); break;
            case GOOME_MOTION: mx = ev.x, my = ev.y; break;
            case GOOME_DOWN:
                printf("down %.0f,%.0f button %d\n", ev.x, ev.y, ev.button);
                fflush(stdout);
                if (in(ev.x, ev.y, 0, 0, w, 36)) goome_window_drag(g, 0);
                else if (in(ev.x, ev.y, w - 20, h - 20, 20, 20)) goome_window_drag(g, GOOME_EDGE_RIGHT | GOOME_EDGE_BOTTOM);
                else if (in(ev.x, ev.y, 20, 196, 110, 32)) ev.type = GOOME_KEY, ev.key = 'c', ev.mods = GOOME_CTRL;
                else if (in(ev.x, ev.y, 140, 196, 110, 32)) ev.type = GOOME_KEY, ev.key = 'v', ev.mods = GOOME_CTRL;
                break;
            }
            if (ev.type == GOOME_KEY) {
                if (ev.key == GOOME_K_ESC) running = 0;
                if ((ev.key == 'm' || ev.key == GOOME_K_F1 + 10) && !ev.mods) {
                    int st = goome_window_get_state(g), to = ev.key == 'm' ? GOOME_STATE_MAXIMIZED : GOOME_STATE_FULLSCREEN;
                    goome_window_state(g, st == to ? GOOME_STATE_NORMAL : to);
                }
                if (ev.key == 's' && !ev.mods) {
                    int x = 0, y = 0;
                    goome_window_pos(g, &x, &y, 0);
                    printf("state %d pos %d,%d\n", goome_window_get_state(g), x, y);
                }
                if (ev.key == 'r' && !ev.mods) goome_window_size(g, 500, 360);
                if (ev.key == 'c' && ev.mods == GOOME_CTRL) {
                    char t[64];
                    snprintf(t, sizeof(t), "copied from goome %d", ++copies);
                    say(goome_clipboard_set(g, t) == GOOME_OK ? "copy '%s'" : "copy failed %s", t);
                }
                if (ev.key == 'v' && ev.mods == GOOME_CTRL) {
                    int n = goome_clipboard_get(g, clip, sizeof(clip));
                    if (n < 0) snprintf(clip, sizeof(clip), "paste failed: %s", goome_error(g));
                    say("paste '%s'", clip);
                    printf("pasted %d bytes\n", n);
                }
            }
            fflush(stdout);
        }
        double w = m.area.w, h = m.area.h;
        int want = GOOME_CURSOR_ARROW;
        for (int i = 0; i < 5; i++)
            if (in(mx, my, 20 + i * 100, 256, 90, 40)) want = cur_shapes[i];
        if (in(mx, my, w - 20, h - 20, 20, 20)) want = GOOME_CURSOR_RESIZE_NWSE;
        if (want != cur) cur = want, goome_cursor(g, cur), printf("cursor %d\n", cur), fflush(stdout);
        if (!dirty) continue;
        dirty = 0;
        goome_canvas *c = goome_begin(g, 0);
        if (!c) continue;
        double r = flags & GOOME_WINDOW_TRANSPARENT ? 14 : 0;
        goome_draw_round_rect(c, 0, 0, w, h, r, r ? 0xC0181A20 : 0xFF181A20);
        goome_draw_rrect(c, 0, 0, w, 36, (goome_radii){r, r, 0, 0}, focused ? 0xFF262A34 : 0xFF1F222A);
        goome_text(c, font, 15, 14, 9, "goome app, drag here to move", focused ? 0xFFE8EAF0 : 0xFF8A90A0);
        char size[64];
        snprintf(size, sizeof(size), "%dx%d at %.2fx", m.area.w, m.area.h, m.scale);
        goome_text(c, font, 13, w - 14 - goome_text_measure(font, 13, size, NULL), 11, size, 0xFF8A90A0);
        goome_text(c, font, 14, 20, 52, "drop files, copy and paste, hover the boxes", 0xFFB8BECC);
        goome_draw_round_rect(c, 20, 76, w - 40, 108, 8, 0xFF20232B);
        for (int i = 0; i < nlines && i < 6; i++)
            goome_text(c, font, 13, 30, 84 + i * 16, lines[nlines - 1 - i], i ? 0xFF8A90A0 : 0xFFE8EAF0);
        goome_draw_round_rect(c, 20, 196, 110, 32, 8, in(mx, my, 20, 196, 110, 32) ? 0xFF4A7CF0 : 0xFF3A62C8);
        goome_text(c, font, 14, 52, 204, "Copy", 0xFFFFFFFF);
        goome_draw_round_rect(c, 140, 196, 110, 32, 8, in(mx, my, 140, 196, 110, 32) ? 0xFF3A3F4C : 0xFF2C303A);
        goome_text(c, font, 14, 172, 204, "Paste", 0xFFFFFFFF);
        goome_text(c, font, 13, 264, 205, clip, 0xFFB8BECC);
        for (int i = 0; i < 5; i++) {
            int on = cur == cur_shapes[i];
            goome_draw_round_rect(c, 20 + i * 100, 256, 90, 40, 8, on ? 0xFF2E5C3A : 0xFF262A34);
            goome_text(c, font, 13, 34 + i * 100, 268, cur_names[i], 0xFFE8EAF0);
        }
        goome_draw_rrect_line(c, 0, 0, w, h, goome_radius(r), 1, 0xFF3A62C8);
        for (int i = 0; i < 3; i++) goome_draw_line(c, w - 6 - i * 4, h - 3, w - 3, h - 6 - i * 4, 1.2, 0xFF6A7080);
        goome_end(g, c, NULL, 0);
    }
    goome_font_free(font);
    goome_close(g);
    return 0;
}
