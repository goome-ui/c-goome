#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "goome.h"

typedef struct {
    char name[32], kind[12];
    int size, day;
} file_row;

static file_row files[40];
static int nfiles, sort_col = 0, sort_desc = 0;

static const char *const pages[] = {"Table", "Big list", "Tree", "Drag and drop", "Plots", "Numbers", "Editor", "Skins"};
enum { NPAGES = 8 };

static double wave(double x)
{
    while (x > 3.14159265) x -= 6.2831853;
    while (x < -3.14159265) x += 6.2831853;
    double x2 = x * x;
    return x * (1 - x2 / 6 * (1 - x2 / 20 * (1 - x2 / 42)));
}

static int cmp_files(const void *pa, const void *pb)
{
    const file_row *a = pa, *b = pb;
    int r = 0;
    if (sort_col == 0) r = strcmp(a->name, b->name);
    else if (sort_col == 1) r = strcmp(a->kind, b->kind);
    else if (sort_col == 2) r = a->size - b->size;
    else r = a->day - b->day;
    if (!r) r = strcmp(a->name, b->name);
    return sort_desc ? -r : r;
}

static void make_files(void)
{
    static const char *const stems[] = {"notes", "budget", "photo", "track", "report", "sketch", "backup", "invoice", "map", "readme"};
    static const char *const exts[] = {"txt", "csv", "png", "mp3", "pdf", "svg", "zip", "pdf", "png", "md"};
    static const char *const kinds[] = {"text", "sheet", "image", "audio", "doc", "image", "archive", "doc", "image", "text"};
    unsigned seed = 7;
    for (int i = 0; i < 40; i++) {
        seed = seed * 1103515245u + 12345u;
        int k = (int)((seed >> 16) % 10);
        file_row *f = &files[nfiles++];
        snprintf(f->name, sizeof(f->name), "%s-%02d.%s", stems[k], i + 1, exts[k]);
        snprintf(f->kind, sizeof(f->kind), "%s", kinds[k]);
        f->size = (int)((seed >> 8) % 90000) + 120;
        f->day = (int)((seed >> 4) % 28) + 1;
    }
    qsort(files, (size_t)nfiles, sizeof(file_row), cmp_files);
}

static uint32_t kind_color(const char *k)
{
    if (!strcmp(k, "image")) return 0xFF5B8DEF;
    if (!strcmp(k, "audio")) return 0xFFE0904A;
    if (!strcmp(k, "doc")) return 0xFFD9605A;
    if (!strcmp(k, "archive")) return 0xFF8F7BE8;
    if (!strcmp(k, "sheet")) return 0xFF4DB36A;
    return 0x33FFFFFF;
}

static void page_table(goome_ui *u, int *picked)
{
    goome_ui_heading(u, "Files");
    goome_ui_wrapped(u, "click a header to sort, drag the lines between headers to resize.");
    if (goome_ui_table_begin(u, "files", 4, 360, GOOME_TABLE_SORTABLE | GOOME_TABLE_RESIZABLE | GOOME_TABLE_STRIPES | GOOME_TABLE_BORDERS)) {
        goome_ui_table_column(u, "Name", 0);
        goome_ui_table_column(u, "Kind", 110);
        goome_ui_table_column(u, "Size", 100);
        goome_ui_table_column(u, "Modified", 120);
        if (goome_ui_table_sort(u, &sort_col, &sort_desc)) qsort(files, (size_t)nfiles, sizeof(file_row), cmp_files);
        for (int i = 0; i < nfiles; i++) {
            file_row *f = &files[i];
            goome_ui_table_row(u);
            goome_ui_push_id(u, f->name);
            if (goome_ui_selectable(u, f->name, *picked == i)) *picked = i;
            goome_ui_table_next(u);
            goome_ui_badge(u, f->kind, kind_color(f->kind));
            goome_ui_table_next(u);
            goome_ui_label(u, "%d.%d KB", f->size / 1000, f->size / 100 % 10);
            goome_ui_table_next(u);
            goome_ui_label(u, "Nov %d", f->day);
            goome_ui_pop_id(u);
        }
    }
    goome_ui_table_end(u);
}

