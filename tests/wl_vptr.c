// test helper, links the real libwayland: wl_vptr W H cmd...  (W H = layout size for absolute moves)
// m x y = move, c x y [button] = click, d / u = left down / up, s n = wheel n notches, a v = touchpad scroll v units, w ms = wait
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wayland-client.h>

extern const struct wl_interface zwlr_virtual_pointer_v1_interface;
static const struct wl_interface *types[] = {&wl_seat_interface, &zwlr_virtual_pointer_v1_interface, NULL, NULL, NULL, NULL, NULL};
static const struct wl_message mgr_req[] = {{"create_virtual_pointer", "?on", types}, {"destroy", "", types + 2}};
const struct wl_interface zwlr_virtual_pointer_manager_v1_interface = {"zwlr_virtual_pointer_manager_v1", 1, 2, mgr_req, 0, NULL};
static const struct wl_message vp_req[] = {{"motion", "uff", types + 2}, {"motion_absolute", "uuuuu", types + 2}, {"button", "uuu", types + 2},
    {"axis", "uuf", types + 2}, {"frame", "", types + 2}, {"axis_source", "u", types + 2}, {"axis_stop", "uu", types + 2},
    {"axis_discrete", "uufi", types + 2}, {"destroy", "", types + 2}};
const struct wl_interface zwlr_virtual_pointer_v1_interface = {"zwlr_virtual_pointer_v1", 1, 9, vp_req, 0, NULL};
static struct wl_seat *seat;
static struct wl_proxy *mgr;
static void glob(void *d, struct wl_registry *r, uint32_t n, const char *i, uint32_t v)
{
    if (!strcmp(i, "wl_seat") && !seat) seat = wl_registry_bind(r, n, &wl_seat_interface, 1);
    if (!strcmp(i, "zwlr_virtual_pointer_manager_v1")) mgr = wl_registry_bind(r, n, &zwlr_virtual_pointer_manager_v1_interface, 1);
}
static void glob_rm(void *d, struct wl_registry *r, uint32_t n) {}
static const struct wl_registry_listener rl = {glob, glob_rm};
static struct wl_display *dpy;
static struct wl_proxy *vp;
static uint32_t ms = 1;
static void flush(int sleep_ms)
{
    wl_proxy_marshal_flags(vp, 4, NULL, 1, 0);
    wl_display_roundtrip(dpy);
    usleep(sleep_ms * 1000);
    ms += sleep_ms + 1;
}
static void btn(int b, int down)
{
    wl_proxy_marshal_flags(vp, 2, NULL, 1, 0, ms, (uint32_t)(b == 3 ? 0x111 : b == 2 ? 0x112 : 0x110), (uint32_t)down);
    flush(120);
}
int main(int argc, char **argv)
{
    if (argc < 3) return 2;
    uint32_t W = (uint32_t)atoi(argv[1]), H = (uint32_t)atoi(argv[2]);
    dpy = wl_display_connect(NULL);
    if (!dpy) return 1;
    struct wl_registry *r = wl_display_get_registry(dpy);
    wl_registry_add_listener(r, &rl, NULL);
    wl_display_roundtrip(dpy);
    if (!mgr) {
        puts("no virtual pointer");
        return 1;
    }
    vp = wl_proxy_marshal_flags(mgr, 0, &zwlr_virtual_pointer_v1_interface, 1, 0, seat, NULL);
    wl_display_roundtrip(dpy);
    usleep(300000);
    for (int i = 3; i < argc; i++) {
        const char *c = argv[i];
        if ((!strcmp(c, "m") || !strcmp(c, "c")) && i + 2 < argc) {
            uint32_t x = (uint32_t)atoi(argv[i + 1]), y = (uint32_t)atoi(argv[i + 2]);
            i += 2;
            wl_proxy_marshal_flags(vp, 1, NULL, 1, 0, ms, x, y, W, H);
            flush(150);
            if (c[0] == 'c') {
                int b = i + 1 < argc && argv[i + 1][0] >= '1' && argv[i + 1][0] <= '3' && !argv[i + 1][1] ? atoi(argv[++i]) : 1;
                btn(b, 1), btn(b, 0);
            }
        } else if (!strcmp(c, "d")) btn(1, 1);
        else if (!strcmp(c, "u")) btn(1, 0);
        else if (!strcmp(c, "s") && i + 1 < argc) {
            int n = atoi(argv[++i]);
            wl_proxy_marshal_flags(vp, 5, NULL, 1, 0, 0u); // wheel
            wl_proxy_marshal_flags(vp, 7, NULL, 1, 0, ms, 0u, wl_fixed_from_int(15 * n), n);
            flush(150);
        } else if (!strcmp(c, "a") && i + 1 < argc) {
            int v = atoi(argv[++i]);
            wl_proxy_marshal_flags(vp, 5, NULL, 1, 0, 1u); // finger
            wl_proxy_marshal_flags(vp, 3, NULL, 1, 0, ms, 0u, wl_fixed_from_int(v));
            flush(150);
        } else if (!strcmp(c, "w") && i + 1 < argc) usleep(atoi(argv[++i]) * 1000);
    }
    usleep(300000);
    wl_display_disconnect(dpy);
    return 0;
}
