#include "internal.h"

#if defined(GM_LINUX)

#include <dlfcn.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>

// Xlib.h / Xutil.h / Xrandr.h / XShm.h, only the bits we use
typedef unsigned long x11_xid;
typedef struct x11_dpy x11_dpy;
typedef struct x11_visual x11_visual;
typedef struct x11_gcrec *x11_gc;

typedef struct x11_image {
    int width, height, xoffset, format;
    char *data;
    int byte_order, bitmap_unit, bitmap_bit_order, bitmap_pad, depth, bytes_per_line, bits_per_pixel;
    unsigned long red_mask, green_mask, blue_mask;
    char *obdata;
    struct {
        void *create_image;
        int (*destroy_image)(struct x11_image *);
        void *get_pixel, *put_pixel, *sub_image, *add_pixel;
    } f;
} x11_image;

typedef struct {
    x11_visual *visual;
    x11_xid visualid;
    int screen, depth, class_;
    unsigned long red_mask, green_mask, blue_mask;
    int colormap_size, bits_per_rgb;
} x11_visinfo;

typedef struct {
    x11_xid background_pixmap;
    unsigned long background_pixel;
    x11_xid border_pixmap;
    unsigned long border_pixel;
    int bit_gravity, win_gravity, backing_store;
    unsigned long backing_planes, backing_pixel;
    int save_under;
    long event_mask, do_not_propagate_mask;
    int override_redirect;
    x11_xid colormap, cursor;
} x11_winattr;

typedef struct {
    short x, y;
    unsigned short width, height;
} x11_xrect;

typedef struct {
    int type;
    unsigned long serial;
    int send_event;
    x11_dpy *display;
    x11_xid window, root, subwindow;
    unsigned long time;
    int x, y, x_root, y_root;
    unsigned int state, detail;
    int same_screen;
} x11_input_ev;

typedef struct {
    int type;
    unsigned long serial;
    int send_event;
    x11_dpy *display;
    x11_xid window;
    int x, y, width, height, count;
} x11_expose_ev;

typedef union {
    int type;
    x11_input_ev in;
    x11_expose_ev ex;
    long pad[24];
} x11_event;

typedef struct {
    int type;
    x11_dpy *display;
    x11_xid resourceid;
    unsigned long serial;
    unsigned char error_code, request_code, minor_code;
} x11_errev;

typedef struct {
    x11_xid name;
    int primary, automatic, noutput;
    int x, y, width, height, mwidth, mheight;
    x11_xid *outputs;
} x11_rrmon;

typedef struct {
    x11_xid shmseg;
    int shmid;
    char *shmaddr;
    int read_only;
} x11_shminfo;

typedef struct {
    int type;
    unsigned long serial;
    int send_event;
    x11_dpy *display;
    x11_xid window, message_type;
    int format;
    long l[5];
} x11_client_ev;

typedef int (*x11_errfn)(x11_dpy *, x11_errev *);

enum {
    X11_KEYPRESS = 2, X11_BUTTONPRESS = 4, X11_BUTTONRELEASE = 5, X11_MOTION = 6, X11_ENTER = 7, X11_EXPOSE = 12,
    X11_SHIFT = 1, X11_LOCK = 2, X11_CONTROL = 4, X11_MOD1 = 8, X11_MOD2 = 16, X11_MOD4 = 64,
    X11_BUTTON1 = 1 << 8, X11_BUTTON2 = 1 << 9, X11_BUTTON3 = 1 << 10,
    X11_TRUECOLOR = 4, X11_ZPIXMAP = 2, X11_INPUTOUTPUT = 1,
    X11_CW_BACKPIXEL = 2, X11_CW_BORDERPIXEL = 8, X11_CW_OVERRIDE = 512, X11_CW_COLORMAP = 8192,
    X11_XA_ATOM = 4, X11_XA_CARDINAL = 6, X11_GRAB_ASYNC = 1,
    X11_SHAPE_BOUNDING = 0, X11_SHAPE_INPUT = 2,
    X11_RR_MASK = 1 | 2 | 4, // screen, crtc, output
};
#define X11_EVMASK (1L | 4L | 8L | 16L | 32L | 64L | (1L << 15)) // keys, buttons, enter/leave, motion, expose

static const struct {
    int key;
    unsigned long sym;
} x11_keys[] = {
    {GOOME_K_BACKSPACE, 0xff08}, {GOOME_K_TAB, 0xff09}, {GOOME_K_TAB, 0xfe20}, {GOOME_K_ENTER, 0xff0d}, {GOOME_K_ENTER, 0xff8d},
    {GOOME_K_ESC, 0xff1b}, {GOOME_K_DELETE, 0xffff}, {GOOME_K_DELETE, 0xff9f}, {GOOME_K_HOME, 0xff50}, {GOOME_K_LEFT, 0xff51},
    {GOOME_K_UP, 0xff52}, {GOOME_K_RIGHT, 0xff53}, {GOOME_K_DOWN, 0xff54}, {GOOME_K_PGUP, 0xff55}, {GOOME_K_PGDN, 0xff56},
    {GOOME_K_END, 0xff57}, {GOOME_K_INSERT, 0xff63}, {GOOME_K_HOME, 0xff95}, {GOOME_K_LEFT, 0xff96}, {GOOME_K_UP, 0xff97},
    {GOOME_K_RIGHT, 0xff98}, {GOOME_K_DOWN, 0xff99}, {GOOME_K_PGUP, 0xff9a}, {GOOME_K_PGDN, 0xff9b}, {GOOME_K_END, 0xff9c},
    {GOOME_K_INSERT, 0xff9e},
};