static void page_big(goome_ui *u)
{
    static char names[10000][24];
    if (!names[0][0])
        for (int i = 0; i < 10000; i++) snprintf(names[i], sizeof(names[i]), "row %05d", i);
    goome_ui_heading(u, "Ten thousand rows");
    goome_ui_wrapped(u, "only the rows you can see get drawn, the rest is skipped by goome_ui_list_clip.");
    goome_ui_row(u, 2);
    goome_ui_scroll_begin(u, "biglist", 300);
    int first, last;
    goome_ui_list_clip(u, 10000, 0, &first, &last);
    for (int i = first; i < last; i++) goome_ui_selectable(u, names[i], i % 7 == 0);
    goome_ui_scroll_end(u);
    if (goome_ui_table_begin(u, "bigtable", 3, 300, GOOME_TABLE_STRIPES)) {
        goome_ui_table_column(u, "#", 70);
        goome_ui_table_column(u, "Name", 0);
        goome_ui_table_column(u, "Value", 80);
        goome_ui_list_clip(u, 10000, 0, &first, &last);
        for (int i = first; i < last; i++) {
            goome_ui_table_row(u);
            goome_ui_label(u, "%d", i);
            goome_ui_table_next(u);
            goome_ui_label(u, "%s", names[i]);
            goome_ui_table_next(u);
            goome_ui_label(u, "%d", (i * 37) % 1000);
        }
    }
    goome_ui_table_end(u);
}

static void page_tree(goome_ui *u, int *leaf)
{
    static int open[8] = {1, 1, 0, 1, 0, 0, 0, 0};
    static const char *const srcs[] = {"draw.c", "font.c", "image.c", "ui.c"};
    static const char *const tests[] = {"unit.c", "x11.sh", "wayland.sh"};
    goome_ui_heading(u, "Project");
    if (goome_ui_tree(u, "goome", &open[0])) {
        if (goome_ui_tree(u, "src", &open[1])) {
            for (int i = 0; i < 4; i++)
                if (goome_ui_selectable(u, srcs[i], *leaf == i)) *leaf = i;
            if (goome_ui_tree(u, "backends", &open[2])) {
                goome_ui_selectable(u, "x11.c", 0);
                goome_ui_selectable(u, "wl.c", 0);
                goome_ui_selectable(u, "win32.c", 0);
                goome_ui_tree_end(u);
            }
            goome_ui_tree_end(u);
        }
        if (goome_ui_tree(u, "tests", &open[3])) {
            for (int i = 0; i < 3; i++)
                if (goome_ui_selectable(u, tests[i], *leaf == 10 + i)) *leaf = 10 + i;
            goome_ui_tree_end(u);
        }
        if (goome_ui_tree(u, "examples", &open[4])) {
            goome_ui_selectable(u, "widgets.c", 0);
            goome_ui_tree_end(u);
        }
        goome_ui_tree_end(u);
    }
}

