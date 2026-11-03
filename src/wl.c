#include "internal.h"

#if defined(GM_LINUX)

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/syscall.h>
#endif

// no wayland headers since we dlopen it, these are copied out of wayland-util.h and wayland-client-core.h
struct wl_proxy;
struct wl_display;
struct wl_interface;
struct wl_message {
    const char *name;
    const char *signature;
    const struct wl_interface **types;
};
struct wl_interface {
    const char *name;
    int version;
    int method_count;
    const struct wl_message *methods;
    int event_count;
    const struct wl_message *events;
};
struct wl_array {
    size_t size, alloc;
    void *data;
};
typedef int32_t wl_fixed_t;

static struct {
    void *lib;
    struct wl_proxy *(*proxy_marshal_flags)(struct wl_proxy *, uint32_t, const struct wl_interface *, uint32_t, uint32_t, ...);
    int (*proxy_add_listener)(struct wl_proxy *, void (**)(void), void *);
    uint32_t (*proxy_get_version)(struct wl_proxy *);
    void (*proxy_destroy)(struct wl_proxy *);
    void (*proxy_set_user_data)(struct wl_proxy *, void *);
    void *(*proxy_get_user_data)(struct wl_proxy *);
    struct wl_display *(*display_connect)(const char *);
    void (*display_disconnect)(struct wl_display *);
    int (*display_roundtrip)(struct wl_display *);
    int (*display_dispatch_pending)(struct wl_display *);
    int (*display_prepare_read)(struct wl_display *);
    int (*display_read_events)(struct wl_display *);
    void (*display_cancel_read)(struct wl_display *);
    int (*display_flush)(struct wl_display *);
    int (*display_get_fd)(struct wl_display *);
} wl_dl;

#define WL_MARSHAL_FLAG_DESTROY (1 << 0)
#define wl_proxy_marshal_flags wl_dl.proxy_marshal_flags
#define wl_proxy_add_listener wl_dl.proxy_add_listener
#define wl_proxy_get_version wl_dl.proxy_get_version
#define wl_proxy_destroy wl_dl.proxy_destroy
#define wl_proxy_set_user_data wl_dl.proxy_set_user_data
#define wl_proxy_get_user_data wl_dl.proxy_get_user_data
#define wl_display_connect wl_dl.display_connect
#define wl_display_disconnect wl_dl.display_disconnect
#define wl_display_roundtrip wl_dl.display_roundtrip
#define wl_display_dispatch_pending wl_dl.display_dispatch_pending
#define wl_display_prepare_read wl_dl.display_prepare_read
#define wl_display_read_events wl_dl.display_read_events
#define wl_display_cancel_read wl_dl.display_cancel_read
#define wl_display_flush wl_dl.display_flush
#define wl_display_get_fd wl_dl.display_get_fd

#include "wl_proto.h"

typedef struct {
    struct wl_buffer *buf;
    void *data;
    size_t size;
    int w, h, stride, busy;
    uint32_t format;
} wl_shmbuf;

typedef struct {
    struct wl_output *output;
    uint32_t gname;
    int alive, done, mon;
    struct zxdg_output_v1 *xdg;
    int gx, gy, lx, ly, lw, lh, have_logical, mw, mh, scale, transform;
    char name[64];
    goome_rect area;

    struct wl_surface *surface;
    struct zwlr_layer_surface_v1 *layer;
    int cw, ch, configured, mapped, set_bs;
    wl_shmbuf draw[2];
    int pend[2][4], pend_all[2], dirty, empty;
    goome_canvas cv;
    uint32_t *cpx;
    int cbs;
    goome_rect *hits;
    int nhits, hitcap;

    struct zwlr_screencopy_frame_v1 *frame;
    wl_shmbuf cap;
    uint32_t cfmt, offered[8];
    int have_fmt, fw, fh, fstride, cstate, yinv, noffered, warned;
    uint32_t *conv, *img;
    size_t nconv, nimg;
} wl_out;

static struct {
    goome *g;
    struct wl_display *dpy;
    struct wl_registry *reg;
    struct wl_compositor *comp;
    struct wl_shm *shm;
    struct zwlr_layer_shell_v1 *ls;
    struct zwlr_screencopy_manager_v1 *sc;
    struct zxdg_output_manager_v1 *xom;
    struct wl_seat *seat;
    struct wl_pointer *ptr;
    struct wl_keyboard *kb;
    uint32_t comp_v, sc_v, ls_v;
    wl_out outs[GM_MAX_MON];
    int nouts, live, hypr;
    int m2s[GM_MAX_MON];

    struct wl_surface *psurf;
    double px, py;
    int held, scroll, got_discrete;
    struct wl_surface *arrow;
    wl_shmbuf arrow_buf;
    int arrow_k;

    int kb_on, kb_mods;
} wls;

static struct {
    void *lib, *ctx, *keymap, *state;
    void *(*context_new)(int);
    void (*context_unref)(void *);
    void *(*keymap_new_from_string)(void *, const char *, int, int);
    void (*keymap_unref)(void *);
    uint32_t (*mod_index)(void *, const char *);
    void *(*state_new)(void *);
    void (*state_unref)(void *);
    int (*key_get_utf8)(void *, uint32_t, char *, size_t);
    uint32_t (*key_get_sym)(void *, uint32_t);
    int (*update_mask)(void *, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
    uint32_t shift, ctrl, alt, super;
} wl_xkb;

static double wl_fx(wl_fixed_t f) { return f / 256.0; }

static int wl_memfd(void)
{
    int fd;
#ifdef __linux__
    fd = (int)syscall(SYS_memfd_create, "goome", 1u); // 1 is MFD_CLOEXEC
    if (fd >= 0) return fd;
#endif
    char name[64];
    snprintf(name, sizeof(name), "/goome-%d-%ld", (int)getpid(), goome_now_ms());
    fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
    if (fd >= 0) shm_unlink(name);
    return fd;
}

static void wl_buf_release(void *data, struct wl_buffer *buf) { ((wl_shmbuf *)data)->busy = 0; }
static const struct wl_buffer_listener wl_buf_listener = {wl_buf_release};

static int wl_make_shm(wl_shmbuf *b, int w, int h, int stride, uint32_t format)
{
    size_t size = (size_t)stride * h;
    int fd = wl_memfd();
    if (fd < 0) return 0;
    if (ftruncate(fd, (off_t)size) < 0) {
        close(fd);
        return 0;
    }
    void *data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        close(fd);
        return 0;
    }
    struct wl_shm_pool *pool = wl_shm_create_pool(wls.shm, fd, (int32_t)size);
    b->buf = wl_shm_pool_create_buffer(pool, 0, w, h, stride, format);
    wl_shm_pool_destroy(pool);
    close(fd);
    b->data = data, b->size = size, b->w = w, b->h = h, b->stride = stride, b->format = format, b->busy = 0;
    wl_buffer_add_listener(b->buf, &wl_buf_listener, b);
    return 1;
}

static void wl_free_shm(wl_shmbuf *b)
{
    if (b->buf) wl_buffer_destroy(b->buf);
    if (b->data) munmap(b->data, b->size);
    memset(b, 0, sizeof(*b));
}

static int wl_bufscale(wl_out *o)
{
    if (wls.comp_v < 3) return 1;
    return o->scale < 1 ? 1 : o->scale > 8 ? 8 : o->scale;
}