static struct {
    x11_dpy *(*OpenDisplay)(const char *);
    int (*CloseDisplay)(x11_dpy *);
    int (*DefaultScreen)(x11_dpy *);
    x11_xid (*RootWindow)(x11_dpy *, int);
    x11_visual *(*DefaultVisual)(x11_dpy *, int);
    int (*DefaultDepth)(x11_dpy *, int);
    int (*MatchVisualInfo)(x11_dpy *, int, int, int, x11_visinfo *);
    x11_xid (*CreateColormap)(x11_dpy *, x11_xid, x11_visual *, int);
    int (*FreeColormap)(x11_dpy *, x11_xid);
    x11_xid (*CreateWindow)(x11_dpy *, x11_xid, int, int, unsigned, unsigned, unsigned, int, unsigned, x11_visual *, unsigned long,
                            x11_winattr *);
    int (*DestroyWindow)(x11_dpy *, x11_xid);
    int (*MapRaised)(x11_dpy *, x11_xid);
    int (*UnmapWindow)(x11_dpy *, x11_xid);
    int (*RaiseWindow)(x11_dpy *, x11_xid);
    int (*MoveResizeWindow)(x11_dpy *, x11_xid, int, int, unsigned, unsigned);
    int (*SelectInput)(x11_dpy *, x11_xid, long);
    x11_image *(*CreateImage)(x11_dpy *, x11_visual *, unsigned, int, int, char *, unsigned, unsigned, int, int);
    int (*PutImage)(x11_dpy *, x11_xid, x11_gc, x11_image *, int, int, int, int, unsigned, unsigned);
    x11_image *(*GetImage)(x11_dpy *, x11_xid, int, int, unsigned, unsigned, unsigned long, int);
    x11_gc (*CreateGC)(x11_dpy *, x11_xid, unsigned long, void *);
    int (*FreeGC)(x11_dpy *, x11_gc);
    int (*SetGraphicsExposures)(x11_dpy *, x11_gc, int);
    x11_xid (*CreatePixmap)(x11_dpy *, x11_xid, unsigned, unsigned, unsigned);
    int (*FreePixmap)(x11_dpy *, x11_xid);
    int (*CopyArea)(x11_dpy *, x11_xid, x11_xid, x11_gc, int, int, unsigned, unsigned, int, int);
    int (*Flush)(x11_dpy *);
    int (*Sync)(x11_dpy *, int);
    int (*Pending)(x11_dpy *);
    int (*NextEvent)(x11_dpy *, x11_event *);
    int (*ConnectionNumber)(x11_dpy *);
    x11_xid (*InternAtom)(x11_dpy *, const char *, int);
    char *(*GetAtomName)(x11_dpy *, x11_xid);
    int (*Free)(void *);
    int (*ChangeProperty)(x11_dpy *, x11_xid, x11_xid, x11_xid, int, int, const unsigned char *, int);
    int (*StoreName)(x11_dpy *, x11_xid, const char *);
    x11_xid (*GetSelectionOwner)(x11_dpy *, x11_xid);
    int (*GrabKeyboard)(x11_dpy *, x11_xid, int, int, int, unsigned long);
    int (*UngrabKeyboard)(x11_dpy *, unsigned long);
    int (*LookupString)(x11_input_ev *, char *, int, unsigned long *, void *);
    x11_errfn (*SetErrorHandler)(x11_errfn);
    int (*GetGeometry)(x11_dpy *, x11_xid, x11_xid *, int *, int *, unsigned *, unsigned *, unsigned *, unsigned *);
    int (*GrabKey)(x11_dpy *, int, unsigned, x11_xid, int, int, int);
    int (*UngrabKey)(x11_dpy *, int, unsigned, x11_xid);
    unsigned char (*KeysymToKeycode)(x11_dpy *, unsigned long);
    int (*QueryPointer)(x11_dpy *, x11_xid, x11_xid *, x11_xid *, int *, int *, int *, int *, unsigned *);
    int (*SetInputFocus)(x11_dpy *, x11_xid, int, unsigned long);
    int (*GetInputFocus)(x11_dpy *, x11_xid *, int *);
    int (*SendEvent)(x11_dpy *, x11_xid, int, long, x11_client_ev *);
    int (*GetWindowProperty)(x11_dpy *, x11_xid, x11_xid, long, long, int, x11_xid, x11_xid *, int *, unsigned long *, unsigned long *,
                             unsigned char **);
    int (*FixesQueryExtension)(x11_dpy *, int *, int *);
    x11_xid (*FixesCreateRegion)(x11_dpy *, x11_xrect *, int);
    void (*FixesSetWindowShapeRegion)(x11_dpy *, x11_xid, int, int, int, x11_xid);
    void (*FixesDestroyRegion)(x11_dpy *, x11_xid);
    int (*RRQueryExtension)(x11_dpy *, int *, int *);
    x11_rrmon *(*RRGetMonitors)(x11_dpy *, x11_xid, int, int *);
    void (*RRFreeMonitors)(x11_rrmon *);
    void (*RRSelectInput)(x11_dpy *, x11_xid, int);
    int (*RRUpdateConfiguration)(x11_event *);
    int (*ShmQueryExtension)(x11_dpy *);
    int (*ShmAttach)(x11_dpy *, x11_shminfo *);
    int (*ShmDetach)(x11_dpy *, x11_shminfo *);
    x11_image *(*ShmCreateImage)(x11_dpy *, x11_visual *, unsigned, int, char *, x11_shminfo *, unsigned, unsigned);
    int (*ShmGetImage)(x11_dpy *, x11_xid, x11_image *, int, int, unsigned long);
} X11;

static int x11_loaded, x11_have_rr, x11_have_shm;
static char x11_missing[64];

static int x11_sym(void *lib, void **slot, const char *name)
{
    *slot = lib ? dlsym(lib, name) : NULL;
    if (!*slot && !x11_missing[0]) snprintf(x11_missing, sizeof(x11_missing), "%s", name);
    return *slot != NULL;
}

static int x11_load(goome *g)
{
    if (x11_loaded) return x11_loaded > 0 ? GOOME_OK : gm_fail(g, GOOME_ENODISPLAY, "%s", x11_missing);
    x11_loaded = -1;
    void *xl = dlopen("libX11.so.6", RTLD_NOW | RTLD_GLOBAL), *xf = dlopen("libXfixes.so.3", RTLD_NOW);
    if (!xl) snprintf(x11_missing, sizeof(x11_missing), "cant find libX11.so.6");
    else if (!xf) snprintf(x11_missing, sizeof(x11_missing), "cant find libXfixes.so.3");
    if (!xl || !xf) return gm_fail(g, GOOME_ENODISPLAY, "%s", x11_missing);
    int ok = 1;
#define X11_L(lib, f, n) ok &= x11_sym(lib, (void **)&X11.f, n)
    X11_L(xl, OpenDisplay, "XOpenDisplay");
    X11_L(xl, CloseDisplay, "XCloseDisplay");
    X11_L(xl, DefaultScreen, "XDefaultScreen");
    X11_L(xl, RootWindow, "XRootWindow");
    X11_L(xl, DefaultVisual, "XDefaultVisual");
    X11_L(xl, DefaultDepth, "XDefaultDepth");
    X11_L(xl, MatchVisualInfo, "XMatchVisualInfo");
    X11_L(xl, CreateColormap, "XCreateColormap");
    X11_L(xl, FreeColormap, "XFreeColormap");
    X11_L(xl, CreateWindow, "XCreateWindow");
    X11_L(xl, DestroyWindow, "XDestroyWindow");
    X11_L(xl, MapRaised, "XMapRaised");
    X11_L(xl, UnmapWindow, "XUnmapWindow");
    X11_L(xl, RaiseWindow, "XRaiseWindow");
    X11_L(xl, MoveResizeWindow, "XMoveResizeWindow");
    X11_L(xl, SelectInput, "XSelectInput");
    X11_L(xl, CreateImage, "XCreateImage");
    X11_L(xl, PutImage, "XPutImage");
    X11_L(xl, GetImage, "XGetImage");
    X11_L(xl, CreateGC, "XCreateGC");
    X11_L(xl, FreeGC, "XFreeGC");
    X11_L(xl, SetGraphicsExposures, "XSetGraphicsExposures");
    X11_L(xl, CreatePixmap, "XCreatePixmap");
    X11_L(xl, FreePixmap, "XFreePixmap");
    X11_L(xl, CopyArea, "XCopyArea");
    X11_L(xl, Flush, "XFlush");
    X11_L(xl, Sync, "XSync");
    X11_L(xl, Pending, "XPending");
    X11_L(xl, NextEvent, "XNextEvent");
    X11_L(xl, ConnectionNumber, "XConnectionNumber");
    X11_L(xl, InternAtom, "XInternAtom");
    X11_L(xl, GetAtomName, "XGetAtomName");
    X11_L(xl, Free, "XFree");
    X11_L(xl, ChangeProperty, "XChangeProperty");
    X11_L(xl, StoreName, "XStoreName");
    X11_L(xl, GetSelectionOwner, "XGetSelectionOwner");
    X11_L(xl, GrabKeyboard, "XGrabKeyboard");
    X11_L(xl, UngrabKeyboard, "XUngrabKeyboard");
    X11_L(xl, LookupString, "XLookupString");
    X11_L(xl, SetErrorHandler, "XSetErrorHandler");
    X11_L(xl, GetGeometry, "XGetGeometry");
    X11_L(xl, GrabKey, "XGrabKey");
    X11_L(xl, UngrabKey, "XUngrabKey");
    X11_L(xl, KeysymToKeycode, "XKeysymToKeycode");
    X11_L(xl, QueryPointer, "XQueryPointer");
    X11_L(xl, SetInputFocus, "XSetInputFocus");
    X11_L(xl, GetInputFocus, "XGetInputFocus");
    X11_L(xl, SendEvent, "XSendEvent");
    X11_L(xl, GetWindowProperty, "XGetWindowProperty");
    X11_L(xf, FixesQueryExtension, "XFixesQueryExtension");
    X11_L(xf, FixesCreateRegion, "XFixesCreateRegion");
    X11_L(xf, FixesSetWindowShapeRegion, "XFixesSetWindowShapeRegion");
    X11_L(xf, FixesDestroyRegion, "XFixesDestroyRegion");
    if (!ok) {
        char m[64];
        snprintf(m, sizeof(m), "%s", x11_missing);
        snprintf(x11_missing, sizeof(x11_missing), "libX11 has no %.40s", m);
        return gm_fail(g, GOOME_ENODISPLAY, "%s", x11_missing);
    }
    char keep[64];
    snprintf(keep, sizeof(keep), "%s", x11_missing);
    void *rr = dlopen("libXrandr.so.2", RTLD_NOW), *xe = dlopen("libXext.so.6", RTLD_NOW);
    x11_have_rr = x11_sym(rr, (void **)&X11.RRQueryExtension, "XRRQueryExtension") & x11_sym(rr, (void **)&X11.RRGetMonitors, "XRRGetMonitors") &
                  x11_sym(rr, (void **)&X11.RRFreeMonitors, "XRRFreeMonitors") & x11_sym(rr, (void **)&X11.RRSelectInput, "XRRSelectInput") &
                  x11_sym(rr, (void **)&X11.RRUpdateConfiguration, "XRRUpdateConfiguration");
    x11_have_shm = x11_sym(xe, (void **)&X11.ShmQueryExtension, "XShmQueryExtension") & x11_sym(xe, (void **)&X11.ShmAttach, "XShmAttach") &
                   x11_sym(xe, (void **)&X11.ShmDetach, "XShmDetach") & x11_sym(xe, (void **)&X11.ShmCreateImage, "XShmCreateImage") &
                   x11_sym(xe, (void **)&X11.ShmGetImage, "XShmGetImage");
#undef X11_L
    snprintf(x11_missing, sizeof(x11_missing), "%s", keep);
    x11_loaded = 1;
    return GOOME_OK;
}

