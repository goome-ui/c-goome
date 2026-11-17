#ifndef MU_BENCH_H
#define MU_BENCH_H

#include <stdio.h>
#include <string.h>
#include "microui.h"

#define BENCH_ROWS 50

static int mub_text_width(mu_Font font, const char *s, int len)
{
    (void)font;
    if (len < 0) len = (int)strlen(s);
    return len * 8;
}

static int mub_text_height(mu_Font font)
{
    (void)font;
    return 16;
}

typedef struct {
    mu_Context ctx;
    int clicks;
    int checks[BENCH_ROWS];
    mu_Real vals[BENCH_ROWS];
} mub;

static void mub_init(mub *b)
{
    memset(b, 0, sizeof *b);
    mu_init(&b->ctx);
    b->ctx.text_width = mub_text_width;
    b->ctx.text_height = mub_text_height;
    for (int i = 0; i < BENCH_ROWS; i++) b->vals[i] = i / (mu_Real)BENCH_ROWS;
}

static void mub_hello(mub *b)
{
    mu_Context *c = &b->ctx;
    if (mu_begin_window(c, "Hello", mu_rect(0, 0, 300, 200))) {
        mu_layout_row(c, 1, (int[]){-1}, 0);
        mu_label(c, "hello world");
        if (mu_button(c, "Click me")) b->clicks++;
        char buf[32];
        snprintf(buf, sizeof buf, "clicks: %d", b->clicks);
        mu_label(c, buf);
        mu_end_window(c);
    }
}

static void mub_dash(mub *b)
{
    mu_Context *c = &b->ctx;
    if (mu_begin_window(c, "bench", mu_rect(0, 0, 960, 640))) {
        for (int i = 0; i < BENCH_ROWS; i++) {
            char buf[16];
            mu_push_id(c, &i, sizeof i);
            mu_layout_row(c, 4, (int[]){70, 80, 60, -1}, 0);
            snprintf(buf, sizeof buf, "row %d", i);
            mu_label(c, buf);
            mu_button(c, "Open");
            mu_checkbox(c, "On", &b->checks[i]);
            mu_slider(c, &b->vals[i], 0, 1);
            mu_pop_id(c);
        }
        mu_end_window(c);
    }
}

static int mub_frame(mub *b, int dash)
{
    mu_begin(&b->ctx);
    if (dash) mub_dash(b); else mub_hello(b);
    mu_end(&b->ctx);
    int n = 0;
    mu_Command *cmd = NULL;
    while (mu_next_command(&b->ctx, &cmd)) n++;
    return n;
}

#endif