static void wl_sync(void)
{
    goome *g = wls.g;
    goome_monitor m[GM_MAX_MON];
    int n = 0;
    if (!g) return;
    for (int i = 0; i < wls.nouts; i++) {
        wl_out *o = &wls.outs[i];
        o->mon = -1;
        if (!o->alive || !o->done || (o->xdg && !o->have_logical)) continue;
        int sc = o->scale < 1 ? 1 : o->scale, side = o->transform & 1;
        int pw = side ? o->mh : o->mw, ph = side ? o->mw : o->mh;
        goome_rect a = o->have_logical ? (goome_rect){o->lx, o->ly, o->lw, o->lh} : (goome_rect){o->gx, o->gy, pw / sc, ph / sc};
        if (pw <= 0 || ph <= 0) pw = a.w * sc, ph = a.h * sc;
        if (a.w <= 0 || a.h <= 0) continue;
        goome_monitor *mm = &m[n];
        memset(mm, 0, sizeof(*mm));
        mm->id = (int)o->gname;
        snprintf(mm->name, sizeof(mm->name), "%s", o->name);
        mm->area = a;
        mm->pw = pw, mm->ph = ph;
        mm->scale = (double)pw / a.w;
        mm->rotation = (o->transform & 3) * 90 + (o->transform & 4 ? 360 : 0);
        o->area = a;
        o->mon = n;
        wls.m2s[n++] = i;
    }
    gm_set_monitors(g, m, n);
}

static wl_out *wl_slot(int mon)
{
    if (mon < 0 || !wls.g || mon >= wls.g->nmon) return NULL;
    wl_out *o = &wls.outs[wls.m2s[mon]];
    return o->alive && o->mon == mon ? o : NULL;
}

static wl_out *wl_by_surface(struct wl_surface *s)
{
    for (int i = 0; s && i < wls.nouts; i++)
        if (wls.outs[i].alive && wls.outs[i].surface == s) return &wls.outs[i];
    return NULL;
}

static void out_geometry(void *d, struct wl_output *w, int32_t x, int32_t y, int32_t pw, int32_t ph, int32_t sub, const char *make,
                         const char *model, int32_t transform)
{
    wl_out *o = d;
    o->gx = x, o->gy = y, o->transform = transform & 7;
}
static void out_mode(void *d, struct wl_output *w, uint32_t flags, int32_t mw, int32_t mh, int32_t refresh)
{
    wl_out *o = d;
    if (flags & WL_OUTPUT_MODE_CURRENT) o->mw = mw, o->mh = mh;
}
static void out_done(void *d, struct wl_output *w) { ((wl_out *)d)->done = 1; }
static void out_scale(void *d, struct wl_output *w, int32_t f) { ((wl_out *)d)->scale = f; }
static void out_name(void *d, struct wl_output *w, const char *n)
{
    wl_out *o = d;
    if (!o->xdg || !strncmp(o->name, "output-", 7)) snprintf(o->name, sizeof(o->name), "%s", n);
}
static void out_desc(void *d, struct wl_output *w, const char *t) {}
static const struct wl_output_listener wl_out_listener = {out_geometry, out_mode, out_done, out_scale, out_name, out_desc};

static void xo_pos(void *d, struct zxdg_output_v1 *x, int32_t px, int32_t py)
{
    wl_out *o = d;
    o->lx = px, o->ly = py;
}
static void xo_size(void *d, struct zxdg_output_v1 *x, int32_t w, int32_t h)
{
    wl_out *o = d;
    o->lw = w, o->lh = h, o->have_logical = 1;
}
static void xo_done(void *d, struct zxdg_output_v1 *x) {}
static void xo_name(void *d, struct zxdg_output_v1 *x, const char *n) { snprintf(((wl_out *)d)->name, 64, "%s", n); }
static void xo_desc(void *d, struct zxdg_output_v1 *x, const char *n) {}
static const struct zxdg_output_v1_listener wl_xo_listener = {xo_pos, xo_size, xo_done, xo_name, xo_desc};

static void wl_add_xdg(wl_out *o)
{
    if (o->xdg || !wls.xom) return;
    o->xdg = zxdg_output_manager_v1_get_xdg_output(wls.xom, o->output);
    zxdg_output_v1_add_listener(o->xdg, &wl_xo_listener, o);
    if (zxdg_output_v1_get_version(o->xdg) >= 3) o->done = 0;
}

static const char *wl_arrow_art[19] = {
    "X           ", "XX          ", "X.X         ", "X..X        ", "X...X       ", "X....X      ", "X.....X     ",
    "X......X    ", "X.......X   ", "X........X  ", "X.........X ", "X......XXXXX", "X...X..X    ", "X..XX..X    ",
    "X.X  X..X   ", "XX   X..X   ", "X     X..X  ", "      X..X  ", "       XX   "};

// our own arrow on every enter, or it stays whatever the last app had. games like roblox hide it
static void wl_set_arrow(uint32_t serial)
{
    if (!wls.ptr || !wls.comp || !wls.shm) return;
    int k = 1;
    for (int i = 0; i < wls.nouts; i++)
        if (wls.outs[i].alive && wl_bufscale(&wls.outs[i]) > k) k = wl_bufscale(&wls.outs[i]);
    if (k > 3) k = 3;
    if (!wls.arrow) wls.arrow = wl_compositor_create_surface(wls.comp);
    if (!wls.arrow) return;
    if (wls.arrow_k != k) {
        wl_free_shm(&wls.arrow_buf);
        if (!wl_make_shm(&wls.arrow_buf, 12 * k, 19 * k, 12 * k * 4, WL_SHM_FORMAT_ARGB8888)) return;
        uint32_t *px = wls.arrow_buf.data;
        for (int y = 0; y < 19 * k; y++)
            for (int x = 0; x < 12 * k; x++) {
                char ch = wl_arrow_art[y / k][x / k];
                px[y * 12 * k + x] = ch == 'X' ? 0xFF000000 : ch == '.' ? 0xFFFFFFFF : 0;
            }
        if (wls.comp_v >= 3) wl_surface_set_buffer_scale(wls.arrow, k);
        wl_surface_attach(wls.arrow, wls.arrow_buf.buf, 0, 0);
        wl_surface_damage(wls.arrow, 0, 0, 12, 19);
        wl_surface_commit(wls.arrow);
        wls.arrow_k = k;
    }
    wl_pointer_set_cursor(wls.ptr, serial, wls.arrow, 0, 0);
}

static void wl_ptr_ev(int type, int button, int dy)
{
    wl_out *o = wl_by_surface(wls.psurf);
    if (!o || !wls.g) return;
    goome_event e;
    memset(&e, 0, sizeof(e));
    e.type = type, e.x = o->area.x + wls.px, e.y = o->area.y + wls.py, e.mon = o->mon, e.button = button, e.dy = dy;
    gm_push(wls.g, &e);
}