static int x11_err;
static int x11_on_err(x11_dpy *d, x11_errev *e) { return x11_err = e->error_code ? e->error_code : 1, 0; }

typedef struct {
    int x0, y0, x1, y1;
} x11_box;

static struct {
    x11_dpy *dpy;
    x11_xid root, win, cmap, cm_atom, pix;
    x11_gc gc;
    x11_visual *visual;
    x11_image *img;
    uint32_t *buf;
    int w, h, wh, scr;
    int composited, mapped, shaped;
    x11_box at;
    int rr_base;
    long checked, empty_at;
    int recheck;
    int nmon;
    goome_canvas cv[GM_MAX_MON];
    x11_box drawn[GM_MAX_MON];
    goome_rect *hits[GM_MAX_MON];
    int nhits[GM_MAX_MON], caphits[GM_MAX_MON];
    x11_xrect *rects;
    int caprects;
    struct {
        int kc;
        unsigned mask;
    } grabbed[GM_MAX_HK];
    int ngrabbed, kb;
    x11_xid kwin;
    int kwin_up;
    unsigned long last_time;
    x11_errfn old_err;
} x11;

static int x11_mon_at(goome *g, double x, double y)
{
    for (int i = 0; i < g->nmon; i++) {
        goome_rect a = g->mon[i].area;
        if (x >= a.x && y >= a.y && x < a.x + a.w && y < a.y + a.h) return i;
    }
    return 0;
}

static int x11_root_size(x11_dpy *d, x11_xid root, int *w, int *h)
{
    x11_xid r;
    int gx, gy;
    unsigned gw = 0, gh = 0, gb, gd;
    if (!X11.GetGeometry(d, root, &r, &gx, &gy, &gw, &gh, &gb, &gd) || !gw || !gh) return 0;
    *w = (int)gw, *h = (int)gh;
    return 1;
}

static int x11_read_monitors(goome *g, goome_monitor *m)
{
    int n = 0;
    if (x11.rr_base >= 0) {
        int got = 0;
        x11_err = 0;
        x11_rrmon *rm = X11.RRGetMonitors(x11.dpy, x11.root, 1, &got);
        for (int i = 0; rm && !x11_err && i < got && n < GM_MAX_MON; i++) {
            int x0 = rm[i].x < 0 ? 0 : rm[i].x, y0 = rm[i].y < 0 ? 0 : rm[i].y;
            int x1 = rm[i].x + rm[i].width, y1 = rm[i].y + rm[i].height;
            if (x1 > x11.w) x1 = x11.w;
            if (y1 > x11.h) y1 = x11.h;
            if (x1 <= x0 || y1 <= y0) continue;
            goome_monitor *o = &m[n];
            memset(o, 0, sizeof(*o));
            char *name = rm[i].name ? X11.GetAtomName(x11.dpy, rm[i].name) : NULL;
            snprintf(o->name, sizeof(o->name), "%s", name ? name : "monitor");
            if (name) X11.Free(name);
            o->id = (int)rm[i].name;
            o->area = (goome_rect){x0, y0, x1 - x0, y1 - y0};
            if (rm[i].primary && n) {
                goome_monitor t = *o;
                memmove(m + 1, m, sizeof(*m) * (size_t)n);
                m[0] = t;
            }
            n++;
        }
        if (rm) X11.RRFreeMonitors(rm);
    }
    if (!n) {
        memset(m, 0, sizeof(*m));
        snprintf(m->name, sizeof(m->name), "screen %d", x11.scr);
        m->area = (goome_rect){0, 0, x11.w, x11.h};
        n = 1;
    }
    for (int i = 0; i < n; i++) m[i].scale = 1, m[i].pw = m[i].area.w, m[i].ph = m[i].area.h;
    return n;
}

static void x11_free_img(void)
{
    if (!x11.img) return;
    x11.img->data = NULL;
    x11.img->f.destroy_image(x11.img);
    x11.img = NULL;
}

static void x11_hide(void)
{
    if (x11.mapped) X11.UnmapWindow(x11.dpy, x11.win), x11.mapped = 0;
    if (x11.pix) X11.FreePixmap(x11.dpy, x11.pix), x11.pix = 0;
    x11.empty_at = 0;
}

static int x11_remake(goome *g, int force)
{
    int w = x11.w, h = x11.h;
    if (!x11_root_size(x11.dpy, x11.root, &w, &h)) w = x11.w, h = x11.h;
    int resized = w != x11.w || h != x11.h || !x11.buf;
    x11.w = w, x11.h = h;
    goome_monitor m[GM_MAX_MON];
    int n = x11_read_monitors(g, m);
    int same = !resized && n == g->nmon && !memcmp(m, g->mon, sizeof(*m) * (size_t)n);
    if (same && !force) return GOOME_OK;
    if (resized) {
        x11_free_img();
        free(x11.buf);
        x11.buf = calloc((size_t)w * h, 4);
        if (!x11.buf) return gm_fail(g, GOOME_ENOMEM, "no memory for a %dx%d overlay", w, h);
        x11.img = X11.CreateImage(x11.dpy, x11.visual, 32, X11_ZPIXMAP, 0, (char *)x11.buf, (unsigned)w, (unsigned)h, 32, w * 4);
        if (!x11.img) return gm_fail(g, GOOME_ENOMEM, "XCreateImage failed for %dx%d", w, h);
    } else
        memset(x11.buf, 0, (size_t)w * h * 4);
    x11_hide();
    x11.wh = h > 1 ? h - 1 : h; // screen sized window = fullscreen to xfwm, 1px short dodges it
    X11.MoveResizeWindow(x11.dpy, x11.win, 0, 0, (unsigned)w, (unsigned)x11.wh);
    x11.at = (x11_box){0, 0, 0, 0};
    for (int i = 0; i < GM_MAX_MON; i++) x11.drawn[i] = (x11_box){0, 0, 0, 0}, x11.nhits[i] = 0;
    x11.nmon = n;
    for (int i = 0; i < n; i++) {
        goome_rect a = m[i].area;
        gm_canvas_reset(&x11.cv[i], x11.buf + (size_t)a.y * w + a.x, a.w, a.h, w, a.x, a.y, 1, i);
    }
    gm_set_monitors(g, m, n);
    return GOOME_OK;
}

