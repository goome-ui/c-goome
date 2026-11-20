#include "internal.h"

#if defined(GM_LINUX)

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <time.h>
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

typedef struct {
    goome *g;
    void *keymap, *state;
    uint32_t shift, ctrl, alt, super, serial;
    int on, mods, ref, rate, delay, rkey;
    long rnext;
    goome_event rev;
} wl_keys;

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

    wl_keys keys;
    int lib_ref, grab;
} wls;

static struct {
    void *lib, *ctx;
    int users;
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
} wl_xkb;

static int wl_lib_users;

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

static int wl_make_shm(struct wl_shm *shm, wl_shmbuf *b, int w, int h, int stride, uint32_t format)
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
    struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, (int32_t)size);
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
        if (!wl_make_shm(wls.shm, &wls.arrow_buf, 12 * k, 19 * k, 12 * k * 4, WL_SHM_FORMAT_ARGB8888)) return;
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

static void wl_keys_free(wl_keys *k)
{
    if (k->state) wl_xkb.state_unref(k->state);
    if (k->keymap) wl_xkb.keymap_unref(k->keymap);
    k->state = k->keymap = NULL;
    if (k->ref && !--wl_xkb.users) {
        if (wl_xkb.ctx && wl_xkb.context_unref) wl_xkb.context_unref(wl_xkb.ctx);
        if (wl_xkb.lib) dlclose(wl_xkb.lib);
        memset(&wl_xkb, 0, sizeof(wl_xkb));
    }
    k->ref = 0;
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

static uint32_t wl_xkb_bit(void *km, const char *name)
{
    uint32_t i = wl_xkb.mod_index(km, name);
    return i < 32 ? 1u << i : 0;
}

static void kb_keymap(void *d, struct wl_keyboard *kbd, uint32_t fmt, int32_t fd, uint32_t size)
{
    wl_keys *k = d;
    if (!wl_xkb.lib) wl_xkb_load();
    if (wl_xkb.ctx && !k->ref) k->ref = 1, wl_xkb.users++;
    if (wl_xkb.ctx && fmt == WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1 && size > 0) {
        char *map = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (map != MAP_FAILED) {
            void *km = wl_xkb.keymap_new_from_string(wl_xkb.ctx, map, 1, 0);
            munmap(map, size);
            void *st = km ? wl_xkb.state_new(km) : NULL;
            if (st) {
                if (k->state) wl_xkb.state_unref(k->state);
                if (k->keymap) wl_xkb.keymap_unref(k->keymap);
                k->keymap = km, k->state = st;
                k->shift = wl_xkb_bit(km, "Shift"), k->ctrl = wl_xkb_bit(km, "Control"), k->alt = wl_xkb_bit(km, "Mod1"), k->super = wl_xkb_bit(km, "Mod4");
            } else if (km)
                wl_xkb.keymap_unref(km);
        }
    }
    close(fd);
}
static void kb_enter(void *d, struct wl_keyboard *k, uint32_t serial, struct wl_surface *s, struct wl_array *keys) { ((wl_keys *)d)->serial = serial; }
static void kb_leave(void *d, struct wl_keyboard *k, uint32_t serial, struct wl_surface *s) { ((wl_keys *)d)->mods = 0, ((wl_keys *)d)->rkey = 0; }
static void kb_key(void *d, struct wl_keyboard *kbd, uint32_t serial, uint32_t t, uint32_t key, uint32_t state)
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
                   {87, 0xffc8, GOOME_K_F1 + 10}, {88, 0xffc9, GOOME_K_F1 + 11},
                   {0, 0xff95, GOOME_K_HOME}, {0, 0xff96, GOOME_K_LEFT}, {0, 0xff97, GOOME_K_UP}, {0, 0xff98, GOOME_K_RIGHT},
                   {0, 0xff99, GOOME_K_DOWN}, {0, 0xff9a, GOOME_K_PGUP}, {0, 0xff9b, GOOME_K_PGDN}, {0, 0xff9c, GOOME_K_END},
                   {0, 0xff9e, GOOME_K_INSERT}, {0, 0xff9f, GOOME_K_DELETE}};
    wl_keys *k = d;
    k->serial = serial;
    if (state != WL_KEYBOARD_KEY_STATE_PRESSED) {
        if (k->rkey == (int)key + 1) k->rkey = 0;
        return;
    }
    if (!k->on || !k->g) return;
    int mods = k->mods, kc = 0;
    goome_event e;
    memset(&e, 0, sizeof(e));
    e.type = GOOME_KEY, e.mon = -1;
    if (k->state) { // by keysym, wtype and other virtual keyboards put keys on whatever keycode they like
        uint32_t sym = wl_xkb.key_get_sym(k->state, key + 8);
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
        if (k->state) {
            int n = wl_xkb.key_get_utf8(k->state, key + 8, buf, sizeof(buf)); // xkb keycodes are evdev + 8
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
    gm_push(k->g, &e);
    if (k->rate > 0) k->rev = e, k->rkey = (int)key + 1, k->rnext = goome_now_ms() + k->delay;
}
static void kb_mods(void *d, struct wl_keyboard *kbd, uint32_t serial, uint32_t dep, uint32_t lat, uint32_t lock, uint32_t grp)
{
    wl_keys *k = d;
    uint32_t on = dep | lat, sh = 1, ct = 4, al = 8, su = 64; // xkb's usual bits if theres no keymap
    if (k->state) {
        wl_xkb.update_mask(k->state, dep, lat, lock, 0, 0, grp);
        sh = k->shift, ct = k->ctrl, al = k->alt, su = k->super;
    }
    k->mods = (on & sh ? GOOME_SHIFT : 0) | (on & ct ? GOOME_CTRL : 0) | (on & al ? GOOME_ALT : 0) | (on & su ? GOOME_SUPER : 0);
}
static void kb_repeat(void *d, struct wl_keyboard *kbd, int32_t rate, int32_t delay)
{
    wl_keys *k = d;
    if (k->rate >= 0) k->rate = rate, k->delay = delay;
}
static const struct wl_keyboard_listener wl_kb_listener = {kb_keymap, kb_enter, kb_leave, kb_key, kb_mods, kb_repeat};

static void seat_caps(void *d, struct wl_seat *s, uint32_t caps)
{
    if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !wls.kb) {
        wls.kb = wl_seat_get_keyboard(s);
        wl_keyboard_add_listener(wls.kb, &wl_kb_listener, &wls.keys);
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
    zwlr_layer_surface_v1_set_keyboard_interactivity(o->layer, wls.keys.on || wls.grab ? 1 : 0);
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

static int wlt_reg(struct wl_display *dpy, struct wl_registry *r, uint32_t name, const char *iface, uint32_t version);
static void wlt_drop(struct wl_display *dpy);

static void reg_global(void *d, struct wl_registry *r, uint32_t name, const char *iface, uint32_t version)
{
    if (wlt_reg(wls.dpy, r, name, iface, version)) return;
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
        if (!o->draw[k].buf && wl_make_shm(wls.shm, &o->draw[k], c->w, c->h, c->w * 4, WL_SHM_FORMAT_ARGB8888)) o->pend_all[k] = 1;
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

static void wl_lib_put(void)
{
    if (wl_lib_users > 0 && --wl_lib_users) return;
    void *lib = wl_dl.lib;
    memset(&wl_dl, 0, sizeof(wl_dl));
    wl_lib_users = 0;
    if (lib) dlclose(lib);
}

static void wl_teardown(void)
{
    for (int i = 0; i < wls.nouts; i++)
        if (wls.outs[i].alive) wl_free_out(&wls.outs[i]);
    if (wls.dpy) {
        wlt_drop(wls.dpy);
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
    wl_keys_free(&wls.keys);
    int ref = wls.lib_ref;
    memset(&wls, 0, sizeof(wls));
    if (ref) wl_lib_put();
}

static int wl_load(goome *g)
{
    if (wl_dl.lib) return wl_lib_users++, GOOME_OK;
    void *h = dlopen("libwayland-client.so.0", RTLD_NOW | RTLD_LOCAL);
    if (!h) return gm_fail(g, GOOME_ENODISPLAY, "cant find libwayland-client.so.0");
    wl_dl.lib = h, wl_lib_users = 1;
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
        if (!(*slots[i] = dlsym(h, names[i]))) {
            wl_lib_put();
            return gm_fail(g, GOOME_ENODISPLAY, "libwayland-client.so.0 has no %s (older than 1.20?)", names[i]);
        }
    return GOOME_OK;
}

static int wl_connect(goome *g)
{
    if (!gm_env("WAYLAND_DISPLAY") && !gm_env("WAYLAND_SOCKET")) return gm_fail(g, GOOME_ENODISPLAY, "WAYLAND_DISPLAY isnt set");
    memset(&wls, 0, sizeof(wls));
    int e = wl_load(g);
    if (e != GOOME_OK) return e;
    wls.lib_ref = 1;
    wls.keys.g = g, wls.keys.rate = -1;
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
static const gm_ov wl_wov;
static int wl_win_open(goome *g);
static void wl_win_close(goome *g);

static int wayland_ov_open(goome *g)
{
    if (g->opts.mode == GOOME_MODE_WINDOW) {
        g->ov = &wl_wov;
        return wl_win_open(g);
    }
    if (g->opts.mode != GOOME_MODE_OVERLAY) return gm_fail(g, GOOME_ENOTSUP, "wayland does overlay and window mode only");
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

static int wl_interactive(void)
{
    int on = wls.keys.on || wls.grab, n = 0;
    for (int i = 0; i < wls.nouts; i++) {
        wl_out *o = &wls.outs[i];
        if (!o->alive || !o->layer) continue;
        zwlr_layer_surface_v1_set_keyboard_interactivity(o->layer, on ? 1 : 0); // exclusive only while needed or the game loses its keys
        wl_surface_commit(o->surface);
        n++;
    }
    wl_display_flush(wls.dpy);
    return n;
}

static int wayland_ov_keyboard(goome *g, int on)
{
    wls.keys.on = on;
    wl_interactive();
    return GOOME_OK;
}

static int wayland_ov_grab(goome *g, int on)
{
    if (g->ov_priv) return GOOME_OK;
    wls.grab = on;
    if (!wl_interactive() && on) return gm_fail(g, GOOME_ENOTSUP, "wayland: no layer surface to take the keyboard, and the pointer cant be grabbed");
    return GOOME_OK;
}

static void wayland_ov_close(goome *g)
{
    if (g->ov_priv) wl_win_close(g);
    else wl_teardown();
}

static gm_ov wayland_ov = {"wayland", wayland_ov_open, wayland_ov_canvas, wayland_ov_commit, wayland_ov_wait, wayland_ov_pointer,
                           wayland_ov_keyboard, NULL, wayland_ov_close, GOOME_CAP_KEYBOARD, NULL, NULL, NULL, NULL, wayland_ov_grab};

const gm_ov *gm_ov_wayland(void) { return &wayland_ov; }

typedef struct {
    struct wl_data_offer *o;
    int text, uri;
    char mime[32];
} wl_offer;

typedef struct {
    goome *g;
    struct wl_display *dpy;
    struct wl_registry *reg;
    struct wl_compositor *comp;
    struct wl_shm *shm;
    struct wl_seat *seat;
    struct wl_pointer *ptr;
    struct wl_keyboard *kb;
    struct xdg_wm_base *wm;
    struct zxdg_decoration_manager_v1 *decm;
    struct wp_cursor_shape_manager_v1 *csm;
    struct wp_cursor_shape_device_v1 *csd;
    struct wl_data_device_manager *ddm;
    struct wl_data_device *dd;
    struct wp_fractional_scale_manager_v1 *fsm;
    struct wp_viewporter *vp;
    uint32_t comp_v, ddm_v;
    struct {
        struct wl_output *o;
        uint32_t name;
        int scale, on;
    } outs[GM_MAX_MON];
    int nouts;

    struct wl_surface *surf;
    struct xdg_surface *xs;
    struct xdg_toplevel *top;
    struct zxdg_toplevel_decoration_v1 *deco;
    struct wp_fractional_scale_v1 *frac;
    struct wp_viewport *view;
    int configured, ssd, cssd, cw, ch, cstate, w, h, maxed, full, active, mini, shown, pref_scale, frac120, bs;
    double sc;

    wl_shmbuf buf[2];
    int pend[2][4], pend_all[2], dirty;
    uint32_t *px;
    int pw, ph, ox, oy, m, bh, lw, lh;
    double lsc;
    goome_canvas cv, dcv;
    void *font;
    int font_tried;

    uint32_t enter_serial, serial;
    double mx, my;
    int inside, held, area, edges, hover, pressed, shape, scroll, got_discrete;
    long last_bar;
    struct wl_surface *csurf;
    void *ctheme;
    int ctheme_k;
    wl_shmbuf arrow;
    int arrow_k;
    wl_keys keys;

    struct wl_data_source *src;
    wl_offer *sel, *dnd;
    double dx, dy;
    char *drops;
    int dlen, doff;
} wl_win;

static struct {
    void *lib;
    void *(*theme_load)(const char *, int, struct wl_shm *);
    void (*theme_destroy)(void *);
    void *(*get_cursor)(void *, const char *);
    struct wl_buffer *(*image_buffer)(void *);
    int tried;
} wl_cur;

typedef struct {
    unsigned int count;
    struct {
        uint32_t width, height, hx, hy, delay;
    } **images;
    char *name;
} wl_cursor_t;

enum { WL_BAR = 32, WL_EDGE = 10, WL_BTN = 24 };

static void wl_win_mons(wl_win *w)
{
    goome_monitor m;
    memset(&m, 0, sizeof(m));
    snprintf(m.name, sizeof(m.name), "%s", w->g->title);
    m.area = (goome_rect){0, 0, w->w, w->h};
    m.scale = w->sc, m.pw = (int)(w->w * w->sc + 0.5), m.ph = (int)(w->h * w->sc + 0.5);
    gm_set_monitors(w->g, &m, 1);
}

static void wl_win_scale(wl_win *w)
{
    double sc = 1;
    if (w->frac120 > 0) sc = w->frac120 / 120.0;
    else if (w->pref_scale > 0) sc = w->pref_scale;
    else {
        int k = 0, any = 0;
        for (int i = 0; i < w->nouts; i++)
            if (w->outs[i].o && w->outs[i].on) any = 1, k = w->outs[i].scale > k ? w->outs[i].scale : k;
        for (int i = 0; !any && i < w->nouts; i++)
            if (w->outs[i].o && w->outs[i].scale > k) k = w->outs[i].scale;
        sc = k < 1 ? 1 : k > 8 ? 8 : k;
    }
    w->bs = w->frac120 > 0 ? 1 : (int)sc;
    if (sc != w->sc) w->sc = sc, w->dirty = 1, wl_win_mons(w);
}

static void wl_win_layout(wl_win *w, int *m, int *bh)
{
    int deco = !w->ssd && !(w->g->opts.window_flags & GOOME_WINDOW_BORDERLESS) && !w->full;
    *bh = deco ? WL_BAR : 0;
    *m = deco && !w->maxed ? WL_EDGE : 0;
}

static void wl_win_damage(wl_win *w, int x0, int y0, int x1, int y1)
{
    for (int k = 0; k < 2; k++) {
        int *q = w->pend[k];
        if (q[2] <= q[0]) q[0] = x0, q[1] = y0, q[2] = x1, q[3] = y1;
        if (x0 < q[0]) q[0] = x0;
        if (y0 < q[1]) q[1] = y0;
        if (x1 > q[2]) q[2] = x1;
        if (y1 > q[3]) q[3] = y1;
    }
    w->dirty = 1;
}

static void wl_win_deco(wl_win *w);

static int wl_win_build(wl_win *w)
{
    int m, bh;
    wl_win_layout(w, &m, &bh);
    double sc = w->sc;
    int ox = (int)(m * sc + 0.5), oy = (int)((m + bh) * sc + 0.5), cpw = (int)(w->w * sc + 0.5), cph = (int)(w->h * sc + 0.5);
    int pw = cpw + 2 * ox, ph = cph + oy + ox;
    if (w->px && pw == w->pw && ph == w->ph && ox == w->ox && oy == w->oy && sc == w->lsc) return 1;
    uint32_t *px = calloc((size_t)pw * ph, 4);
    if (!px) return 0;
    free(w->px);
    w->px = px, w->pw = pw, w->ph = ph, w->ox = ox, w->oy = oy, w->m = m, w->bh = bh, w->lsc = sc;
    w->lw = w->w + 2 * m, w->lh = w->h + bh + 2 * m;
    wl_free_shm(&w->buf[0]);
    wl_free_shm(&w->buf[1]);
    w->pend_all[0] = w->pend_all[1] = 1;
    gm_canvas_reset(&w->cv, px + (size_t)oy * pw + ox, cpw, cph, pw, 0, 0, sc, 0);
    gm_canvas_reset(&w->dcv, px, pw, ph, pw, -m, -(m + bh), sc, 0);
    wl_win_deco(w);
    w->dirty = 1;
    return 1;
}

#ifndef GOOME_NO_DRAW
static void wl_win_strip(wl_win *w, double x, double y, double ww, double hh)
{
    goome_canvas *c = &w->dcv;
    double r = 8;
    goome_clip_push(c, x, y, ww, hh);
    goome_erase(c, x, y, ww, hh);
    goome_draw_shadow(c, 0, -w->bh, w->w, w->h + w->bh, r, WL_EDGE, w->active ? 0x66000000 : 0x38000000);
    goome_draw_rrect_line(c, -1, -w->bh - 1, w->w + 2, w->h + w->bh + 2, (goome_radii){r + 1, r + 1, 0, 0}, 1, 0x30000000);
    goome_clip_pop(c);
}
#endif

static void wl_win_deco(wl_win *w)
{
    if (!w->px) return;
    if (w->bh > 0) wl_win_damage(w, 0, 0, w->pw, w->oy);
    if (w->m > 0) wl_win_damage(w, 0, 0, w->pw, w->ph);
    if (w->bh <= 0) return;
    int act = w->active;
#ifndef GOOME_NO_DRAW
    goome_canvas *c = &w->dcv;
    double m = w->m, bh = w->bh, W = w->w, H = w->h;
    if (m > 0) {
        wl_win_strip(w, -m, -m - bh, W + 2 * m, m);
        wl_win_strip(w, -m, -bh, m, H + bh);
        wl_win_strip(w, W, -bh, m, H + bh);
        wl_win_strip(w, -m, H, W + 2 * m, m);
    }
    if (!w->font_tried) w->font = goome_font_system(NULL), w->font_tried = 1;
    double r = m > 0 ? 8 : 0;
    goome_clip_push(c, 0, -bh, W, bh);
    goome_erase(c, 0, -bh, W, bh);
    goome_draw_rrect(c, 0, -bh, W, bh, (goome_radii){r, r, 0, 0}, act ? 0xFF2B2D33 : 0xFF222327);
    goome_draw_rect(c, 0, -1, W, 1, 0xFF18191C);
    double bx = W - 8 - WL_BTN, by = -bh + (bh - WL_BTN) / 2, cy = by + WL_BTN / 2.0;
    uint32_t fg = act ? 0xFFDADBDF : 0xFF7D8088;
    for (int i = 0; i < 3; i++) {
        double x = bx - i * (WL_BTN + 6), cx = x + WL_BTN / 2.0;
        int hov = w->hover == i + 1, prs = hov && w->pressed == i + 1;
        if (hov) goome_draw_circle(c, cx, cy, WL_BTN / 2.0, i == 0 ? (prs ? 0xFFB83A3E : 0xFFE5484D) : (prs ? 0xFF33363D : 0xFF3D4048));
        uint32_t gc = hov && i == 0 ? 0xFFFFFFFF : fg;
        if (i == 0) {
            goome_draw_line(c, cx - 4.5, cy - 4.5, cx + 4.5, cy + 4.5, 1.5, gc);
            goome_draw_line(c, cx + 4.5, cy - 4.5, cx - 4.5, cy + 4.5, 1.5, gc);
        } else if (i == 1 && w->maxed) {
            goome_draw_frame(c, cx - 2.5, cy - 5.5, 8, 8, 1.2, gc);
            goome_erase(c, cx - 5.5, cy - 2.5, 8, 8);
            if (hov) goome_draw_rect(c, cx - 5.5, cy - 2.5, 8, 8, prs ? 0xFF33363D : 0xFF3D4048);
            else goome_draw_rect(c, cx - 5.5, cy - 2.5, 8, 8, act ? 0xFF2B2D33 : 0xFF222327);
            goome_draw_frame(c, cx - 5.5, cy - 2.5, 8, 8, 1.2, gc);
        } else if (i == 1)
            goome_draw_frame(c, cx - 4.5, cy - 4.5, 9, 9, 1.2, gc);
        else
            goome_draw_rect(c, cx - 4.5, cy + 3, 9, 1.4, gc);
    }
    double th = 0, tw = goome_text_measure(w->font, 14, w->g->title, &th);
    double lim = bx - 2 * (WL_BTN + 6) - 8, tx = (W - tw) / 2;
    if (tx + tw > lim) tx = lim - tw;
    if (tx < 12) tx = 12;
    goome_clip_push(c, 12, -bh, lim - 12 > 0 ? lim - 12 : 0, bh);
    goome_text(c, w->font, 14, tx, -bh + (bh - goome_text_line_height(w->font, 14)) / 2, w->g->title, act ? 0xFFE6E7EA : 0xFF8A8D95);
    goome_clip_pop(c);
    goome_clip_pop(c);
#else
    for (int y = (int)(w->m * w->sc); y < w->oy; y++)
        for (int x = w->ox; x < w->pw - w->ox; x++) w->px[(size_t)y * w->pw + x] = act ? 0xFF2B2D33 : 0xFF222327;
#endif
}

static void wl_win_flush(wl_win *w)
{
    if (!w->dirty || !w->surf || !w->configured || !w->px) return;
    int bi = -1;
    for (int k = 0; k < 2 && bi < 0; k++) {
        if (!w->buf[k].buf && wl_make_shm(w->shm, &w->buf[k], w->pw, w->ph, w->pw * 4, WL_SHM_FORMAT_ARGB8888)) w->pend_all[k] = 1;
        if (w->buf[k].buf && !w->buf[k].busy) bi = k;
    }
    if (bi < 0) return;
    wl_shmbuf *b = &w->buf[bi];
    int *q = w->pend[bi];
    if (w->pend_all[bi]) q[0] = 0, q[1] = 0, q[2] = w->pw, q[3] = w->ph;
    if (q[0] < 0) q[0] = 0;
    if (q[1] < 0) q[1] = 0;
    if (q[2] > w->pw) q[2] = w->pw;
    if (q[3] > w->ph) q[3] = w->ph;
    int opaque = !(w->g->opts.window_flags & GOOME_WINDOW_TRANSPARENT), cx0 = w->ox, cx1 = w->ox + w->cv.w, cy0 = w->oy, cy1 = w->oy + w->cv.h;
    for (int y = q[1]; y < q[3]; y++) {
        uint32_t *d = (uint32_t *)b->data + (size_t)y * w->pw;
        memcpy(d + q[0], w->px + (size_t)y * w->pw + q[0], (size_t)(q[2] - q[0]) * 4);
        if (opaque && y >= cy0 && y < cy1)
            for (int x = q[0] > cx0 ? q[0] : cx0, e = q[2] < cx1 ? q[2] : cx1; x < e; x++) d[x] |= 0xFF000000u;
    }
    if (w->view) wp_viewport_set_destination(w->view, w->lw, w->lh);
    else if (w->comp_v >= 3) wl_surface_set_buffer_scale(w->surf, w->bs);
    if (w->xs) xdg_surface_set_window_geometry(w->xs, w->m, w->m, w->w, w->h + w->bh);
    wl_surface_attach(w->surf, b->buf, 0, 0);
    if (q[2] > q[0] && q[3] > q[1]) {
        if (w->comp_v >= 4) wl_surface_damage_buffer(w->surf, q[0], q[1], q[2] - q[0], q[3] - q[1]);
        else wl_surface_damage(w->surf, 0, 0, w->lw, w->lh);
    }
    wl_surface_commit(w->surf);
    q[0] = q[1] = q[2] = q[3] = 0;
    w->pend_all[bi] = 0, b->busy = 1, w->dirty = 0;
}

static const char *wl_cur_names[GOOME_CURSOR_HIDDEN][3] = {
    {"default", "left_ptr", NULL}, {"text", "xterm", NULL}, {"pointer", "hand2", NULL}, {"move", "fleur", NULL},
    {"ew-resize", "sb_h_double_arrow", NULL}, {"ns-resize", "sb_v_double_arrow", NULL}, {"nwse-resize", "bottom_right_corner", NULL},
    {"nesw-resize", "bottom_left_corner", NULL}, {"wait", "watch", NULL}, {"not-allowed", "crossed_circle", NULL}};

static void wl_win_cursor_set(wl_win *w, int shape)
{
    static const uint32_t css[GOOME_CURSOR_HIDDEN] = {1, 9, 4, 13, 26, 27, 29, 28, 6, 15};
    w->shape = shape;
    if (!w->ptr || !w->enter_serial) return;
    if (shape == GOOME_CURSOR_HIDDEN) {
        wl_pointer_set_cursor(w->ptr, w->enter_serial, NULL, 0, 0);
        return;
    }
    if (w->csd) {
        wp_cursor_shape_device_v1_set_shape(w->csd, w->enter_serial, css[shape]);
        return;
    }
    int k = (int)(w->sc + 0.99);
    if (k < 1) k = 1;
    if (!w->csurf) w->csurf = wl_compositor_create_surface(w->comp);
    if (!wl_cur.tried) {
        wl_cur.tried = 1;
        void *h = dlopen("libwayland-cursor.so.0", RTLD_NOW | RTLD_LOCAL);
        if (h) {
            *(void **)&wl_cur.theme_load = dlsym(h, "wl_cursor_theme_load");
            *(void **)&wl_cur.theme_destroy = dlsym(h, "wl_cursor_theme_destroy");
            *(void **)&wl_cur.get_cursor = dlsym(h, "wl_cursor_theme_get_cursor");
            *(void **)&wl_cur.image_buffer = dlsym(h, "wl_cursor_image_get_buffer");
            if (wl_cur.theme_load && wl_cur.theme_destroy && wl_cur.get_cursor && wl_cur.image_buffer) wl_cur.lib = h;
            else dlclose(h);
        }
    }
    if (wl_cur.lib && w->ctheme_k != k) {
        if (w->ctheme) wl_cur.theme_destroy(w->ctheme);
        w->ctheme = wl_cur.theme_load(gm_env("XCURSOR_THEME"), wl_cursor_size() * k, w->shm);
        w->ctheme_k = k;
    }
    wl_cursor_t *cur = NULL;
    for (int i = 0; w->ctheme && !cur && wl_cur_names[shape][i]; i++) cur = wl_cur.get_cursor(w->ctheme, wl_cur_names[shape][i]);
    for (int i = 0; w->ctheme && !cur && wl_cur_names[0][i]; i++) cur = wl_cur.get_cursor(w->ctheme, wl_cur_names[0][i]);
    if (cur && cur->count) {
        struct wl_buffer *b = wl_cur.image_buffer(cur->images[0]);
        if (b) {
            if (w->comp_v >= 3) wl_surface_set_buffer_scale(w->csurf, k);
            wl_surface_attach(w->csurf, b, 0, 0);
            wl_surface_damage(w->csurf, 0, 0, 1 << 14, 1 << 14);
            wl_surface_commit(w->csurf);
            wl_pointer_set_cursor(w->ptr, w->enter_serial, w->csurf, (int32_t)(cur->images[0]->hx / k), (int32_t)(cur->images[0]->hy / k));
            return;
        }
    }
    if (k > 3) k = 3;
    if (w->arrow_k != k) {
        wl_free_shm(&w->arrow);
        if (!wl_make_shm(w->shm, &w->arrow, 12 * k, 19 * k, 12 * k * 4, WL_SHM_FORMAT_ARGB8888)) return;
        uint32_t *px = w->arrow.data;
        for (int y = 0; y < 19 * k; y++)
            for (int x = 0; x < 12 * k; x++) {
                char ch = wl_arrow_art[y / k][x / k];
                px[y * 12 * k + x] = ch == 'X' ? 0xFF000000 : ch == '.' ? 0xFFFFFFFF : 0;
            }
        w->arrow_k = k;
    }
    if (w->comp_v >= 3) wl_surface_set_buffer_scale(w->csurf, k);
    wl_surface_attach(w->csurf, w->arrow.buf, 0, 0);
    wl_surface_damage(w->csurf, 0, 0, 12, 19);
    wl_surface_commit(w->csurf);
    wl_pointer_set_cursor(w->ptr, w->enter_serial, w->csurf, 0, 0);
}

static void wl_win_where(wl_win *w)
{
    double cx = w->mx - w->m, cy = w->my - w->m - w->bh;
    int hov = 0;
    w->edges = 0;
    if (cx >= 0 && cy >= 0 && cx < w->w && cy < w->h) w->area = 0;
    else if (cx >= 0 && cx < w->w && cy >= -w->bh && cy < 0) {
        w->area = 1;
        double bx = w->w - 8 - WL_BTN, by = -w->bh + (w->bh - WL_BTN) / 2;
        for (int i = 0; i < 3; i++)
            if (cx >= bx - i * (WL_BTN + 6) && cx < bx - i * (WL_BTN + 6) + WL_BTN && cy >= by && cy < by + WL_BTN) hov = i + 1;
    } else {
        w->area = 2;
        int e = (cx < 0 ? 4 : 0) | (cx >= w->w ? 8 : 0) | (cy < -w->bh ? 1 : 0) | (cy >= w->h ? 2 : 0);
        if (e & 3) e |= cx < 16 ? 4 : cx >= w->w - 16 ? 8 : 0;
        if (e & 12) e |= cy < -w->bh + 16 ? 1 : cy >= w->h - 16 ? 2 : 0;
        w->edges = e;
    }
    if (hov != w->hover) {
        w->hover = hov;
        wl_win_deco(w);
    }
    int want = GOOME_CURSOR_ARROW, e = w->edges;
    if (w->area == 0 || (w->held && !w->area)) want = w->g->cursor;
    else if (w->area == 2) want = (e == 5 || e == 10) ? GOOME_CURSOR_RESIZE_NWSE : (e == 9 || e == 6) ? GOOME_CURSOR_RESIZE_NESW : (e & 3) ? GOOME_CURSOR_RESIZE_NS : GOOME_CURSOR_RESIZE_EW;
    if (want != w->shape) wl_win_cursor_set(w, want);
}

static void wl_win_ptr_ev(wl_win *w, int type, int button, int dy)
{
    goome_event e;
    memset(&e, 0, sizeof(e));
    e.type = type, e.x = w->mx - w->m, e.y = w->my - w->m - w->bh, e.button = button, e.dy = dy, e.mods = w->keys.mods;
    gm_push(w->g, &e);
}

static void wp_enter(void *d, struct wl_pointer *p, uint32_t serial, struct wl_surface *s, wl_fixed_t x, wl_fixed_t y)
{
    wl_win *w = d;
    if (s != w->surf) return;
    w->enter_serial = serial, w->inside = 1, w->mx = wl_fx(x), w->my = wl_fx(y), w->shape = -1;
    wl_win_where(w);
    if (w->area == 0) wl_win_ptr_ev(w, GOOME_MOTION, 0, 0);
}
static void wp_leave(void *d, struct wl_pointer *p, uint32_t serial, struct wl_surface *s)
{
    wl_win *w = d;
    w->inside = 0, w->held = 0, w->scroll = 0, w->pressed = 0;
    if (w->hover) w->hover = 0, wl_win_deco(w);
}
static void wp_motion(void *d, struct wl_pointer *p, uint32_t t, wl_fixed_t x, wl_fixed_t y)
{
    wl_win *w = d;
    w->mx = wl_fx(x), w->my = wl_fx(y);
    wl_win_where(w);
    if (w->held || w->area == 0) wl_win_ptr_ev(w, GOOME_MOTION, 0, 0);
}

static void wl_win_toggle_max(wl_win *w)
{
    if (!w->top) return;
    if (w->maxed) xdg_toplevel_unset_maximized(w->top);
    else xdg_toplevel_set_maximized(w->top);
}

static void wp_button(void *d, struct wl_pointer *p, uint32_t serial, uint32_t t, uint32_t button, uint32_t state)
{
    wl_win *w = d;
    int b = button == 0x110 ? 1 : button == 0x112 ? 2 : button == 0x111 ? 3 : 0, down = state == WL_POINTER_BUTTON_STATE_PRESSED;
    if (!b) return;
    if (down) w->serial = serial;
    if (!down && (w->held & (1 << (b - 1)))) {
        w->held &= ~(1 << (b - 1));
        wl_win_ptr_ev(w, GOOME_UP, b, 0);
        return;
    }
    if (!down) {
        int was = w->pressed;
        w->pressed = 0;
        if (was && was == w->hover) {
            if (was == 1) {
                goome_event e;
                memset(&e, 0, sizeof(e));
                e.type = GOOME_CLOSE;
                gm_push(w->g, &e);
            } else if (was == 2)
                wl_win_toggle_max(w);
            else if (w->top)
                xdg_toplevel_set_minimized(w->top), w->mini = 1;
        }
        if (was) wl_win_deco(w);
        return;
    }
    if (w->area == 0) {
        w->held |= 1 << (b - 1);
        wl_win_ptr_ev(w, GOOME_DOWN, b, 0);
    } else if (w->area == 1 && w->top) {
        if (b == 3) xdg_toplevel_show_window_menu(w->top, w->seat, serial, (int)(w->mx - w->m), (int)(w->my - w->m));
        else if (b == 1 && w->hover) w->pressed = w->hover, wl_win_deco(w);
        else if (b == 1) {
            long now = goome_now_ms();
            if (now - w->last_bar < 400) wl_win_toggle_max(w), w->last_bar = 0;
            else xdg_toplevel_move(w->top, w->seat, serial), w->last_bar = now;
        }
    } else if (w->area == 2 && b == 1 && w->top && w->edges)
        xdg_toplevel_resize(w->top, w->seat, serial, (uint32_t)w->edges);
}
static void wp_axis(void *d, struct wl_pointer *p, uint32_t t, uint32_t a, wl_fixed_t v)
{
    wl_win *w = d;
    if (a != WL_POINTER_AXIS_VERTICAL_SCROLL || w->got_discrete || w->area) return;
    w->scroll += v;
    int n = 0;
    while (w->scroll >= 10 * 256) n++, w->scroll -= 10 * 256;
    while (w->scroll <= -10 * 256) n--, w->scroll += 10 * 256;
    if (n) wl_win_ptr_ev(w, GOOME_SCROLL, 0, n);
}
static void wp_frame(void *d, struct wl_pointer *p) { ((wl_win *)d)->got_discrete = 0; }
static void wp_axis_stop(void *d, struct wl_pointer *p, uint32_t t, uint32_t a) { ((wl_win *)d)->scroll = 0; }
static void wp_axis_discrete(void *d, struct wl_pointer *p, uint32_t a, int32_t v)
{
    wl_win *w = d;
    if (a != WL_POINTER_AXIS_VERTICAL_SCROLL || !v || w->area) return;
    w->got_discrete = 1, w->scroll = 0;
    wl_win_ptr_ev(w, GOOME_SCROLL, 0, v);
}
static const struct wl_pointer_listener wl_win_ptr_listener = {wp_enter, wp_leave, wp_motion, wp_button, wp_axis, wp_frame,
                                                               ptr_axis_source, wp_axis_stop, wp_axis_discrete, ptr_axis_v120, ptr_axis_rel};

static char *wl_read_fd(int fd, int ms, size_t *len)
{
    size_t cap = 4096, n = 0;
    char *r = malloc(cap);
    long end = goome_now_ms() + ms;
    while (r) {
        long left = end - goome_now_ms();
        struct pollfd p = {fd, POLLIN, 0};
        if (left <= 0 || poll(&p, 1, (int)left) <= 0) break;
        if (n + 4096 > cap) {
            char *m = realloc(r, cap * 2);
            if (!m) break;
            r = m, cap *= 2;
        }
        ssize_t got = read(fd, r + n, cap - n - 1);
        if (got < 0 && errno == EINTR) continue;
        if (got <= 0) {
            r[n] = 0, *len = n;
            close(fd);
            return r;
        }
        n += (size_t)got;
    }
    free(r);
    close(fd);
    *len = 0;
    return NULL;
}

static void wl_offer_free(wl_offer *o)
{
    if (!o) return;
    wl_data_offer_destroy(o->o);
    free(o);
}

static void do_offer(void *d, struct wl_data_offer *o, const char *mime)
{
    wl_offer *f = d;
    int t = !strcmp(mime, "text/plain;charset=utf-8") ? 3 : !strcmp(mime, "UTF8_STRING") ? 2 : !strcmp(mime, "text/plain") ? 1 : 0;
    if (t > f->text) f->text = t, snprintf(f->mime, sizeof(f->mime), "%s", mime);
    if (!strcmp(mime, "text/uri-list")) f->uri = 1;
}
static void do_source_actions(void *d, struct wl_data_offer *o, uint32_t a) {}
static void do_action(void *d, struct wl_data_offer *o, uint32_t a) {}
static const struct wl_data_offer_listener wl_offer_listener = {do_offer, do_source_actions, do_action};

static void dd_data_offer(void *d, struct wl_data_device *dd, struct wl_data_offer *o)
{
    wl_offer *f = calloc(1, sizeof(*f));
    if (!f) {
        wl_data_offer_destroy(o);
        return;
    }
    f->o = o;
    wl_data_offer_add_listener(o, &wl_offer_listener, f);
}
static void dd_enter(void *d, struct wl_data_device *dd, uint32_t serial, struct wl_surface *s, wl_fixed_t x, wl_fixed_t y, struct wl_data_offer *o)
{
    wl_win *w = d;
    if (w->dnd) wl_offer_free(w->dnd);
    w->dnd = o ? wl_data_offer_get_user_data(o) : NULL;
    w->dx = wl_fx(x) - w->m, w->dy = wl_fx(y) - w->m - w->bh;
    if (!w->dnd) return;
    wl_data_offer_accept(o, serial, w->dnd->uri ? "text/uri-list" : NULL);
    if (w->ddm_v >= 3) wl_data_offer_set_actions(o, w->dnd->uri ? 1 : 0, 1);
}
static void dd_leave(void *d, struct wl_data_device *dd)
{
    wl_win *w = d;
    wl_offer_free(w->dnd);
    w->dnd = NULL;
}
static void dd_motion(void *d, struct wl_data_device *dd, uint32_t t, wl_fixed_t x, wl_fixed_t y)
{
    wl_win *w = d;
    w->dx = wl_fx(x) - w->m, w->dy = wl_fx(y) - w->m - w->bh;
}
static void dd_drop(void *d, struct wl_data_device *dd)
{
    wl_win *w = d;
    wl_offer *o = w->dnd;
    w->dnd = NULL;
    if (!o) return;
    int fds[2];
    if (o->uri && pipe(fds) == 0) {
        wl_data_offer_receive(o->o, "text/uri-list", fds[1]);
        close(fds[1]);
        wl_display_flush(w->dpy);
        size_t n = 0;
        char *s = wl_read_fd(fds[0], 1000, &n), *paths = NULL;
        int len = s ? gm_uri_paths(s, n, &paths) : 0;
        free(s);
        if (len > 0) {
            free(w->drops);
            w->drops = paths, w->dlen = len, w->doff = 0;
        } else
            free(paths);
        if (w->ddm_v >= 3) wl_data_offer_finish(o->o);
    }
    wl_offer_free(o);
}
static void dd_selection(void *d, struct wl_data_device *dd, struct wl_data_offer *o)
{
    wl_win *w = d;
    wl_offer *f = o ? wl_data_offer_get_user_data(o) : NULL;
    if (w->sel && w->sel != f) wl_offer_free(w->sel);
    w->sel = f;
}
static const struct wl_data_device_listener wl_dd_listener = {dd_data_offer, dd_enter, dd_leave, dd_motion, dd_drop, dd_selection};

static void wl_win_seat_caps(void *d, struct wl_seat *s, uint32_t caps)
{
    wl_win *w = d;
    if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !w->kb) {
        w->kb = wl_seat_get_keyboard(s);
        wl_keyboard_add_listener(w->kb, &wl_kb_listener, &w->keys);
    }
    if ((caps & WL_SEAT_CAPABILITY_POINTER) && !w->ptr) {
        w->ptr = wl_seat_get_pointer(s);
        wl_pointer_add_listener(w->ptr, &wl_win_ptr_listener, w);
        if (w->csm) w->csd = wp_cursor_shape_manager_v1_get_pointer(w->csm, w->ptr);
    } else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && w->ptr) {
        if (w->csd) wp_cursor_shape_device_v1_destroy(w->csd);
        wl_pointer_release(w->ptr);
        w->ptr = NULL, w->csd = NULL, w->inside = 0, w->held = 0;
    }
    if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && w->kb) {
        wl_keyboard_release(w->kb);
        w->kb = NULL, w->keys.rkey = 0;
    }
}
static const struct wl_seat_listener wl_win_seat_listener = {wl_win_seat_caps, seat_name};

static void wo_geometry(void *d, struct wl_output *o, int32_t x, int32_t y, int32_t pw, int32_t ph, int32_t sub, const char *mk, const char *md, int32_t t) {}
static void wo_mode(void *d, struct wl_output *o, uint32_t f, int32_t w, int32_t h, int32_t r) {}
static void wo_done(void *d, struct wl_output *o) {}
static void wo_scale(void *d, struct wl_output *o, int32_t f)
{
    wl_win *w = d;
    for (int i = 0; i < w->nouts; i++)
        if (w->outs[i].o == o) w->outs[i].scale = f;
    wl_win_scale(w);
}
static const struct wl_output_listener wl_wo_listener = {wo_geometry, wo_mode, wo_done, wo_scale, out_desc, out_desc};

static void wl_win_reg(void *d, struct wl_registry *r, uint32_t name, const char *iface, uint32_t v);
static void wl_win_unreg(void *d, struct wl_registry *r, uint32_t name)
{
    wl_win *w = d;
    for (int i = 0; i < w->nouts; i++)
        if (w->outs[i].o && w->outs[i].name == name) {
            wl_output_destroy(w->outs[i].o);
            w->outs[i].o = NULL, w->outs[i].on = 0;
            wl_win_scale(w);
        }
}
static const struct wl_registry_listener wl_win_reg_listener = {wl_win_reg, wl_win_unreg};

static void ws_enter(void *d, struct wl_surface *s, struct wl_output *o)
{
    wl_win *w = d;
    for (int i = 0; i < w->nouts; i++)
        if (w->outs[i].o == o) w->outs[i].on = 1;
    wl_win_scale(w);
}
static void ws_leave(void *d, struct wl_surface *s, struct wl_output *o)
{
    wl_win *w = d;
    for (int i = 0; i < w->nouts; i++)
        if (w->outs[i].o == o) w->outs[i].on = 0;
    wl_win_scale(w);
}
static void ws_pref_scale(void *d, struct wl_surface *s, int32_t f)
{
    wl_win *w = d;
    w->pref_scale = f;
    wl_win_scale(w);
}
static void ws_pref_transform(void *d, struct wl_surface *s, uint32_t t) {}
static const struct wl_surface_listener wl_ws_listener = {ws_enter, ws_leave, ws_pref_scale, ws_pref_transform};

static void wf_scale(void *d, struct wp_fractional_scale_v1 *f, uint32_t s)
{
    wl_win *w = d;
    w->frac120 = (int)s;
    wl_win_scale(w);
}
static const struct wp_fractional_scale_v1_listener wl_wf_listener = {wf_scale};

static void wm_ping(void *d, struct xdg_wm_base *b, uint32_t serial) { xdg_wm_base_pong(b, serial); }
static const struct xdg_wm_base_listener wl_wm_listener = {wm_ping};

static void wt_configure(void *d, struct xdg_toplevel *t, int32_t cw, int32_t ch, struct wl_array *states)
{
    wl_win *w = d;
    w->cw = cw, w->ch = ch, w->cstate = 0;
    uint32_t *s = states->data;
    for (size_t i = 0; i < states->size / 4; i++)
        if (s[i] < 31) w->cstate |= 1 << s[i];
}
static void wt_close(void *d, struct xdg_toplevel *t)
{
    goome_event e;
    memset(&e, 0, sizeof(e));
    e.type = GOOME_CLOSE;
    gm_push(((wl_win *)d)->g, &e);
}
static void wt_bounds(void *d, struct xdg_toplevel *t, int32_t w, int32_t h) {}
static void wt_caps(void *d, struct xdg_toplevel *t, struct wl_array *c) {}
static const struct xdg_toplevel_listener wl_wt_listener = {wt_configure, wt_close, wt_bounds, wt_caps};

static void wd_configure(void *d, struct zxdg_toplevel_decoration_v1 *z, uint32_t mode)
{
    ((wl_win *)d)->ssd = mode == ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE;
}
static const struct zxdg_toplevel_decoration_v1_listener wl_wd_listener = {wd_configure};

static void wl_win_minmax(wl_win *w)
{
    if (!w->top) return;
    int bh = w->ssd || (w->g->opts.window_flags & GOOME_WINDOW_BORDERLESS) ? 0 : WL_BAR;
    goome_opts *o = &w->g->opts;
    if (!(o->window_flags & GOOME_WINDOW_RESIZABLE)) {
        xdg_toplevel_set_min_size(w->top, w->w, w->h + bh);
        xdg_toplevel_set_max_size(w->top, w->w, w->h + bh);
    } else
        xdg_toplevel_set_min_size(w->top, o->min_width > 0 ? o->min_width : 0, o->min_height > 0 ? o->min_height + bh : 0);
}

static void ws_configure(void *d, struct xdg_surface *x, uint32_t serial)
{
    wl_win *w = d;
    xdg_surface_ack_configure(x, serial);
    int act = !!(w->cstate & (1 << XDG_TOPLEVEL_STATE_ACTIVATED)), was = w->active, flip = w->configured && w->cssd != w->ssd;
    w->maxed = (w->cstate & ((1 << XDG_TOPLEVEL_STATE_MAXIMIZED) | (15 << 5))) != 0;
    w->full = (w->cstate & (1 << XDG_TOPLEVEL_STATE_FULLSCREEN)) != 0;
    w->active = act;
    if (act) w->mini = 0;
    int m, bh;
    wl_win_layout(w, &m, &bh);
    if (w->cw > 0 && w->ch > bh && (!flip || w->maxed || w->full)) {
        int nw = w->cw, nh = w->ch - bh;
        if (nw != w->w || nh != w->h) w->w = nw, w->h = nh, wl_win_mons(w);
    }
    if (!w->configured || flip) wl_win_minmax(w);
    w->configured = 1, w->cssd = w->ssd;
    if (act != was) {
        goome_event e;
        memset(&e, 0, sizeof(e));
        e.type = GOOME_FOCUS, e.id = act;
        gm_push(w->g, &e);
    }
    if (w->px && wl_win_build(w)) wl_win_deco(w);
    w->dirty = 1;
}
static const struct xdg_surface_listener wl_ws_xdg_listener = {ws_configure};

static void wl_win_reg(void *d, struct wl_registry *r, uint32_t name, const char *iface, uint32_t v)
{
    wl_win *w = d;
    if (wlt_reg(w->dpy, r, name, iface, v)) return;
    if (!strcmp(iface, "wl_compositor") && !w->comp) {
        w->comp_v = v < 6 ? v : 6;
        w->comp = wl_registry_bind(r, name, &gm_wl_compositor_interface, w->comp_v);
    } else if (!strcmp(iface, "wl_shm") && !w->shm)
        w->shm = wl_registry_bind(r, name, &gm_wl_shm_interface, 1);
    else if (!strcmp(iface, "wl_seat") && !w->seat) {
        w->seat = wl_registry_bind(r, name, &gm_wl_seat_interface, v < 5 ? v : 5);
        wl_seat_add_listener(w->seat, &wl_win_seat_listener, w);
    } else if (!strcmp(iface, "xdg_wm_base") && !w->wm) {
        w->wm = wl_registry_bind(r, name, &gm_xdg_wm_base_interface, v < 5 ? v : 5);
        xdg_wm_base_add_listener(w->wm, &wl_wm_listener, w);
    } else if (!strcmp(iface, "zxdg_decoration_manager_v1") && !w->decm)
        w->decm = wl_registry_bind(r, name, &gm_zxdg_decoration_manager_v1_interface, 1);
    else if (!strcmp(iface, "wp_cursor_shape_manager_v1") && !w->csm)
        w->csm = wl_registry_bind(r, name, &gm_wp_cursor_shape_manager_v1_interface, 1);
    else if (!strcmp(iface, "wl_data_device_manager") && !w->ddm) {
        w->ddm_v = v < 3 ? v : 3;
        w->ddm = wl_registry_bind(r, name, &gm_wl_data_device_manager_interface, w->ddm_v);
    } else if (!strcmp(iface, "wp_fractional_scale_manager_v1") && !w->fsm)
        w->fsm = wl_registry_bind(r, name, &gm_wp_fractional_scale_manager_v1_interface, 1);
    else if (!strcmp(iface, "wp_viewporter") && !w->vp)
        w->vp = wl_registry_bind(r, name, &gm_wp_viewporter_interface, 1);
    else if (!strcmp(iface, "wl_output")) {
        int i = 0;
        while (i < w->nouts && w->outs[i].o) i++;
        if (i >= GM_MAX_MON) return;
        if (i == w->nouts) w->nouts++;
        w->outs[i].o = wl_registry_bind(r, name, &gm_wl_output_interface, v < 2 ? v : 2);
        w->outs[i].name = name, w->outs[i].scale = 1, w->outs[i].on = 0;
        wl_output_add_listener(w->outs[i].o, &wl_wo_listener, w);
    }
}

static int wl_win_pump(wl_win *w, int ms)
{
    while (wl_display_prepare_read(w->dpy) != 0)
        if (wl_display_dispatch_pending(w->dpy) < 0) return -1;
    if (w->g->qlen) ms = 0;
    if (wl_display_flush(w->dpy) < 0 && errno != EAGAIN) {
        wl_display_cancel_read(w->dpy);
        return -1;
    }
    struct pollfd pfd = {wl_display_get_fd(w->dpy), POLLIN, 0};
    int r = poll(&pfd, 1, ms < 0 ? 0 : ms);
    if (r > 0) {
        if (wl_display_read_events(w->dpy) < 0) return -1;
    } else
        wl_display_cancel_read(w->dpy);
    if (wl_display_dispatch_pending(w->dpy) < 0) return -1;
    wl_win_flush(w);
    wl_display_flush(w->dpy);
    return 0;
}

static void wl_win_unmap(wl_win *w)
{
    if (w->deco) zxdg_toplevel_decoration_v1_destroy(w->deco);
    if (w->top) xdg_toplevel_destroy(w->top);
    if (w->xs) xdg_surface_destroy(w->xs);
    if (w->view) wp_viewport_destroy(w->view);
    if (w->frac) wp_fractional_scale_v1_destroy(w->frac);
    if (w->surf) wl_surface_destroy(w->surf);
    w->deco = NULL, w->top = NULL, w->xs = NULL, w->view = NULL, w->frac = NULL, w->surf = NULL;
    wl_free_shm(&w->buf[0]);
    wl_free_shm(&w->buf[1]);
    w->configured = w->shown = w->inside = w->held = 0;
    for (int i = 0; i < w->nouts; i++) w->outs[i].on = 0;
}

static int wl_win_map(wl_win *w)
{
    goome *g = w->g;
    w->surf = wl_compositor_create_surface(w->comp);
    wl_surface_add_listener(w->surf, &wl_ws_listener, w);
    if (w->fsm && w->vp) {
        w->frac = wp_fractional_scale_manager_v1_get_fractional_scale(w->fsm, w->surf);
        wp_fractional_scale_v1_add_listener(w->frac, &wl_wf_listener, w);
        w->view = wp_viewporter_get_viewport(w->vp, w->surf);
    }
    w->xs = xdg_wm_base_get_xdg_surface(w->wm, w->surf);
    xdg_surface_add_listener(w->xs, &wl_ws_xdg_listener, w);
    w->top = xdg_surface_get_toplevel(w->xs);
    xdg_toplevel_add_listener(w->top, &wl_wt_listener, w);
    xdg_toplevel_set_title(w->top, g->title);
    xdg_toplevel_set_app_id(w->top, g->title);
    if (w->decm) {
        w->deco = zxdg_decoration_manager_v1_get_toplevel_decoration(w->decm, w->top);
        zxdg_toplevel_decoration_v1_add_listener(w->deco, &wl_wd_listener, w);
        zxdg_toplevel_decoration_v1_set_mode(w->deco, (g->opts.window_flags & GOOME_WINDOW_BORDERLESS) || gm_env("GOOME_CSD") ? ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE
                                                                                                 : ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
    }
    wl_win_minmax(w);
    wl_surface_commit(w->surf);
    for (int i = 0; i < 20 && !w->configured; i++)
        if (wl_display_roundtrip(w->dpy) < 0) return gm_fail(g, GOOME_ENODISPLAY, "the wayland display hung up on us");
    if (!w->configured) return gm_fail(g, GOOME_ENODISPLAY, "the compositor never configured the window");
    wl_win_minmax(w);
    w->shown = 1;
    if (!wl_win_build(w)) return gm_fail(g, GOOME_ENOMEM, "out of memory for the window");
    wl_win_flush(w);
    wl_display_flush(w->dpy);
    return GOOME_OK;
}

static void wl_win_close(goome *g);

static int wl_win_open(goome *g)
{
    if (!gm_env("WAYLAND_DISPLAY") && !gm_env("WAYLAND_SOCKET")) return gm_fail(g, GOOME_ENODISPLAY, "WAYLAND_DISPLAY isnt set");
    int e = wl_load(g);
    if (e != GOOME_OK) return e;
    wl_win *w = calloc(1, sizeof(*w));
    if (!w) {
        wl_lib_put();
        return gm_fail(g, GOOME_ENOMEM, "out of memory");
    }
    g->ov_priv = w;
    w->g = g, w->sc = 1, w->bs = 1, w->shape = -1;
    w->keys.g = g, w->keys.on = 1, w->keys.rate = 25, w->keys.delay = 600;
    w->w = g->opts.width, w->h = g->opts.height;
    w->dpy = wl_display_connect(NULL);
    if (!w->dpy) {
        wl_win_close(g);
        return gm_fail(g, GOOME_ENODISPLAY, "cant connect to the wayland display %s", gm_env("WAYLAND_DISPLAY") ? gm_env("WAYLAND_DISPLAY") : "");
    }
    w->reg = wl_display_get_registry(w->dpy);
    wl_registry_add_listener(w->reg, &wl_win_reg_listener, w);
    if (wl_display_roundtrip(w->dpy) < 0 || wl_display_roundtrip(w->dpy) < 0) e = gm_fail(g, GOOME_ENODISPLAY, "the wayland display hung up on us");
    else if (!w->comp || !w->shm) e = gm_fail(g, GOOME_ENODISPLAY, "this compositor has no wl_compositor or wl_shm");
    else if (!w->wm) e = gm_fail(g, GOOME_ENOTSUP, "this compositor has no xdg-shell, cant make a window");
    if (e != GOOME_OK) {
        wl_win_close(g);
        return e;
    }
    if (w->ddm && w->seat) {
        w->dd = wl_data_device_manager_get_data_device(w->ddm, w->seat);
        wl_data_device_add_listener(w->dd, &wl_dd_listener, w);
    }
    g->opts.no_capture = 1;
    wl_win_scale(w);
    wl_win_mons(w);
    if (!(g->opts.window_flags & GOOME_WINDOW_HIDDEN)) e = wl_win_map(w);
    if (e != GOOME_OK) {
        wl_win_close(g);
        return e;
    }
    wl_win_mons(w);
    return GOOME_OK;
}

static goome_canvas *wl_win_canvas(goome *g, int mon)
{
    wl_win *w = g->ov_priv;
    if (mon) return NULL;
    if (!wl_win_build(w)) return NULL;
    return &w->cv;
}

static int wl_win_commit(goome *g, int mon, goome_canvas *c, const goome_rect *hits, int n)
{
    wl_win *w = g->ov_priv;
    if (c != &w->cv) return GOOME_EARG;
    int x0, y0, x1, y1;
    if (gm_canvas_damage(c, &x0, &y0, &x1, &y1)) wl_win_damage(w, x0 + w->ox, y0 + w->oy, x1 + w->ox, y1 + w->oy);
    wl_win_flush(w);
    wl_display_flush(w->dpy);
    return GOOME_OK;
}

static int wl_win_wait(goome *g, int ms)
{
    wl_win *w = g->ov_priv;
    long end = goome_now_ms() + (ms > 0 ? ms : 0);
    int dropped = 0;
    for (;;) {
        long left = end - goome_now_ms();
        wl_keys *k = &w->keys;
        if (k->rkey && k->rate > 0) {
            long now = goome_now_ms(), step = 1000 / k->rate;
            if (now >= k->rnext) {
                gm_push(g, &k->rev);
                k->rnext = now - k->rnext > step ? now + step : k->rnext + step;
            }
            if (k->rnext - now < left) left = k->rnext - now;
        }
        if (!dropped && w->doff < w->dlen) gm_drop_next(g, w->drops, &w->doff, w->dlen, w->dx, w->dy), dropped = 1;
        if (wl_win_pump(w, g->qlen || left < 0 ? 0 : (int)left) < 0) return -1;
        if (g->qlen || goome_now_ms() >= end) return 0;
    }
}

static int wl_win_pointer(goome *g, double *x, double *y, int *buttons)
{
    wl_win *w = g->ov_priv;
    if (!w->inside) return 0;
    *x = w->mx - w->m, *y = w->my - w->m - w->bh, *buttons = w->held;
    return 1;
}

static int wl_win_keyboard(goome *g, int on) { return GOOME_OK; }

static int wl_win_op(goome *g, int op, int a, int b, int *ra, int *rb)
{
    wl_win *w = g->ov_priv;
    switch (op) {
    case GM_WIN_TITLE:
        if (w->top) xdg_toplevel_set_title(w->top, g->title);
        wl_win_deco(w);
        wl_win_mons(w);
        break;
    case GM_WIN_SIZE:
        w->w = a, w->h = b;
        wl_win_minmax(w);
        wl_win_build(w);
        wl_win_mons(w);
        break;
    case GM_WIN_POS_GET:
    case GM_WIN_POS_SET: return gm_fail(g, GOOME_ENOTSUP, "wayland doesnt let apps see or set where their window is");
    case GM_WIN_GET_STATE:
        *ra = w->full ? GOOME_STATE_FULLSCREEN : w->maxed ? GOOME_STATE_MAXIMIZED : w->mini ? GOOME_STATE_MINIMIZED : GOOME_STATE_NORMAL;
        return GOOME_OK;
    case GM_WIN_STATE:
        if (!w->top) return gm_fail(g, GOOME_EGONE, "the window is hidden");
        if (a == GOOME_STATE_MINIMIZED) {
            xdg_toplevel_set_minimized(w->top), w->mini = 1;
            break;
        }
        if (a != GOOME_STATE_FULLSCREEN && w->full) xdg_toplevel_unset_fullscreen(w->top);
        if (a != GOOME_STATE_MAXIMIZED && w->maxed) xdg_toplevel_unset_maximized(w->top);
        if (a == GOOME_STATE_FULLSCREEN) xdg_toplevel_set_fullscreen(w->top, NULL);
        if (a == GOOME_STATE_MAXIMIZED) xdg_toplevel_set_maximized(w->top);
        break;
    case GM_WIN_DRAG:
        if (!w->top || !w->held || !w->serial) return gm_fail(g, GOOME_EARG, "no mouse button is down");
        if (a) xdg_toplevel_resize(w->top, w->seat, w->serial, (uint32_t)((a & 1 ? 4 : 0) | (a & 2 ? 8 : 0) | (a & 4 ? 1 : 0) | (a & 8 ? 2 : 0)));
        else xdg_toplevel_move(w->top, w->seat, w->serial);
        for (int i = 0; i < 3; i++)
            if (w->held & (1 << i)) wl_win_ptr_ev(w, GOOME_UP, i + 1, 0);
        w->held = 0;
        break;
    case GM_WIN_SHOW:
        if (a && !w->surf) return wl_win_map(w);
        if (!a && w->surf) wl_win_unmap(w);
        break;
    default: return GOOME_ENOTSUP;
    }
    wl_display_flush(w->dpy);
    return GOOME_OK;
}

static int wl_win_cursor(goome *g, int shape)
{
    wl_win *w = g->ov_priv;
    if (w->inside && (w->area == 0 || w->held)) wl_win_cursor_set(w, shape);
    wl_display_flush(w->dpy);
    return GOOME_OK;
}

static int wl_write_all(int fd, const char *s, size_t n)
{
    sigset_t pipe_set, old;
    sigemptyset(&pipe_set);
    sigaddset(&pipe_set, SIGPIPE);
    sigprocmask(SIG_BLOCK, &pipe_set, &old);
    int ok = 1;
    while (n && ok) {
        ssize_t r = write(fd, s, n);
        if (r < 0 && errno == EINTR) continue;
        if (r <= 0) ok = 0;
        else s += r, n -= (size_t)r;
    }
    struct timespec zero = {0, 0};
    while (sigtimedwait(&pipe_set, NULL, &zero) > 0) {
    }
    sigprocmask(SIG_SETMASK, &old, NULL);
    return ok;
}

static void ds_target(void *d, struct wl_data_source *s, const char *m) {}
static void ds_send(void *d, struct wl_data_source *s, const char *m, int32_t fd)
{
    wl_win *w = d;
    int fl = fcntl(fd, F_GETFL);
    if (fl >= 0) fcntl(fd, F_SETFL, fl & ~O_NONBLOCK);
    const char *t = w->g->clip ? w->g->clip : "";
    wl_write_all(fd, t, strlen(t));
    close(fd);
}
static void ds_cancelled(void *d, struct wl_data_source *s)
{
    wl_win *w = d;
    if (w->src == s) w->src = NULL;
    wl_data_source_destroy(s);
}
static void ds_dnd(void *d, struct wl_data_source *s) {}
static void ds_action(void *d, struct wl_data_source *s, uint32_t a) {}
static const struct wl_data_source_listener wl_ds_listener = {ds_target, ds_send, ds_cancelled, ds_dnd, ds_dnd, ds_action};

static int wl_win_clip_set(goome *g, const char *s)
{
    wl_win *w = g->ov_priv;
    if (!w->ddm || !w->dd) return gm_fail(g, GOOME_ENOTSUP, "this compositor has no clipboard (wl_data_device_manager)");
    uint32_t serial = w->keys.serial > w->serial ? w->keys.serial : w->serial;
    if (!serial) return gm_fail(g, GOOME_EDENIED, "wayland only lets a window set the clipboard after the user did something in it");
    int e = gm_clip_keep(g, s);
    if (e != GOOME_OK) return e;
    struct wl_data_source *src = wl_data_device_manager_create_data_source(w->ddm);
    wl_data_source_add_listener(src, &wl_ds_listener, w);
    static const char *mimes[] = {"text/plain;charset=utf-8", "UTF8_STRING", "text/plain", "TEXT", "STRING"};
    for (int i = 0; i < 5; i++) wl_data_source_offer(src, mimes[i]);
    wl_data_device_set_selection(w->dd, src, serial);
    if (w->src) wl_data_source_destroy(w->src);
    w->src = src;
    wl_display_flush(w->dpy);
    return GOOME_OK;
}

static int wl_win_clip_get(goome *g, char **out)
{
    wl_win *w = g->ov_priv;
    *out = NULL;
    wl_win_pump(w, 0);
    if (w->src) return gm_clip_copy(g, out);
    if (!w->sel || !w->sel->text) return GOOME_OK;
    int fds[2];
    if (pipe(fds)) return gm_fail(g, GOOME_ENOMEM, "cant make a pipe");
    wl_data_offer_receive(w->sel->o, w->sel->mime, fds[1]);
    close(fds[1]);
    wl_display_flush(w->dpy);
    size_t n = 0;
    *out = wl_read_fd(fds[0], 250, &n);
    return *out ? GOOME_OK : gm_fail(g, GOOME_EAGAIN, "the app that owns the clipboard didnt answer");
}

static void wl_win_close(goome *g)
{
    wl_win *w = g->ov_priv;
    if (!w) return;
    if (w->dpy) {
        wlt_drop(w->dpy);
        wl_win_unmap(w);
        wl_offer_free(w->sel), wl_offer_free(w->dnd);
        if (w->src) wl_data_source_destroy(w->src);
        if (w->dd) {
            if (w->ddm_v >= 2) wl_data_device_release(w->dd);
            else wl_data_device_destroy(w->dd);
        }
        if (w->csd) wp_cursor_shape_device_v1_destroy(w->csd);
        if (w->ctheme) wl_cur.theme_destroy(w->ctheme);
        if (w->csurf) wl_surface_destroy(w->csurf);
        wl_free_shm(&w->arrow);
        if (w->ptr) wl_pointer_release(w->ptr);
        if (w->kb) wl_keyboard_release(w->kb);
        if (w->seat) {
            if (wl_seat_get_version(w->seat) >= 5) wl_seat_release(w->seat);
            else wl_seat_destroy(w->seat);
        }
        for (int i = 0; i < w->nouts; i++)
            if (w->outs[i].o) wl_output_destroy(w->outs[i].o);
        if (w->ddm) wl_data_device_manager_destroy(w->ddm);
        if (w->csm) wp_cursor_shape_manager_v1_destroy(w->csm);
        if (w->decm) zxdg_decoration_manager_v1_destroy(w->decm);
        if (w->fsm) wp_fractional_scale_manager_v1_destroy(w->fsm);
        if (w->vp) wp_viewporter_destroy(w->vp);
        if (w->wm) xdg_wm_base_destroy(w->wm);
        if (w->shm) wl_shm_destroy(w->shm);
        if (w->comp) wl_compositor_destroy(w->comp);
        if (w->reg) wl_registry_destroy(w->reg);
        wl_display_flush(w->dpy);
        wl_display_disconnect(w->dpy);
    }
    wl_keys_free(&w->keys);
#ifndef GOOME_NO_DRAW
    goome_font_free(w->font);
#endif
    free(w->px), free(w->drops);
    free(w);
    wl_lib_put();
    g->ov_priv = NULL;
}

static const gm_ov wl_wov = {"wayland", wl_win_open, wl_win_canvas, wl_win_commit, wl_win_wait, wl_win_pointer, wl_win_keyboard, NULL,
                             wl_win_close, GOOME_CAP_KEYBOARD | GOOME_CAP_WINDOW | GOOME_CAP_CLIPBOARD | GOOME_CAP_CURSOR,
                             wl_win_op, wl_win_cursor, wl_win_clip_set, wl_win_clip_get, wayland_ov_grab};

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
            if (!wl_make_shm(wls.shm, &o->cap, o->fw, o->fh, o->fstride, fmt)) {
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
        if (!wl_make_shm(wls.shm, &o->cap, o->fw, o->fh, o->fstride, o->cfmt)) {
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
typedef struct {
    struct wl_display *dpy;
    struct zwp_text_input_manager_v3 *mgr;
    struct zwp_text_input_v3 *ti;
    struct wl_surface *in;
    goome *g;
    int want, enabled, sent_pre, pcur, has_pre, has_commit;
    goome_rect r, sent;
    char *pre, *commit;
} wlt_slot;

static wlt_slot wlt[4];

static wlt_slot *wlt_of(struct wl_display *dpy)
{
    for (int i = 0; i < 4; i++)
        if (dpy && wlt[i].dpy == dpy) return &wlt[i];
    return NULL;
}

static int wlt_reg(struct wl_display *dpy, struct wl_registry *r, uint32_t name, const char *iface, uint32_t version)
{
    if (strcmp(iface, "zwp_text_input_manager_v3")) return 0;
    if (wlt_of(dpy)) return 1;
    for (int i = 0; i < 4; i++)
        if (!wlt[i].dpy) {
            wlt[i].dpy = dpy;
            wlt[i].mgr = wl_registry_bind(r, name, &gm_zwp_text_input_manager_v3_interface, 1);
            break;
        }
    return 1;
}

static void wlt_drop(struct wl_display *dpy)
{
    wlt_slot *t = wlt_of(dpy);
    if (!t) return;
    if (t->ti) zwp_text_input_v3_destroy(t->ti);
    if (t->mgr) zwp_text_input_manager_v3_destroy(t->mgr);
    free(t->pre), free(t->commit);
    memset(t, 0, sizeof(*t));
}

static void wlt_keep(char **to, const char *s)
{
    free(*to);
    *to = NULL;
    if (!s) return;
    size_t n = strlen(s);
    *to = malloc(n + 1);
    if (*to) memcpy(*to, s, n + 1);
}

static void wlt_push_pre(wlt_slot *t, const char *s, int cur)
{
    int n = s ? (int)strlen(s) : 0;
    if (!n && !t->sent_pre) return;
    gm_ime_push(t->g, GOOME_PREEDIT, s ? s : "", n, cur < 0 ? n : cur);
    t->sent_pre = n > 0;
}

static void wlt_apply(wlt_slot *t)
{
    if (!t->ti || !t->in) return;
    if (!t->want) {
        if (t->enabled) zwp_text_input_v3_disable(t->ti), zwp_text_input_v3_commit(t->ti), t->enabled = 0;
        wlt_push_pre(t, NULL, 0);
        return;
    }
    if (!t->enabled) {
        zwp_text_input_v3_enable(t->ti);
        zwp_text_input_v3_set_content_type(t->ti, ZWP_TEXT_INPUT_V3_CONTENT_HINT_NONE, ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_NORMAL);
        t->enabled = 1;
    } else if (!memcmp(&t->r, &t->sent, sizeof(t->r)))
        return;
    zwp_text_input_v3_set_cursor_rectangle(t->ti, t->r.x, t->r.y, t->r.w, t->r.h);
    zwp_text_input_v3_commit(t->ti);
    t->sent = t->r;
}

static void wlt_enter(void *d, struct zwp_text_input_v3 *ti, struct wl_surface *s)
{
    wlt_slot *t = d;
    t->in = s, t->enabled = 0;
    wlt_apply(t);
}

static void wlt_leave(void *d, struct zwp_text_input_v3 *ti, struct wl_surface *s)
{
    wlt_slot *t = d;
    if (t->in != s) return;
    if (t->enabled) zwp_text_input_v3_disable(t->ti), zwp_text_input_v3_commit(t->ti);
    t->in = NULL, t->enabled = 0;
    wlt_push_pre(t, NULL, 0);
}

static void wlt_preedit(void *d, struct zwp_text_input_v3 *ti, const char *text, int32_t begin, int32_t end)
{
    wlt_slot *t = d;
    wlt_keep(&t->pre, text);
    t->pcur = begin, t->has_pre = 1;
}

static void wlt_commit_str(void *d, struct zwp_text_input_v3 *ti, const char *text)
{
    wlt_slot *t = d;
    wlt_keep(&t->commit, text);
    t->has_commit = 1;
}

static void wlt_delete(void *d, struct zwp_text_input_v3 *ti, uint32_t before, uint32_t after) {}

static void wlt_done(void *d, struct zwp_text_input_v3 *ti, uint32_t serial)
{
    wlt_slot *t = d;
    if (t->has_commit && t->commit && t->commit[0]) gm_ime_push(t->g, GOOME_TEXT, t->commit, -1, 0);
    wlt_push_pre(t, t->has_pre ? t->pre : NULL, t->pcur);
    t->has_pre = t->has_commit = 0;
    wlt_keep(&t->pre, NULL), wlt_keep(&t->commit, NULL);
}

static const struct zwp_text_input_v3_listener wlt_listener = {wlt_enter, wlt_leave, wlt_preedit, wlt_commit_str, wlt_delete, wlt_done};

int gm_ime_wayland(goome *g, int on, const goome_rect *caret)
{
    struct wl_display *dpy;
    struct wl_seat *seat;
    goome_rect r = *caret;
    if (g->ov_priv) {
        wl_win *w = g->ov_priv;
        dpy = w->dpy, seat = w->seat;
        r.x += w->m, r.y += w->m + w->bh;
    } else {
        dpy = wls.dpy, seat = wls.seat;
        for (int i = 0; i < wls.nouts; i++) {
            wl_out *o = &wls.outs[i];
            if (o->alive && o->surface && caret->x >= o->area.x && caret->y >= o->area.y && caret->x < o->area.x + o->area.w && caret->y < o->area.y + o->area.h)
                r.x -= o->area.x, r.y -= o->area.y;
        }
    }
    wlt_slot *t = wlt_of(dpy);
    if (!t || !t->mgr) return on ? gm_fail(g, GOOME_ENOTSUP, "this compositor has no text-input-v3") : GOOME_OK;
    if (!t->ti && on) {
        if (!seat) return gm_fail(g, GOOME_ENOTSUP, "no wayland seat for text input");
        t->ti = zwp_text_input_manager_v3_get_text_input(t->mgr, seat);
        if (!t->ti) return gm_fail(g, GOOME_ENOMEM, "couldnt make a text input");
        zwp_text_input_v3_add_listener(t->ti, &wlt_listener, t);
    }
    t->g = g, t->want = on, t->r = r;
    wlt_apply(t);
    wl_display_flush(dpy);
    return GOOME_OK;
}

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
