#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "goome.h"

#define MAX_NOTES 32

typedef struct {
    char title[64], tags[64], body[4096];
    int pinned, color, made, edited, changed;
} note;

static note notes[MAX_NOTES];
static int nnotes, made, sort_by;

static void add_note(const char *title, const char *tags, const char *body, int pinned, int color)
{
    if (nnotes >= MAX_NOTES) return;
    note *n = &notes[nnotes++];
    snprintf(n->title, sizeof(n->title), "%s", title);
    snprintf(n->tags, sizeof(n->tags), "%s", tags);
    snprintf(n->body, sizeof(n->body), "%s", body);
    n->pinned = pinned, n->color = color;
    n->made = n->edited = ++made, n->changed = 0;
}

static int cmp_notes(const void *a, const void *b)
{
    const note *x = &notes[*(const int *)a], *y = &notes[*(const int *)b];
    if (x->pinned != y->pinned) return y->pinned - x->pinned;
    if (sort_by == 1) {
        int c = strcmp(x->title, y->title);
        if (c) return c;
    }
    if (sort_by == 0) return y->edited - x->edited;
    return x->made - y->made;
}

static void save_note(goome_ui *u, note *n, int quiet)
{
    n->changed = 0;
    if (quiet) return;
    char msg[100];
    snprintf(msg, sizeof(msg), "Saved \"%.60s\"", n->title);
    goome_ui_toast(u, msg, 2500);
}

static int unsaved(void)
{
    int k = 0;
    for (int i = 0; i < nnotes; i++) k += notes[i].changed;
    return k;
}

static void del_note(int i)
{
    if (i < 0 || i >= nnotes) return;
    memmove(&notes[i], &notes[i + 1], sizeof(note) * (size_t)(nnotes - i - 1));
    nnotes--;
}

static int words(const char *s)
{
    int n = 0, in = 0;
    for (; *s; s++) {
        int w = *s != ' ' && *s != '\n';
        if (w && !in) n++;
        in = w;
    }
    return n;
}

static void apply_theme(goome_ui *u, int light, uint32_t accent, double size, double radius)
{
    goome_theme *t = goome_ui_theme(u);
    goome_font *f = t->font;
    if (light) goome_ui_theme_light(t);
    else goome_ui_theme_dark(t);
    t->font = f, t->accent = accent, t->font_size = size, t->radius = radius;
}