static void x11_check(goome *g, int now)
{
    long t = goome_now_ms();
    if (x11.empty_at && t - x11.empty_at > 1000) x11_hide(), X11.Flush(x11.dpy); // not on the first empty frame, a remap shows up blank
    if (!now && !x11.recheck && t - x11.checked < 2000) return;
    x11.checked = t, x11.recheck = 0;
    x11_xid owner = X11.GetSelectionOwner(x11.dpy, x11.cm_atom);
    if ((owner != 0) != x11.composited) {
        x11.composited = owner != 0;
        if (x11.composited && x11.shaped) X11.FixesSetWindowShapeRegion(x11.dpy, x11.win, X11_SHAPE_BOUNDING, 0, 0, 0), x11.shaped = 0;
        x11.at = (x11_box){0, 0, 0, 0};
    }
    x11_remake(g, 0);
}

static void x11_ov_close(goome *g);

static int x11_ov_open(goome *g)
{
    memset(&x11, 0, sizeof(x11));
    x11.rr_base = -1;
    if (!gm_env("DISPLAY")) return gm_fail(g, GOOME_ENODISPLAY, "DISPLAY isnt set, no X server to talk to");
    int e = x11_load(g);
    if (e != GOOME_OK) return e;
    x11.dpy = X11.OpenDisplay(NULL);
    if (!x11.dpy) return gm_fail(g, GOOME_ENODISPLAY, "cant connect to the X server on %s", gm_env("DISPLAY"));
    x11.old_err = X11.SetErrorHandler(x11_on_err);
    int ev, er;
    if (!X11.FixesQueryExtension(x11.dpy, &ev, &er)) {
        X11.SetErrorHandler(x11.old_err);
        X11.CloseDisplay(x11.dpy);
        x11.dpy = NULL;
        return gm_fail(g, GOOME_ENODISPLAY, "the X server has no XFixes, cant make a click-through window");
    }
    x11.scr = X11.DefaultScreen(x11.dpy);
    x11.root = X11.RootWindow(x11.dpy, x11.scr);
    x11_visinfo vi;
    memset(&vi, 0, sizeof(vi));
    if (!X11.MatchVisualInfo(x11.dpy, x11.scr, 32, X11_TRUECOLOR, &vi)) {
        X11.SetErrorHandler(x11.old_err);
        X11.CloseDisplay(x11.dpy);
        x11.dpy = NULL;
        return gm_fail(g, GOOME_ENODISPLAY, "the X server has no 32 bit visual, cant make a see-through window");
    }
    x11.visual = vi.visual;
    char cm[32];
    snprintf(cm, sizeof(cm), "_NET_WM_CM_S%d", x11.scr);
    x11.cm_atom = X11.InternAtom(x11.dpy, cm, 0);
    x11.composited = X11.GetSelectionOwner(x11.dpy, x11.cm_atom) != 0;
    if (x11_have_rr && X11.RRQueryExtension(x11.dpy, &ev, &er)) {
        x11.rr_base = ev;
        X11.RRSelectInput(x11.dpy, x11.root, X11_RR_MASK);
    }
    x11_winattr a;
    memset(&a, 0, sizeof(a));
    a.override_redirect = 1;
    a.colormap = x11.cmap = X11.CreateColormap(x11.dpy, x11.root, vi.visual, 0);
    x11.win = X11.CreateWindow(x11.dpy, x11.root, 0, 0, 1, 1, 0, 32, X11_INPUTOUTPUT, vi.visual,
                               X11_CW_OVERRIDE | X11_CW_COLORMAP | X11_CW_BACKPIXEL | X11_CW_BORDERPIXEL, &a);
    X11.StoreName(x11.dpy, x11.win, g->title);
    x11_xid type = X11.InternAtom(x11.dpy, "_NET_WM_WINDOW_TYPE", 0), notif = X11.InternAtom(x11.dpy, "_NET_WM_WINDOW_TYPE_NOTIFICATION", 0);
    X11.ChangeProperty(x11.dpy, x11.win, type, X11_XA_ATOM, 32, 0, (unsigned char *)&notif, 1);
    long keep = 2; // never unredirect us, xfwm did and the screen went black
    X11.ChangeProperty(x11.dpy, x11.win, X11.InternAtom(x11.dpy, "_NET_WM_BYPASS_COMPOSITOR", 0), X11_XA_CARDINAL, 32, 0, (unsigned char *)&keep, 1);
    x11_xid empty = X11.FixesCreateRegion(x11.dpy, NULL, 0); // clicks go through until the first commit
    X11.FixesSetWindowShapeRegion(x11.dpy, x11.win, X11_SHAPE_INPUT, 0, 0, empty);
    X11.FixesDestroyRegion(x11.dpy, empty);
    X11.SelectInput(x11.dpy, x11.win, X11_EVMASK);
    x11.gc = X11.CreateGC(x11.dpy, x11.win, 0, NULL);
    X11.SetGraphicsExposures(x11.dpy, x11.gc, 0);
    x11.checked = goome_now_ms();
    e = x11_remake(g, 1);
    X11.Sync(x11.dpy, 0);
    if (e == GOOME_OK && x11_err) e = gm_fail(g, GOOME_ENODISPLAY, "the X server said no while making the overlay (error %d)", x11_err);
    if (e != GOOME_OK) {
        x11_ov_close(g);
        return e;
    }
    if (!x11.composited)
        fprintf(stderr, "goome: no compositor running, the overlay has no see-through edges. on xfce turn it on in Settings > Window Manager Tweaks > Compositor\n");
    return GOOME_OK;
}

static goome_canvas *x11_ov_canvas(goome *g, int mon)
{
    if (!x11.dpy) return NULL;
    x11_check(g, 0);
    return mon >= 0 && mon < x11.nmon ? &x11.cv[mon] : NULL;
}

static void x11_bounding_shape(void)
{
    int n = 0, bw = x11.at.x1 - x11.at.x0, bh = x11.at.y1 - x11.at.y0;
    for (int y = 0; y < bh; y++) {
        const uint32_t *row = x11.buf + (size_t)(x11.at.y0 + y) * x11.w + x11.at.x0;
        int x = 0;
        while (x < bw) {
            while (x < bw && (row[x] >> 24) < 110) x++;
            int start = x;
            while (x < bw && (row[x] >> 24) >= 110) x++;
            if (x <= start) continue;
            if (n == x11.caprects) {
                int c = x11.caprects ? x11.caprects * 2 : 4096;
                x11_xrect *r = realloc(x11.rects, sizeof(*r) * (size_t)c);
                if (!r) return;
                x11.rects = r, x11.caprects = c;
            }
            x11.rects[n++] = (x11_xrect){(short)(start + x11.at.x0), (short)(y + x11.at.y0), (unsigned short)(x - start), 1};
        }
    }
    x11_xid reg = X11.FixesCreateRegion(x11.dpy, n ? x11.rects : NULL, n);
    X11.FixesSetWindowShapeRegion(x11.dpy, x11.win, X11_SHAPE_BOUNDING, 0, 0, reg);
    X11.FixesDestroyRegion(x11.dpy, reg);
    x11.shaped = 1;
}