static void ptr_enter(void *d, struct wl_pointer *p, uint32_t serial, struct wl_surface *s, wl_fixed_t x, wl_fixed_t y)
{
    wl_set_arrow(serial);
    wls.psurf = s, wls.px = wl_fx(x), wls.py = wl_fx(y);
    wl_ptr_ev(GOOME_MOTION, 0, 0);
}
static void ptr_leave(void *d, struct wl_pointer *p, uint32_t serial, struct wl_surface *s)
{
    wls.psurf = NULL, wls.held = 0, wls.scroll = 0;
}
static void ptr_motion(void *d, struct wl_pointer *p, uint32_t t, wl_fixed_t x, wl_fixed_t y)
{
    wls.px = wl_fx(x), wls.py = wl_fx(y);
    wl_ptr_ev(GOOME_MOTION, 0, 0);
}
static void ptr_button(void *d, struct wl_pointer *p, uint32_t serial, uint32_t t, uint32_t button, uint32_t state)
{
    int b = button == 0x110 ? 1 : button == 0x112 ? 2 : button == 0x111 ? 3 : 0; // BTN_LEFT, BTN_MIDDLE, BTN_RIGHT
    if (!b) return;
    int down = state == WL_POINTER_BUTTON_STATE_PRESSED;
    if (down) wls.held |= 1 << (b - 1);
    else wls.held &= ~(1 << (b - 1));
    wl_ptr_ev(down ? GOOME_DOWN : GOOME_UP, b, 0);
}
static void ptr_axis(void *d, struct wl_pointer *p, uint32_t t, uint32_t a, wl_fixed_t v)
{
    if (a != WL_POINTER_AXIS_VERTICAL_SCROLL || wls.got_discrete) return;
    wls.scroll += v;
    int n = 0;
    while (wls.scroll >= 10 * 256) n++, wls.scroll -= 10 * 256; // wl_fixed is 256 a unit, a wheel notch is 10-15
    while (wls.scroll <= -10 * 256) n--, wls.scroll += 10 * 256;
    if (n) wl_ptr_ev(GOOME_SCROLL, 0, n);
}
static void ptr_frame(void *d, struct wl_pointer *p) { wls.got_discrete = 0; }
static void ptr_axis_source(void *d, struct wl_pointer *p, uint32_t s) {}
static void ptr_axis_stop(void *d, struct wl_pointer *p, uint32_t t, uint32_t a) { wls.scroll = 0; }
static void ptr_axis_discrete(void *d, struct wl_pointer *p, uint32_t a, int32_t v)
{
    if (a != WL_POINTER_AXIS_VERTICAL_SCROLL || !v) return;
    wls.got_discrete = 1, wls.scroll = 0;
    wl_ptr_ev(GOOME_SCROLL, 0, v);
}
static void ptr_axis_v120(void *d, struct wl_pointer *p, uint32_t a, int32_t v) {}
static void ptr_axis_rel(void *d, struct wl_pointer *p, uint32_t a, uint32_t v) {}
static const struct wl_pointer_listener wl_ptr_listener = {ptr_enter, ptr_leave, ptr_motion, ptr_button, ptr_axis, ptr_frame,
                                                           ptr_axis_source, ptr_axis_stop, ptr_axis_discrete, ptr_axis_v120, ptr_axis_rel};

static void wl_xkb_free(void)
{
    if (wl_xkb.state) wl_xkb.state_unref(wl_xkb.state);
    if (wl_xkb.keymap) wl_xkb.keymap_unref(wl_xkb.keymap);
    if (wl_xkb.ctx && wl_xkb.context_unref) wl_xkb.context_unref(wl_xkb.ctx);
    if (wl_xkb.lib) dlclose(wl_xkb.lib);
    memset(&wl_xkb, 0, sizeof(wl_xkb));
}

static void wl_xkb_load(void)
{
    void *h = dlopen("libxkbcommon.so.0", RTLD_NOW | RTLD_LOCAL);
    if (!h) return;
    wl_xkb.lib = h;
    *(void **)&wl_xkb.context_new = dlsym(h, "xkb_context_new");
    *(void **)&wl_xkb.context_unref = dlsym(h, "xkb_context_unref");
    *(void **)&wl_xkb.keymap_new_from_string = dlsym(h, "xkb_keymap_new_from_string");
    *(void **)&wl_xkb.keymap_unref = dlsym(h, "xkb_keymap_unref");
    *(void **)&wl_xkb.mod_index = dlsym(h, "xkb_keymap_mod_get_index");
    *(void **)&wl_xkb.state_new = dlsym(h, "xkb_state_new");
    *(void **)&wl_xkb.state_unref = dlsym(h, "xkb_state_unref");
    *(void **)&wl_xkb.key_get_utf8 = dlsym(h, "xkb_state_key_get_utf8");
    *(void **)&wl_xkb.key_get_sym = dlsym(h, "xkb_state_key_get_one_sym");
    *(void **)&wl_xkb.update_mask = dlsym(h, "xkb_state_update_mask");
    if (wl_xkb.context_new && wl_xkb.keymap_new_from_string && wl_xkb.keymap_unref && wl_xkb.mod_index && wl_xkb.state_new &&
        wl_xkb.state_unref && wl_xkb.key_get_utf8 && wl_xkb.key_get_sym && wl_xkb.update_mask)
        wl_xkb.ctx = wl_xkb.context_new(0);
    if (!wl_xkb.ctx) {
        dlclose(h);
        memset(&wl_xkb, 0, sizeof(wl_xkb));
    }
}

static uint32_t wl_xkb_bit(const char *name)
{
    uint32_t i = wl_xkb.mod_index(wl_xkb.keymap, name);
    return i < 32 ? 1u << i : 0;
}

static void kb_keymap(void *d, struct wl_keyboard *k, uint32_t fmt, int32_t fd, uint32_t size)
{
    if (!wl_xkb.lib) wl_xkb_load();
    if (wl_xkb.ctx && fmt == WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1 && size > 0) {
        char *map = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (map != MAP_FAILED) {
            void *km = wl_xkb.keymap_new_from_string(wl_xkb.ctx, map, 1, 0);
            munmap(map, size);
            void *st = km ? wl_xkb.state_new(km) : NULL;
            if (st) {
                if (wl_xkb.state) wl_xkb.state_unref(wl_xkb.state);
                if (wl_xkb.keymap) wl_xkb.keymap_unref(wl_xkb.keymap);
                wl_xkb.keymap = km, wl_xkb.state = st;
                wl_xkb.shift = wl_xkb_bit("Shift"), wl_xkb.ctrl = wl_xkb_bit("Control"), wl_xkb.alt = wl_xkb_bit("Mod1"),
                wl_xkb.super = wl_xkb_bit("Mod4");
            } else if (km)
                wl_xkb.keymap_unref(km);
        }
    }
    close(fd);
}
static void kb_enter(void *d, struct wl_keyboard *k, uint32_t serial, struct wl_surface *s, struct wl_array *keys) {}
static void kb_leave(void *d, struct wl_keyboard *k, uint32_t serial, struct wl_surface *s) { wls.kb_mods = 0; }
static void kb_key(void *d, struct wl_keyboard *k, uint32_t serial, uint32_t t, uint32_t key, uint32_t state)
{
    static const char plain[] = "\0\x1b" "1234567890-=\b\t" "qwertyuiop[]\r\0" "asdfghjkl;'`\0\\" "zxcvbnm,./\0*\0 ";
    static const char shifted[] = "\0\x1b" "!@#$%^&*()_+\b\t" "QWERTYUIOP{}\r\0" "ASDFGHJKL:\"~\0|" "ZXCVBNM<>?\0*\0 ";
    static const struct {
        uint32_t code, sym;
        int key;
    } special[] = {{1, 0xff1b, GOOME_K_ESC}, {14, 0xff08, GOOME_K_BACKSPACE}, {15, 0xff09, GOOME_K_TAB}, {15, 0xfe20, GOOME_K_TAB},
                   {28, 0xff0d, GOOME_K_ENTER}, {96, 0xff8d, GOOME_K_ENTER}, {111, 0xffff, GOOME_K_DELETE}, {102, 0xff50, GOOME_K_HOME},
                   {103, 0xff52, GOOME_K_UP}, {104, 0xff55, GOOME_K_PGUP}, {105, 0xff51, GOOME_K_LEFT}, {106, 0xff53, GOOME_K_RIGHT},
                   {107, 0xff57, GOOME_K_END}, {108, 0xff54, GOOME_K_DOWN}, {109, 0xff56, GOOME_K_PGDN}, {110, 0xff63, GOOME_K_INSERT},
                   {87, 0xffc8, GOOME_K_F1 + 10}, {88, 0xffc9, GOOME_K_F1 + 11}};
    if (state != WL_KEYBOARD_KEY_STATE_PRESSED || !wls.kb_on || !wls.g) return;
    int mods = wls.kb_mods, kc = 0;
    goome_event e;
    memset(&e, 0, sizeof(e));
    e.type = GOOME_KEY, e.mon = -1;
    if (wl_xkb.state) { // by keysym, wtype and other virtual keyboards put keys on whatever keycode they like
        uint32_t sym = wl_xkb.key_get_sym(wl_xkb.state, key + 8);
        if (sym >= 0xffbe && sym <= 0xffc9) kc = GOOME_K_F1 + (int)(sym - 0xffbe);
        for (size_t i = 0; i < sizeof(special) / sizeof(special[0]) && !kc; i++)
            if (special[i].sym == sym) kc = special[i].key;
    } else {
        if (key >= 59 && key <= 68) kc = GOOME_K_F1 + (int)key - 59;
        for (size_t i = 0; i < sizeof(special) / sizeof(special[0]) && !kc; i++)
            if (special[i].code == key) kc = special[i].key;
    }
    char buf[8] = {0};
    if (!kc) {
        if (wl_xkb.state) {
            int n = wl_xkb.key_get_utf8(wl_xkb.state, key + 8, buf, sizeof(buf)); // xkb keycodes are evdev + 8
            if (n < 0 || n >= (int)sizeof(buf)) buf[0] = 0;
        } else if (key < sizeof(plain) - 1) {
            buf[0] = (mods & GOOME_SHIFT ? shifted : plain)[key];
        }
        unsigned char c = (unsigned char)buf[0];
        if (c && !buf[1] && c < 128) {
            int v = gm_combo_key(c, mods);
            kc = v & 0xFF, mods = v & ~0xFF;
        }
        if (c < 32 || c == 127 || (mods & (GOOME_CTRL | GOOME_ALT | GOOME_SUPER))) buf[0] = 0;
    }
    if (!kc && !buf[0]) return;
    e.key = kc, e.mods = mods;
    memcpy(e.text, buf, sizeof(e.text));
    e.text[sizeof(e.text) - 1] = 0;
    gm_push(wls.g, &e);
}
static void kb_mods(void *d, struct wl_keyboard *k, uint32_t serial, uint32_t dep, uint32_t lat, uint32_t lock, uint32_t grp)
{
    uint32_t on = dep | lat, sh = 1, ct = 4, al = 8, su = 64; // xkb's usual bits if theres no keymap
    if (wl_xkb.state) {
        wl_xkb.update_mask(wl_xkb.state, dep, lat, lock, 0, 0, grp);
        sh = wl_xkb.shift, ct = wl_xkb.ctrl, al = wl_xkb.alt, su = wl_xkb.super;
    }
    wls.kb_mods = (on & sh ? GOOME_SHIFT : 0) | (on & ct ? GOOME_CTRL : 0) | (on & al ? GOOME_ALT : 0) | (on & su ? GOOME_SUPER : 0);
}
static void kb_repeat(void *d, struct wl_keyboard *k, int32_t rate, int32_t delay) {}
static const struct wl_keyboard_listener wl_kb_listener = {kb_keymap, kb_enter, kb_leave, kb_key, kb_mods, kb_repeat};

