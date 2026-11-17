#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "goome.h"

enum { MAX_TASKS = 64, TITLE = 48 };

typedef struct {
    char title[TITLE];
    int pri, done;
} task;

static goome *g;
static goome_ui *u;
static task tasks[MAX_TASKS];
static int ntasks, tab, sort_col = -1, sort_desc, show, new_pri = 1;
static int dark = 1, preset, radius = 10, scale = 100, tips = 1, clear_open, about_open;
static uint32_t accent = 0xFF6FDC5F;
static char draft[TITLE], search[TITLE];
static float week[14] = {2, 3, 1, 4, 3, 5, 2, 0, 3, 4, 6, 3, 2, 0};
static const char *const tabs[] = {"Tasks", "Stats", "Settings"};
static const char *const pris[] = {"low", "medium", "high"};
static const char *const shows[] = {"All", "Open", "Done"};
static const char *const presets[] = {"Custom", "Midnight", "Paper", "Mint", "Rose", "High contrast"};
static const int preset_id[] = {0, GOOME_PRESET_MIDNIGHT, GOOME_PRESET_PAPER, GOOME_PRESET_MINT, GOOME_PRESET_ROSE, GOOME_PRESET_HIGH_CONTRAST};
static const uint32_t pri_col[] = {0xFF4FB3F7, 0xFFE0A030, 0xFFE5484D};

static void add(const char *title, int pri)
{
    if (ntasks == MAX_TASKS || !*title) return;
    snprintf(tasks[ntasks].title, TITLE, "%s", title);
    tasks[ntasks].pri = pri;
    tasks[ntasks].done = 0;
    ntasks++;
}

static void drop(int i)
{
    memmove(tasks + i, tasks + i + 1, sizeof(task) * (size_t)(ntasks - i - 1));
    ntasks--;
}

static int count(int done, int pri)
{
    int n = 0;
    for (int i = 0; i < ntasks; i++) n += (done < 0 || tasks[i].done == done) && (pri < 0 || tasks[i].pri == pri);
    return n;
}

static int lower(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }

static int contains(const char *hay, const char *needle)
{
    size_t n = strlen(needle);
    if (!n) return 1;
    for (; *hay; hay++) {
        size_t i = 0;
        while (i < n && hay[i] && lower(hay[i]) == lower(needle[i])) i++;
        if (i == n) return 1;
    }
    return 0;
}

static int by_title(const void *a, const void *b)
{
    int r = strcmp(((const task *)a)->title, ((const task *)b)->title);
    return sort_desc ? -r : r;
}

static int by_pri(const void *a, const void *b)
{
    int r = ((const task *)a)->pri - ((const task *)b)->pri;
    if (!r) r = strcmp(((const task *)a)->title, ((const task *)b)->title);
    return sort_desc ? -r : r;
}

static int by_done(const void *a, const void *b)
{
    int r = ((const task *)a)->done - ((const task *)b)->done;
    return sort_desc ? -r : r;
}

static void apply_theme(void)
{
    goome_theme *th = goome_ui_theme(u);
    if (preset) goome_ui_theme_preset(th, preset_id[preset]);
    else goome_ui_theme_make(th, accent, dark, radius);
    goome_ui_scale(u, scale / 100.0);
}

int goome_web_init(void)
{
    int err;
    g = goome_open(&(goome_opts){.title = "Tidy", .mode = GOOME_MODE_WINDOW, .width = 520, .height = 640, .min_width = 300, .min_height = 420,
                                 .window_flags = GOOME_WINDOW_RESIZABLE | GOOME_WINDOW_CENTER},
                   &err);
    if (!g) {
        fprintf(stderr, "cant open a window: %s\n", goome_strerror(err));
        return 1;
    }
    u = goome_ui_new(g);
    if (!u) return 1;
    add("Water the plants", 1);
    add("Reply to mum", 2);
    add("Finish the goome demo", 2);
    add("Buy oat milk", 0);
    add("Back up the homelab", 1);
    add("Learn the guitar riff", 0);
    tasks[3].done = 1;
    apply_theme();
    return 0;
}

void goome_web_done(void)
{
    goome_ui_free(u);
    goome_close(g);
}

static void tasks_tab(void)
{
    if (goome_ui_input_ex(u, "New task", draft, sizeof(draft), GOOME_INPUT_ENTER_RETURNS, "what needs doing?") == 2 && *draft) {
        add(draft, new_pri);
        draft[0] = 0;
        goome_ui_toast(u, "task added", 1400);
    }
    goome_ui_row(u, 2);
    goome_ui_combo(u, "Priority", &new_pri, pris, 3);
    if (goome_ui_button(u, "Add task") && *draft) {
        add(draft, new_pri);
        draft[0] = 0;
        goome_ui_toast(u, "task added", 1400);
    }
    goome_ui_tooltip(u, "enter in the box does the same");
    goome_ui_separator(u);
    goome_ui_row(u, 2);
    goome_ui_input(u, "Search", search, sizeof(search));
    goome_ui_combo(u, "Show", &show, shows, 3);
    goome_ui_scroll_begin(u, "list", 0);
    int gone = -1;
    if (goome_ui_table_begin(u, "tasks", 4, 0, GOOME_TABLE_SORTABLE | GOOME_TABLE_STRIPES)) {
        goome_ui_table_column(u, "Done", 56);
        goome_ui_table_column(u, "Task", 0);
        goome_ui_table_column(u, "Priority", 98);
        goome_ui_table_column(u, "", 40);
        if (goome_ui_table_sort(u, &sort_col, &sort_desc))
            qsort(tasks, (size_t)ntasks, sizeof(task), sort_col == 0 ? by_done : sort_col == 2 ? by_pri : by_title);
        for (int i = 0; i < ntasks; i++) {
            task *t = &tasks[i];
            if ((show == 1 && t->done) || (show == 2 && !t->done) || !contains(t->title, search)) continue;
            goome_ui_table_row(u);
            goome_ui_push_id(u, t->title);
            int was = t->done;
            goome_ui_checkbox(u, "", &t->done);
            if (t->done != was) week[13] += t->done ? 1 : -1;
            goome_ui_table_next(u);
            goome_ui_label(u, "%s", t->title);
            goome_ui_table_next(u);
            goome_ui_badge(u, pris[t->pri], pri_col[t->pri]);
            goome_ui_table_next(u);
            if (goome_ui_button(u, "x")) gone = i;
            goome_ui_tooltip(u, "delete it");
            goome_ui_pop_id(u);
        }
    }
    goome_ui_table_end(u);
    if (gone >= 0) drop(gone);
    if (!ntasks) goome_ui_label(u, "nothing here, add something above");
    goome_ui_scroll_end(u);
}