static void x11_input_shape(void)
{
    int total = 0, n = 0;
    for (int i = 0; i < x11.nmon; i++) total += x11.nhits[i];
    if (total > x11.caprects) {
        x11_xrect *r = realloc(x11.rects, sizeof(*r) * (size_t)total);
        if (!r) total = x11.caprects;
        else x11.rects = r, x11.caprects = total;
    }
    for (int i = 0; i < x11.nmon; i++)
        for (int k = 0; k < x11.nhits[i] && n < total; k++) {
            goome_rect h = x11.hits[i][k];
            int x0 = h.x, y0 = h.y, x1 = x0 + h.w, y1 = y0 + h.h;
            if (x0 < 0) x0 = 0;
            if (y0 < 0) y0 = 0;
            if (x1 > 32767) x1 = 32767;
            if (y1 > 32767) y1 = 32767;
            if (x1 <= x0 || y1 <= y0) continue;
            x11.rects[n++] = (x11_xrect){(short)x0, (short)y0, (unsigned short)(x1 - x0), (unsigned short)(y1 - y0)};
        }
    x11_xid reg = X11.FixesCreateRegion(x11.dpy, n ? x11.rects : NULL, n);
    X11.FixesSetWindowShapeRegion(x11.dpy, x11.win, X11_SHAPE_INPUT, 0, 0, reg);
    X11.FixesDestroyRegion(x11.dpy, reg);
}

// pixels go to a pixmap first and then one CopyArea to the window. a big XPutImage gets split up
// and xwayland can show the window halfway through it
static void x11_put(int x0, int y0, int x1, int y1, int show)
{
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > x11.w) x1 = x11.w;
    if (y1 > x11.wh) y1 = x11.wh;
    if (x1 <= x0 || y1 <= y0 || !x11.pix) return;
    X11.PutImage(x11.dpy, x11.pix, x11.gc, x11.img, x0, y0, x0, y0, (unsigned)(x1 - x0), (unsigned)(y1 - y0));
    if (show) X11.CopyArea(x11.dpy, x11.pix, x11.win, x11.gc, x0, y0, (unsigned)(x1 - x0), (unsigned)(y1 - y0), x0, y0);
}

static int x11_ov_commit(goome *g, int mon, goome_canvas *c, const goome_rect *hits, int n)
{
    if (!x11.dpy || mon < 0 || mon >= x11.nmon || c != &x11.cv[mon]) return GOOME_EARG;
    int ox = (int)c->ox, oy = (int)c->oy;
    x11_box b = {c->w, c->h, -1, -1};
    int sx0 = c->dx0 < 0 ? 0 : c->dx0, sx1 = c->dx1 > c->w ? c->w : c->dx1, sy0 = c->dy0 < 0 ? 0 : c->dy0, sy1 = c->dy1 > c->h ? c->h : c->dy1;
    for (int y = sy0; y < sy1; y++) {
        const uint32_t *row = c->px + (size_t)y * c->stride;
        int first = -1, last = -1;
        for (int x = sx0; x < sx1; x++)
            if (row[x] >> 24) {
                if (first < 0) first = x;
                last = x;
            }
        if (first < 0) continue;
        if (first < b.x0) b.x0 = first;
        if (last > b.x1) b.x1 = last;
        if (y < b.y0) b.y0 = y;
        b.y1 = y;
    }
    x11.drawn[mon] = b.x1 < 0 ? (x11_box){0, 0, 0, 0} : (x11_box){b.x0 + ox, b.y0 + oy, b.x1 + 1 + ox, b.y1 + 1 + oy};
    if (n > x11.caphits[mon]) {
        goome_rect *h = realloc(x11.hits[mon], sizeof(*h) * (size_t)n);
        if (!h) n = x11.caphits[mon];
        else x11.hits[mon] = h, x11.caphits[mon] = n;
    }
    if (n) memcpy(x11.hits[mon], hits, sizeof(*hits) * (size_t)n);
    x11.nhits[mon] = n;

    x11_box u = {0, 0, 0, 0};
    for (int i = 0; i < x11.nmon; i++) {
        x11_box d = x11.drawn[i];
        if (d.x1 <= d.x0) continue;
        if (u.x1 <= u.x0) u = d;
        if (d.x0 < u.x0) u.x0 = d.x0;
        if (d.y0 < u.y0) u.y0 = d.y0;
        if (d.x1 > u.x1) u.x1 = d.x1;
        if (d.y1 > u.y1) u.y1 = d.y1;
    }
    if (u.y1 > x11.wh) u.y1 = x11.wh;
    int empty = u.x1 <= u.x0 || u.y1 <= u.y0;
    if (empty && !x11.mapped) {
        x11.at = (x11_box){0, 0, 0, 0};
        X11.Flush(x11.dpy);
        return GOOME_OK;
    }
    x11.at = empty ? (x11_box){0, 0, 0, 0} : u;
    if (empty && !x11.empty_at) x11.empty_at = goome_now_ms();
    if (!empty) x11.empty_at = 0;
    x11_input_shape();
    if (!x11.composited) x11_bounding_shape(); // no compositor = no alpha, cut the window down to the solid px
    int d0, e0, d1, e1;
    if (!x11.mapped) {
        x11.pix = X11.CreatePixmap(x11.dpy, x11.win, (unsigned)x11.w, (unsigned)x11.wh, 32);
        x11_put(u.x0, u.y0, u.x1, u.y1, 0);
        X11.MapRaised(x11.dpy, x11.win), x11.mapped = 1;
        X11.CopyArea(x11.dpy, x11.pix, x11.win, x11.gc, u.x0, u.y0, (unsigned)(u.x1 - u.x0), (unsigned)(u.y1 - u.y0), u.x0, u.y0);
    } else if (gm_canvas_damage(c, &d0, &e0, &d1, &e1))
        x11_put(d0 + ox, e0 + oy, d1 + ox, e1 + oy, 1);
    X11.RaiseWindow(x11.dpy, x11.win);
    X11.Flush(x11.dpy);
    return GOOME_OK;
}

static int x11_mods(unsigned state)
{
    return (state & X11_SHIFT ? GOOME_SHIFT : 0) | (state & X11_CONTROL ? GOOME_CTRL : 0) | (state & X11_MOD1 ? GOOME_ALT : 0) |
           (state & X11_MOD4 ? GOOME_SUPER : 0);
}

static int x11_utf8(char *out, unsigned cp)
{
    if (cp < 0x80) return out[0] = (char)cp, 1;
    if (cp < 0x800) return out[0] = (char)(0xC0 | cp >> 6), out[1] = (char)(0x80 | (cp & 63)), 2;
    if (cp < 0x10000) return out[0] = (char)(0xE0 | cp >> 12), out[1] = (char)(0x80 | ((cp >> 6) & 63)), out[2] = (char)(0x80 | (cp & 63)), 3;
    if (cp < 0x110000)
        return out[0] = (char)(0xF0 | cp >> 18), out[1] = (char)(0x80 | ((cp >> 12) & 63)), out[2] = (char)(0x80 | ((cp >> 6) & 63)),
               out[3] = (char)(0x80 | (cp & 63)), 4;
    return 0;
}

static void x11_key(goome *g, x11_input_ev *k)
{
    unsigned want = k->state & (X11_SHIFT | X11_CONTROL | X11_MOD1 | X11_MOD4);
    for (int i = 0; i < x11.ngrabbed && i < g->nhk; i++)
        if (x11.grabbed[i].kc && x11.grabbed[i].kc == (int)k->detail && x11.grabbed[i].mask == want) {
            goome_event e;
            memset(&e, 0, sizeof(e));
            e.type = GOOME_HOTKEY, e.id = g->hk[i].id, e.key = g->hk[i].key, e.mods = g->hk[i].mods;
            gm_push(g, &e);
            return;
        }
    if (!g->kb_on) return;
    char buf[16] = {0};
    unsigned long sym = 0;
    int n = X11.LookupString(k, buf, (int)sizeof(buf) - 1, &sym, NULL);
    goome_event e;
    memset(&e, 0, sizeof(e));
    e.type = GOOME_KEY;
    int mods = x11_mods(k->state), ch = 0;
    for (size_t i = 0; i < sizeof(x11_keys) / sizeof(x11_keys[0]) && !ch; i++)
        if (x11_keys[i].sym == sym) ch = x11_keys[i].key;
    if (!ch && sym >= 0xffbe && sym <= 0xffc9) ch = GOOME_K_F1 + (int)(sym - 0xffbe);
    if (!ch) {
        unsigned cp = 0;
        if (sym >= 0x20 && sym <= 0xff) cp = (unsigned)sym;
        else if ((sym & 0xff000000ul) == 0x01000000ul) cp = (unsigned)(sym & 0xffffff);
        else if (n == 1 && (unsigned char)buf[0] < 0x80) cp = (unsigned char)buf[0];
        if (!cp && n == 1) cp = (unsigned char)buf[0];
        if (!cp) return;
        if (cp >= 32 && cp != 127 && !(mods & (GOOME_CTRL | GOOME_ALT | GOOME_SUPER))) x11_utf8(e.text, cp);
        ch = cp < 128 ? (int)cp : 0;
        if (!ch && !e.text[0]) return;
    }
    if (ch) {
        int c = gm_combo_key(ch, mods);
        e.key = c & 0xFF, e.mods = c & ~0xFF;
    } else
        e.mods = mods;
    gm_push(g, &e);
}