static void seat_caps(void *d, struct wl_seat *s, uint32_t caps)
{
    if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !wls.kb) {
        wls.kb = wl_seat_get_keyboard(s);
        wl_keyboard_add_listener(wls.kb, &wl_kb_listener, NULL);
    } else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && wls.kb) {
        wl_keyboard_release(wls.kb);
        wls.kb = NULL;
    }
    if ((caps & WL_SEAT_CAPABILITY_POINTER) && !wls.ptr) {
        wls.ptr = wl_seat_get_pointer(s);
        wl_pointer_add_listener(wls.ptr, &wl_ptr_listener, NULL);
    } else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && wls.ptr) {
        wl_pointer_release(wls.ptr);
        wls.ptr = NULL, wls.psurf = NULL, wls.held = 0;
    }
}
static void seat_name(void *d, struct wl_seat *s, const char *n) {}
static const struct wl_seat_listener wl_seat_listener = {seat_caps, seat_name};

static void ls_configure(void *d, struct zwlr_layer_surface_v1 *l, uint32_t serial, uint32_t w, uint32_t h)
{
    wl_out *o = d;
    zwlr_layer_surface_v1_ack_configure(l, serial);
    o->cw = (int)w, o->ch = (int)h, o->configured = 1;
    if (o->cpx) o->dirty = 1;
}
static void wl_drop_layer(wl_out *o);
static void ls_closed(void *d, struct zwlr_layer_surface_v1 *l) { wl_drop_layer(d); }
static const struct zwlr_layer_surface_v1_listener wl_ls_listener = {ls_configure, ls_closed};

static void wl_layer_setup(wl_out *o)
{
    zwlr_layer_surface_v1_set_anchor(o->layer, 15);
    zwlr_layer_surface_v1_set_exclusive_zone(o->layer, -1); // or waybar pushes it around
    zwlr_layer_surface_v1_set_keyboard_interactivity(o->layer, wls.kb_on ? 1 : 0);
    zwlr_layer_surface_v1_set_size(o->layer, 0, 0);
}

static void wl_add_layer(wl_out *o)
{
    if (o->layer || !wls.ls || !o->alive) return;
    o->surface = wl_compositor_create_surface(wls.comp);
    o->layer = zwlr_layer_shell_v1_get_layer_surface(wls.ls, o->surface, o->output, ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY,
                                                     wls.g ? wls.g->title : "goome");
    zwlr_layer_surface_v1_add_listener(o->layer, &wl_ls_listener, o);
    wl_layer_setup(o);
    struct wl_region *empty = wl_compositor_create_region(wls.comp); // or the first frame eats clicks
    wl_surface_set_input_region(o->surface, empty);
    wl_region_destroy(empty);
    wl_surface_commit(o->surface);
    o->configured = o->mapped = o->set_bs = 0;
}

static void wl_drop_layer(wl_out *o)
{
    if (wls.psurf && wls.psurf == o->surface) wls.psurf = NULL, wls.held = 0;
    if (o->layer) zwlr_layer_surface_v1_destroy(o->layer);
    if (o->surface) wl_surface_destroy(o->surface);
    o->layer = NULL, o->surface = NULL;
    o->configured = o->mapped = o->set_bs = 0;
    wl_free_shm(&o->draw[0]);
    wl_free_shm(&o->draw[1]);
}

static void wl_free_out(wl_out *o)
{
    if (o->frame) zwlr_screencopy_frame_v1_destroy(o->frame);
    wl_drop_layer(o);
    if (o->xdg) zxdg_output_v1_destroy(o->xdg);
    if (o->output) {
        if (wl_output_get_version(o->output) >= 3) wl_output_release(o->output);
        else wl_output_destroy(o->output);
    }
    wl_free_shm(&o->cap);
    free(o->cpx), free(o->hits), free(o->conv), free(o->img);
    memset(o, 0, sizeof(*o));
    o->mon = -1;
}

