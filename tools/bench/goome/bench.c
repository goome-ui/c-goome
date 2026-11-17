#define GOOME_IMPLEMENTATION
#include "goome.h"
#include "../bench_util.h"

static void scene(goome_ui *u, int dash, double *vals, int *checks)
{
    static double x = 0, y = 0, w = 960, h = 640;
    goome_ui_window(u, "bench", &x, &y, &w, &h, 0, NULL);
    if (!dash) {
        goome_ui_label(u, "hello world");
        goome_ui_button(u, "Click me");
        goome_ui_label(u, "clicks: %d", 0);
    } else {
        for (int i = 0; i < BENCH_ROWS; i++) {
            char id[16];
            snprintf(id, sizeof id, "r%d", i);
            goome_ui_push_id(u, id);
            goome_ui_label(u, "row %d", i);
            goome_ui_same_line(u);
            goome_ui_button(u, "Open");
            goome_ui_same_line(u);
            goome_ui_checkbox(u, "On", &checks[i]);
            goome_ui_same_line(u);
            goome_ui_slider(u, "Value", &vals[i], 0, 1, "%.2f");
            goome_ui_pop_id(u);
        }
    }
    goome_ui_window_end(u);
}

static double run(goome *g, goome_ui *u, int dash, int frames, int idle, double *vals, int *checks)
{
    for (int i = 0; i < 20; i++)
        if (goome_ui_begin(u, 0)) {
            scene(u, dash, vals, checks);
            goome_ui_end(u);
        }
    double t0 = bench_now_ms();
    int drawn = 0;
    for (int i = 0; i < frames; i++) {
        goome_event ev;
        while (goome_poll(g, &ev, 0) > 0) goome_ui_feed(u, &ev);
        if (idle && !goome_ui_animating(u) && drawn) continue;
        if (!goome_ui_begin(u, 0)) continue;
        scene(u, dash, vals, checks);
        goome_ui_end(u);
        drawn++;
    }
    return bench_now_ms() - t0;
}

int main(int argc, char **argv)
{
    int frames = argc > 1 ? atoi(argv[1]) : 2000;
    goome *g = goome_open(&(goome_opts){.mode = GOOME_MODE_OFFSCREEN, .width = 960, .height = 640}, NULL);
    if (!g) return 1;
    goome_ui *u = goome_ui_new(g);
    goome_ui_theme(u)->anim_ms = 0;
    double vals[BENCH_ROWS];
    int checks[BENCH_ROWS] = {0};
    for (int i = 0; i < BENCH_ROWS; i++) vals[i] = i / (double)BENCH_ROWS;
    bench_report("goome", "hello", frames, run(g, u, 0, frames, 0, vals, checks));
    bench_report("goome", "dash", frames, run(g, u, 1, frames, 0, vals, checks));
    bench_report("goome", "dash_idle_skip", frames, run(g, u, 1, frames, 1, vals, checks));
    goome_ui_free(u);
    goome_close(g);
    return 0;
}