static void x11_button(goome *g, x11_input_ev *b, int down)
{
    goome_event e;
    memset(&e, 0, sizeof(e));
    e.x = b->x_root, e.y = b->y_root, e.mon = x11_mon_at(g, e.x, e.y), e.mods = x11_mods(b->state);
    if (b->time) x11.last_time = b->time;
    if (b->detail >= 1 && b->detail <= 3) e.type = down ? GOOME_DOWN : GOOME_UP, e.button = (int)b->detail;
    else if (down && (b->detail == 4 || b->detail == 5)) e.type = GOOME_SCROLL, e.dy = b->detail == 5 ? 1 : -1;
    else return; // 6 7 = sideways wheel
    gm_push(g, &e);
}

static void x11_handle(goome *g, x11_event *ev)
{
    int t = ev->type & 0x7f;
    if (x11.rr_base >= 0 && (t == x11.rr_base || t == x11.rr_base + 1)) {
        X11.RRUpdateConfiguration(ev);
        x11.recheck = 1;
        return;
    }
    switch (t) {
    case X11_MOTION:
    case X11_ENTER: {
        goome_event e;
        memset(&e, 0, sizeof(e));
        e.type = GOOME_MOTION, e.x = ev->in.x_root, e.y = ev->in.y_root, e.mon = x11_mon_at(g, e.x, e.y);
        gm_push(g, &e);
        break;
    }
    case X11_BUTTONPRESS: x11_button(g, &ev->in, 1); break;
    case X11_BUTTONRELEASE: x11_button(g, &ev->in, 0); break;
    case X11_KEYPRESS: x11_key(g, &ev->in); break;
    case X11_EXPOSE:
        if (x11.mapped && ev->ex.window == x11.win)
            x11_put(ev->ex.x, ev->ex.y, ev->ex.x + ev->ex.width, ev->ex.y + ev->ex.height, 1);
        break;
    }
}

static int x11_ov_wait(goome *g, int ms)
{
    if (!x11.dpy) return -1;
    long end = goome_now_ms() + (ms > 0 ? ms : 0);
    int fd = X11.ConnectionNumber(x11.dpy);
    for (;;) {
        struct pollfd p = {fd, POLLIN, 0};
        if (poll(&p, 1, 0) > 0 && (p.revents & (POLLHUP | POLLERR | POLLNVAL))) return -1; // catch it first, xlib's io error handler would exit()
        while (X11.Pending(x11.dpy)) {
            x11_event ev;
            X11.NextEvent(x11.dpy, &ev);
            x11_handle(g, &ev);
        }
        x11_check(g, 0);
        if (g->qlen) return 0;
        long left = end - goome_now_ms();
        if (left <= 0) return 0;
        long tick = x11.checked + 2000 - goome_now_ms();
        if (tick < left) left = tick < 1 ? 1 : tick;
        p.revents = 0;
        if (poll(&p, 1, (int)left) > 0 && (p.revents & (POLLHUP | POLLERR | POLLNVAL))) return -1;
    }
}

static int x11_ov_pointer(goome *g, double *x, double *y, int *buttons)
{
    if (!x11.dpy) return 0;
    x11_xid r, child;
    int rx, ry, wx, wy;
    unsigned mask = 0;
    if (!X11.QueryPointer(x11.dpy, x11.root, &r, &child, &rx, &ry, &wx, &wy, &mask)) return 0;
    *x = rx, *y = ry;
    *buttons = (mask & X11_BUTTON1 ? 1 : 0) | (mask & X11_BUTTON2 ? 2 : 0) | (mask & X11_BUTTON3 ? 4 : 0);
    return 1;
}

static void x11_prop(x11_xid w, const char *name, const char *type, int fmt, const void *data, int n)
{
    X11.ChangeProperty(x11.dpy, w, X11.InternAtom(x11.dpy, name, 0), X11.InternAtom(x11.dpy, type, 0), fmt, 0, data, n);
}

static int x11_have_wm(void)
{
    x11_xid type = 0, *w = NULL;
    int fmt = 0;
    unsigned long n = 0, after = 0;
    x11_err = 0;
    int r = X11.GetWindowProperty(x11.dpy, x11.root, X11.InternAtom(x11.dpy, "_NET_SUPPORTING_WM_CHECK", 0), 0, 1, 0, 33, &type, &fmt, &n, &after,
                                  (unsigned char **)&w);
    int yes = r == 0 && !x11_err && w && n == 1 && *w;
    if (w) X11.Free(w);
    return yes;
}

static int x11_focused(void)
{
    x11_xid f = 0;
    int rev;
    X11.GetInputFocus(x11.dpy, &f, &rev);
    return f && f == x11.kwin;
}

// mutter only gives keys to windows it manages, so this is a 1x1 see-through one under the pointer
static void x11_kwin_make(goome *g)
{
    x11_xid r, child;
    int px = 0, py = 0, wx, wy;
    unsigned mask;
    X11.QueryPointer(x11.dpy, x11.root, &r, &child, &px, &py, &wx, &wy, &mask);
    x11_visinfo vi;
    memset(&vi, 0, sizeof(vi));
    X11.MatchVisualInfo(x11.dpy, x11.scr, 32, X11_TRUECOLOR, &vi);
    x11_winattr a;
    memset(&a, 0, sizeof(a));
    a.colormap = x11.cmap;
    x11.kwin = X11.CreateWindow(x11.dpy, x11.root, px, py, 1, 1, 0, 32, X11_INPUTOUTPUT, vi.visual, X11_CW_COLORMAP | X11_CW_BACKPIXEL | X11_CW_BORDERPIXEL, &a);
    X11.StoreName(x11.dpy, x11.kwin, g->title);
    long hints[9] = {1, 1}; // InputHint, input = yes
    x11_prop(x11.kwin, "WM_HINTS", "WM_HINTS", 32, hints, 9);
    long size[18] = {1 | 4 | 16 | 32, px, py, 1, 1, 1, 1, 1, 1}; // USPosition PPosition PMinSize PMaxSize
    x11_prop(x11.kwin, "WM_NORMAL_HINTS", "WM_SIZE_HINTS", 32, size, 18);
    long motif[5] = {2, 0, 0, 0, 0}; // no decorations, a 1px window with a titlebar would be the flash
    x11_prop(x11.kwin, "_MOTIF_WM_HINTS", "_MOTIF_WM_HINTS", 32, motif, 5);
    x11_xid st[3] = {X11.InternAtom(x11.dpy, "_NET_WM_STATE_SKIP_TASKBAR", 0), X11.InternAtom(x11.dpy, "_NET_WM_STATE_SKIP_PAGER", 0),
                     X11.InternAtom(x11.dpy, "_NET_WM_STATE_ABOVE", 0)};
    x11_prop(x11.kwin, "_NET_WM_STATE", "ATOM", 32, st, 3);
    long bypass = 2;
    x11_prop(x11.kwin, "_NET_WM_BYPASS_COMPOSITOR", "CARDINAL", 32, &bypass, 1);
    x11_xid empty = X11.FixesCreateRegion(x11.dpy, NULL, 0);
    X11.FixesSetWindowShapeRegion(x11.dpy, x11.kwin, X11_SHAPE_INPUT, 0, 0, empty);
    X11.FixesDestroyRegion(x11.dpy, empty);
    X11.SelectInput(x11.dpy, x11.kwin, 1L | (1L << 21));
}