static void reg_global(void *d, struct wl_registry *r, uint32_t name, const char *iface, uint32_t version)
{
    if (!strcmp(iface, "wl_compositor") && !wls.comp) {
        wls.comp_v = version < 4 ? version : 4;
        wls.comp = wl_registry_bind(r, name, &gm_wl_compositor_interface, wls.comp_v);
    } else if (!strcmp(iface, "wl_shm") && !wls.shm) {
        wls.shm = wl_registry_bind(r, name, &gm_wl_shm_interface, 1);
    } else if (!strcmp(iface, "zwlr_layer_shell_v1") && !wls.ls) {
        wls.ls_v = version < 4 ? version : 4;
        wls.ls = wl_registry_bind(r, name, &gm_zwlr_layer_shell_v1_interface, wls.ls_v);
    } else if (!strcmp(iface, "zwlr_screencopy_manager_v1") && !wls.sc) {
        wls.sc_v = version < 3 ? version : 3;
        wls.sc = wl_registry_bind(r, name, &gm_zwlr_screencopy_manager_v1_interface, wls.sc_v);
    } else if (!strcmp(iface, "zxdg_output_manager_v1") && !wls.xom) {
        wls.xom = wl_registry_bind(r, name, &gm_zxdg_output_manager_v1_interface, version < 3 ? version : 3);
        for (int i = 0; i < wls.nouts; i++)
            if (wls.outs[i].alive) wl_add_xdg(&wls.outs[i]);
    } else if (!strcmp(iface, "wl_seat") && !wls.seat) {
        wls.seat = wl_registry_bind(r, name, &gm_wl_seat_interface, version < 5 ? version : 5);
        wl_seat_add_listener(wls.seat, &wl_seat_listener, NULL);
    } else if (!strcmp(iface, "wl_output")) {
        int i = 0;
        while (i < wls.nouts && wls.outs[i].alive) i++; // reuse a dead slot so the others dont move
        if (i >= GM_MAX_MON) return;
        if (i == wls.nouts) wls.nouts++;
        wl_out *o = &wls.outs[i];
        memset(o, 0, sizeof(*o));
        o->alive = 1, o->gname = name, o->mon = -1, o->scale = 1;
        o->output = wl_registry_bind(r, name, &gm_wl_output_interface, version < 4 ? version : 4);
        wl_output_add_listener(o->output, &wl_out_listener, o);
        if (version < 2) o->done = 1;
        snprintf(o->name, sizeof(o->name), "output-%u", name);
        wl_add_xdg(o);
        if (wls.live) wl_add_layer(o);
    }
}

static void reg_remove(void *d, struct wl_registry *r, uint32_t name)
{
    for (int i = 0; i < wls.nouts; i++) {
        wl_out *o = &wls.outs[i];
        if (!o->alive || o->gname != name) continue;
        wl_free_out(o);
        while (wls.nouts && !wls.outs[wls.nouts - 1].alive) wls.nouts--;
        wl_sync();
        return;
    }
}
static const struct wl_registry_listener wl_reg_listener = {reg_global, reg_remove};

static void wl_flush_out(wl_out *o)
{
    if (!o->dirty || !o->layer || !o->configured || !o->cpx) return;
    if (o->empty) { // nothing drawn, give the buffers back so an idle overlay costs nothing
        if (o->mapped) {
            wl_surface_attach(o->surface, NULL, 0, 0);
            wl_surface_commit(o->surface);
            wl_free_shm(&o->draw[0]);
            wl_free_shm(&o->draw[1]);
            wl_layer_setup(o);
            wl_surface_commit(o->surface);
            o->mapped = o->configured = o->set_bs = 0;
        }
        o->dirty = 0;
        return;
    }
    goome_canvas *c = &o->cv;
    int bi = -1;
    for (int k = 0; k < 2 && bi < 0; k++) {
        if (!o->draw[k].buf && wl_make_shm(&o->draw[k], c->w, c->h, c->w * 4, WL_SHM_FORMAT_ARGB8888)) o->pend_all[k] = 1;
        if (o->draw[k].buf && !o->draw[k].busy) bi = k;
    }
    if (bi < 0) return;
    wl_shmbuf *b = &o->draw[bi];
    int *q = o->pend[bi], full = o->pend_all[bi];
    if (full) {
        memcpy(b->data, o->cpx, (size_t)c->w * c->h * 4);
        q[0] = 0, q[1] = 0, q[2] = c->w, q[3] = c->h;
    } else
        for (int y = q[1]; y < q[3]; y++)
            memcpy((uint32_t *)b->data + (size_t)y * c->w + q[0], o->cpx + (size_t)y * c->w + q[0], (size_t)(q[2] - q[0]) * 4);
    struct wl_region *region = wl_compositor_create_region(wls.comp);
    for (int h = 0; h < o->nhits; h++)
        wl_region_add(region, o->hits[h].x - o->area.x, o->hits[h].y - o->area.y, o->hits[h].w, o->hits[h].h);
    wl_surface_set_input_region(o->surface, region);
    wl_region_destroy(region);
    if (o->set_bs != o->cbs && wls.comp_v >= 3) wl_surface_set_buffer_scale(o->surface, o->cbs), o->set_bs = o->cbs;
    wl_surface_attach(o->surface, b->buf, 0, 0);
    if (q[2] > q[0] && q[3] > q[1]) {
        if (wls.comp_v >= 4)
            wl_surface_damage_buffer(o->surface, q[0], q[1], q[2] - q[0], q[3] - q[1]);
        else {
            int s = o->cbs;
            wl_surface_damage(o->surface, q[0] / s, q[1] / s, (q[2] + s - 1) / s - q[0] / s, (q[3] + s - 1) / s - q[1] / s);
        }
    }
    wl_surface_commit(o->surface);
    q[0] = q[1] = q[2] = q[3] = 0;
    o->pend_all[bi] = 0;
    b->busy = 1;
    o->mapped = 1, o->dirty = 0;
}

static void wl_after(void)
{
    wl_sync();
    for (int i = 0; i < wls.nouts; i++)
        if (wls.outs[i].alive && wls.outs[i].dirty) wl_flush_out(&wls.outs[i]);
    wl_display_flush(wls.dpy);
}

static int wl_pump(int ms)
{
    if (!wls.dpy) return -1;
    while (wl_display_prepare_read(wls.dpy) != 0)
        if (wl_display_dispatch_pending(wls.dpy) < 0) return -1;
    if (wls.g && wls.g->qlen) ms = 0;
    if (wl_display_flush(wls.dpy) < 0 && errno != EAGAIN) {
        wl_display_cancel_read(wls.dpy);
        return -1;
    }
    struct pollfd pfd = {wl_display_get_fd(wls.dpy), POLLIN, 0};
    int r = poll(&pfd, 1, ms < 0 ? 0 : ms);
    if (r > 0) {
        if (wl_display_read_events(wls.dpy) < 0) return -1;
    } else
        wl_display_cancel_read(wls.dpy);
    if (wl_display_dispatch_pending(wls.dpy) < 0) return -1;
    wl_after();
    return r > 0 ? r : 0;
}

static int wl_round(void)
{
    int r = wl_display_roundtrip(wls.dpy);
    wl_after();
    return r;
}

static int wl_hypr_num(const char **p, int *ok)
{
    *ok = **p == '-' || (**p >= '0' && **p <= '9');
    return gm_num(p);
}

// hyprland with software cursors draws the mouse into screencopy even when we ask it not to, so ask where it is
static int wl_hypr_cursor(double *x, double *y)
{
    const char *sig = gm_env("HYPRLAND_INSTANCE_SIGNATURE"), *rt = gm_env("XDG_RUNTIME_DIR");
    if (!sig || strchr(sig, '/')) return 0;
    struct sockaddr_un a;
    for (int k = 0; k < 2; k++) {
        if (k == 0 && !rt) continue;
        memset(&a, 0, sizeof(a));
        a.sun_family = AF_UNIX;
        int n = k == 0 ? snprintf(a.sun_path, sizeof(a.sun_path), "%s/hypr/%s/.socket.sock", rt, sig)
                       : snprintf(a.sun_path, sizeof(a.sun_path), "/tmp/hypr/%s/.socket.sock", sig);
        if (n < 0 || n >= (int)sizeof(a.sun_path)) continue;
        int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
        if (fd < 0) return 0;
        struct timeval tv = {0, 100000};
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        char buf[64] = {0};
        int got = 0;
        if (connect(fd, (struct sockaddr *)&a, sizeof(a)) == 0 && write(fd, "cursorpos", 9) == 9) {
            ssize_t r;
            while (got < (int)sizeof(buf) - 1 && (r = read(fd, buf + got, sizeof(buf) - 1 - (size_t)got)) > 0) got += (int)r;
        }
        close(fd);
        if (got <= 0) continue;
        const char *p = buf;
        int ok1, ok2, cx = wl_hypr_num(&p, &ok1);
        while (*p == ',' || *p == ' ') p++;
        int cy = wl_hypr_num(&p, &ok2);
        if (!ok1 || !ok2) return 0;
        *x = cx, *y = cy;
        return 1;
    }
    return 0;
}