static void page_dnd(goome_ui *u)
{
    static char left[8][24] = {"apples", "bread", "coffee", "eggs", "limes"}, right[8][24] = {"rice", "tea"};
    static int nl = 5, nr = 2;
    goome_ui_heading(u, "Drag between the lists");
    goome_ui_row(u, 2);
    goome_ui_label(u, "Shopping");
    goome_ui_label(u, "In the basket");
    goome_ui_row(u, 2);
    goome_ui_scroll_begin(u, "left", 250);
    for (int i = 0; i < nl; i++) {
        goome_ui_push_id(u, "l");
        goome_ui_selectable(u, left[i], 0);
        char msg[32];
        snprintf(msg, sizeof(msg), "L%d", i);
        goome_ui_drag_source(u, "item", msg, (int)strlen(msg) + 1);
        goome_ui_pop_id(u);
    }
    goome_ui_scroll_end(u);
    char got[32];
    int n = goome_ui_drop_target(u, "item", got, sizeof(got));
    if (n && got[0] == 'R' && nl < 8) {
        int k = got[1] - '0';
        if (k >= 0 && k < nr) {
            memcpy(left[nl++], right[k], 24);
            memmove(right[k], right[k + 1], 24 * (size_t)(nr - k - 1));
            nr--;
        }
    }
    goome_ui_scroll_begin(u, "right", 250);
    for (int i = 0; i < nr; i++) {
        goome_ui_push_id(u, "r");
        goome_ui_selectable(u, right[i], 0);
        char msg[32];
        snprintf(msg, sizeof(msg), "R%d", i);
        goome_ui_drag_source(u, "item", msg, (int)strlen(msg) + 1);
        goome_ui_pop_id(u);
    }
    goome_ui_scroll_end(u);
    n = goome_ui_drop_target(u, "item", got, sizeof(got));
    if (n && got[0] == 'L' && nr < 8) {
        int k = got[1] - '0';
        if (k >= 0 && k < nl) {
            memcpy(right[nr++], left[k], 24);
            memmove(left[k], left[k + 1], 24 * (size_t)(nl - k - 1));
            nl--;
        }
    }
}

static void page_plots(goome_ui *u, int frame)
{
    static float line[120], bars[16], noise[60];
    for (int i = 0; i < 120; i++) line[i] = (float)(wave(i * 0.09 + frame * 0.03) * 40 + wave(i * 0.31) * 8 + 50);
    for (int i = 0; i < 16; i++) bars[i] = (float)((i * 37 + 11) % 23 + 4 + wave(frame * 0.05 + i) * 3);
    unsigned s = 99;
    for (int i = 0; i < 60; i++) s = s * 1664525u + 1013904223u, noise[i] = (float)((int)(s >> 24) - 128) / 4.0f;
    goome_ui_heading(u, "Charts");
    goome_ui_plot_lines(u, "CPU", line, 120, 0, 100, 110);
    goome_ui_row(u, 2);
    goome_ui_plot_bars(u, "Requests per hour", bars, 16, 0, 0, 120);
    goome_ui_plot_bars(u, "Change", noise, 24, 0, 0, 120);
    goome_ui_space(u, 4);
    goome_ui_spinner(u, 0);
    goome_ui_same_line(u);
    goome_ui_label(u, "loading the next batch");
    goome_ui_same_line(u);
    goome_ui_spinner(u, 28);
    goome_ui_progress(u, -1, NULL);
}

static void page_numbers(goome_ui *u)
{
    static double speed = 1.25, volume = 60, price = 9.99;
    static int count = 12, port = 8080;
    goome_ui_heading(u, "Numbers");
    goome_ui_row(u, 2);
    goome_ui_drag_float(u, "Speed", &speed, 0.01, 0, 5, "%.2f x");
    goome_ui_drag_int(u, "Count", &count, 0.25, 0, 100);
    goome_ui_row(u, 2);
    goome_ui_drag_float(u, "Volume", &volume, 0.5, 0, 0, "%.0f %%");
    goome_ui_number(u, "Price", &price, 0.5, "%.2f");
    goome_ui_row(u, 2);
    double p = port;
    if (goome_ui_number(u, "Port", &p, 1, "%.0f")) port = (int)p;
    goome_ui_label(u, "drag sideways, double click to type");
    goome_ui_separator(u);
    goome_ui_label(u, "Badges");
    goome_ui_badge(u, "new", 0);
    goome_ui_same_line(u);
    goome_ui_badge(u, "beta", 0xFF5B8DEF);
    goome_ui_same_line(u);
    goome_ui_badge(u, "3", 0xFFE5484D);
    goome_ui_same_line(u);
    goome_ui_badge(u, "draft", 0x33FFFFFF);
    goome_ui_same_line(u);
    goome_ui_badge(u, "v0.3.0", 0xFFF2C94C);
    goome_ui_space(u, 4);
    if (goome_ui_link(u, "open the docs")) goome_ui_toast(u, "goome-ui.github.io/docs", 2500);
    goome_ui_same_line(u);
    if (goome_ui_link(u, "report a bug")) goome_ui_toast(u, "github.com/goome-ui/c-goome/issues", 2500);
}

