#include <stdio.h>
#include <string.h>

#include "goome.h"

static const char *const icon_files[GOOME_ICON_COUNT] = {
    "arrow_down", "arrow_right", "arrow_up", "arrow_left", "check", "radio_dot", "close", "minimize", "maximize", "restore", "resize",
    "sort_up", "sort_down", "plus", "minus", "slider_knob", "toggle_knob", "collapse", "expand", "drag_handle", "toast", "picker",
};

typedef struct {
    int kind;
    const char *file;
    double l, t, r, b;
} skin_def;

static const skin_def skin_files[] = {
    {GOOME_SKIN_BUTTON, "button", 8, 8, 8, 8},
    {GOOME_SKIN_INPUT, "input", 8, 8, 8, 8},
    {GOOME_SKIN_CHECKBOX, "checkbox", 18, 18, 18, 18},
    {GOOME_SKIN_RADIO, "radio", 18, 18, 18, 18},
    {GOOME_SKIN_TOGGLE, "toggle", 38, 22, 38, 22},
    {GOOME_SKIN_SLIDER_TRACK, "slider", 3, 3, 3, 3},
    {GOOME_SKIN_SLIDER_FILL, "slider_fill", 3, 3, 3, 3},
    {GOOME_SKIN_SCROLLBAR, "scrollbar", 3, 3, 3, 3},
    {GOOME_SKIN_TAB, "tab", 8, 8, 8, 8},
    {GOOME_SKIN_TITLEBAR, "titlebar", 12, 12, 12, 3},
    {GOOME_SKIN_TOOLTIP, "tooltip", 8, 8, 8, 8},
    {GOOME_SKIN_MENU, "menu", 10, 10, 10, 10},
    {GOOME_SKIN_POPUP, "menu", 10, 10, 10, 10},
};
enum { NSKIN = sizeof(skin_files) / sizeof(skin_files[0]) };

typedef struct {
    goome_bitmap icons[GOOME_ICON_COUNT], skins[NSKIN], heart, app_bar;
    goome_skin app_skin;
} assets;

typedef struct {
    const char *name, *kind;
    int size;
} file_row;

static const file_row files[] = {
    {"sketch.png", "image", 482}, {"notes.txt", "text", 12}, {"palette.gpl", "colors", 3}, {"cover.jpg", "image", 1210},
    {"draft.psd", "layers", 8840}, {"todo.md", "text", 4}, {"brushes.zip", "archive", 2304},
};
enum { NFILE = sizeof(files) / sizeof(files[0]) };

static int load(const char *dir, const char *name, goome_bitmap *b)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/%s.png", dir, name);
    if (goome_bitmap_load(path, b) == GOOME_OK) return 1;
    fprintf(stderr, "cant load %s: %s\n", path, goome_bitmap_error());
    return 0;
}

static const char *find_assets(void)
{
    static const char *const dirs[] = {"examples/assets", "assets", "../examples/assets", "../../examples/assets"};
    for (int i = 0; i < 4; i++) {
        char path[512];
        snprintf(path, sizeof(path), "%s/arrow_down.png", dirs[i]);
        FILE *f = fopen(path, "rb");
        if (f) {
            fclose(f);
            return dirs[i];
        }
    }
    return NULL;
}

static void paper(goome_theme *t)
{
    t->bg = 0xFFEFE8DD, t->panel = 0xFFFAF6F0, t->border = 0x2E2B1E35;
    t->text = 0xFF2B2233, t->text_dim = 0xFF85798C;
    t->accent = 0xFF7B4FD0, t->accent_text = 0xFFFFFFFF;
    t->hover = 0xFFE7DEF0, t->active = 0xFFDACDEA, t->input_bg = 0xFFEDE6DE, t->shadow = 0x3A281838;
    t->radius = 9, t->pad = 10, t->gap = 9, t->border_width = 1, t->shadow_blur = 24, t->anim_ms = 120;
}