static int wl_cursor_size(void)
{
    const char *e = gm_env("HYPRCURSOR_SIZE");
    if (!e) e = gm_env("XCURSOR_SIZE");
    int v = e ? gm_num(&e) : 0;
    return v >= 8 && v <= 256 ? v : 24;
}

static void wl_teardown(void)
{
    for (int i = 0; i < wls.nouts; i++)
        if (wls.outs[i].alive) wl_free_out(&wls.outs[i]);
    if (wls.dpy) {
        if (wls.ptr) wl_pointer_release(wls.ptr);
        if (wls.kb) wl_keyboard_release(wls.kb);
        if (wls.arrow) wl_surface_destroy(wls.arrow);
        wl_free_shm(&wls.arrow_buf);
        if (wls.seat) {
            if (wl_seat_get_version(wls.seat) >= 5) wl_seat_release(wls.seat);
            else wl_seat_destroy(wls.seat);
        }
        if (wls.sc) zwlr_screencopy_manager_v1_destroy(wls.sc);
        if (wls.ls) {
            if (wls.ls_v >= 3) zwlr_layer_shell_v1_destroy(wls.ls);
            else wl_proxy_destroy((struct wl_proxy *)wls.ls);
        }
        if (wls.xom) zxdg_output_manager_v1_destroy(wls.xom);
        if (wls.shm) wl_shm_destroy(wls.shm);
        if (wls.comp) wl_compositor_destroy(wls.comp);
        if (wls.reg) wl_registry_destroy(wls.reg);
        wl_display_flush(wls.dpy);
        wl_display_disconnect(wls.dpy);
    }
    wl_xkb_free();
    void *lib = wl_dl.lib;
    memset(&wls, 0, sizeof(wls));
    memset(&wl_dl, 0, sizeof(wl_dl));
    if (lib) dlclose(lib);
}

static int wl_load(goome *g)
{
    void *h = dlopen("libwayland-client.so.0", RTLD_NOW | RTLD_LOCAL);
    if (!h) return gm_fail(g, GOOME_ENODISPLAY, "cant find libwayland-client.so.0");
    wl_dl.lib = h;
    static const char *names[] = {"wl_proxy_marshal_flags", "wl_proxy_add_listener", "wl_proxy_get_version", "wl_proxy_destroy",
                                  "wl_proxy_set_user_data", "wl_proxy_get_user_data", "wl_display_connect", "wl_display_disconnect",
                                  "wl_display_roundtrip", "wl_display_dispatch_pending", "wl_display_prepare_read",
                                  "wl_display_read_events", "wl_display_cancel_read", "wl_display_flush", "wl_display_get_fd"};
    void **slots[] = {(void **)&wl_dl.proxy_marshal_flags, (void **)&wl_dl.proxy_add_listener, (void **)&wl_dl.proxy_get_version,
                      (void **)&wl_dl.proxy_destroy, (void **)&wl_dl.proxy_set_user_data, (void **)&wl_dl.proxy_get_user_data,
                      (void **)&wl_dl.display_connect, (void **)&wl_dl.display_disconnect, (void **)&wl_dl.display_roundtrip,
                      (void **)&wl_dl.display_dispatch_pending, (void **)&wl_dl.display_prepare_read,
                      (void **)&wl_dl.display_read_events, (void **)&wl_dl.display_cancel_read, (void **)&wl_dl.display_flush,
                      (void **)&wl_dl.display_get_fd};
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++)
        if (!(*slots[i] = dlsym(h, names[i]))) return gm_fail(g, GOOME_ENODISPLAY, "libwayland-client.so.0 has no %s (older than 1.20?)", names[i]);
    return GOOME_OK;
}

static int wl_connect(goome *g)
{
    if (!gm_env("WAYLAND_DISPLAY") && !gm_env("WAYLAND_SOCKET")) return gm_fail(g, GOOME_ENODISPLAY, "WAYLAND_DISPLAY isnt set");
    memset(&wls, 0, sizeof(wls));
    int e = wl_load(g);
    if (e != GOOME_OK) return e;
    wls.dpy = wl_display_connect(NULL);
    if (!wls.dpy) return gm_fail(g, GOOME_ENODISPLAY, "cant connect to the wayland display %s", gm_env("WAYLAND_DISPLAY") ? gm_env("WAYLAND_DISPLAY") : "");
    wls.g = g;
    wls.reg = wl_display_get_registry(wls.dpy);
    wl_registry_add_listener(wls.reg, &wl_reg_listener, NULL);
    if (wl_round() < 0) return gm_fail(g, GOOME_ENODISPLAY, "the wayland display hung up on us");
    if (!wls.comp || !wls.shm) return gm_fail(g, GOOME_ENODISPLAY, "this compositor has no wl_compositor or wl_shm");
    if (wl_round() < 0 || wl_round() < 0) return gm_fail(g, GOOME_ENODISPLAY, "the wayland display hung up on us");
    return GOOME_OK;
}

static gm_ov wayland_ov;

static int wayland_ov_open(goome *g)
{
    int e = wl_connect(g);
    if (e == GOOME_OK && !wls.ls) e = gm_fail(g, GOOME_ENOTSUP, "this compositor has no layer-shell (zwlr_layer_shell_v1)");
    if (e != GOOME_OK) {
        wl_teardown();
        return e;
    }
    wls.live = 1;
    for (int i = 0; i < wls.nouts; i++) wl_add_layer(&wls.outs[i]);
    if (wl_round() < 0 || wl_round() < 0) e = gm_fail(g, GOOME_ENODISPLAY, "the wayland display hung up on us");
    else if (g->nmon < 1) e = gm_fail(g, GOOME_ENODISPLAY, "the compositor didnt tell me about any outputs");
    if (e != GOOME_OK) {
        wl_teardown();
        return e;
    }
    double x, y;
    wls.hypr = wl_hypr_cursor(&x, &y);
    wayland_ov.caps = GOOME_CAP_KEYBOARD | (wls.hypr ? GOOME_CAP_GLOBAL_POINTER : 0);
    return GOOME_OK;
}

static goome_canvas *wayland_ov_canvas(goome *g, int mon)
{
    wl_out *o = wl_slot(mon);
    if (!o) return NULL;
    if (!o->layer) {
        wl_add_layer(o);
        wl_display_flush(wls.dpy);
        return NULL;
    }
    if (o->cw <= 0 || o->ch <= 0) return NULL;
    int bs = wl_bufscale(o), w = o->cw * bs, h = o->ch * bs;
    if (!o->cpx || o->cv.w != w || o->cv.h != h || o->cbs != bs) {
        uint32_t *px = calloc((size_t)w * h, 4);
        if (!px) return NULL;
        free(o->cpx);
        o->cpx = px, o->cbs = bs;
        wl_free_shm(&o->draw[0]);
        wl_free_shm(&o->draw[1]);
        gm_canvas_reset(&o->cv, px, w, h, w, o->area.x, o->area.y, bs, mon);
    }
    o->cv.ox = o->area.x, o->cv.oy = o->area.y, o->cv.mon = mon;
    return &o->cv;
}

