#include <gtk/gtk.h>
#include <stdio.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
static double now_ms(void)
{
    LARGE_INTEGER f, c;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart * 1000.0 / (double)f.QuadPart;
}
#else
static double now_ms(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
}
#endif

#define W 960
#define H 640

static GtkWidget *show(GtkWidget *root)
{
    GtkWidget *win = gtk_window_new();
    int i;
    gtk_window_set_default_size(GTK_WINDOW(win), W, H);
    gtk_window_set_child(GTK_WINDOW(win), root);
    gtk_window_present(GTK_WINDOW(win));
    for (i = 0; i < 2000 && !gtk_widget_get_mapped(root); i++) g_main_context_iteration(NULL, TRUE);
    for (i = 0; i < 50; i++) g_main_context_iteration(NULL, FALSE);
    return win;
}

static void frame(GtkWidget *root, cairo_surface_t *surf)
{
    GtkSnapshot *snap;
    GskRenderNode *node;
    cairo_t *cr;
    int min, nat;
    gtk_widget_measure(root, GTK_ORIENTATION_VERTICAL, W, &min, &nat, NULL, NULL);
    gtk_widget_allocate(root, W, H, -1, NULL);
    snap = gtk_snapshot_new();
    GTK_WIDGET_GET_CLASS(root)->snapshot(root, snap);
    node = gtk_snapshot_free_to_node(snap);
    cr = cairo_create(surf);
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_paint(cr);
    if (node) gsk_render_node_draw(node, cr);
    cairo_destroy(cr);
    if (node) gsk_render_node_unref(node);
}

static void run(const char *scene, GtkWidget *root, GtkWidget **labels, GtkWidget **checks, GtkWidget **scales, int rows, cairo_surface_t *surf)
{
    int i, k, warm = 100, n = 2000;
    double t = 0;
    char buf[32];
    for (i = 0; i < warm + n; i++) {
        if (i == warm) t = now_ms();
        for (k = 0; k < rows; k++) {
            if (checks) {
                snprintf(buf, sizeof buf, "row %d", k + (i & 1));
                gtk_label_set_text(GTK_LABEL(labels[k]), buf);
                gtk_check_button_set_active(GTK_CHECK_BUTTON(checks[k]), ((i + k) & 1) == 0);
                gtk_range_set_value(GTK_RANGE(scales[k]), ((i + k) % 100) / 100.0);
            } else {
                snprintf(buf, sizeof buf, "clicks: %d", i);
                gtk_label_set_text(GTK_LABEL(labels[0]), buf);
            }
        }
        frame(root, surf);
    }
    printf("{\"lib\":\"gtk4\",\"scene\":\"%s\",\"frames\":%d,\"ms_per_frame\":%.5f}\n", scene, n, (now_ms() - t) / n);
    fflush(stdout);
}

int main(int argc, char **argv)
{
    GtkWidget *labels[50], *checks[50], *scales[50];
    GtkWidget *root, *lab, *win;
    cairo_surface_t *surf;
    int i;
    (void)argc;
    (void)argv;
    gtk_init();
    surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, W, H);

    root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX(root), gtk_label_new("hello world"));
    gtk_box_append(GTK_BOX(root), gtk_button_new_with_label("Click me"));
    lab = gtk_label_new("clicks: 0");
    gtk_box_append(GTK_BOX(root), lab);
    labels[0] = lab;
    win = show(root);
    run("hello", root, labels, NULL, NULL, 1, surf);
    gtk_window_destroy(GTK_WINDOW(win));

    root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    for (i = 0; i < 50; i++) {
        char buf[32];
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        GtkWidget *sc = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 1, 0.01);
        snprintf(buf, sizeof buf, "row %d", i);
        labels[i] = gtk_label_new(buf);
        checks[i] = gtk_check_button_new_with_label("On");
        scales[i] = sc;
        gtk_widget_set_hexpand(sc, TRUE);
        gtk_box_append(GTK_BOX(row), labels[i]);
        gtk_box_append(GTK_BOX(row), gtk_button_new_with_label("Open"));
        gtk_box_append(GTK_BOX(row), checks[i]);
        gtk_box_append(GTK_BOX(row), gtk_label_new("Value"));
        gtk_box_append(GTK_BOX(row), sc);
        gtk_box_append(GTK_BOX(root), row);
    }
    win = show(root);
    run("dash", root, labels, checks, scales, 50, surf);
    gtk_window_destroy(GTK_WINDOW(win));
    return 0;
}