static void page_editor(goome_ui *u, char *code, int cap, int *wrap, int *mono)
{
    goome_ui_heading(u, "Editor");
    goome_ui_toggle(u, "Wrap long lines", wrap);
    goome_ui_toggle(u, "Monospace", mono);
    goome_ui_text_edit(u, "code", code, cap, 0, GOOME_EDIT_LINE_NUMBERS | (*wrap ? GOOME_EDIT_WRAP : 0) | (*mono ? GOOME_EDIT_MONO : 0));
}

static double root(double v)
{
    double x = v > 1 ? v : 1;
    for (int i = 0; i < 20; i++) x = (x + v / x) / 2;
    return v > 0 ? x : 0;
}

static uint32_t mix(uint32_t a, uint32_t b, double t)
{
    uint32_t o = 0;
    for (int s = 0; s < 32; s += 8) o |= (uint32_t)(((a >> s) & 255) + (((double)((b >> s) & 255) - ((a >> s) & 255)) * t) + 0.5) << s;
    return o;
}

static void make_skin(goome_bitmap *b, uint32_t top, uint32_t bottom, uint32_t edge, int size, double r)
{
    uint32_t *px = calloc((size_t)size * size, 4);
    if (!px) return;
    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++) {
            double cx = x + 0.5, cy = y + 0.5, dx = cx < r ? r - cx : cx > size - r ? cx - (size - r) : 0, dy = cy < r ? r - cy : cy > size - r ? cy - (size - r) : 0;
            double d = r - root(dx * dx + dy * dy), cover = d + 0.5 > 1 ? 1 : d + 0.5 < 0 ? 0 : d + 0.5;
            if (cover <= 0) continue;
            uint32_t c = mix(top, bottom, (double)y / (size - 1));
            if (y < 3) c = mix(c, 0xFFFFFFFF, 0.12);
            c = mix(c, edge, d < 1.5 ? 1 : d < 2.5 ? 2.5 - d : 0);
            px[y * size + x] = ((uint32_t)(cover * 255 + 0.5) << 24) | (c & 0xFFFFFF);
        }
    goome_bitmap_from_argb(px, size, size, b);
    free(px);
}

static void page_skins(goome_ui *u, goome_bitmap *btn, goome_bitmap *panel)
{
    static double wx = -1, wy, ww = 300, wh = 0, v = 3;
    static char name[32] = "skinned input";
    goome_theme *t = goome_ui_theme(u);
    goome_ui_heading(u, "Skins");
    goome_ui_wrapped(u, "nine slice bitmaps: the corners stay the same and the middle stretches. set theme.skin_button, skin_panel or skin_input.");
    goome_ui_image(u, btn, 24, 24, 0);
    goome_ui_same_line(u);
    goome_ui_image(u, panel, 24, 24, 0);
    goome_ui_same_line(u);
    goome_ui_label(u, "the two 24 px bitmaps");
    t->skin_button = (goome_skin){btn, 8, 8, 8, 8};
    t->skin_input = (goome_skin){panel, 8, 8, 8, 8};
    goome_ui_button(u, "Skinned button");
    goome_ui_same_line(u);
    goome_ui_button(u, "Another one");
    goome_ui_input(u, "Name", name, sizeof(name));
    goome_ui_drag_float(u, "Drag me", &v, 0.05, 0, 10, "%.2f");
    t->skin_input.bm = NULL;
    if (wx < 0) wx = 560, wy = 360;
    t->skin_panel = (goome_skin){panel, 8, 8, 8, 8};
    if (goome_ui_window(u, "Skinned window", &wx, &wy, &ww, &wh, GOOME_WIN_MOVE, NULL)) {
        goome_ui_label(u, "the panel is a nine slice too");
        goome_ui_button(u, "OK");
    }
    goome_ui_window_end(u);
    t->skin_panel.bm = NULL, t->skin_button.bm = NULL;
}