static int wayland_ov_commit(goome *g, int mon, goome_canvas *c, const goome_rect *hits, int n)
{
    wl_out *o = wl_slot(mon);
    if (!o || c != &o->cv) return gm_fail(g, GOOME_EGONE, "monitor %d is gone", mon);
    int x0, y0, x1, y1, dmg = gm_canvas_damage(c, &x0, &y0, &x1, &y1), empty = c->dx1 <= c->dx0;
    if (!dmg && empty == o->empty && n == o->nhits && (!n || !memcmp(hits, o->hits, sizeof(*hits) * (size_t)n))) return GOOME_OK;
    if (n > o->hitcap) {
        goome_rect *nh = realloc(o->hits, sizeof(*nh) * (size_t)n);
        if (!nh) return gm_fail(g, GOOME_ENOMEM, "out of memory for %d hit rects", n);
        o->hits = nh, o->hitcap = n;
    }
    if (n) memcpy(o->hits, hits, sizeof(*hits) * (size_t)n);
    o->nhits = n, o->empty = empty, o->dirty = 1;
    for (int k = 0; k < 2 && dmg; k++) { // the other one's a frame behind
        int *q = o->pend[k];
        if (q[2] <= q[0]) q[0] = x0, q[1] = y0, q[2] = x1, q[3] = y1;
        if (x0 < q[0]) q[0] = x0;
        if (y0 < q[1]) q[1] = y0;
        if (x1 > q[2]) q[2] = x1;
        if (y1 > q[3]) q[3] = y1;
    }
    wl_flush_out(o);
    wl_display_flush(wls.dpy);
    return GOOME_OK;
}

static int wayland_ov_wait(goome *g, int ms) { return wl_pump(ms) < 0 ? -1 : 0; }

static int wayland_ov_pointer(goome *g, double *x, double *y, int *buttons)
{
    if (wls.dpy) wl_display_dispatch_pending(wls.dpy);
    wl_out *o = wl_by_surface(wls.psurf);
    if (o) {
        *x = o->area.x + wls.px, *y = o->area.y + wls.py, *buttons = wls.held;
        return 1;
    }
    if (wls.hypr && wl_hypr_cursor(x, y)) return 1;
    return 0;
}

static int wayland_ov_keyboard(goome *g, int on)
{
    wls.kb_on = on;
    for (int i = 0; i < wls.nouts; i++) {
        wl_out *o = &wls.outs[i];
        if (!o->alive || !o->layer) continue;
        zwlr_layer_surface_v1_set_keyboard_interactivity(o->layer, on ? 1 : 0); // exclusive only while needed or the game loses its keys
        wl_surface_commit(o->surface);
    }
    wl_display_flush(wls.dpy);
    return GOOME_OK;
}

static void wayland_ov_close(goome *g) { wl_teardown(); }

static gm_ov wayland_ov = {"wayland", wayland_ov_open, wayland_ov_canvas, wayland_ov_commit, wayland_ov_wait, wayland_ov_pointer,
                           wayland_ov_keyboard, NULL, wayland_ov_close, GOOME_CAP_KEYBOARD};

const gm_ov *gm_ov_wayland(void) { return &wayland_ov; }

static int wl_is_ok_fmt(uint32_t f)
{
    switch (f) {
    case WL_SHM_FORMAT_XRGB8888: case WL_SHM_FORMAT_ARGB8888: case WL_SHM_FORMAT_XBGR8888: case WL_SHM_FORMAT_ABGR8888:
    case WL_SHM_FORMAT_XRGB2101010: case WL_SHM_FORMAT_ARGB2101010: case WL_SHM_FORMAT_XBGR2101010: case WL_SHM_FORMAT_ABGR2101010:
    case WL_SHM_FORMAT_RGB888: case WL_SHM_FORMAT_BGR888: case WL_SHM_FORMAT_RGB565: return 1;
    }
    return 0;
}

static void fr_buffer(void *d, struct zwlr_screencopy_frame_v1 *f, uint32_t fmt, uint32_t w, uint32_t h, uint32_t stride)
{
    wl_out *o = d;
    if (o->noffered < 8) o->offered[o->noffered++] = fmt;
    if (o->have_fmt || !wl_is_ok_fmt(fmt)) return; // have_fmt and not cfmt, ARGB8888 is 0
    o->cfmt = fmt, o->have_fmt = 1, o->fw = (int)w, o->fh = (int)h, o->fstride = (int)stride;
    if (wls.sc_v < 3) {
        if (o->cap.w != o->fw || o->cap.h != o->fh || o->cap.format != fmt || o->cap.stride != o->fstride) {
            wl_free_shm(&o->cap);
            if (!wl_make_shm(&o->cap, o->fw, o->fh, o->fstride, fmt)) {
                o->cstate = -1;
                return;
            }
        }
        zwlr_screencopy_frame_v1_copy(f, o->cap.buf);
    }
}
static void fr_flags(void *d, struct zwlr_screencopy_frame_v1 *f, uint32_t flags)
{
    ((wl_out *)d)->yinv = !!(flags & ZWLR_SCREENCOPY_FRAME_V1_FLAGS_Y_INVERT);
}
static void fr_ready(void *d, struct zwlr_screencopy_frame_v1 *f, uint32_t a, uint32_t b, uint32_t c)
{
    wl_out *o = d;
    o->cstate = 1;
    zwlr_screencopy_frame_v1_destroy(f);
    o->frame = NULL;
}
static void fr_failed(void *d, struct zwlr_screencopy_frame_v1 *f)
{
    wl_out *o = d;
    o->cstate = -1;
    zwlr_screencopy_frame_v1_destroy(f);
    o->frame = NULL;
}
static void fr_damage(void *d, struct zwlr_screencopy_frame_v1 *f, uint32_t x, uint32_t y, uint32_t w, uint32_t h) {}
static void fr_dmabuf(void *d, struct zwlr_screencopy_frame_v1 *f, uint32_t fmt, uint32_t w, uint32_t h) {}
static void fr_buffer_done(void *d, struct zwlr_screencopy_frame_v1 *f)
{
    wl_out *o = d;
    if (!o->have_fmt) {
        o->cstate = -2;
        return;
    }
    if (o->cap.w != o->fw || o->cap.h != o->fh || o->cap.format != o->cfmt || o->cap.stride != o->fstride) {
        wl_free_shm(&o->cap);
        if (!wl_make_shm(&o->cap, o->fw, o->fh, o->fstride, o->cfmt)) {
            o->cstate = -1;
            return;
        }
    }
    zwlr_screencopy_frame_v1_copy(f, o->cap.buf);
}
static const struct zwlr_screencopy_frame_v1_listener wl_fr_listener = {fr_buffer, fr_flags, fr_ready, fr_failed,
                                                                       fr_damage, fr_dmabuf, fr_buffer_done};

