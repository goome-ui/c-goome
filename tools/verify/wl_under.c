#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <wayland-client.h>

#include "xdg-shell-client-protocol.h"

static struct wl_compositor *comp;
static struct wl_shm *shm;
static struct xdg_wm_base *wm;
static struct wl_seat *seat;
static struct wl_surface *surf;
static int W, H, bw, bh, mapped;
static double px, py;

static void make_buffer(void)
{
    int stride = W * 4;
    size_t size = (size_t)stride * H;
    int fd = memfd_create("under", 0);
    if (fd < 0 || ftruncate(fd, (off_t)size)) exit(3);
    uint32_t *d = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    for (size_t i = 0; i < size / 4; i++) d[i] = 0xFF204060;
    munmap(d, size);
    struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, (int)size);
    struct wl_buffer *b = wl_shm_pool_create_buffer(pool, 0, W, H, stride, WL_SHM_FORMAT_XRGB8888);
    wl_shm_pool_destroy(pool);
    close(fd);
    wl_surface_attach(surf, b, 0, 0);
    wl_surface_damage(surf, 0, 0, W, H);
    wl_surface_commit(surf);
    bw = W;
    bh = H;
}

static void wm_ping(void *d, struct xdg_wm_base *w, uint32_t s) { xdg_wm_base_pong(w, s); }
static const struct xdg_wm_base_listener wml = {wm_ping};

static void xs_conf(void *d, struct xdg_surface *x, uint32_t serial)
{
    xdg_surface_ack_configure(x, serial);
    if (!W) W = 1280, H = 720;
    if (!mapped || W != bw || H != bh) {
        make_buffer();
        mapped = 1;
        printf("under ready %dx%d\n", W, H);
        fflush(stdout);
    }
}
static const struct xdg_surface_listener xsl = {xs_conf};

static void tl_conf(void *d, struct xdg_toplevel *t, int32_t w, int32_t h, struct wl_array *s)
{
    if (w > 0 && h > 0) W = w, H = h;
}
static void tl_close(void *d, struct xdg_toplevel *t) { exit(0); }
static const struct xdg_toplevel_listener tll = {.configure = tl_conf, .close = tl_close};

static void p_enter(void *d, struct wl_pointer *p, uint32_t s, struct wl_surface *sf, wl_fixed_t x, wl_fixed_t y)
{
    px = wl_fixed_to_double(x), py = wl_fixed_to_double(y);
    printf("under enter %.0f,%.0f\n", px, py);
    fflush(stdout);
}
static void p_leave(void *d, struct wl_pointer *p, uint32_t s, struct wl_surface *sf) {}
static void p_motion(void *d, struct wl_pointer *p, uint32_t t, wl_fixed_t x, wl_fixed_t y) { px = wl_fixed_to_double(x), py = wl_fixed_to_double(y); }
static void p_button(void *d, struct wl_pointer *p, uint32_t s, uint32_t t, uint32_t b, uint32_t st)
{
    if (st == WL_POINTER_BUTTON_STATE_PRESSED) printf("under down %u %.0f,%.0f\n", b, px, py), fflush(stdout);
}
static void p_axis(void *d, struct wl_pointer *p, uint32_t t, uint32_t a, wl_fixed_t v) {}
static const struct wl_pointer_listener pl = {.enter = p_enter, .leave = p_leave, .motion = p_motion, .button = p_button, .axis = p_axis};

static void s_caps(void *d, struct wl_seat *s, uint32_t caps)
{
    if (caps & WL_SEAT_CAPABILITY_POINTER) wl_pointer_add_listener(wl_seat_get_pointer(s), &pl, NULL);
}
static void s_name(void *d, struct wl_seat *s, const char *n) {}
static const struct wl_seat_listener sl = {s_caps, s_name};

static void glob(void *d, struct wl_registry *r, uint32_t n, const char *i, uint32_t v)
{
    if (!strcmp(i, "wl_compositor")) comp = wl_registry_bind(r, n, &wl_compositor_interface, 4);
    else if (!strcmp(i, "wl_shm")) shm = wl_registry_bind(r, n, &wl_shm_interface, 1);
    else if (!strcmp(i, "xdg_wm_base")) wm = wl_registry_bind(r, n, &xdg_wm_base_interface, 1);
    else if (!strcmp(i, "wl_seat") && !seat) seat = wl_registry_bind(r, n, &wl_seat_interface, 1);
}
static void glob_rm(void *d, struct wl_registry *r, uint32_t n) {}
static const struct wl_registry_listener rl = {glob, glob_rm};

int main(void)
{
    struct wl_display *dpy = wl_display_connect(NULL);
    if (!dpy) {
        puts("under no display");
        return 1;
    }
    struct wl_registry *r = wl_display_get_registry(dpy);
    wl_registry_add_listener(r, &rl, NULL);
    wl_display_roundtrip(dpy);
    if (!comp || !shm || !wm) {
        printf("under missing globals comp %d shm %d xdg %d seat %d\n", !!comp, !!shm, !!wm, !!seat);
        return 1;
    }
    if (seat) wl_seat_add_listener(seat, &sl, NULL);
    xdg_wm_base_add_listener(wm, &wml, NULL);
    surf = wl_compositor_create_surface(comp);
    struct xdg_surface *xs = xdg_wm_base_get_xdg_surface(wm, surf);
    xdg_surface_add_listener(xs, &xsl, NULL);
    struct xdg_toplevel *t = xdg_surface_get_toplevel(xs);
    xdg_toplevel_add_listener(t, &tll, NULL);
    xdg_toplevel_set_title(t, "goome under");
    xdg_toplevel_set_app_id(t, "goome-under");
    xdg_toplevel_set_fullscreen(t, NULL);
    wl_surface_commit(surf);
    while (wl_display_dispatch(dpy) != -1) {
    }
    return 0;
}