static void stats_tab(void)
{
    int open = count(0, -1), done = count(1, -1);
    goome_ui_heading(u, "Right now");
    goome_ui_badge(u, "open", 0xFF4FB3F7);
    goome_ui_same_line(u);
    goome_ui_label(u, "%d", open);
    goome_ui_badge(u, "done", 0xFF4DB36A);
    goome_ui_same_line(u);
    goome_ui_label(u, "%d", done);
    goome_ui_badge(u, "high priority open", 0xFFE5484D);
    goome_ui_same_line(u);
    goome_ui_label(u, "%d", count(0, 2));
    char pct[32];
    snprintf(pct, sizeof(pct), "%d%% done", ntasks ? done * 100 / ntasks : 0);
    goome_ui_progress(u, ntasks ? (double)done / ntasks : 0, pct);
    goome_ui_separator(u);
    float per[3];
    for (int i = 0; i < 3; i++) per[i] = (float)count(-1, i);
    goome_ui_plot_bars(u, "Tasks per priority (low, medium, high)", per, 3, 0, 0, 80);
    goome_ui_plot_lines(u, "Finished per day, last two weeks", week, 14, 0, 0, 80);
}

static void settings_tab(void)
{
    int changed = 0;
    goome_ui_heading(u, "Look");
    changed |= goome_ui_combo(u, "Theme", &preset, presets, 6);
    if (!preset) {
        changed |= goome_ui_toggle(u, "Dark mode", &dark);
        changed |= goome_ui_color(u, "Accent", &accent);
        changed |= goome_ui_slider_int(u, "Roundness", &radius, 0, 20);
    }
    changed |= goome_ui_slider_int(u, "Size %", &scale, 80, 160);
    goome_ui_tooltip(u, "bigger is easier on a phone");
    if (changed) apply_theme();
    goome_ui_separator(u);
    goome_ui_heading(u, "Behaviour");
    if (goome_ui_toggle(u, "Tooltips", &tips)) goome_ui_cfg(u)->tip_delay_ms = tips ? 500 : 3600000;
    if (goome_ui_button(u, "Clear finished tasks")) clear_open = 1;
    goome_ui_tooltip(u, "asks first");
}

static void menus(void)
{
    if (goome_ui_menubar_begin(u)) {
        if (goome_ui_menu_begin(u, "Tasks")) {
            if (goome_ui_menu_item(u, "Add three samples", NULL, NULL)) add("Water the plants again", 1), add("Take a walk", 0), add("Call the dentist", 2);
            if (goome_ui_menu_item(u, "Clear finished", NULL, NULL)) clear_open = 1;
            goome_ui_menu_separator(u);
            if (goome_ui_menu_item(u, "About", NULL, NULL)) about_open = 1;
            goome_ui_menu_end(u);
        }
        if (goome_ui_menu_begin(u, "View")) {
            if (goome_ui_menu_item(u, "Dark mode", NULL, &dark)) preset = 0, apply_theme();
            goome_ui_menu_end(u);
        }
        goome_ui_menubar_end(u);
    }
    if (goome_ui_modal_begin(u, "Clear finished tasks?", &clear_open)) {
        goome_ui_label(u, "%d finished tasks will go away.", count(1, -1));
        goome_ui_row(u, 2);
        if (goome_ui_button(u, "Clear them")) {
            for (int i = ntasks - 1; i >= 0; i--)
                if (tasks[i].done) drop(i);
            clear_open = 0;
        }
        if (goome_ui_button(u, "Keep them")) clear_open = 0;
        goome_ui_modal_end(u);
    }
    if (goome_ui_modal_begin(u, "About Tidy", &about_open)) {
        goome_ui_wrapped(u, "A tiny task list to show the goome ui kit. It is one C file, compiled to webassembly with goome build --web. Nothing is saved, reload and it resets.");
        if (goome_ui_button(u, "Nice")) about_open = 0;
        goome_ui_modal_end(u);
    }
}

int goome_web_frame(void)
{
    goome_event ev;
    while (goome_poll(g, &ev, 16) > 0) {
        if (ev.type == GOOME_QUIT || ev.type == GOOME_CLOSE) return 0;
        goome_ui_feed(u, &ev);
    }
    if (!goome_ui_begin(u, 0)) return 1;
    goome_ui_app(u, "Tidy", GOOME_APP_MENUBAR | GOOME_APP_STATUSBAR);
    menus();
    goome_ui_tabs(u, "tabs", tabs, 3, &tab);
    goome_ui_push_id(u, tabs[tab]);
    if (tab == 0) tasks_tab();
    else if (tab == 1) stats_tab();
    else settings_tab();
    goome_ui_pop_id(u);
    char line[64];
    snprintf(line, sizeof(line), "%d open, %d done", count(0, -1), count(1, -1));
    goome_ui_statusbar(u, line);
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
