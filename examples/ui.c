// ui kit demo, everything outside the windows still clicks through
// esc in a text box drops the keyboard, closing stats hides it
#include <stdio.h>
#include <string.h>

#include "goome.h"

static void make_gradient(goome_bitmap *b)
{
    static uint32_t px[128 * 128];
    for (int y = 0; y < 128; y++)
        for (int x = 0; x < 128; x++) {
            int r = 0x2a + x * (0x6f - 0x2a) / 127, g = 0x40 + y * (0xdc - 0x40) / 127, bl = 0x9a - (x + y) * 0x50 / 254;
            px[y * 128 + x] = 0xFF000000u | (uint32_t)r << 16 | (uint32_t)g << 8 | (uint32_t)bl;
        }
    goome_bitmap_from_argb(px, 128, 128, b);
}

static void set_theme(goome_ui *u, int light, uint32_t accent, int own_accent, double radius, double anim, int solid)
{
    goome_theme *t = goome_ui_theme(u);
    goome_font *f = t->font;
    if (light) goome_ui_theme_light(t);
    else goome_ui_theme_dark(t);
    t->font = f;
    if (own_accent) t->accent = accent;
    t->radius = radius, t->anim_ms = anim;
    if (solid) t->panel |= 0xFF000000u;
}

int main(void)
{
    int err;
    goome *g = goome_open(&(goome_opts){.title = "goome ui", .no_capture = 1}, &err);
    if (!g) {
        fprintf(stderr, "cant open an overlay: %s\n", goome_strerror(err));
        return 1;
    }
    goome_ui *u = goome_ui_new(g);
    if (!u) {
        fprintf(stderr, "no memory for the ui\n");
        goome_close(g);
        return 1;
    }
    goome_monitor m;
    if (goome_monitors(g, &m, 1) < 1) m.area = (goome_rect){0, 0, 1280, 720};

    goome_bitmap logo;
    int have_logo = goome_bitmap_load("docs/logo.png", &logo) == GOOME_OK || goome_bitmap_load("../docs/logo.png", &logo) == GOOME_OK; // repo root or build/
    if (!have_logo) make_gradient(&logo);

    double wx = m.area.x + 40, wy = m.area.y + 40, ww = 440, wh = 620;
    double sx = m.area.x + 500, sy = m.area.y + 40, sw = 280, sh = 0;
    int stats_open = 1, tab = 0, clicks = 0, grid = 1, wifi = 1, notify = 0, mode = 1, count = 42, fruit = 2, picked = -1, more = 1;
    int light = 0, own_accent = 0, solid = 0, always = 0, img_clicks = 0, running = 1;
    int test = !strcmp(goome_backend_name(g), "test"); // test backend only moves time on when we draw
    double volume = 0.65, radius = 10, anim = 120;
    uint32_t highlight = 0xFF4FA3FF, accent = 0xFF6FDC5F;
    char name[64] = "goome", search[64] = "";
    static const char *const fruits[] = {"Mango", "Banana", "Cherry", "Kiwi", "Apple", "Lemon", "Grape", "Fig", "Dragonfruit", "Pear"};
    static const char *const tabs[] = {"Widgets", "Images", "Theme", "About"};

    long spent = 0, tframes = 0;
    double fps = 0, frame_ms = 0, acc_ms = 0;
    int acc_n = 0, fps_n = 0;
    long fps_t = goome_now_ms();
    goome_pacer pace = {0};
    int dirty = 1;
    while (running) {
        goome_event ev;
        int wait = dirty || always || test || goome_ui_animating(u) ? goome_pace(&pace, 60) : 500;
        while (goome_poll(g, &ev, wait) > 0) {
            if (ev.type == GOOME_QUIT) running = 0;
            goome_ui_feed(u, &ev);
            dirty = 1;
            wait = 0;
        }
        if (!running) break;
        if (!dirty && !always && !test && !goome_ui_animating(u)) continue;
        dirty = 0;

        long t0 = goome_now_ms();
        goome_canvas *c = goome_ui_begin(u, 0);
        if (!c) continue;
        if (goome_ui_window(u, "goome ui", &wx, &wy, &ww, &wh, GOOME_WIN_MOVE | GOOME_WIN_RESIZE | GOOME_WIN_COLLAPSE, NULL)) {
            goome_ui_tabs(u, "tabs", tabs, 4, &tab);
            goome_ui_push_id(u, tabs[tab]);
            goome_ui_scroll_begin(u, "page", 0);
            if (tab == 0) {
                goome_ui_heading(u, "Buttons");
                goome_ui_row(u, 3);
                if (goome_ui_button(u, "Click me")) clicks++;
                if (goome_ui_button(u, "Reset")) clicks = 0;
                goome_ui_button(u, "Hover me");
                goome_ui_tooltip(u, "hi :)");
                goome_ui_label(u, clicks == 0 ? "not clicked yet" : clicks == 1 ? "clicked once" : "clicked %d times", clicks);
                goome_ui_separator(u);
                goome_ui_heading(u, "Switches");
                goome_ui_checkbox(u, "Show grid", &grid);
                goome_ui_same_line(u);
                goome_ui_checkbox(u, "Notifications", &notify);
                goome_ui_toggle(u, "Wi-Fi", &wifi);
                goome_ui_radio(u, "Small", &mode, 0);
                goome_ui_same_line(u);
                goome_ui_radio(u, "Medium", &mode, 1);
                goome_ui_same_line(u);
                goome_ui_radio(u, "Large", &mode, 2);
                goome_ui_separator(u);
                goome_ui_heading(u, "Values");
                goome_ui_slider(u, "Volume", &volume, 0, 1, "%.2f");
                goome_ui_slider_int(u, "Count", &count, 0, 100);
                char pct[32];
                snprintf(pct, sizeof(pct), "%d%%", (int)(volume * 100 + 0.5));
                goome_ui_progress(u, volume, pct);
                goome_ui_progress(u, -1, NULL);
                goome_ui_row(u, 2);
                goome_ui_input(u, "Name", name, sizeof(name));
                goome_ui_input(u, "Search", search, sizeof(search));
                goome_ui_combo(u, "Fruit", &fruit, fruits, 10);
                goome_ui_color(u, "Highlight", &highlight);
                if (goome_ui_collapsing(u, "More stuff", &more)) {
                    goome_ui_wrapped(u, "a long list so theres something to scroll. pick one, it shows up under it.");
                    goome_ui_scroll_begin(u, "list", 150);
                    for (int i = 0; i < 30; i++) {
                        char item[32];
                        snprintf(item, sizeof(item), "Item %d", i + 1);
                        if (goome_ui_selectable(u, item, picked == i)) picked = i;
                    }
                    goome_ui_scroll_end(u);
                    goome_ui_label(u, picked >= 0 ? "picked item %d" : "nothing picked", picked + 1);
                }
            } else if (tab == 1) {
                goome_ui_heading(u, have_logo ? "docs/logo.png" : "Generated gradient");
                goome_ui_image(u, &logo, 120, 120, 18);
                goome_ui_same_line(u);
                goome_ui_image(u, &logo, 120, 120, 60);
                goome_ui_same_line(u);
                goome_ui_image(u, &logo, 120, 120, 0);
                goome_ui_label(u, "image buttons");
                for (int i = 0; i < 4; i++) {
                    char id[16];
                    snprintf(id, sizeof(id), "img%d", i);
                    if (i) goome_ui_same_line(u);
                    if (goome_ui_image_button(u, id, &logo, 56, 56)) img_clicks++;
                }
                goome_ui_label(u, img_clicks == 0 ? "no image clicks yet" : img_clicks == 1 ? "one image click" : "%d image clicks", img_clicks);
                goome_ui_image(u, &logo, 0, 90, 10);
            } else if (tab == 2) {
                goome_ui_heading(u, "Theme");
                int was = light;
                goome_ui_radio(u, "Dark", &light, 0);
                goome_ui_same_line(u);
                goome_ui_radio(u, "Light", &light, 1);
                int ch = light != was;
                if (goome_ui_color(u, "Accent", &accent)) own_accent = 1, ch = 1;
                ch |= goome_ui_slider(u, "Corner radius", &radius, 0, 20, "%.0f px");
                ch |= goome_ui_slider(u, "Animation", &anim, 0, 400, "%.0f ms");
                ch |= goome_ui_toggle(u, "Solid panels", &solid);
                if (goome_ui_button(u, "Reset theme")) light = 0, own_accent = 0, radius = 10, anim = 120, solid = 0, accent = 0xFF6FDC5F, ch = 1;
                if (ch) set_theme(u, light, accent, own_accent, radius, anim, solid);
                if (!own_accent) accent = goome_ui_theme(u)->accent;
            } else {
                goome_ui_heading(u, "About");
                goome_ui_wrapped(u, "made with goome. try clicking next to this window, your desktop still gets it.");
                goome_ui_label(u, "goome %s on %s", GOOME_VERSION, goome_backend_name(g));
                goome_ui_label(u, "%.0f fps, %.2f ms a frame", fps, frame_ms);
                if (!stats_open && goome_ui_button(u, "Show stats")) stats_open = 1;
            }
            goome_ui_scroll_end(u);
            goome_ui_pop_id(u);
        }
        goome_ui_window_end(u);

        if (goome_ui_window(u, "Stats", &sx, &sy, &sw, &sh, GOOME_WIN_MOVE | GOOME_WIN_COLLAPSE | GOOME_WIN_CLOSE | GOOME_WIN_RESIZE, &stats_open)) {
            goome_ui_label(u, "%.0f fps", fps);
            goome_ui_label(u, "%.2f ms a frame", frame_ms);
            char used[48];
            snprintf(used, sizeof(used), "%d%% of a 60 fps frame", (int)(frame_ms * 100 / 16.7 + 0.5));
            goome_ui_progress(u, frame_ms / 16.7, used);
            goome_ui_toggle(u, "Redraw every frame", &always);
            if (goome_ui_button(u, "Quit")) running = 0;
        }
        goome_ui_window_end(u);
        goome_ui_end(u);

        long t1 = goome_now_ms();
        spent += t1 - t0, tframes++;
        acc_ms += (double)(t1 - t0), acc_n++;
        fps_n++;
        if (t1 - fps_t >= 500) {
            fps = fps_n * 1000.0 / (double)(t1 - fps_t);
            frame_ms = acc_ms / acc_n;
            fps_t = t1, fps_n = 0, acc_ms = 0, acc_n = 0;
        }
    }
    if (tframes) printf("%ld frames, %.2f ms a frame\n", tframes, (double)spent / (double)tframes);
    goome_bitmap_free(&logo);
    goome_ui_free(u);
    goome_close(g);
    return 0;
}