static void x11_activate(void)
{
    x11_client_ev c;
    memset(&c, 0, sizeof(c));
    c.type = 33, c.window = x11.kwin, c.format = 32; // ClientMessage
    c.message_type = X11.InternAtom(x11.dpy, "_NET_ACTIVE_WINDOW", 0);
    c.l[0] = 1, c.l[1] = (long)x11.last_time; // 1 = normal app
    X11.SendEvent(x11.dpy, x11.root, 0, (1L << 20) | (1L << 19), &c);
    X11.SetInputFocus(x11.dpy, x11.kwin, 2, x11.last_time);
}

static int x11_ov_keyboard(goome *g, int on)
{
    if (!x11.dpy) return GOOME_EGONE;
    int e = GOOME_OK;
    if (on && !x11.kb && !x11.kwin_up) {
        if (x11_have_wm()) {
            if (!x11.kwin) x11_kwin_make(g);
            unsigned long t = x11.last_time;
            x11_prop(x11.kwin, "_NET_WM_USER_TIME", "CARDINAL", 32, &t, 1);
            X11.MapRaised(x11.dpy, x11.kwin);
            x11.kwin_up = 1;
            X11.Sync(x11.dpy, 0);
            long end = goome_now_ms() + 600;
            int asked = 0;
            while (!x11_focused() && goome_now_ms() < end) {
                while (X11.Pending(x11.dpy)) {
                    x11_event ev;
                    X11.NextEvent(x11.dpy, &ev);
                    x11_handle(g, &ev);
                }
                if (!asked || goome_now_ms() > end - 300) x11_activate(), asked = 1;
                X11.Sync(x11.dpy, 0);
                struct pollfd p = {X11.ConnectionNumber(x11.dpy), POLLIN, 0};
                poll(&p, 1, 20);
            }
            if (x11_focused()) return GOOME_OK;
            X11.UnmapWindow(x11.dpy, x11.kwin);
            x11.kwin_up = 0;
            if (gm_env("WAYLAND_DISPLAY")) { // xwayland grabs only see X windows, the keys would go to the wayland app anyway
                X11.Flush(x11.dpy);
                return gm_fail(g, GOOME_EDENIED, "the desktop wont give keyboard focus to our X window");
            }
        }
        int r = X11.GrabKeyboard(x11.dpy, x11.root, 0, X11_GRAB_ASYNC, X11_GRAB_ASYNC, 0);
        if (r != 0) e = gm_fail(g, GOOME_EDENIED, "couldnt grab the keyboard (X said %d), another app or the desktop holds it", r);
        else x11.kb = 1;
    } else if (!on) {
        if (x11.kb) X11.UngrabKeyboard(x11.dpy, 0), x11.kb = 0;
        if (x11.kwin_up) X11.UnmapWindow(x11.dpy, x11.kwin), x11.kwin_up = 0;
    }
    X11.Flush(x11.dpy);
    return e;
}

static unsigned long x11_keysym(int key)
{
    if (key >= GOOME_K_F1 && key <= GOOME_K_F12) return 0xffbe + (unsigned long)(key - GOOME_K_F1);
    for (size_t i = 0; i < sizeof(x11_keys) / sizeof(x11_keys[0]); i++)
        if (x11_keys[i].key == key) return x11_keys[i].sym;
    return key >= 32 && key < 127 ? (unsigned long)key : 0;
}

static int x11_ov_hotkeys(goome *g)
{
    if (!x11.dpy) return GOOME_EGONE;
    static const unsigned lock[4] = {0, X11_LOCK, X11_MOD2, X11_LOCK | X11_MOD2}; // grabs are exact, so once per caps/num lock mix
    for (int i = 0; i < x11.ngrabbed; i++)
        for (int v = 0; x11.grabbed[i].kc && v < 4; v++) X11.UngrabKey(x11.dpy, x11.grabbed[i].kc, x11.grabbed[i].mask | lock[v], x11.root);
    x11.ngrabbed = g->nhk;
    int bad = -1;
    for (int i = 0; i < g->nhk; i++) {
        int m = g->hk[i].mods;
        unsigned mask = (m & GOOME_SHIFT ? X11_SHIFT : 0) | (m & GOOME_CTRL ? X11_CONTROL : 0) | (m & GOOME_ALT ? X11_MOD1 : 0) | (m & GOOME_SUPER ? X11_MOD4 : 0);
        unsigned long sym = x11_keysym(g->hk[i].key);
        int kc = sym ? X11.KeysymToKeycode(x11.dpy, sym) : 0;
        x11.grabbed[i].kc = 0, x11.grabbed[i].mask = mask;
        if (!kc) {
            if (bad < 0) bad = i;
            continue;
        }
        x11_err = 0;
        for (int v = 0; v < 4; v++) X11.GrabKey(x11.dpy, kc, mask | lock[v], x11.root, 1, X11_GRAB_ASYNC, X11_GRAB_ASYNC);
        X11.Sync(x11.dpy, 0);
        if (x11_err) {
            for (int v = 0; v < 4; v++) X11.UngrabKey(x11.dpy, kc, mask | lock[v], x11.root);
            if (bad < 0) bad = i;
            continue;
        }
        x11.grabbed[i].kc = kc;
    }
    X11.Flush(x11.dpy);
    if (bad >= 0) return gm_fail(g, GOOME_EDENIED, "hotkey %d is taken by another app or isnt on this keyboard", g->hk[bad].id);
    return GOOME_OK;
}

static void x11_ov_close(goome *g)
{
    if (x11.dpy) {
        for (int i = 0; i < x11.ngrabbed; i++)
            for (int v = 0; x11.grabbed[i].kc && v < 4; v++) {
                static const unsigned lock[4] = {0, X11_LOCK, X11_MOD2, X11_LOCK | X11_MOD2};
                X11.UngrabKey(x11.dpy, x11.grabbed[i].kc, x11.grabbed[i].mask | lock[v], x11.root);
            }
        if (x11.kb) X11.UngrabKeyboard(x11.dpy, 0);
        x11_free_img();
        if (x11.pix) X11.FreePixmap(x11.dpy, x11.pix);
        if (x11.gc) X11.FreeGC(x11.dpy, x11.gc);
        if (x11.win) X11.DestroyWindow(x11.dpy, x11.win);
        if (x11.kwin) X11.DestroyWindow(x11.dpy, x11.kwin);
        if (x11.cmap) X11.FreeColormap(x11.dpy, x11.cmap);
        X11.Sync(x11.dpy, 0);
        X11.SetErrorHandler(x11.old_err);
        X11.CloseDisplay(x11.dpy);
    }
    free(x11.buf), free(x11.rects);
    for (int i = 0; i < GM_MAX_MON; i++) free(x11.hits[i]);
    memset(&x11, 0, sizeof(x11));
}

static const gm_ov x11_ov = {"x11", x11_ov_open, x11_ov_canvas, x11_ov_commit, x11_ov_wait, x11_ov_pointer, x11_ov_keyboard, x11_ov_hotkeys,
                             x11_ov_close, GOOME_CAP_GLOBAL_POINTER | GOOME_CAP_KEYBOARD | GOOME_CAP_HOTKEYS};
const gm_ov *gm_ov_x11(void) { return &x11_ov; }

