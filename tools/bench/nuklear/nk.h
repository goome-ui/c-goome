#ifndef NK_BENCH_H
#define NK_BENCH_H

#include <stdio.h>
#include <string.h>

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_IMPLEMENTATION
#include "nuklear.h"

#define BENCH_ROWS 50

struct nk_vertex {
    float pos[2];
    float uv[2];
    nk_byte col[4];
};

typedef struct {
    struct nk_context ctx;
    struct nk_font_atlas atlas;
    struct nk_buffer cmds, verts, idx;
    struct nk_convert_config cfg;
    struct nk_draw_null_texture null_tex;
    int atlas_w, atlas_h;
    int clicks;
    nk_bool checks[BENCH_ROWS];
    float vals[BENCH_ROWS];
} nkb;

static void nkb_init(nkb *b)
{
    static const struct nk_draw_vertex_layout_element layout[] = {
        {NK_VERTEX_POSITION, NK_FORMAT_FLOAT, 0},
        {NK_VERTEX_TEXCOORD, NK_FORMAT_FLOAT, 8},
        {NK_VERTEX_COLOR, NK_FORMAT_R8G8B8A8, 16},
        {NK_VERTEX_LAYOUT_END}};
    memset(b, 0, sizeof *b);
    const void *img;
    nk_font_atlas_init_default(&b->atlas);
    nk_font_atlas_begin(&b->atlas);
    struct nk_font *font = nk_font_atlas_add_default(&b->atlas, 13.0f, 0);
    img = nk_font_atlas_bake(&b->atlas, &b->atlas_w, &b->atlas_h, NK_FONT_ATLAS_RGBA32);
    nk_font_atlas_end(&b->atlas, nk_handle_id(0), &b->null_tex);
    (void)img;
    nk_init_default(&b->ctx, &font->handle);
    nk_buffer_init_default(&b->cmds);
    nk_buffer_init_default(&b->verts);
    nk_buffer_init_default(&b->idx);
    b->cfg.vertex_layout = layout;
    b->cfg.vertex_size = sizeof(struct nk_vertex);
    b->cfg.vertex_alignment = NK_ALIGNOF(struct nk_vertex);
    b->cfg.tex_null = b->null_tex;
    b->cfg.circle_segment_count = 22;
    b->cfg.curve_segment_count = 22;
    b->cfg.arc_segment_count = 22;
    b->cfg.global_alpha = 1.0f;
    b->cfg.shape_AA = NK_ANTI_ALIASING_ON;
    b->cfg.line_AA = NK_ANTI_ALIASING_ON;
    for (int i = 0; i < BENCH_ROWS; i++) b->vals[i] = i / (float)BENCH_ROWS;
}

static void nkb_free(nkb *b)
{
    nk_buffer_free(&b->cmds);
    nk_buffer_free(&b->verts);
    nk_buffer_free(&b->idx);
    nk_font_atlas_clear(&b->atlas);
    nk_free(&b->ctx);
}

static void nkb_hello(nkb *b)
{
    struct nk_context *c = &b->ctx;
    if (nk_begin(c, "Hello", nk_rect(0, 0, 300, 200), NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_TITLE)) {
        nk_layout_row_dynamic(c, 24, 1);
        nk_label(c, "hello world", NK_TEXT_LEFT);
        if (nk_button_label(c, "Click me")) b->clicks++;
        nk_labelf(c, NK_TEXT_LEFT, "clicks: %d", b->clicks);
    }
    nk_end(c);
}

static void nkb_dash(nkb *b)
{
    struct nk_context *c = &b->ctx;
    if (nk_begin(c, "bench", nk_rect(0, 0, 960, 640), NK_WINDOW_BORDER | NK_WINDOW_TITLE)) {
        for (int i = 0; i < BENCH_ROWS; i++) {
            nk_layout_row_template_begin(c, 24);
            nk_layout_row_template_push_static(c, 70);
            nk_layout_row_template_push_static(c, 80);
            nk_layout_row_template_push_static(c, 60);
            nk_layout_row_template_push_dynamic(c);
            nk_layout_row_template_end(c);
            nk_labelf(c, NK_TEXT_LEFT, "row %d", i);
            nk_button_label(c, "Open");
            nk_checkbox_label(c, "On", &b->checks[i]);
            nk_slider_float(c, 0.0f, &b->vals[i], 1.0f, 0.01f);
        }
    }
    nk_end(c);
}

static int nkb_frame(nkb *b, int dash)
{
    nk_input_begin(&b->ctx);
    nk_input_end(&b->ctx);
    if (dash) nkb_dash(b); else nkb_hello(b);
    nk_convert(&b->ctx, &b->cmds, &b->verts, &b->idx, &b->cfg);
    int n = 0;
    const struct nk_draw_command *cmd;
    nk_draw_foreach(cmd, &b->ctx, &b->cmds) n += (int)cmd->elem_count;
    nk_buffer_clear(&b->cmds);
    nk_buffer_clear(&b->verts);
    nk_buffer_clear(&b->idx);
    nk_clear(&b->ctx);
    return n;
}

#endif