static void wl_conv_row(const uint8_t *s, uint32_t fmt, int w, uint32_t *d)
{
    for (int x = 0; x < w; x++) {
        uint32_t v, r, g, b;
        if (fmt == WL_SHM_FORMAT_RGB888 || fmt == WL_SHM_FORMAT_BGR888) {
            const uint8_t *p = s + x * 3;
            r = fmt == WL_SHM_FORMAT_RGB888 ? p[2] : p[0], g = p[1], b = fmt == WL_SHM_FORMAT_RGB888 ? p[0] : p[2];
        } else if (fmt == WL_SHM_FORMAT_RGB565) {
            v = (uint32_t)s[x * 2] | (uint32_t)s[x * 2 + 1] << 8;
            r = (v >> 11) & 31, g = (v >> 5) & 63, b = v & 31;
            r = r << 3 | r >> 2, g = g << 2 | g >> 4, b = b << 3 | b >> 2;
        } else {
            const uint8_t *p = s + x * 4;
            v = (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
            switch (fmt) {
            case WL_SHM_FORMAT_XBGR8888: case WL_SHM_FORMAT_ABGR8888: r = v & 255, g = (v >> 8) & 255, b = (v >> 16) & 255; break;
            case WL_SHM_FORMAT_XRGB2101010: case WL_SHM_FORMAT_ARGB2101010:
                r = (v >> 22) & 255, g = (v >> 12) & 255, b = (v >> 2) & 255;
                break;
            case WL_SHM_FORMAT_XBGR2101010: case WL_SHM_FORMAT_ABGR2101010: b = (v >> 22) & 255, g = (v >> 12) & 255, r = (v >> 2) & 255; break;
            default: r = (v >> 16) & 255, g = (v >> 8) & 255, b = v & 255; // XRGB8888 is B G R X in memory
            }
        }
        d[x] = 0xFF000000u | r << 16 | g << 8 | b;
    }
}

static void wl_untransform(const uint32_t *src, int sw, int sh, int t, uint32_t *dst, int *dw, int *dh)
{
    int rot = t & 3, w = rot & 1 ? sh : sw, h = rot & 1 ? sw : sh;
    for (int y = 0; y < h; y++) {
        uint32_t *out = dst + (size_t)y * w;
        for (int x0 = 0; x0 < w; x0++) {
            int x = t & 4 ? w - 1 - x0 : x0, sx, sy;
            switch (rot) {
            case 1: sx = y, sy = sh - 1 - x; break;
            case 2: sx = sw - 1 - x, sy = sh - 1 - y; break;
            case 3: sx = sw - 1 - y, sy = x; break;
            default: sx = x, sy = y;
            }
            out[x0] = src[(size_t)sy * sw + sx];
        }
    }
    *dw = w, *dh = h;
}

static int wl_grow(uint32_t **p, size_t *have, size_t n)
{
    if (n <= *have) return 1;
    uint32_t *q = realloc(*p, n * 4);
    if (!q) return 0;
    *p = q, *have = n;
    return 1;
}

static int wlr_cap_probe(goome *g) { return wls.dpy && wls.sc; }

static int wlr_cap_open(goome *g)
{
    if (!wls.dpy || !wls.sc) return gm_fail(g, GOOME_ENOTSUP, "this compositor has no wlr-screencopy");
    return GOOME_OK;
}

static int wlr_cap_grab(goome *g, int mon, goome_rect r, goome_image *img)
{
    wl_out *o = wl_slot(mon);
    if (!o || !wls.sc) return gm_fail(g, GOOME_EGONE, "monitor %d is gone", mon);
    uint32_t gname = o->gname;
    goome_rect a = o->area;
    int full = r.x == a.x && r.y == a.y && r.w == a.w && r.h == a.h;
    if (o->frame) zwlr_screencopy_frame_v1_destroy(o->frame);
    o->cstate = 0, o->have_fmt = 0, o->noffered = 0, o->yinv = 0;
    o->frame = full ? zwlr_screencopy_manager_v1_capture_output(wls.sc, 0, o->output) // 0 = no cursor
                    : zwlr_screencopy_manager_v1_capture_output_region(wls.sc, 0, o->output, r.x - a.x, r.y - a.y, r.w, r.h);
    zwlr_screencopy_frame_v1_add_listener(o->frame, &wl_fr_listener, o);
    long deadline = goome_now_ms() + 600; // a screen thats asleep may never answer
    while (o->alive && o->gname == gname && o->cstate == 0) {
        long left = deadline - goome_now_ms();
        if (left <= 0) break;
        if (wl_pump((int)left) < 0) return gm_fail(g, GOOME_ENODISPLAY, "lost the wayland display");
    }
    if (!o->alive || o->gname != gname) return gm_fail(g, GOOME_EGONE, "monitor %d got unplugged", mon);
    if (o->frame) {
        zwlr_screencopy_frame_v1_destroy(o->frame);
        o->frame = NULL;
        if (o->cstate == 0) return gm_fail(g, GOOME_EAGAIN, "%s never sent a frame (asleep?)", o->name);
    }
    if (o->cstate == -2) {
        char list[128] = "";
        for (int i = 0; i < o->noffered; i++) {
            uint32_t f = o->offered[i];
            size_t l = strlen(list);
            if (f <= 1) snprintf(list + l, sizeof(list) - l, " %s", f ? "XRGB8888" : "ARGB8888");
            else snprintf(list + l, sizeof(list) - l, " %c%c%c%c", (char)f, (char)(f >> 8), (char)(f >> 16), (char)(f >> 24));
        }
        return gm_fail(g, GOOME_ENOTSUP, "%s only offers screen formats i cant read:%s", o->name, list);
    }
    if (o->cstate != 1) return gm_fail(g, GOOME_EDENIED, "the compositor wont let me grab %s", o->name);
    int w = o->fw, h = o->fh;
    if (w <= 0 || h <= 0 || !wl_grow(&o->conv, &o->nconv, (size_t)w * h)) return gm_fail(g, GOOME_ENOMEM, "out of memory for a %dx%d grab", w, h);
    for (int y = 0; y < h; y++) {
        const uint8_t *src = (const uint8_t *)o->cap.data + (size_t)(o->yinv ? h - 1 - y : y) * o->fstride;
        wl_conv_row(src, o->cfmt, w, o->conv + (size_t)y * w);
    }
    uint32_t *px = o->conv;
    if (o->transform) {
        if (!wl_grow(&o->img, &o->nimg, (size_t)w * h)) return gm_fail(g, GOOME_ENOMEM, "out of memory for a %dx%d grab", w, h);
        wl_untransform(o->conv, w, h, o->transform, o->img, &w, &h);
        px = o->img;
    }
    img->px = px, img->w = w, img->h = h, img->stride = w, img->area = r;
    img->has_self = 1; // cant keep layer surfaces out, hyprland noscreenshare just paints them black
    double cx, cy;
    if (wls.hypr && wl_hypr_cursor(&cx, &cy)) {
        int sz = wl_cursor_size();
        goome_rect cr = {(int)cx, (int)cy, sz, sz};
        if (cr.x < r.x + r.w && cr.y < r.y + r.h && cr.x + cr.w > r.x && cr.y + cr.h > r.y) img->has_cursor = 1, img->cursor = cr;
    }
    return GOOME_OK;
}

static void wlr_cap_close(goome *g)
{
    for (int i = 0; i < wls.nouts; i++) {
        wl_out *o = &wls.outs[i];
        if (o->frame) zwlr_screencopy_frame_v1_destroy(o->frame);
        o->frame = NULL;
        wl_free_shm(&o->cap);
        free(o->conv), free(o->img);
        o->conv = o->img = NULL, o->nconv = o->nimg = 0;
    }
}

static const gm_cap wlr_cap = {"wlr", wlr_cap_probe, wlr_cap_open, wlr_cap_grab, wlr_cap_close, GOOME_CAP_CAPTURE};
const gm_cap *gm_cap_wlr(void) { return &wlr_cap; }

#undef WL_MARSHAL_FLAG_DESTROY
#undef wl_proxy_marshal_flags
#undef wl_proxy_add_listener
#undef wl_proxy_get_version
#undef wl_proxy_destroy
#undef wl_proxy_set_user_data
#undef wl_proxy_get_user_data
#undef wl_display_connect
#undef wl_display_disconnect
#undef wl_display_roundtrip
#undef wl_display_dispatch_pending
#undef wl_display_prepare_read
#undef wl_display_read_events
#undef wl_display_cancel_read
#undef wl_display_flush
#undef wl_display_get_fd
#undef GM_WL_PRIVATE

#endif
