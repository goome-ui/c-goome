#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>

static int clicks;
static GtkWidget *lab;
static int done;

static void on_click(GtkButton *b, gpointer u)
{
    char buf[32];
    (void)b;
    (void)u;
    snprintf(buf, sizeof buf, "clicks: %d", ++clicks);
    gtk_label_set_text(GTK_LABEL(lab), buf);
}

static void after_paint(GdkFrameClock *c, gpointer u)
{
    (void)c;
    (void)u;
    if (!done) {
        done = 1;
        printf("READY\n");
        fflush(stdout);
    }
}

static void activate(GtkApplication *app, gpointer u)
{
    GtkWidget *win = gtk_application_window_new(app);
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *btn = gtk_button_new_with_label("Click me");
    (void)u;
    lab = gtk_label_new("clicks: 0");
    gtk_window_set_title(GTK_WINDOW(win), "Hello");
    gtk_window_set_default_size(GTK_WINDOW(win), 640, 400);
    gtk_box_append(GTK_BOX(box), gtk_label_new("hello world"));
    gtk_box_append(GTK_BOX(box), btn);
    gtk_box_append(GTK_BOX(box), lab);
    g_signal_connect(btn, "clicked", G_CALLBACK(on_click), NULL);
    gtk_window_set_child(GTK_WINDOW(win), box);
    gtk_window_present(GTK_WINDOW(win));
    g_signal_connect(gtk_widget_get_frame_clock(win), "after-paint", G_CALLBACK(after_paint), NULL);
}

int main(int argc, char **argv)
{
    GtkApplication *app;
    int st;
    if (argc > 1 && !strcmp(argv[1], "version")) {
        printf("%d.%d.%d\n", gtk_get_major_version(), gtk_get_minor_version(), gtk_get_micro_version());
        return 0;
    }
    app = gtk_application_new("org.goome.benchhello", G_APPLICATION_NON_UNIQUE);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    st = g_application_run(G_APPLICATION(app), 1, argv);
    g_object_unref(app);
    return st;
}