int main(void)
{
    int err;
    goome *g = goome_open(&(goome_opts){.title = "Notes", .mode = GOOME_MODE_WINDOW, .width = 980, .height = 640, .min_width = 560, .min_height = 380,
                                        .window_flags = GOOME_WINDOW_RESIZABLE | GOOME_WINDOW_BORDERLESS | GOOME_WINDOW_TRANSPARENT | GOOME_WINDOW_CENTER},
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
    add_note("Groceries", "home", "milk\neggs\ncoffee\nrice\ncat food", 1, 2);
    add_note("Release checklist", "work", "bump the version\nrun the tests\nwrite the changelog\ntag it", 0, 1);
    add_note("Books", "reading", "finish the one on the nightstand first", 0, 3);
    add_note("Workout", "health", "mon legs, wed push, fri pull", 0, 0);
    add_note("Trip", "travel", "train at 8:10, book the hotel", 0, 4);

    static const char *const colors[] = {"None", "Blue", "Green", "Yellow", "Red"};
    static const char *const sorts[] = {"Recently edited", "Title", "Created"};
    int sel = 0, running = 1, settings = 0, about = 0, confirm = 0, sidebar = 1, statusbar = 1, wrap = 1, sort = 0;
    int light = 0, autosave = 1, test = !strncmp(goome_backend_name(g), "test", 4);
    long typed = 0;
    double size = 15, radius = 10, side = 0.27;
    uint32_t accent = 0xFF6FDC5F;
    char search[64] = "";
    apply_theme(u, light, accent, size, radius);
    goome_pacer pace = {0};
    int dirty = 1;
    while (running) {
        goome_event ev;
        int wait = dirty || test || goome_ui_animating(u) ? goome_pace(&pace, 60) : typed ? 250 : 500;
        while (goome_poll(g, &ev, wait) > 0) {
            if (ev.type == GOOME_QUIT || ev.type == GOOME_CLOSE) running = 0;
            goome_ui_feed(u, &ev);
            dirty = 1, wait = 0;
        }
        if (!running) break;
        if (!dirty && !test && !typed && !goome_ui_animating(u)) continue;
        dirty = 0;
        goome_canvas *c = goome_ui_begin(u, 0);
        if (!c) continue;
        note *n = sel >= 0 && sel < nnotes ? &notes[sel] : NULL;
        goome_ui_app(u, "Notes", GOOME_APP_TITLEBAR | GOOME_APP_MENUBAR | (statusbar ? GOOME_APP_STATUSBAR : 0) | (sidebar ? GOOME_APP_NO_PAD : 0));
        if (goome_ui_menubar_begin(u)) {
            if (goome_ui_menu_begin(u, "File")) {
                if (goome_ui_menu_item(u, "New note", "Ctrl+N", NULL)) add_note("Untitled", "", "", 0, 0), sel = nnotes - 1;
                if (goome_ui_menu_begin(u, "Open recent")) {
                    for (int i = 0; i < nnotes && i < 4; i++)
                        if (goome_ui_menu_item(u, notes[i].title, NULL, NULL)) sel = i;
                    goome_ui_menu_end(u);
                }
                if (goome_ui_menu_item(u, "Save", "Ctrl+S", NULL) && n) save_note(u, n, 0);
                goome_ui_menu_separator(u);
                if (goome_ui_menu_item(u, "Settings", "Ctrl+,", NULL)) settings = 1;
                goome_ui_menu_separator(u);
                if (goome_ui_menu_item(u, "Quit", "Ctrl+Q", NULL)) running = 0;
                goome_ui_menu_end(u);
            }
            if (goome_ui_menu_begin(u, "Edit")) {
                if (goome_ui_menu_item(u, "Copy note", NULL, NULL) && n && goome_clipboard_set(g, n->body) == GOOME_OK) goome_ui_toast(u, "Copied", 1500);
                if (goome_ui_menu_item(u, "Paste at the end", NULL, NULL) && n) {
                    size_t len = strlen(n->body);
                    char *add = n->body + len;
                    if (goome_clipboard_get(g, add, (int)(sizeof(n->body) - len)) > 0) n->edited = ++made, n->changed = 1, typed = goome_now_ms();
                    else *add = 0;
                }
                goome_ui_menu_separator(u);
                if (goome_ui_menu_item(u, "Delete note", "Del", NULL) && n) confirm = 1;
                goome_ui_menu_end(u);
            }
            if (goome_ui_menu_begin(u, "View")) {
                goome_ui_menu_item(u, "Sidebar", "Ctrl+B", &sidebar);
                goome_ui_menu_item(u, "Status bar", NULL, &statusbar);
                goome_ui_menu_item(u, "Wrap text", NULL, &wrap);
                goome_ui_menu_separator(u);
                if (goome_ui_menu_begin(u, "Theme")) {
                    int dark = !light, lt = light;
                    if (goome_ui_menu_item(u, "Dark", NULL, &dark)) light = 0, apply_theme(u, light, accent, size, radius);
                    if (goome_ui_menu_item(u, "Light", NULL, &lt)) light = 1, apply_theme(u, light, accent, size, radius);
                    goome_ui_menu_end(u);
                }
                if (goome_ui_menu_begin(u, "Sort by")) {
                    for (int i = 0; i < 3; i++) {
                        int on = sort == i;
                        if (goome_ui_menu_item(u, sorts[i], NULL, &on)) sort = i;
                    }
                    goome_ui_menu_end(u);
                }
                goome_ui_menu_end(u);
            }
            if (goome_ui_menu_begin(u, "Help")) {
                if (goome_ui_menu_item(u, "About Notes", NULL, NULL)) about = 1;
                goome_ui_menu_end(u);
            }
            goome_ui_menubar_end(u);
        }

        if (sidebar) {
            goome_ui_split_begin(u, "panes", 0, &side);
            goome_ui_input(u, "##search", search, sizeof(search));
            goome_ui_tooltip(u, "filter notes by title");
            goome_ui_space(u, 2);
            int order[MAX_NOTES];
            for (int i = 0; i < nnotes; i++) order[i] = i;
            sort_by = sort;
            qsort(order, (size_t)nnotes, sizeof(int), cmp_notes);
            for (int k = 0; k < nnotes; k++) {
                int i = order[k];
                if (search[0] && !strstr(notes[i].title, search)) continue;
                char row[96];
                snprintf(row, sizeof(row), "%s%.60s##%d", notes[i].pinned ? "* " : "", notes[i].title, i);
                if (goome_ui_selectable(u, row, i == sel)) sel = i;
                char ctx[16];
                snprintf(ctx, sizeof(ctx), "note%d", i);
                if (goome_ui_context_begin(u, ctx)) {
                    if (goome_ui_menu_item(u, "Open", NULL, NULL)) sel = i;
                    goome_ui_menu_item(u, "Pin to top", NULL, &notes[i].pinned);
                    if (goome_ui_menu_item(u, "Duplicate", NULL, NULL)) add_note(notes[i].title, notes[i].tags, notes[i].body, 0, notes[i].color);
                    goome_ui_menu_separator(u);
                    if (goome_ui_menu_item(u, "Delete", NULL, NULL)) sel = i, confirm = 1;
                    goome_ui_context_end(u);
                }
            }
            goome_ui_space(u, 4);
            if (goome_ui_button(u, "New note")) add_note("Untitled", "", "", 0, 0), sel = nnotes - 1;
            goome_ui_split_next(u);
        } else
            goome_ui_scroll_begin(u, "main", 0);
        if (n) {
            goome_ui_heading(u, n->title[0] ? n->title : "Untitled");
            int ch = 0;
            goome_ui_row(u, 2);
            ch |= goome_ui_input(u, "Title", n->title, sizeof(n->title));
            ch |= goome_ui_input(u, "Tags", n->tags, sizeof(n->tags));
            goome_ui_row(u, 2);
            ch |= goome_ui_combo(u, "Label", &n->color, colors, 5);
            goome_ui_combo(u, "Sort notes by", &sort, sorts, 3);
            ch |= goome_ui_toggle(u, "Pinned", &n->pinned);
            if (goome_ui_button(u, "Save")) save_note(u, n, 0);
            goome_ui_same_line(u);
            if (goome_ui_button(u, "Delete")) confirm = 1;
            goome_ui_same_line(u);
            if (goome_ui_button(u, "More...")) goome_ui_popup_open(u, "more");
            if (goome_ui_popup_begin(u, "more")) {
                goome_ui_label(u, "%d words", words(n->body));
                goome_ui_toggle(u, "Autosave", &autosave);
                if (goome_ui_button(u, "Duplicate")) add_note(n->title, n->tags, n->body, 0, n->color);
                if (goome_ui_button(u, "Close")) goome_ui_popup_close(u);
                goome_ui_popup_end(u);
            }
            char bid[16];
            snprintf(bid, sizeof(bid), "note%d", sel);
            goome_ui_push_id(u, bid);
            ch |= goome_ui_text_edit(u, "body", n->body, sizeof(n->body), 0, wrap ? GOOME_EDIT_WRAP : 0);
            goome_ui_pop_id(u);
            if (ch) n->edited = ++made, n->changed = 1, typed = goome_now_ms();
        } else
            goome_ui_label(u, "no note picked");
        if (sidebar) goome_ui_split_end(u);
        else goome_ui_scroll_end(u);

        if (goome_ui_modal_begin(u, "Settings", &settings)) {
            int was = light;
            goome_ui_label(u, "Appearance");
            goome_ui_radio(u, "Dark", &light, 0);
            goome_ui_same_line(u);
            goome_ui_radio(u, "Light", &light, 1);
            int ch = light != was;
            ch |= goome_ui_color(u, "Accent color", &accent);
            ch |= goome_ui_slider(u, "Text size", &size, 12, 20, "%.0f px");
            ch |= goome_ui_slider(u, "Corner radius", &radius, 0, 16, "%.0f px");
            goome_ui_separator(u);
            goome_ui_toggle(u, "Save while typing", &autosave);
            goome_ui_space(u, 6);
            if (goome_ui_button(u, "Done")) goome_ui_popup_close(u);
            goome_ui_same_line(u);
            if (goome_ui_button(u, "Reset")) light = 0, accent = 0xFF6FDC5F, size = 15, radius = 10, ch = 1;
            if (ch) apply_theme(u, light, accent, size, radius);
            goome_ui_modal_end(u);
        }
        if (goome_ui_modal_begin(u, "Delete this note?", &confirm)) {
            goome_ui_wrapped(u, n ? "it goes away for good, theres no trash folder yet." : "nothing picked");
            goome_ui_space(u, 6);
            if (goome_ui_button(u, "Delete")) {
                del_note(sel);
                if (sel >= nnotes) sel = nnotes - 1;
                goome_ui_toast(u, "Note deleted", 2000);
                goome_ui_popup_close(u);
            }
            goome_ui_same_line(u);
            if (goome_ui_button(u, "Keep it")) goome_ui_popup_close(u);
            goome_ui_modal_end(u);
        }
        if (goome_ui_modal_begin(u, "About Notes", &about)) {
            goome_ui_label(u, "Notes, a goome %s demo", GOOME_VERSION);
            goome_ui_wrapped(u, "everything here is drawn by goome: the title bar, the menus, this dialog.");
            goome_ui_label(u, "running on %s", goome_backend_name(g));
            if (goome_ui_button(u, "Nice")) goome_ui_popup_close(u);
            goome_ui_modal_end(u);
        }
        if (autosave && typed && goome_now_ms() - typed > 1500) {
            for (int i = 0; i < nnotes; i++) save_note(u, &notes[i], 1);
            typed = 0;
        }
        int left = unsaved();
        char st[128];
        snprintf(st, sizeof(st), "%d notes   %d words   %s", nnotes, n ? words(n->body) : 0, left ? "unsaved changes" : "all saved");
        goome_ui_statusbar(u, st);
        goome_ui_app_end(u);
        goome_ui_end(u);
    }
    goome_ui_free(u);
    goome_close(g);
    return 0;
}