static struct {
    x11_dpy *dpy;
    int own;
    x11_xid root;
    x11_visual *visual;
    int depth;
    x11_shminfo shm;
    int shm_on, shm_w, shm_h;
    uint32_t *out[GM_MAX_MON];
    size_t outn[GM_MAX_MON];
} x11c;

static void x11_shm_drop(void)
{
    if (!x11c.shm_on) return;
    X11.ShmDetach(x11c.dpy, &x11c.shm);
    X11.Sync(x11c.dpy, 0);
    shmdt(x11c.shm.shmaddr);
    memset(&x11c.shm, 0, sizeof(x11c.shm));
    x11c.shm_on = 0;
}

static int x11_shm_make(int w, int h)
{
    if (x11c.shm_on && x11c.shm_w == w && x11c.shm_h == h) return 1;
    x11_shm_drop();
    if (!x11_have_shm || !X11.ShmQueryExtension(x11c.dpy)) return 0;
    int id = shmget(IPC_PRIVATE, (size_t)w * h * 4, IPC_CREAT | 0600);
    if (id < 0) return 0;
    void *addr = shmat(id, NULL, 0);
    if (addr == (void *)-1) {
        shmctl(id, IPC_RMID, NULL);
        return 0;
    }
    x11c.shm.shmid = id, x11c.shm.shmaddr = addr, x11c.shm.read_only = 0;
    x11_err = 0;
    int ok = X11.ShmAttach(x11c.dpy, &x11c.shm);
    X11.Sync(x11c.dpy, 0);
    shmctl(id, IPC_RMID, NULL);
    if (!ok || x11_err) {
        shmdt(addr);
        memset(&x11c.shm, 0, sizeof(x11c.shm));
        x11_have_shm = 0;
        return 0;
    }
    x11c.shm_on = 1, x11c.shm_w = w, x11c.shm_h = h;
    return 1;
}

static int x11_cap_open(goome *g)
{
    memset(&x11c, 0, sizeof(x11c));
    if (x11.dpy) x11c.dpy = x11.dpy;
    else {
        if (!gm_env("DISPLAY")) return gm_fail(g, GOOME_ENODISPLAY, "DISPLAY isnt set, no X server to grab");
        int e = x11_load(g);
        if (e != GOOME_OK) return e;
        x11c.dpy = X11.OpenDisplay(NULL);
        if (!x11c.dpy) return gm_fail(g, GOOME_ENODISPLAY, "cant connect to the X server on %s", gm_env("DISPLAY"));
        x11c.own = 1;
        X11.SetErrorHandler(x11_on_err);
    }
    int scr = X11.DefaultScreen(x11c.dpy);
    x11c.root = X11.RootWindow(x11c.dpy, scr);
    x11c.visual = X11.DefaultVisual(x11c.dpy, scr);
    x11c.depth = X11.DefaultDepth(x11c.dpy, scr);
    return GOOME_OK;
}

static void x11_cap_close(goome *g)
{
    if (x11c.dpy) x11_shm_drop();
    if (x11c.own && x11c.dpy) X11.CloseDisplay(x11c.dpy);
    for (int i = 0; i < GM_MAX_MON; i++) free(x11c.out[i]);
    memset(&x11c, 0, sizeof(x11c));
}

static int x11_convert(goome *g, const x11_image *im, uint32_t *dst, int w, int h)
{
    int bpp = im->bits_per_pixel, swap = im->byte_order == 1;
    unsigned long rm = im->red_mask;
    if (bpp != 32 && bpp != 24) return gm_fail(g, GOOME_ENOTSUP, "the screen is %d bits a pixel, only 24 and 32 work", bpp);
    int ten = rm == 0x3ff00000 || rm == 0x3ff, bgr = rm == 0xff || rm == 0x3ff; // depth 30
    if (!ten && rm != 0xff0000 && rm != 0xff) return gm_fail(g, GOOME_ENOTSUP, "odd pixel layout, red mask %lx", rm);
    for (int y = 0; y < h; y++) {
        const unsigned char *s = (const unsigned char *)im->data + (size_t)y * im->bytes_per_line;
        uint32_t *d = dst + (size_t)y * w;
        for (int x = 0; x < w; x++) {
            uint32_t v;
            if (bpp == 32) {
                const unsigned char *p = s + x * 4;
                v = swap ? (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]
                         : (uint32_t)p[3] << 24 | (uint32_t)p[2] << 16 | (uint32_t)p[1] << 8 | p[0];
            } else {
                const unsigned char *p = s + x * 3;
                v = swap ? (uint32_t)p[0] << 16 | (uint32_t)p[1] << 8 | p[2] : (uint32_t)p[2] << 16 | (uint32_t)p[1] << 8 | p[0];
            }
            uint32_t r, gg, b;
            if (ten) r = (v >> 22) & 255, gg = (v >> 12) & 255, b = (v >> 2) & 255;
            else r = (v >> 16) & 255, gg = (v >> 8) & 255, b = v & 255;
            if (bgr) {
                uint32_t t = r;
                r = b, b = t;
            }
            d[x] = 0xFF000000u | r << 16 | gg << 8 | b;
        }
    }
    return GOOME_OK;
}

static int x11_cap_grab(goome *g, int mon, goome_rect r, goome_image *img)
{
    if (!x11c.dpy) return GOOME_EGONE;
    int rw = x11.w, rh = x11.h;
    if (x11c.own || !rw) x11_root_size(x11c.dpy, x11c.root, &rw, &rh);
    if (r.x < 0) r.w += r.x, r.x = 0;
    if (r.y < 0) r.h += r.y, r.y = 0;
    if (r.x + r.w > rw) r.w = rw - r.x;
    if (r.y + r.h > rh) r.h = rh - r.y;
    if (r.w <= 0 || r.h <= 0) return gm_fail(g, GOOME_EARG, "that rect is off the screen");
    size_t need = (size_t)r.w * r.h;
    if (need > x11c.outn[mon]) {
        uint32_t *p = realloc(x11c.out[mon], need * 4);
        if (!p) return gm_fail(g, GOOME_ENOMEM, "no memory for a %dx%d grab", r.w, r.h);
        x11c.out[mon] = p, x11c.outn[mon] = need;
    }
    x11_image *im = NULL;
    int shm = 0;
    if (x11_shm_make(rw, rh)) {
        im = X11.ShmCreateImage(x11c.dpy, x11c.visual, (unsigned)x11c.depth, X11_ZPIXMAP, x11c.shm.shmaddr, &x11c.shm, (unsigned)r.w, (unsigned)r.h);
        x11_err = 0;
        if (im && (X11.ShmGetImage(x11c.dpy, x11c.root, im, r.x, r.y, ~0ul) == 0 || x11_err)) {
            im->f.destroy_image(im);
            im = NULL;
        }
        shm = im != NULL;
    }
    if (!im) {
        x11_err = 0;
        im = X11.GetImage(x11c.dpy, x11c.root, r.x, r.y, (unsigned)r.w, (unsigned)r.h, ~0ul, X11_ZPIXMAP);
        if (!im || x11_err) {
            if (im) im->f.destroy_image(im);
            return gm_fail(g, GOOME_EGONE, "XGetImage failed, the screen changed under us?");
        }
    }
    int e = x11_convert(g, im, x11c.out[mon], r.w, r.h);
    if (shm) im->data = NULL;
    im->f.destroy_image(im);
    if (e != GOOME_OK) return e;
    img->px = x11c.out[mon], img->w = r.w, img->h = r.h, img->stride = r.w, img->area = r;
    img->has_self = 1;
    return GOOME_OK;
}

static const gm_cap x11_cap = {"x11", NULL, x11_cap_open, x11_cap_grab, x11_cap_close, GOOME_CAP_CAPTURE};
const gm_cap *gm_cap_x11(void) { return &x11_cap; }

#endif