static int dress(goome_ui *u, assets *a)
{
    const char *dir = find_assets();
    if (!dir) {
        fprintf(stderr, "cant find examples/assets, run it from the repo folder\n");
        return 0;
    }
    goome_theme *t = goome_ui_theme(u);
    paper(t);
    for (int i = 0; i < GOOME_ICON_COUNT; i++)
        if (load(dir, icon_files[i], &a->icons[i])) t->icon[i] = &a->icons[i];
    for (int i = 0; i < NSKIN; i++) {
        if (!load(dir, skin_files[i].file, &a->skins[i])) continue;
        goome_skin s = {&a->skins[i], skin_files[i].l, skin_files[i].t, skin_files[i].r, skin_files[i].b};
        switch (skin_files[i].kind) {
        case GOOME_SKIN_BUTTON: t->skin_button = s; break;
        case GOOME_SKIN_INPUT: t->skin_input = s; break;
        case GOOME_SKIN_CHECKBOX: t->skin_checkbox = s; break;
        case GOOME_SKIN_RADIO: t->skin_radio = s; break;
        case GOOME_SKIN_TOGGLE: t->skin_toggle = s; break;
        case GOOME_SKIN_SLIDER_TRACK: t->skin_slider = s; break;
        case GOOME_SKIN_SLIDER_FILL: t->skin_slider_fill = s; break;
        case GOOME_SKIN_SCROLLBAR: t->skin_scrollbar = s; break;
        case GOOME_SKIN_TAB: t->skin_tab = s; break;
        case GOOME_SKIN_TITLEBAR: t->skin_titlebar = s; break;
        case GOOME_SKIN_TOOLTIP: t->skin_tooltip = s; break;
        case GOOME_SKIN_MENU: t->skin_menu = s; break;
        case GOOME_SKIN_POPUP: t->skin_popup = s; break;
        }
    }
    load(dir, "heart", &a->heart);
    if (load(dir, "titlebar_app", &a->app_bar)) a->app_skin = (goome_skin){&a->app_bar, 4, 4, 4, 3};
    return 1;
}

static void undress(assets *a)
{
    for (int i = 0; i < GOOME_ICON_COUNT; i++) goome_bitmap_free(&a->icons[i]);
    for (int i = 0; i < NSKIN; i++) goome_bitmap_free(&a->skins[i]);
    goome_bitmap_free(&a->heart);
    goome_bitmap_free(&a->app_bar);
}

typedef struct {
    int tab, snap, hints, fav, size, corners, autosave, grid, brush, sort_col, sort_desc, layers, bg_open, sketch_open, inspector, outline;
    double volume, copies, opacity, frac, wx, wy, ww, wh, zoom;
    uint32_t tint;
    char name[64];
} state;

static void menus(goome_ui *u, state *s, int *running)
{
    if (!goome_ui_menubar_begin(u)) return;
    if (goome_ui_menu_begin(u, "File")) {
        goome_ui_menu_item(u, "New", "Ctrl+N", NULL);
        goome_ui_menu_item(u, "Open", "Ctrl+O", NULL);
        goome_ui_menu_separator(u);
        if (goome_ui_menu_item(u, "Quit", "Ctrl+Q", NULL)) *running = 0;
        goome_ui_menu_end(u);
    }
    if (goome_ui_menu_begin(u, "View")) {
        goome_ui_menu_item(u, "Grid", NULL, &s->grid);
        goome_ui_menu_item(u, "Outline", NULL, &s->outline);
        if (goome_ui_menu_begin(u, "Zoom")) {
            if (goome_ui_menu_item(u, "50%", NULL, NULL)) s->zoom = 0.5;
            if (goome_ui_menu_item(u, "100%", NULL, NULL)) s->zoom = 1;
            if (goome_ui_menu_item(u, "200%", NULL, NULL)) s->zoom = 2;
            goome_ui_menu_end(u);
        }
        if (goome_ui_menu_item(u, "Inspector", NULL, &s->inspector)) s->wh = s->wh > 0 ? s->wh : 300;
        goome_ui_menu_end(u);
    }
    goome_ui_menubar_end(u);
}

