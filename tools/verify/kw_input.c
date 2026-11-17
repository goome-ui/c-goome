#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wayland-client.h>

#include "fake-input-client-protocol.h"

static struct org_kde_kwin_fake_input *fake;
static void glob(void *d, struct wl_registry *r, uint32_t n, const char *i, uint32_t v)
{
    if (!strcmp(i, "org_kde_kwin_fake_input")) fake = wl_registry_bind(r, n, &org_kde_kwin_fake_input_interface, v < 6 ? v : 6);
}
static void glob_rm(void *d, struct wl_registry *r, uint32_t n) {}
static const struct wl_registry_listener rl = {glob, glob_rm};

int main(int argc, char **argv)
{
    struct wl_display *dpy = wl_display_connect(NULL);
    if (!dpy) return 1;
    struct wl_registry *r = wl_display_get_registry(dpy);
    wl_registry_add_listener(r, &rl, NULL);
    wl_display_roundtrip(dpy);
    if (!fake) {
        puts("no fake_input");
        return 1;
    }
    org_kde_kwin_fake_input_authenticate(fake, "goome-verify", "click injection for ci");
    wl_display_roundtrip(dpy);
    for (int i = 3; i < argc; i++) {
        if (!strcmp(argv[i], "c") && i + 2 < argc) {
            double x = atof(argv[i + 1]), y = atof(argv[i + 2]);
            i += 2;
            org_kde_kwin_fake_input_pointer_motion_absolute(fake, wl_fixed_from_double(x), wl_fixed_from_double(y));
            wl_display_roundtrip(dpy);
            usleep(250000);
            org_kde_kwin_fake_input_button(fake, 0x110, 1);
            wl_display_roundtrip(dpy);
            usleep(120000);
            org_kde_kwin_fake_input_button(fake, 0x110, 0);
            wl_display_roundtrip(dpy);
            usleep(250000);
        } else if (!strcmp(argv[i], "m") && i + 2 < argc) {
            org_kde_kwin_fake_input_pointer_motion_absolute(fake, wl_fixed_from_double(atof(argv[i + 1])), wl_fixed_from_double(atof(argv[i + 2])));
            wl_display_roundtrip(dpy);
            usleep(250000);
            i += 2;
        }
    }
    wl_display_disconnect(dpy);
    return 0;
}