int main(void)
{
    int err;
    goome *g = goome_open(&(goome_opts){.title = "Widgets", .mode = GOOME_MODE_WINDOW, .width = 1040, .height = 700, .min_width = 640, .min_height = 420,
                                        .window_flags = GOOME_WINDOW_RESIZABLE | GOOME_WINDOW_CENTER},
                          &err);
    if (!g) {
        fprintf(stderr, "cant open a window: %s\n", goome_strerror(err));
        return 1;
    }
    goome_ui *u = goome_ui_new(g);
    if (!u) {
        fprintf(stderr, "no memory for the ui\n");
        goome_close(g);
        return 1;
    }
    make_files();
    goome_bitmap btn, panel;
    memset(&btn, 0, sizeof(btn)), memset(&panel, 0, sizeof(panel));
    make_skin(&btn, 0xFF5A8CE6, 0xFF3460B8, 0xFF1F3F80, 24, 7);
    make_skin(&panel, 0xFF353A44, 0xFF262A31, 0xFF4A505C, 24, 8);
    static char code[1 << 16] =
        "#include \"goome.h\"\n"
        "\n"
        "int main(void)\n"
        "{\n"
        "    int err;\n"
        "    goome *g = goome_open_app(\"hello\", 640, 400, &err);\n"
        "    if (!g) return 1;\n"
        "    goome_ui *u = goome_ui_new(g);\n"
        "    for (;;) {\n"
        "        goome_event ev;\n"
        "        while (goome_poll(g, &ev, 16) > 0) goome_ui_feed(u, &ev);\n"
        "        if (!goome_ui_begin(u, 0)) continue;\n"
        "        goome_ui_app(u, \"hello\", GOOME_APP_TITLEBAR);\n"
        "        goome_ui_label(u, \"a very long line that keeps going so you can see what wrap does to it when the box is narrow\");\n"
        "        goome_ui_app_end(u);\n"
        "        goome_ui_end(u);\n"
        "    }\n"
        "}\n";
    int page = 0, picked = 0, leaf = 0, wrap = 0, mono = 1, running = 1, frame = 0, test = !strncmp(goome_backend_name(g), "test", 4);
    double frac = 0.22;
    goome_pacer pace = {0};
    while (running) {
        goome_event ev;
        int wait = test || goome_ui_animating(u) ? goome_pace(&pace, 60) : 500;
        while (goome_poll(g, &ev, wait) > 0) {
            if (ev.type == GOOME_QUIT || ev.type == GOOME_CLOSE) running = 0;
            goome_ui_feed(u, &ev);
            wait = 0;
        }
        if (!running) break;
        if (!goome_ui_begin(u, 0)) continue;
        frame++;
        goome_ui_app(u, "Widgets", GOOME_APP_STATUSBAR | GOOME_APP_NO_PAD);
        goome_ui_split_begin(u, "main", 0, &frac);
        goome_ui_label(u, "goome %s", GOOME_VERSION);
        goome_ui_space(u, 4);
        for (int i = 0; i < NPAGES; i++)
            if (goome_ui_selectable(u, pages[i], page == i)) page = i;
        goome_ui_split_next(u);
        if (page == 0) page_table(u, &picked);
        else if (page == 1) page_big(u);
        else if (page == 2) page_tree(u, &leaf);
        else if (page == 3) page_dnd(u);
        else if (page == 4) page_plots(u, frame);
        else if (page == 5) page_numbers(u);
        else if (page == 6) page_editor(u, code, sizeof(code), &wrap, &mono);
        else page_skins(u, &btn, &panel);
        goome_ui_split_end(u);
        char st[96];
        snprintf(st, sizeof(st), "%s   %s", pages[page], page == 0 ? files[picked].name : "");
        goome_ui_statusbar(u, st);
        goome_ui_app_end(u);
        goome_ui_end(u);
    }
    goome_bitmap_free(&btn);
    goome_bitmap_free(&panel);
    goome_ui_free(u);
    goome_close(g);
    return 0;
}