static void left_pane(goome_ui *u, state *s, const assets *a)
{
    static const char *const tabs[] = {"General", "Sound", "About"};
    static const char *const brushes[] = {"Round", "Pencil", "Ink pen", "Marker", "Charcoal"};
    goome_ui_heading(u, "Look");
    goome_ui_tabs(u, "tabs", tabs, 3, &s->tab);
    goome_ui_space(u, 2);
    goome_ui_row(u, 2);
    goome_ui_checkbox(u, "Snap to grid", &s->snap);
    goome_ui_checkbox(u, "Show hints", &s->hints);
    if (a->heart.px) goome_ui_push_icon(u, GOOME_ICON_CHECK, &a->heart);
    goome_ui_push_color(u, GOOME_S_ACCENT, 0xFFE0567A);
    goome_ui_checkbox(u, "Favorite", &s->fav);
    goome_ui_pop(u, a->heart.px ? 2 : 1);
    goome_ui_tooltip(u, "this one gets a heart, pushed with goome_ui_push_icon");
    goome_ui_radio(u, "Small", &s->size, 0);
    goome_ui_same_line(u);
    goome_ui_radio(u, "Medium", &s->size, 1);
    goome_ui_same_line(u);
    goome_ui_radio(u, "Large", &s->size, 2);
    goome_ui_row(u, 2);
    goome_ui_toggle(u, "Soft corners", &s->corners);
    goome_ui_toggle(u, "Autosave", &s->autosave);
    goome_ui_slider(u, "Volume", &s->volume, 0, 100, "%.0f%%");
    goome_ui_row(u, 2);
    goome_ui_combo(u, "Brush", &s->brush, brushes, 5);
    goome_ui_color(u, "Tint", &s->tint);
    goome_ui_row(u, 2);
    goome_ui_number(u, "Copies", &s->copies, 1, "%.0f");
    goome_ui_drag_float(u, "Opacity", &s->opacity, 0.005, 0, 1, "%.2f");
    if (goome_ui_collapsing(u, "Layers", &s->layers)) {
        if (goome_ui_tree(u, "Background", &s->bg_open)) {
            goome_ui_label(u, "paper texture");
            goome_ui_label(u, "warm fill");
            goome_ui_tree_end(u);
        }
        if (goome_ui_tree(u, "Sketch", &s->sketch_open)) {
            goome_ui_label(u, "lines");
            goome_ui_tree_end(u);
        }
    }
}

static int by_col(const file_row *x, const file_row *y, int col)
{
    if (col == 2) return x->size - y->size;
    const char *p = col == 1 ? x->kind : x->name, *q = col == 1 ? y->kind : y->name;
    return strcmp(p, q);
}

static void right_pane(goome_ui *u, state *s)
{
    goome_ui_heading(u, "Files");
    int order[NFILE];
    for (int i = 0; i < NFILE; i++) order[i] = i;
    if (goome_ui_table_begin(u, "files", 3, 0, GOOME_TABLE_SORTABLE | GOOME_TABLE_STRIPES)) {
        goome_ui_table_column(u, "Name", 0);
        goome_ui_table_column(u, "Kind", 90);
        goome_ui_table_column(u, "KB", 70);
        goome_ui_table_sort(u, &s->sort_col, &s->sort_desc);
        for (int i = 1; i < NFILE; i++)
            for (int j = i; j > 0; j--) {
                int c = s->sort_col >= 0 ? by_col(&files[order[j - 1]], &files[order[j]], s->sort_col) : 0;
                if (s->sort_desc ? c >= 0 : c <= 0) break;
                int t = order[j];
                order[j] = order[j - 1], order[j - 1] = t;
            }
        for (int i = 0; i < NFILE; i++) {
            const file_row *f = &files[order[i]];
            goome_ui_table_row(u);
            goome_ui_label(u, "%s", f->name);
            goome_ui_table_next(u);
            goome_ui_label(u, "%s", f->kind);
            goome_ui_table_next(u);
            goome_ui_label(u, "%d", f->size);
        }
    }
    goome_ui_table_end(u);
    goome_ui_space(u, 4);
    goome_ui_input(u, "Name", s->name, sizeof(s->name));
    goome_ui_progress(u, s->volume / 100, "exporting");
    goome_ui_row(u, 3);
    if (goome_ui_button(u, "Export")) goome_ui_toast(u, "exported, check your folder", 2500);
    goome_ui_tooltip(u, "saves every layer as its own png");
    goome_ui_button(u, "Share");
    if (goome_ui_button(u, "Inspector")) s->inspector = 1;
}

static void inspector(goome_ui *u, state *s)
{
    if (goome_ui_window(u, "Inspector", &s->wx, &s->wy, &s->ww, &s->wh, GOOME_WIN_MOVE | GOOME_WIN_RESIZE | GOOME_WIN_COLLAPSE | GOOME_WIN_CLOSE, &s->inspector)) {
        goome_ui_label(u, "Brush settings");
        goome_ui_slider(u, "Flow", &s->opacity, 0, 1, "%.2f");
        goome_ui_checkbox(u, "Pressure", &s->snap);
        goome_ui_separator(u);
        for (int i = 0; i < 14; i++) goome_ui_label(u, "stroke %d, %d px", i + 1, 2 + i * 3);
    }
    goome_ui_window_end(u);
}

int main(void)
{
    int err;
    goome *g = goome_open(&(goome_opts){.title = "Skinned", .mode = GOOME_MODE_WINDOW, .width = 980, .height = 680, .min_width = 760, .min_height = 520,
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
    assets a;
    memset(&a, 0, sizeof(a));
    if (!dress(u, &a)) {
        goome_ui_free(u);
        goome_close(g);
        return 1;
    }
    state s;
    memset(&s, 0, sizeof(s));
    s.snap = 1, s.fav = 1, s.size = 1, s.corners = 1, s.grid = 1, s.layers = 1, s.bg_open = 1, s.inspector = 1, s.sort_col = 2, s.sort_desc = 1;
    s.volume = 64, s.copies = 3, s.opacity = 0.8, s.frac = 0.5, s.wx = 610, s.wy = 300, s.ww = 330, s.wh = 268, s.zoom = 1;
    s.tint = 0xFFE0567A;
    snprintf(s.name, sizeof(s.name), "cozy cat sketch");
    int running = 1, test = !strncmp(goome_backend_name(g), "test", 4), first = 1;
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
        if (first) goome_ui_toast(u, "skin loaded, every icon is a png now", 4000), first = 0;
        if (a.app_bar.px) goome_ui_push_skin(u, GOOME_SKIN_TITLEBAR, &a.app_skin);
        goome_ui_app(u, "Skinned", GOOME_APP_TITLEBAR | GOOME_APP_MENUBAR | GOOME_APP_STATUSBAR | GOOME_APP_NO_PAD);
        if (a.app_bar.px) goome_ui_pop(u, 1);
        menus(u, &s, &running);
        goome_ui_split_begin(u, "main", 0, &s.frac);
        left_pane(u, &s, &a);
        goome_ui_split_next(u);
        right_pane(u, &s);
        goome_ui_split_end(u);
        char status[160];
        snprintf(status, sizeof(status), "%d files, zoom %.0f%%, every glyph and part here is a png from examples/assets", NFILE, s.zoom * 100);
        goome_ui_statusbar(u, status);
        goome_ui_app_end(u);
        if (s.inspector) inspector(u, &s);
        goome_ui_end(u);
    }
    goome_ui_free(u);
    undress(&a);
    goome_close(g);
    return 0;
}
