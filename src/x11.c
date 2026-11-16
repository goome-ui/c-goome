#include "internal.h"

#if defined(GM_LINUX)

#include <dlfcn.h>
#include <locale.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>

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

typedef struct {
    int type;
    unsigned long serial;
    int send_event;
    x11_dpy *display;
    x11_xid event, window;
    int x, y, width, height, border_width;
    x11_xid above;
    int override_redirect;
} x11_conf_ev;

typedef struct {
    int type;
    unsigned long serial;
    int send_event;
    x11_dpy *display;
    x11_xid window;
    int mode, detail;
} x11_focus_ev;

typedef struct {
    int type;
    unsigned long serial;
    int send_event;
    x11_dpy *display;
    x11_xid owner, requestor, selection, target, property;
    unsigned long time;
} x11_selreq_ev;

typedef struct {
    int type;
    unsigned long serial;
    int send_event;
    x11_dpy *display;
    x11_xid requestor, selection, target, property;
    unsigned long time;
} x11_selnot_ev;

typedef struct {
    int type;
    unsigned long serial;
    int send_event;
    x11_dpy *display;
    x11_xid window, atom;
    unsigned long time;
    int state;
} x11_prop_ev;

typedef struct {
    int type;
    unsigned long serial;
    int send_event;
    x11_dpy *display;
    x11_xid window, message_type;
    int format;
    long l[5];
} x11_client_ev;

typedef union {
    int type;
    x11_input_ev in;
    x11_expose_ev ex;
    x11_conf_ev conf;
    x11_focus_ev focus;
    x11_selreq_ev sreq;
    x11_selnot_ev snot;
    x11_prop_ev prop;
    x11_client_ev cm;
    long pad[24];
} x11_event;

typedef struct {
    unsigned long pixel;
    unsigned short red, green, blue;
    char flags, pad;
} x11_color;

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
    int (*InternAtoms)(x11_dpy *, char **, int, int, x11_xid *);
    int (*TranslateCoordinates)(x11_dpy *, x11_xid, x11_xid, int, int, int *, int *, x11_xid *);
    int (*SetSelectionOwner)(x11_dpy *, x11_xid, x11_xid, unsigned long);
    int (*ConvertSelection)(x11_dpy *, x11_xid, x11_xid, x11_xid, x11_xid, unsigned long);
    int (*DeleteProperty)(x11_dpy *, x11_xid, x11_xid);
    int (*DefineCursor)(x11_dpy *, x11_xid, x11_xid);
    x11_xid (*CreateFontCursor)(x11_dpy *, unsigned);
    x11_xid (*CreatePixmapCursor)(x11_dpy *, x11_xid, x11_xid, x11_color *, x11_color *, unsigned, unsigned);
    x11_xid (*CreateBitmapFromData)(x11_dpy *, x11_xid, const char *, unsigned, unsigned);
    int (*FreeCursor)(x11_dpy *, x11_xid);
    int (*UngrabPointer)(x11_dpy *, unsigned long);
    int (*GrabPointer)(x11_dpy *, x11_xid, int, unsigned, int, int, x11_xid, x11_xid, unsigned long);
    int (*MoveWindow)(x11_dpy *, x11_xid, int, int);
    int (*ResizeWindow)(x11_dpy *, x11_xid, unsigned, unsigned);
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
    X11_L(xl, InternAtoms, "XInternAtoms");
    X11_L(xl, TranslateCoordinates, "XTranslateCoordinates");
    X11_L(xl, SetSelectionOwner, "XSetSelectionOwner");
    X11_L(xl, ConvertSelection, "XConvertSelection");
    X11_L(xl, DeleteProperty, "XDeleteProperty");
    X11_L(xl, DefineCursor, "XDefineCursor");
    X11_L(xl, CreateFontCursor, "XCreateFontCursor");
    X11_L(xl, CreatePixmapCursor, "XCreatePixmapCursor");
    X11_L(xl, CreateBitmapFromData, "XCreateBitmapFromData");
    X11_L(xl, FreeCursor, "XFreeCursor");
    X11_L(xl, UngrabPointer, "XUngrabPointer");
    X11_L(xl, GrabPointer, "XGrabPointer");
    X11_L(xl, MoveWindow, "XMoveWindow");
    X11_L(xl, ResizeWindow, "XResizeWindow");
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

typedef struct {
    int kc;
    unsigned mask;
} x11_grab;

static int x11_err_users;
static x11_errfn x11_err_old;

static void x11_err_on(void)
{
    if (!x11_err_users++) x11_err_old = X11.SetErrorHandler(x11_on_err);
}

static void x11_err_off(void)
{
    if (x11_err_users > 0 && !--x11_err_users) X11.SetErrorHandler(x11_err_old);
}

static struct {
    x11_dpy *dpy;
    x11_xid root, win, cmap, cm_atom, pix;
    x11_gc gc;
    x11_visual *visual;
    x11_image *img;
    uint32_t *buf;
    int w, h, wh, scr;
    int composited, mapped, shaped, dry;
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
    x11_grab grabbed[GM_MAX_HK];
    int ngrabbed, kb;
    x11_xid kwin, back;
    int kwin_up, grab, took, has_back;
    unsigned long last_time;
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
static int x11_win_open(goome *g);
static const gm_ov x11_wov;

static x11_xid x11_ov_win(goome *g)
{
    x11_winattr a;
    memset(&a, 0, sizeof(a));
    a.override_redirect = 1;
    a.colormap = x11.cmap;
    x11_xid w = X11.CreateWindow(x11.dpy, x11.root, 0, 0, x11.w > 0 ? (unsigned)x11.w : 1, x11.wh > 0 ? (unsigned)x11.wh : 1, 0, 32, X11_INPUTOUTPUT,
                                 x11.visual, X11_CW_OVERRIDE | X11_CW_COLORMAP | X11_CW_BACKPIXEL | X11_CW_BORDERPIXEL, &a);
    X11.StoreName(x11.dpy, w, g->title);
    x11_xid type = X11.InternAtom(x11.dpy, "_NET_WM_WINDOW_TYPE", 0), notif = X11.InternAtom(x11.dpy, "_NET_WM_WINDOW_TYPE_NOTIFICATION", 0);
    X11.ChangeProperty(x11.dpy, w, type, X11_XA_ATOM, 32, 0, (unsigned char *)&notif, 1);
    long keep = 2; // never unredirect us, xfwm did and the screen went black
    X11.ChangeProperty(x11.dpy, w, X11.InternAtom(x11.dpy, "_NET_WM_BYPASS_COMPOSITOR", 0), X11_XA_CARDINAL, 32, 0, (unsigned char *)&keep, 1);
    x11_xid empty = X11.FixesCreateRegion(x11.dpy, NULL, 0); // clicks go through until the first commit
    X11.FixesSetWindowShapeRegion(x11.dpy, w, X11_SHAPE_INPUT, 0, 0, empty);
    X11.FixesDestroyRegion(x11.dpy, empty);
    X11.SelectInput(x11.dpy, w, X11_EVMASK);
    return w;
}

static int x11_ov_open(goome *g)
{
    if (g->opts.mode == GOOME_MODE_WINDOW) {
        g->ov = &x11_wov;
        return x11_win_open(g);
    }
    if (g->opts.mode != GOOME_MODE_OVERLAY) return gm_fail(g, GOOME_ENOTSUP, "x11 does overlay and window mode only");
    memset(&x11, 0, sizeof(x11));
    x11.rr_base = -1;
    if (!gm_env("DISPLAY")) return gm_fail(g, GOOME_ENODISPLAY, "DISPLAY isnt set, no X server to talk to");
    int e = x11_load(g);
    if (e != GOOME_OK) return e;
    x11.dpy = X11.OpenDisplay(NULL);
    if (!x11.dpy) return gm_fail(g, GOOME_ENODISPLAY, "cant connect to the X server on %s", gm_env("DISPLAY"));
    x11_err_on();
    int ev, er;
    if (!X11.FixesQueryExtension(x11.dpy, &ev, &er)) {
        x11_err_off();
        X11.CloseDisplay(x11.dpy);
        x11.dpy = NULL;
        return gm_fail(g, GOOME_ENODISPLAY, "the X server has no XFixes, cant make a click-through window");
    }
    x11.scr = X11.DefaultScreen(x11.dpy);
    x11.root = X11.RootWindow(x11.dpy, x11.scr);
    x11_visinfo vi;
    memset(&vi, 0, sizeof(vi));
    if (!X11.MatchVisualInfo(x11.dpy, x11.scr, 32, X11_TRUECOLOR, &vi)) {
        x11_err_off();
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
    x11.cmap = X11.CreateColormap(x11.dpy, x11.root, vi.visual, 0);
    x11.win = x11_ov_win(g);
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
    int nh = 0;
    for (int i = 0; i < x11.nmon; i++) nh += x11.nhits[i];
    x11_xid old = 0;
    if (x11.mapped && x11.dry && nh && !empty) {
        old = x11.win;
        x11.win = x11_ov_win(g);
        x11.mapped = 0;
        if (x11.pix) X11.FreePixmap(x11.dpy, x11.pix), x11.pix = 0;
    }
    if (!x11.mapped) x11.dry = !nh;
    x11_input_shape();
    if (!x11.composited) x11_bounding_shape(); // no compositor = no alpha, cut the window down to the solid px
    int d0, e0, d1, e1;
    if (!x11.mapped) {
        x11.pix = X11.CreatePixmap(x11.dpy, x11.win, (unsigned)x11.w, (unsigned)x11.wh, 32);
        x11_put(u.x0, u.y0, u.x1, u.y1, 0);
        X11.MapRaised(x11.dpy, x11.win), x11.mapped = 1;
        X11.CopyArea(x11.dpy, x11.pix, x11.win, x11.gc, u.x0, u.y0, (unsigned)(u.x1 - u.x0), (unsigned)(u.y1 - u.y0), u.x0, u.y0);
        if (old && x11.grab) X11.GrabPointer(x11.dpy, x11.win, 0, 4 | 8 | 64, X11_GRAB_ASYNC, X11_GRAB_ASYNC, 0, 0, 0);
        if (old) X11.DestroyWindow(x11.dpy, old);
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

static int x11i_lookup(goome *g, x11_input_ev *k);
static int x11i_filter(x11_event *ev);
static void x11i_drop(goome *g);

static void x11_key(goome *g, x11_input_ev *k, const x11_grab *gr, int ngr, int keys)
{
    unsigned want = k->state & (X11_SHIFT | X11_CONTROL | X11_MOD1 | X11_MOD4);
    for (int i = 0; i < ngr && i < g->nhk; i++)
        if (gr[i].kc && gr[i].kc == (int)k->detail && gr[i].mask == want) {
            goome_event e;
            memset(&e, 0, sizeof(e));
            e.type = GOOME_HOTKEY, e.id = g->hk[i].id, e.key = g->hk[i].key, e.mods = g->hk[i].mods;
            gm_push(g, &e);
            return;
        }
    if (!keys) return;
    if (x11i_lookup(g, k)) return;
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
    case X11_KEYPRESS: x11_key(g, &ev->in, x11.grabbed, x11.ngrabbed, g->kb_on); break;
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
            if (x11i_filter(&ev)) continue;
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

static void x11_prop_on(x11_dpy *d, x11_xid w, const char *name, const char *type, int fmt, const void *data, int n)
{
    X11.ChangeProperty(d, w, X11.InternAtom(d, name, 0), X11.InternAtom(d, type, 0), fmt, 0, data, n);
}

static void x11_prop(x11_xid w, const char *name, const char *type, int fmt, const void *data, int n) { x11_prop_on(x11.dpy, w, name, type, fmt, data, n); }

static int x11_wm_on(x11_dpy *d, x11_xid root)
{
    x11_xid type = 0, *w = NULL;
    int fmt = 0;
    unsigned long n = 0, after = 0;
    x11_err = 0;
    int r = X11.GetWindowProperty(d, root, X11.InternAtom(d, "_NET_SUPPORTING_WM_CHECK", 0), 0, 1, 0, 33, &type, &fmt, &n, &after,
                                  (unsigned char **)&w);
    int yes = r == 0 && !x11_err && w && n == 1 && *w;
    if (w) X11.Free(w);
    return yes;
}

static int x11_have_wm(void) { return x11_wm_on(x11.dpy, x11.root); }

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

static int x11_kwin_focus(goome *g)
{
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
    if (x11_focused()) return 1;
    X11.UnmapWindow(x11.dpy, x11.kwin);
    x11.kwin_up = 0;
    return 0;
}

static x11_xid x11_active(void)
{
    x11_xid type = 0, *w = NULL, got = 0;
    int fmt = 0, rev = 0;
    unsigned long n = 0, after = 0;
    x11_err = 0;
    if (X11.GetWindowProperty(x11.dpy, x11.root, X11.InternAtom(x11.dpy, "_NET_ACTIVE_WINDOW", 0), 0, 1, 0, 33, &type, &fmt, &n, &after,
                              (unsigned char **)&w) == 0 && !x11_err && w && n == 1)
        got = *w;
    if (w) X11.Free(w);
    if (!got) X11.GetInputFocus(x11.dpy, &got, &rev);
    return got;
}

static void x11_focus_back(void)
{
    if (x11.kwin_up) X11.UnmapWindow(x11.dpy, x11.kwin), x11.kwin_up = 0;
    if (x11.has_back && x11.took) X11.SetInputFocus(x11.dpy, x11.back ? x11.back : 1, 1, 0);
    else if (x11.has_back && x11.back > 1) {
        x11_client_ev c;
        memset(&c, 0, sizeof(c));
        c.type = 33, c.window = x11.back, c.format = 32;
        c.message_type = X11.InternAtom(x11.dpy, "_NET_ACTIVE_WINDOW", 0);
        c.l[0] = 2, c.l[1] = 0;
        X11.SendEvent(x11.dpy, x11.root, 0, (1L << 20) | (1L << 19), &c);
    }
    x11.took = 0, x11.has_back = 0, x11.back = 0;
}

static int x11_ov_keyboard(goome *g, int on)
{
    if (!x11.dpy) return GOOME_EGONE;
    int e = GOOME_OK;
    if (on && !x11.kb && !x11.kwin_up) {
        if (x11_have_wm()) {
            if (x11_kwin_focus(g)) return GOOME_OK;
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
        if (!x11.grab) x11_focus_back();
    }
    X11.Flush(x11.dpy);
    return e;
}

static int x11_ov_grab(goome *g, int on)
{
    if (!x11.dpy) return GOOME_EGONE;
    int e = GOOME_OK;
    if (on && !x11.grab) {
        x11.grab = 1;
        int r = X11.GrabPointer(x11.dpy, x11.win, 0, 4 | 8 | 64, X11_GRAB_ASYNC, X11_GRAB_ASYNC, 0, 0, 0);
        if (r != 0) e = gm_fail(g, GOOME_EDENIED, "couldnt grab the pointer (X said %d)", r);
        if (!x11.kwin_up && !x11.took) {
            x11_xid f = 0;
            int rev;
            X11.GetInputFocus(x11.dpy, &f, &rev);
            x11.back = x11_active(), x11.has_back = 1;
            if (x11_have_wm()) {
                if (!x11_kwin_focus(g) && e == GOOME_OK) e = gm_fail(g, GOOME_EDENIED, "the window manager wont give our window focus");
            } else if (f != x11.win)
                X11.SetInputFocus(x11.dpy, x11.win, 1, 0), x11.took = 1;
        }
    } else if (!on && x11.grab) {
        x11.grab = 0;
        X11.UngrabPointer(x11.dpy, 0);
        if (!g->kb_on) x11_focus_back();
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

static const unsigned x11_lock[4] = {0, X11_LOCK, X11_MOD2, X11_LOCK | X11_MOD2}; // grabs are exact, so once per caps/num lock mix

static void x11_ungrab_keys(x11_dpy *d, x11_xid root, const x11_grab *gr, int n)
{
    for (int i = 0; i < n; i++)
        for (int v = 0; gr[i].kc && v < 4; v++) X11.UngrabKey(d, gr[i].kc, gr[i].mask | x11_lock[v], root);
}

static int x11_grab_keys(goome *g, x11_dpy *d, x11_xid root, x11_grab *gr, int *ngr)
{
    x11_ungrab_keys(d, root, gr, *ngr);
    *ngr = g->nhk;
    int bad = -1;
    for (int i = 0; i < g->nhk; i++) {
        int m = g->hk[i].mods;
        unsigned mask = (m & GOOME_SHIFT ? X11_SHIFT : 0) | (m & GOOME_CTRL ? X11_CONTROL : 0) | (m & GOOME_ALT ? X11_MOD1 : 0) | (m & GOOME_SUPER ? X11_MOD4 : 0);
        unsigned long sym = x11_keysym(g->hk[i].key);
        int kc = sym ? X11.KeysymToKeycode(d, sym) : 0;
        gr[i].kc = 0, gr[i].mask = mask;
        if (!kc) {
            if (bad < 0) bad = i;
            continue;
        }
        x11_err = 0;
        for (int v = 0; v < 4; v++) X11.GrabKey(d, kc, mask | x11_lock[v], root, 1, X11_GRAB_ASYNC, X11_GRAB_ASYNC);
        X11.Sync(d, 0);
        if (x11_err) {
            for (int v = 0; v < 4; v++) X11.UngrabKey(d, kc, mask | x11_lock[v], root);
            if (bad < 0) bad = i;
            continue;
        }
        gr[i].kc = kc;
    }
    X11.Flush(d);
    if (bad >= 0) return gm_fail(g, GOOME_EDENIED, "hotkey %d is taken by another app or isnt on this keyboard", g->hk[bad].id);
    return GOOME_OK;
}

static int x11_ov_hotkeys(goome *g)
{
    if (!x11.dpy) return GOOME_EGONE;
    return x11_grab_keys(g, x11.dpy, x11.root, x11.grabbed, &x11.ngrabbed);
}

enum {
    X11_A_PROTOCOLS, X11_A_DELETE, X11_A_PING, X11_A_NAME, X11_A_UTF8, X11_A_CLIPBOARD, X11_A_TARGETS, X11_A_SEL, X11_A_DND,
    X11_A_STATE, X11_A_MAXV, X11_A_MAXH, X11_A_FULL, X11_A_HIDDEN, X11_A_ABOVE, X11_A_MOVERESIZE, X11_A_CHANGE_STATE, X11_A_WM_STATE,
    X11_A_AWARE, X11_A_ENTER, X11_A_POSITION, X11_A_STATUS, X11_A_LEAVE, X11_A_DROP, X11_A_FINISHED, X11_A_DNDSEL, X11_A_TYPELIST,
    X11_A_COPY, X11_A_URILIST, X11_A_INCR, X11_A_RM, X11_A_STRING, X11_A_TEXT, X11_A_PLAIN, X11_A_N
};
static const char *x11_anames[X11_A_N] = {
    "WM_PROTOCOLS", "WM_DELETE_WINDOW", "_NET_WM_PING", "_NET_WM_NAME", "UTF8_STRING", "CLIPBOARD", "TARGETS", "GOOME_SEL", "GOOME_DND",
    "_NET_WM_STATE", "_NET_WM_STATE_MAXIMIZED_VERT", "_NET_WM_STATE_MAXIMIZED_HORZ", "_NET_WM_STATE_FULLSCREEN", "_NET_WM_STATE_HIDDEN",
    "_NET_WM_STATE_ABOVE", "_NET_WM_MOVERESIZE", "WM_CHANGE_STATE", "WM_STATE", "XdndAware", "XdndEnter", "XdndPosition", "XdndStatus",
    "XdndLeave", "XdndDrop", "XdndFinished", "XdndSelection", "XdndTypeList", "XdndActionCopy", "text/uri-list", "INCR",
    "RESOURCE_MANAGER", "STRING", "TEXT", "text/plain;charset=utf-8"};

typedef struct {
    x11_dpy *dpy;
    x11_xid root, win, cmap, pix, cur[GOOME_CURSOR_HIDDEN + 1];
    x11_xid a[X11_A_N];
    x11_gc gc;
    x11_visual *visual;
    x11_image *img;
    uint32_t *buf;
    int depth, scr, ww, wh, bw, bh, wm, mapped, focused;
    double scale;
    goome_canvas cv;
    x11_grab grabbed[GM_MAX_HK];
    int ngrabbed;
    unsigned long time;
    int prx, pry, pbtn, held;
    char *sel;
    size_t sel_len;
    int own;
    int sel_state, incr;
    x11_xid dnd_src;
    int dnd_ok, dnd_x, dnd_y;
    char *drops;
    int dlen, doff;
} x11_win;

static char *x11_dup(const char *s, size_t n)
{
    char *r = malloc(n + 1);
    if (r) memcpy(r, s, n), r[n] = 0;
    return r;
}

static int x11_hex(int c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; }

int gm_uri_paths(const char *s, size_t n, char **out)
{
    char *r = malloc(n + 1);
    int len = 0;
    *out = r;
    if (!r) return 0;
    size_t i = 0;
    while (i < n) {
        size_t e = i;
        while (e < n && s[e] != '\r' && s[e] != '\n') e++;
        size_t k = i;
        if (e - i > 5 && !memcmp(s + i, "file:", 5) && s[i] != '#') {
            k = i + 5;
            if (e - k > 2 && s[k] == '/' && s[k + 1] == '/') {
                k += 2;
                while (k < e && s[k] != '/') k++;
            }
            int start = len;
            for (; k < e; k++) {
                int a, b;
                if (s[k] == '%' && k + 2 < e && (a = x11_hex((unsigned char)s[k + 1])) >= 0 && (b = x11_hex((unsigned char)s[k + 2])) >= 0)
                    r[len++] = (char)(a * 16 + b), k += 2;
                else
                    r[len++] = s[k];
            }
            if (len > start && r[start] == '/' && !memchr(r + start, 0, (size_t)(len - start))) r[len++] = 0;
            else len = start;
        }
        i = e;
        while (i < n && (s[i] == '\r' || s[i] == '\n')) i++;
    }
    return len;
}

void gm_drop_next(goome *g, const char *drops, int *off, int len, double x, double y)
{
    if (!drops || *off >= len) return;
    const char *p = drops + *off;
    *off += (int)strlen(p) + 1;
    gm_drop(g, x, y, p);
}

static x11_dpy *x11_win_dpy(goome *g) { return ((x11_win *)g->ov_priv)->dpy; }

static double x11_dpi_scale(x11_win *w)
{
    x11_xid type = 0;
    int fmt = 0;
    unsigned long n = 0, after = 0;
    unsigned char *d = NULL;
    double sc = 1;
    if (X11.GetWindowProperty(w->dpy, w->root, w->a[X11_A_RM], 0, 16384, 0, w->a[X11_A_STRING], &type, &fmt, &n, &after, &d) == 0 && d && fmt == 8) {
        const char *p = (const char *)d, *end = p + n;
        for (; p + 8 < end; p++)
            if ((p == (const char *)d || p[-1] == '\n') && !memcmp(p, "Xft.dpi:", 8)) {
                p += 8;
                while (p < end && (*p == ' ' || *p == '\t')) p++;
                int dpi = gm_num(&p);
                if (dpi >= 48 && dpi <= 960) sc = dpi / 96.0;
                break;
            }
    }
    if (d) X11.Free(d);
    return sc;
}

static int x11_px(int v, double sc) { return (int)(v * sc + 0.5); }

static void x11_win_mons(goome *g, x11_win *w)
{
    goome_monitor m;
    memset(&m, 0, sizeof(m));
    snprintf(m.name, sizeof(m.name), "%s", g->title);
    m.area = (goome_rect){0, 0, (int)(w->ww / w->scale + 0.5), (int)(w->wh / w->scale + 0.5)};
    m.scale = w->scale, m.pw = w->ww, m.ph = w->wh;
    gm_set_monitors(g, &m, 1);
}

static void x11_win_hints(goome *g, x11_win *w, int x, int y, int pos)
{
    long h[18] = {0};
    int fixed = !(g->opts.window_flags & GOOME_WINDOW_RESIZABLE);
    h[0] = 16 | 512 | (pos ? 1 | 4 : 0) | (fixed ? 32 : 0);
    h[1] = x, h[2] = y, h[3] = w->ww, h[4] = w->wh;
    h[5] = fixed ? w->ww : x11_px(g->opts.min_width > 0 ? g->opts.min_width : 1, w->scale);
    h[6] = fixed ? w->wh : x11_px(g->opts.min_height > 0 ? g->opts.min_height : 1, w->scale);
    h[7] = w->ww, h[8] = w->wh;
    h[17] = 10;
    x11_prop_on(w->dpy, w->win, "WM_NORMAL_HINTS", "WM_SIZE_HINTS", 32, h, 18);
}

static void x11_win_title(goome *g, x11_win *w)
{
    X11.StoreName(w->dpy, w->win, g->title);
    X11.ChangeProperty(w->dpy, w->win, w->a[X11_A_NAME], w->a[X11_A_UTF8], 8, 0, (const unsigned char *)g->title, (int)strlen(g->title));
}

static void x11_win_send(x11_win *w, x11_xid to, x11_xid win, int type, long a, long b, long c, long d, long e)
{
    x11_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.cm.type = 33, ev.cm.window = win, ev.cm.message_type = w->a[type], ev.cm.format = 32;
    ev.cm.l[0] = a, ev.cm.l[1] = b, ev.cm.l[2] = c, ev.cm.l[3] = d, ev.cm.l[4] = e;
    X11.SendEvent(w->dpy, to, 0, to == w->root ? (1L << 20) | (1L << 19) : 0, &ev.cm);
}

static void x11_win_close(goome *g);

static int x11_win_open(goome *g)
{
    if (!gm_env("DISPLAY")) return gm_fail(g, GOOME_ENODISPLAY, "DISPLAY isnt set, no X server to talk to");
    int e = x11_load(g);
    if (e != GOOME_OK) return e;
    x11_win *w = calloc(1, sizeof(*w));
    if (!w) return gm_fail(g, GOOME_ENOMEM, "out of memory");
    w->dpy = X11.OpenDisplay(NULL);
    if (!w->dpy) {
        free(w);
        return gm_fail(g, GOOME_ENODISPLAY, "cant connect to the X server on %s", gm_env("DISPLAY"));
    }
    g->ov_priv = w;
    x11_err_on();
    x11_err = 0;
    w->scr = X11.DefaultScreen(w->dpy);
    w->root = X11.RootWindow(w->dpy, w->scr);
    X11.InternAtoms(w->dpy, (char **)x11_anames, X11_A_N, 0, w->a);
    w->wm = x11_wm_on(w->dpy, w->root);
    w->scale = x11_dpi_scale(w);
    int fl = g->opts.window_flags;
    x11_visinfo vi;
    memset(&vi, 0, sizeof(vi));
    w->depth = (fl & GOOME_WINDOW_TRANSPARENT) && X11.MatchVisualInfo(w->dpy, w->scr, 32, X11_TRUECOLOR, &vi) ? 32 : 24;
    if (w->depth == 24 && !X11.MatchVisualInfo(w->dpy, w->scr, 24, X11_TRUECOLOR, &vi)) {
        x11_win_close(g);
        return gm_fail(g, GOOME_ENODISPLAY, "the X server has no 24 bit visual");
    }
    w->visual = vi.visual;
    w->ww = x11_px(g->opts.width, w->scale), w->wh = x11_px(g->opts.height, w->scale);
    int x = 0, y = 0, rw = 0, rh = 0;
    if (fl & GOOME_WINDOW_CENTER) {
        goome_rect a = {0, 0, 0, 0};
        x11_root_size(w->dpy, w->root, &rw, &rh);
        a.w = rw, a.h = rh;
        int got = 0, ev, er;
        x11_rrmon *rm = x11_have_rr && X11.RRQueryExtension(w->dpy, &ev, &er) ? X11.RRGetMonitors(w->dpy, w->root, 1, &got) : NULL;
        for (int i = 0; i < got; i++)
            if (rm[i].primary || i == 0) a = (goome_rect){rm[i].x, rm[i].y, rm[i].width, rm[i].height};
        if (rm) X11.RRFreeMonitors(rm);
        x = a.x + (a.w - w->ww) / 2, y = a.y + (a.h - w->wh) / 2;
        if (x < a.x) x = a.x;
        if (y < a.y) y = a.y;
    }
    x11_winattr at;
    memset(&at, 0, sizeof(at));
    at.colormap = w->cmap = X11.CreateColormap(w->dpy, w->root, w->visual, 0);
    at.bit_gravity = 1;
    w->win = X11.CreateWindow(w->dpy, w->root, x, y, (unsigned)w->ww, (unsigned)w->wh, 0, w->depth, X11_INPUTOUTPUT, w->visual,
                              X11_CW_BACKPIXEL | X11_CW_BORDERPIXEL | 16 | X11_CW_COLORMAP, &at);
    X11.SelectInput(w->dpy, w->win, 1L | 4L | 8L | 16L | 64L | (1L << 15) | (1L << 17) | (1L << 21) | (1L << 22));
    X11.SelectInput(w->dpy, w->root, 1L << 22);
    x11_win_title(g, w);
    char cls[140];
    int cl = snprintf(cls, sizeof(cls), "%s", g->title) + 1;
    cl += snprintf(cls + cl, sizeof(cls) - (size_t)cl, "%s", g->title) + 1;
    x11_prop_on(w->dpy, w->win, "WM_CLASS", "STRING", 8, cls, cl);
    x11_xid protos[2] = {w->a[X11_A_DELETE], w->a[X11_A_PING]};
    X11.ChangeProperty(w->dpy, w->win, w->a[X11_A_PROTOCOLS], X11_XA_ATOM, 32, 0, (unsigned char *)protos, 2);
    long pid = (long)getpid(), ver = 5;
    x11_prop_on(w->dpy, w->win, "_NET_WM_PID", "CARDINAL", 32, &pid, 1);
    X11.ChangeProperty(w->dpy, w->win, w->a[X11_A_AWARE], X11_XA_ATOM, 32, 0, (unsigned char *)&ver, 1);
    long hints[9] = {1 | 2, 1, 1};
    x11_prop_on(w->dpy, w->win, "WM_HINTS", "WM_HINTS", 32, hints, 9);
    x11_win_hints(g, w, x, y, fl & GOOME_WINDOW_CENTER);
    if (fl & GOOME_WINDOW_BORDERLESS) {
        long motif[5] = {2, 0, 0, 0, 0};
        x11_prop_on(w->dpy, w->win, "_MOTIF_WM_HINTS", "_MOTIF_WM_HINTS", 32, motif, 5);
    }
    if (fl & GOOME_WINDOW_ON_TOP) X11.ChangeProperty(w->dpy, w->win, w->a[X11_A_STATE], X11_XA_ATOM, 32, 0, (unsigned char *)&w->a[X11_A_ABOVE], 1);
    w->gc = X11.CreateGC(w->dpy, w->win, 0, NULL);
    X11.SetGraphicsExposures(w->dpy, w->gc, 0);
    x11_win_mons(g, w);
    if (!(fl & GOOME_WINDOW_HIDDEN)) X11.MapRaised(w->dpy, w->win), w->mapped = 1;
    X11.Sync(w->dpy, 0);
    if (x11_err) {
        int code = x11_err;
        x11_win_close(g);
        return gm_fail(g, GOOME_ENODISPLAY, "the X server said no while making the window (error %d)", code);
    }
    return GOOME_OK;
}

static void x11_win_free_buf(x11_win *w)
{
    if (w->img) w->img->data = NULL, w->img->f.destroy_image(w->img), w->img = NULL;
    if (w->pix) X11.FreePixmap(w->dpy, w->pix), w->pix = 0;
    free(w->buf), w->buf = NULL;
    w->bw = w->bh = 0;
}

static goome_canvas *x11_win_canvas(goome *g, int mon)
{
    x11_win *w = g->ov_priv;
    if (mon || w->ww <= 0 || w->wh <= 0) return NULL;
    if (!w->buf || w->bw != w->ww || w->bh != w->wh || w->cv.scale != w->scale) {
        x11_win_free_buf(w);
        w->buf = calloc((size_t)w->ww * w->wh, 4);
        if (!w->buf) return NULL;
        w->img = X11.CreateImage(w->dpy, w->visual, (unsigned)w->depth, X11_ZPIXMAP, 0, (char *)w->buf, (unsigned)w->ww, (unsigned)w->wh, 32, w->ww * 4);
        w->pix = X11.CreatePixmap(w->dpy, w->win, (unsigned)w->ww, (unsigned)w->wh, (unsigned)w->depth);
        if (!w->img) {
            x11_win_free_buf(w);
            return NULL;
        }
        w->bw = w->ww, w->bh = w->wh;
        gm_canvas_reset(&w->cv, w->buf, w->bw, w->bh, w->bw, 0, 0, w->scale, 0);
    }
    return &w->cv;
}

static void x11_win_show(x11_win *w, int x0, int y0, int x1, int y1, int put)
{
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > w->bw) x1 = w->bw;
    if (y1 > w->bh) y1 = w->bh;
    if (x1 <= x0 || y1 <= y0 || !w->pix) return;
    if (put) X11.PutImage(w->dpy, w->pix, w->gc, w->img, x0, y0, x0, y0, (unsigned)(x1 - x0), (unsigned)(y1 - y0));
    X11.CopyArea(w->dpy, w->pix, w->win, w->gc, x0, y0, (unsigned)(x1 - x0), (unsigned)(y1 - y0), x0, y0);
}

static int x11_win_commit(goome *g, int mon, goome_canvas *c, const goome_rect *hits, int n)
{
    x11_win *w = g->ov_priv;
    if (c != &w->cv) return GOOME_EARG;
    int x0, y0, x1, y1;
    if (gm_canvas_damage(c, &x0, &y0, &x1, &y1)) x11_win_show(w, x0, y0, x1, y1, 1);
    X11.Flush(w->dpy);
    return GOOME_OK;
}

static char *x11_win_take(x11_win *w, x11_xid prop, x11_xid *type, size_t *len)
{
    unsigned char *d = NULL;
    int fmt = 0;
    unsigned long n = 0, after = 0;
    *type = 0, *len = 0;
    if (X11.GetWindowProperty(w->dpy, w->win, prop, 0, 0x1fffffff, 1, 0, type, &fmt, &n, &after, &d) != 0) return NULL;
    char *r = x11_dup(d ? (const char *)d : "", fmt == 8 ? n : 0);
    if (r) *len = fmt == 8 ? n : 0;
    if (d) X11.Free(d);
    return r;
}

static void x11_win_got(x11_win *w, char *s, size_t n, x11_xid type)
{
    if (type == w->a[X11_A_STRING]) {
        char *u = malloc(n * 2 + 1);
        size_t k = 0;
        for (size_t i = 0; u && i < n; i++) {
            unsigned char c = (unsigned char)s[i];
            if (c < 0x80) u[k++] = (char)c;
            else u[k++] = (char)(0xC0 | c >> 6), u[k++] = (char)(0x80 | (c & 63));
        }
        if (u) u[k] = 0, free(s), s = u, n = k;
    }
    free(w->sel);
    w->sel = s, w->sel_len = n, w->sel_state = s ? 2 : 3;
}

static void x11_win_selnotify(goome *g, x11_win *w, x11_selnot_ev *e)
{
    x11_xid type;
    size_t n;
    if (e->selection == w->a[X11_A_CLIPBOARD] && w->sel_state == 1) {
        if (!e->property) {
            w->sel_state = 3;
            return;
        }
        char *s = x11_win_take(w, w->a[X11_A_SEL], &type, &n);
        if (type == w->a[X11_A_INCR]) {
            free(s);
            free(w->sel);
            w->sel = x11_dup("", 0), w->sel_len = 0, w->incr = 1;
        } else
            x11_win_got(w, s, n, type);
    } else if (e->selection == w->a[X11_A_DNDSEL] && w->dnd_src) {
        char *s = e->property ? x11_win_take(w, w->a[X11_A_DND], &type, &n) : NULL;
        char *paths = NULL;
        int len = s ? gm_uri_paths(s, n, &paths) : 0;
        free(s);
        if (len > 0) {
            free(w->drops);
            w->drops = paths, w->dlen = len, w->doff = 0;
        } else
            free(paths);
        x11_win_send(w, w->dnd_src, w->dnd_src, X11_A_FINISHED, (long)w->win, len > 0, len > 0 ? (long)w->a[X11_A_COPY] : 0, 0, 0);
        w->dnd_src = 0;
    }
}

static void x11_win_selreq(goome *g, x11_win *w, x11_selreq_ev *r)
{
    x11_event n;
    memset(&n, 0, sizeof(n));
    n.snot.type = 31, n.snot.send_event = 1, n.snot.requestor = r->requestor, n.snot.selection = r->selection, n.snot.target = r->target,
    n.snot.time = r->time;
    x11_xid prop = r->property ? r->property : r->target, t = r->target;
    const char *own = w->own && g->clip ? g->clip : NULL;
    if (r->selection == w->a[X11_A_CLIPBOARD] && own) {
        if (t == w->a[X11_A_TARGETS]) {
            x11_xid list[5] = {w->a[X11_A_TARGETS], w->a[X11_A_UTF8], w->a[X11_A_PLAIN], w->a[X11_A_TEXT], w->a[X11_A_STRING]};
            X11.ChangeProperty(w->dpy, r->requestor, prop, X11_XA_ATOM, 32, 0, (unsigned char *)list, 5);
            n.snot.property = prop;
        } else if (t == w->a[X11_A_UTF8] || t == w->a[X11_A_PLAIN] || t == w->a[X11_A_TEXT] || t == w->a[X11_A_STRING]) {
            X11.ChangeProperty(w->dpy, r->requestor, prop, t == w->a[X11_A_STRING] ? t : w->a[X11_A_UTF8], 8, 0, (const unsigned char *)own, (int)strlen(own));
            n.snot.property = prop;
        }
    }
    X11.SendEvent(w->dpy, r->requestor, 0, 0, &n.cm);
}

static int x11_win_has_uris(x11_win *w, const long *l)
{
    if (!(l[1] & 1)) return (x11_xid)l[2] == w->a[X11_A_URILIST] || (x11_xid)l[3] == w->a[X11_A_URILIST] || (x11_xid)l[4] == w->a[X11_A_URILIST];
    x11_xid type = 0, *list = NULL;
    int fmt = 0, ok = 0;
    unsigned long n = 0, after = 0;
    x11_err = 0;
    X11.GetWindowProperty(w->dpy, (x11_xid)l[0], w->a[X11_A_TYPELIST], 0, 256, 0, X11_XA_ATOM, &type, &fmt, &n, &after, (unsigned char **)&list);
    for (unsigned long i = 0; list && !x11_err && i < n; i++)
        if (list[i] == w->a[X11_A_URILIST]) ok = 1;
    if (list) X11.Free(list);
    return ok;
}

static void x11_win_client(goome *g, x11_win *w, x11_client_ev *c)
{
    goome_event e;
    memset(&e, 0, sizeof(e));
    if (c->message_type == w->a[X11_A_PROTOCOLS]) {
        if ((x11_xid)c->l[0] == w->a[X11_A_DELETE]) e.type = GOOME_CLOSE, gm_push(g, &e);
        else if ((x11_xid)c->l[0] == w->a[X11_A_PING] && c->window != w->root) {
            x11_event r;
            memset(&r, 0, sizeof(r));
            r.cm = *c, r.cm.window = w->root;
            X11.SendEvent(w->dpy, w->root, 0, (1L << 20) | (1L << 19), &r.cm);
        }
    } else if (c->message_type == w->a[X11_A_ENTER]) {
        w->dnd_src = (x11_xid)c->l[0], w->dnd_ok = x11_win_has_uris(w, c->l);
    } else if (c->message_type == w->a[X11_A_POSITION] && (x11_xid)c->l[0] == w->dnd_src) {
        x11_xid child;
        int x = 0, y = 0;
        X11.TranslateCoordinates(w->dpy, w->root, w->win, (int)((c->l[2] >> 16) & 0xffff), (int)(c->l[2] & 0xffff), &x, &y, &child);
        w->dnd_x = x, w->dnd_y = y;
        x11_win_send(w, w->dnd_src, w->dnd_src, X11_A_STATUS, (long)w->win, w->dnd_ok ? 3 : 2, 0, 0, w->dnd_ok ? (long)w->a[X11_A_COPY] : 0);
    } else if (c->message_type == w->a[X11_A_DROP] && (x11_xid)c->l[0] == w->dnd_src) {
        if (w->dnd_ok) X11.ConvertSelection(w->dpy, w->a[X11_A_DNDSEL], w->a[X11_A_URILIST], w->a[X11_A_DND], w->win, (unsigned long)c->l[2]);
        else x11_win_send(w, w->dnd_src, w->dnd_src, X11_A_FINISHED, (long)w->win, 0, 0, 0, 0), w->dnd_src = 0;
    } else if (c->message_type == w->a[X11_A_LEAVE] && (x11_xid)c->l[0] == w->dnd_src)
        w->dnd_src = 0;
}

static void x11_win_rescale(goome *g, x11_win *w)
{
    double sc = x11_dpi_scale(w);
    if (sc == w->scale) return;
    int lw = (int)(w->ww / w->scale + 0.5), lh = (int)(w->wh / w->scale + 0.5);
    w->scale = sc;
    w->ww = x11_px(lw, sc), w->wh = x11_px(lh, sc);
    x11_win_hints(g, w, 0, 0, 0);
    X11.ResizeWindow(w->dpy, w->win, (unsigned)w->ww, (unsigned)w->wh);
    x11_win_mons(g, w);
}

static void x11_win_handle(goome *g, x11_win *w, x11_event *ev)
{
    goome_event e;
    memset(&e, 0, sizeof(e));
    switch (ev->type & 0x7f) {
    case X11_MOTION:
    case X11_ENTER:
        if (ev->in.window != w->win) break;
        e.type = GOOME_MOTION, e.x = ev->in.x / w->scale, e.y = ev->in.y / w->scale, e.mods = x11_mods(ev->in.state);
        gm_push(g, &e);
        break;
    case X11_BUTTONPRESS:
    case X11_BUTTONRELEASE: {
        int down = (ev->type & 0x7f) == X11_BUTTONPRESS, b = (int)ev->in.detail;
        if (ev->in.time) w->time = ev->in.time;
        e.x = ev->in.x / w->scale, e.y = ev->in.y / w->scale, e.mods = x11_mods(ev->in.state);
        if (b >= 1 && b <= 3) {
            e.type = down ? GOOME_DOWN : GOOME_UP, e.button = b;
            if (down) w->prx = ev->in.x_root, w->pry = ev->in.y_root, w->pbtn = b, w->held |= 1 << (b - 1);
            else w->held &= ~(1 << (b - 1));
        } else if (down && (b == 4 || b == 5))
            e.type = GOOME_SCROLL, e.dy = b == 5 ? 1 : -1;
        else
            break;
        gm_push(g, &e);
        break;
    }
    case X11_KEYPRESS:
        if (ev->in.time) w->time = ev->in.time;
        x11_key(g, &ev->in, w->grabbed, w->ngrabbed, 1);
        break;
    case X11_EXPOSE:
        if (ev->ex.window == w->win) x11_win_show(w, ev->ex.x, ev->ex.y, ev->ex.x + ev->ex.width, ev->ex.y + ev->ex.height, 0);
        break;
    case 22:
        if (ev->conf.window == w->win && ev->conf.width > 0 && ev->conf.height > 0 && (ev->conf.width != w->ww || ev->conf.height != w->wh)) {
            w->ww = ev->conf.width, w->wh = ev->conf.height;
            x11_win_mons(g, w);
        }
        break;
    case 18:
    case 19:
        if (ev->conf.window == w->win) w->mapped = (ev->type & 0x7f) == 19;
        break;
    case 9:
    case 10: {
        int in = (ev->type & 0x7f) == 9;
        if (ev->focus.window != w->win || ev->focus.detail == 5 || ev->focus.detail == 7 || ev->focus.mode == 1 || ev->focus.mode == 2 || in == w->focused) break;
        w->focused = in;
        e.type = GOOME_FOCUS, e.id = in;
        gm_push(g, &e);
        break;
    }
    case 28:
        if (ev->prop.time) w->time = ev->prop.time;
        if (ev->prop.window == w->root && ev->prop.atom == w->a[X11_A_RM]) x11_win_rescale(g, w);
        else if (ev->prop.window == w->win && ev->prop.atom == w->a[X11_A_SEL] && ev->prop.state == 0 && w->incr) {
            x11_xid type;
            size_t n;
            char *s = x11_win_take(w, w->a[X11_A_SEL], &type, &n);
            if (!s || !n) {
                free(s);
                w->incr = 0, w->sel_state = 2;
                break;
            }
            char *m = realloc(w->sel, w->sel_len + n + 1);
            if (m) memcpy(m + w->sel_len, s, n), w->sel = m, w->sel_len += n, m[w->sel_len] = 0;
            free(s);
            if (!m) w->incr = 0, w->sel_state = 3;
        }
        break;
    case 29:
        if (ev->snot.requestor == w->win && ev->snot.selection == w->a[X11_A_CLIPBOARD]) w->own = 0;
        break;
    case 30: x11_win_selreq(g, w, &ev->sreq); break;
    case 31: x11_win_selnotify(g, w, &ev->snot); break;
    case 33: x11_win_client(g, w, &ev->cm); break;
    }
}

static int x11_win_pump(goome *g, x11_win *w)
{
    struct pollfd p = {X11.ConnectionNumber(w->dpy), POLLIN, 0};
    if (poll(&p, 1, 0) > 0 && (p.revents & (POLLHUP | POLLERR | POLLNVAL))) return -1;
    while (X11.Pending(w->dpy)) {
        x11_event ev;
        X11.NextEvent(w->dpy, &ev);
        if (x11i_filter(&ev)) continue;
        x11_win_handle(g, w, &ev);
    }
    return 0;
}

static int x11_win_wait(goome *g, int ms)
{
    x11_win *w = g->ov_priv;
    long end = goome_now_ms() + (ms > 0 ? ms : 0);
    int dropped = 0;
    for (;;) {
        if (x11_win_pump(g, w) < 0) return -1;
        if (!dropped && w->doff < w->dlen)
            gm_drop_next(g, w->drops, &w->doff, w->dlen, w->dnd_x / w->scale, w->dnd_y / w->scale), dropped = 1;
        if (g->qlen) return 0;
        long left = end - goome_now_ms();
        if (left <= 0) return 0;
        struct pollfd p = {X11.ConnectionNumber(w->dpy), POLLIN, 0};
        if (poll(&p, 1, (int)left) > 0 && (p.revents & (POLLHUP | POLLERR | POLLNVAL))) return -1;
    }
}

static int x11_win_pointer(goome *g, double *x, double *y, int *buttons)
{
    x11_win *w = g->ov_priv;
    x11_xid r, child;
    int rx, ry, wx, wy;
    unsigned mask = 0;
    if (!X11.QueryPointer(w->dpy, w->win, &r, &child, &rx, &ry, &wx, &wy, &mask)) return 0;
    *x = wx / w->scale, *y = wy / w->scale;
    *buttons = (mask & X11_BUTTON1 ? 1 : 0) | (mask & X11_BUTTON2 ? 2 : 0) | (mask & X11_BUTTON3 ? 4 : 0);
    return 1;
}

static int x11_win_keyboard(goome *g, int on) { return GOOME_OK; }

static int x11_win_grab(goome *g, int on)
{
    x11_win *w = g->ov_priv;
    int r = 0;
    if (on) r = X11.GrabPointer(w->dpy, w->win, 0, 4 | 8 | 64, X11_GRAB_ASYNC, X11_GRAB_ASYNC, 0, 0, 0);
    else X11.UngrabPointer(w->dpy, 0);
    X11.Flush(w->dpy);
    return r ? gm_fail(g, GOOME_EDENIED, "couldnt grab the pointer (X said %d)", r) : GOOME_OK;
}

static int x11_win_hotkeys(goome *g)
{
    x11_win *w = g->ov_priv;
    return x11_grab_keys(g, w->dpy, w->root, w->grabbed, &w->ngrabbed);
}

static int x11_win_rect(goome *g, goome_rect *r)
{
    x11_win *w = g->ov_priv;
    x11_xid child;
    int x = 0, y = 0;
    if (!X11.TranslateCoordinates(w->dpy, w->win, w->root, 0, 0, &x, &y, &child)) return 0;
    int x1 = x11_px(r->x + r->w, w->scale), y1 = x11_px(r->y + r->h, w->scale);
    r->x = x11_px(r->x, w->scale), r->y = x11_px(r->y, w->scale);
    r->w = x1 - r->x, r->h = y1 - r->y, r->x += x, r->y += y;
    return 1;
}

static int x11_win_get_state(x11_win *w)
{
    x11_xid type = 0, *st = NULL;
    int fmt = 0, max = 0, full = 0, hid = 0;
    unsigned long n = 0, after = 0;
    if (X11.GetWindowProperty(w->dpy, w->win, w->a[X11_A_STATE], 0, 64, 0, X11_XA_ATOM, &type, &fmt, &n, &after, (unsigned char **)&st) == 0 && st)
        for (unsigned long i = 0; i < n; i++) {
            if (st[i] == w->a[X11_A_MAXV] || st[i] == w->a[X11_A_MAXH]) max++;
            if (st[i] == w->a[X11_A_FULL]) full = 1;
            if (st[i] == w->a[X11_A_HIDDEN]) hid = 1;
        }
    if (st) X11.Free(st);
    long *ws = NULL;
    n = 0;
    if (X11.GetWindowProperty(w->dpy, w->win, w->a[X11_A_WM_STATE], 0, 2, 0, w->a[X11_A_WM_STATE], &type, &fmt, &n, &after, (unsigned char **)&ws) == 0 && ws &&
        n && ws[0] == 3)
        hid = 1;
    if (ws) X11.Free(ws);
    return hid ? GOOME_STATE_MINIMIZED : full ? GOOME_STATE_FULLSCREEN : max == 2 ? GOOME_STATE_MAXIMIZED : GOOME_STATE_NORMAL;
}

static int x11_win_op(goome *g, int op, int a, int b, int *ra, int *rb)
{
    x11_win *w = g->ov_priv;
    x11_xid child;
    int x = 0, y = 0;
    switch (op) {
    case GM_WIN_TITLE:
        x11_win_title(g, w);
        x11_win_mons(g, w);
        break;
    case GM_WIN_SIZE:
        w->ww = x11_px(a, w->scale), w->wh = x11_px(b, w->scale);
        x11_win_hints(g, w, 0, 0, 0);
        X11.ResizeWindow(w->dpy, w->win, (unsigned)w->ww, (unsigned)w->wh);
        x11_win_mons(g, w);
        break;
    case GM_WIN_POS_GET:
        if (!X11.TranslateCoordinates(w->dpy, w->win, w->root, 0, 0, &x, &y, &child)) return gm_fail(g, GOOME_EGONE, "the window is gone");
        *ra = x, *rb = y;
        return GOOME_OK;
    case GM_WIN_POS_SET: X11.MoveWindow(w->dpy, w->win, a, b); break;
    case GM_WIN_GET_STATE: *ra = x11_win_get_state(w); return GOOME_OK;
    case GM_WIN_STATE:
        if (!w->wm) return gm_fail(g, GOOME_ENOTSUP, "no window manager running, nobody to ask");
        if (a == GOOME_STATE_MINIMIZED) {
            x11_win_send(w, w->root, w->win, X11_A_CHANGE_STATE, 3, 0, 0, 0, 0);
            break;
        }
        if (!w->mapped) X11.MapRaised(w->dpy, w->win), w->mapped = 1;
        x11_win_send(w, w->root, w->win, X11_A_STATE, a == GOOME_STATE_FULLSCREEN, (long)w->a[X11_A_FULL], 0, 1, 0);
        x11_win_send(w, w->root, w->win, X11_A_STATE, a == GOOME_STATE_MAXIMIZED, (long)w->a[X11_A_MAXV], (long)w->a[X11_A_MAXH], 1, 0);
        break;
    case GM_WIN_DRAG: {
        static const int dir[16] = {8, 7, 3, 8, 1, 0, 2, 1, 5, 6, 4, 5, 8, 7, 3, 8};
        if (!w->wm) return gm_fail(g, GOOME_ENOTSUP, "no window manager running, nobody to move the window");
        if (!w->held) return gm_fail(g, GOOME_EARG, "no mouse button is down");
        X11.UngrabPointer(w->dpy, w->time);
        x11_win_send(w, w->root, w->win, X11_A_MOVERESIZE, w->prx, w->pry, dir[a & 15], w->pbtn, 1);
        goome_event e;
        memset(&e, 0, sizeof(e));
        e.type = GOOME_UP, e.button = w->pbtn;
        if (x11_win_pointer(g, &e.x, &e.y, &x)) gm_push(g, &e);
        w->held = 0;
        break;
    }
    case GM_WIN_SHOW:
        if (a) X11.MapRaised(w->dpy, w->win), w->mapped = 1;
        else X11.UnmapWindow(w->dpy, w->win), w->mapped = 0;
        break;
    default: return GOOME_ENOTSUP;
    }
    X11.Flush(w->dpy);
    return GOOME_OK;
}

static int x11_win_cursor(goome *g, int shape)
{
    static const unsigned font[GOOME_CURSOR_HIDDEN] = {68, 152, 60, 52, 108, 116, 14, 12, 150, 0};
    x11_win *w = g->ov_priv;
    if (!w->cur[shape]) {
        if (shape == GOOME_CURSOR_HIDDEN) {
            x11_color black;
            memset(&black, 0, sizeof(black));
            x11_xid pm = X11.CreateBitmapFromData(w->dpy, w->win, "\0", 1, 1);
            w->cur[shape] = X11.CreatePixmapCursor(w->dpy, pm, pm, &black, &black, 0, 0);
            X11.FreePixmap(w->dpy, pm);
        } else
            w->cur[shape] = X11.CreateFontCursor(w->dpy, font[shape]);
    }
    X11.DefineCursor(w->dpy, w->win, w->cur[shape]);
    X11.Flush(w->dpy);
    return GOOME_OK;
}

static int x11_win_clip_set(goome *g, const char *s)
{
    x11_win *w = g->ov_priv;
    int e = gm_clip_keep(g, s);
    if (e != GOOME_OK) return e;
    X11.SetSelectionOwner(w->dpy, w->a[X11_A_CLIPBOARD], w->win, w->time);
    w->own = X11.GetSelectionOwner(w->dpy, w->a[X11_A_CLIPBOARD]) == w->win;
    X11.Flush(w->dpy);
    return w->own ? GOOME_OK : gm_fail(g, GOOME_EDENIED, "the X server wont give us the clipboard");
}

static int x11_win_clip_get(goome *g, char **out)
{
    x11_win *w = g->ov_priv;
    *out = NULL;
    if (w->own) return gm_clip_copy(g, out);
    if (!X11.GetSelectionOwner(w->dpy, w->a[X11_A_CLIPBOARD])) return GOOME_OK;
    long end = goome_now_ms() + 250;
    for (int k = 0; k < 2; k++) {
        free(w->sel), w->sel = NULL, w->sel_len = 0, w->sel_state = 1, w->incr = 0;
        X11.ConvertSelection(w->dpy, w->a[X11_A_CLIPBOARD], w->a[k ? X11_A_STRING : X11_A_UTF8], w->a[X11_A_SEL], w->win, w->time);
        X11.Flush(w->dpy);
        while (w->sel_state == 1 || w->incr) {
            long left = end - goome_now_ms();
            if (left <= 0) break;
            struct pollfd p = {X11.ConnectionNumber(w->dpy), POLLIN, 0};
            poll(&p, 1, (int)left);
            if (x11_win_pump(g, w) < 0) return gm_fail(g, GOOME_ENODISPLAY, "lost the X server");
        }
        if (w->sel_state == 2 && !w->incr) {
            *out = w->sel, w->sel = NULL, w->sel_state = 0;
            return GOOME_OK;
        }
        if (w->sel_state != 3) break;
    }
    w->sel_state = 0, w->incr = 0;
    return gm_fail(g, GOOME_EAGAIN, "the app that owns the clipboard didnt answer");
}

static void x11_win_close(goome *g)
{
    x11_win *w = g->ov_priv;
    if (!w) return;
    x11i_drop(g);
    if (w->dpy) {
        x11_ungrab_keys(w->dpy, w->root, w->grabbed, w->ngrabbed);
        x11_win_free_buf(w);
        for (int i = 0; i <= GOOME_CURSOR_HIDDEN; i++)
            if (w->cur[i]) X11.FreeCursor(w->dpy, w->cur[i]);
        if (w->gc) X11.FreeGC(w->dpy, w->gc);
        if (w->win) X11.DestroyWindow(w->dpy, w->win);
        if (w->cmap) X11.FreeColormap(w->dpy, w->cmap);
        X11.Sync(w->dpy, 0);
        x11_err_off();
        X11.CloseDisplay(w->dpy);
    }
    free(w->sel), free(w->drops);
    free(w);
    g->ov_priv = NULL;
}

static void x11_ov_close(goome *g)
{
    if (g->ov_priv) {
        x11_win_close(g);
        return;
    }
    x11i_drop(g);
    if (x11.dpy) {
        x11_ungrab_keys(x11.dpy, x11.root, x11.grabbed, x11.ngrabbed);
        if (x11.kb) X11.UngrabKeyboard(x11.dpy, 0);
        if (x11.grab) X11.UngrabPointer(x11.dpy, 0);
        x11_free_img();
        if (x11.pix) X11.FreePixmap(x11.dpy, x11.pix);
        if (x11.gc) X11.FreeGC(x11.dpy, x11.gc);
        if (x11.win) X11.DestroyWindow(x11.dpy, x11.win);
        if (x11.kwin) X11.DestroyWindow(x11.dpy, x11.kwin);
        if (x11.cmap) X11.FreeColormap(x11.dpy, x11.cmap);
        X11.Sync(x11.dpy, 0);
        x11_err_off();
        X11.CloseDisplay(x11.dpy);
    }
    free(x11.buf), free(x11.rects);
    for (int i = 0; i < GM_MAX_MON; i++) free(x11.hits[i]);
    memset(&x11, 0, sizeof(x11));
}

static const gm_ov x11_ov = {"x11", x11_ov_open, x11_ov_canvas, x11_ov_commit, x11_ov_wait, x11_ov_pointer, x11_ov_keyboard, x11_ov_hotkeys,
                             x11_ov_close, GOOME_CAP_GLOBAL_POINTER | GOOME_CAP_KEYBOARD | GOOME_CAP_HOTKEYS | GOOME_CAP_GRAB, NULL, NULL, NULL, NULL, x11_ov_grab};
const gm_ov *gm_ov_x11(void) { return &x11_ov; }

typedef struct {
    short x, y;
} x11i_point;

typedef struct {
    void *data;
    void (*fn)(void);
} x11i_cb;

typedef struct {
    unsigned short length;
    unsigned long *feedback;
    int wide;
    union {
        char *mb;
        int32_t *wc;
    } s;
} x11i_text;

typedef struct {
    int caret, first, len;
    x11i_text *text;
} x11i_draw;

typedef struct {
    int position, direction, style;
} x11i_caret;

typedef struct {
    unsigned short n;
    unsigned long *styles;
} x11i_styles;

static struct {
    int tried, ok;
    char *(*SetLocaleModifiers)(const char *);
    int (*SupportsLocale)(void);
    void *(*OpenIM)(x11_dpy *, void *, char *, char *);
    int (*CloseIM)(void *);
    char *(*GetIMValues)(void *, ...);
    void *(*CreateIC)(void *, ...);
    void (*DestroyIC)(void *);
    void (*SetICFocus)(void *);
    void (*UnsetICFocus)(void *);
    char *(*SetICValues)(void *, ...);
    char *(*GetICValues)(void *, ...);
    void *(*VaCreateNestedList)(int, ...);
    int (*utf8LookupString)(void *, x11_input_ev *, char *, int, unsigned long *, int *);
    int (*FilterEvent)(x11_event *, x11_xid);
    char *(*utf8ResetIC)(void *);
} XI;

static struct {
    goome *g;
    x11_dpy *dpy;
    x11_xid win;
    void *im, *ic;
    unsigned long style;
    int focused, failed, npre, pcur, sent;
    uint32_t pre[256];
    x11i_cb cbs[4];
} x11i;

enum { X11I_PRE_CALLBACKS = 2, X11I_PRE_POSITION = 4, X11I_PRE_NOTHING = 8, X11I_ST_NOTHING = 0x400, X11I_ST_NONE = 0x800 };

static int x11i_load(void)
{
    if (XI.tried) return XI.ok;
    XI.tried = 1;
    void *xl = dlopen("libX11.so.6", RTLD_NOW | RTLD_GLOBAL);
    if (!xl) return 0;
    int ok = 1;
#define X11I_L(f) ok &= (*(void **)&XI.f = dlsym(xl, "X" #f)) != NULL
    X11I_L(SetLocaleModifiers);
    X11I_L(SupportsLocale);
    X11I_L(OpenIM);
    X11I_L(CloseIM);
    X11I_L(GetIMValues);
    X11I_L(CreateIC);
    X11I_L(DestroyIC);
    X11I_L(SetICFocus);
    X11I_L(UnsetICFocus);
    X11I_L(SetICValues);
    X11I_L(GetICValues);
    X11I_L(VaCreateNestedList);
    X11I_L(utf8LookupString);
    X11I_L(FilterEvent);
    X11I_L(utf8ResetIC);
#undef X11I_L
    return XI.ok = ok;
}

static void x11i_send_pre(void)
{
    char out[256 * 4 + 1];
    int n = 0, cur = 0;
    for (int i = 0; i < x11i.npre; i++) {
        if (i == x11i.pcur) cur = n;
        n += x11_utf8(out + n, x11i.pre[i]);
    }
    if (x11i.pcur >= x11i.npre) cur = n;
    out[n] = 0;
    if (x11i.g && (n || x11i.sent)) gm_ime_push(x11i.g, GOOME_PREEDIT, out, n, cur);
    x11i.sent = n > 0;
}

static int x11i_pre_start(void *ic, void *data, void *call)
{
    x11i.npre = x11i.pcur = 0;
    return -1;
}

static void x11i_pre_done(void *ic, void *data, void *call)
{
    x11i.npre = x11i.pcur = 0;
    x11i_send_pre();
}

static int x11i_u8(const unsigned char *s, int *i, int n)
{
    int c = s[(*i)++], more = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
    c &= more == 3 ? 7 : more == 2 ? 15 : more == 1 ? 31 : 127;
    while (more-- > 0 && *i < n && (s[*i] & 0xC0) == 0x80) c = c << 6 | (s[(*i)++] & 63);
    return c;
}

static void x11i_pre_draw(void *ic, void *data, x11i_draw *d)
{
    int first = d->first < 0 ? 0 : d->first > x11i.npre ? x11i.npre : d->first, del = d->len < 0 ? 0 : d->len;
    if (first + del > x11i.npre) del = x11i.npre - first;
    uint32_t ins[256];
    int ni = 0;
    if (d->text && d->text->length) {
        if (d->text->wide && d->text->s.wc)
            for (int i = 0; i < d->text->length && ni < 256; i++) ins[ni++] = (uint32_t)d->text->s.wc[i];
        else if (d->text->s.mb) {
            const unsigned char *m = (const unsigned char *)d->text->s.mb;
            int n = (int)strlen(d->text->s.mb), i = 0;
            while (i < n && ni < 256 && ni < d->text->length) ins[ni++] = (uint32_t)x11i_u8(m, &i, n);
        }
    }
    if (x11i.npre - del + ni > 256) ni = 256 - (x11i.npre - del);
    memmove(x11i.pre + first + ni, x11i.pre + first + del, sizeof(uint32_t) * (size_t)(x11i.npre - first - del));
    memcpy(x11i.pre + first, ins, sizeof(uint32_t) * (size_t)ni);
    x11i.npre += ni - del;
    x11i.pcur = d->caret < 0 ? 0 : d->caret > x11i.npre ? x11i.npre : d->caret;
    x11i_send_pre();
}

static void x11i_pre_caret(void *ic, void *data, x11i_caret *c)
{
    int p = x11i.pcur;
    if (c->direction == 0) p++;
    else if (c->direction == 1) p--;
    else if (c->direction == 8) p = 0;
    else if (c->direction == 9) p = x11i.npre;
    else if (c->direction == 10) p = c->position;
    x11i.pcur = p < 0 ? 0 : p > x11i.npre ? x11i.npre : p;
    c->position = x11i.pcur;
    x11i_send_pre();
}

static void x11i_drop(goome *g)
{
    if (x11i.g != g) return;
    if (x11i.ic) XI.DestroyIC(x11i.ic);
    if (x11i.im) XI.CloseIM(x11i.im);
    memset(&x11i, 0, sizeof(x11i));
}

static int x11i_filter(x11_event *ev)
{
    if (!x11i.ic) return 0;
    return XI.FilterEvent(ev, 0) != 0;
}

static void *x11i_open_im(x11_dpy *d)
{
    char keep[128];
    const char *was = setlocale(LC_CTYPE, NULL);
    snprintf(keep, sizeof(keep), "%s", was ? was : "C");
    if (!strcmp(keep, "C") || !strcmp(keep, "POSIX")) {
        const char *now = setlocale(LC_CTYPE, "");
        if (!now || !strcmp(now, "C") || !strcmp(now, "POSIX")) setlocale(LC_CTYPE, "C.UTF-8");
    }
    void *im = NULL;
    if (XI.SupportsLocale()) {
        if (XI.SetLocaleModifiers("")) im = XI.OpenIM(d, NULL, NULL, NULL);
        if (!im && XI.SetLocaleModifiers("@im=none")) im = XI.OpenIM(d, NULL, NULL, NULL);
    }
    setlocale(LC_CTYPE, keep);
    return im;
}

static unsigned long x11i_pick(void *im)
{
    x11i_styles *st = NULL;
    unsigned long best = 0;
    int rank = 0;
    if (XI.GetIMValues(im, "queryInputStyle", &st, NULL) || !st) return 0;
    for (int i = 0; i < st->n; i++) {
        unsigned long v = st->styles[i];
        if (!(v & (X11I_ST_NOTHING | X11I_ST_NONE))) continue;
        int r = (v & X11I_PRE_CALLBACKS) ? 4 : (v & X11I_PRE_POSITION) ? 3 : (v & X11I_PRE_NOTHING) ? 2 : (v & 0x10) ? 1 : 0;
        if (r > rank) rank = r, best = v;
    }
    X11.Free(st);
    return best;
}

static int x11i_make(goome *g, x11_dpy *d, x11_xid win, long mask)
{
    if (!x11i_load()) return gm_fail(g, GOOME_ENOTSUP, "libX11 has no XIM functions");
    if (x11i.g && x11i.g != g) x11i_drop(x11i.g);
    if (x11i.ic) return GOOME_OK;
    if (x11i.failed) return gm_fail(g, GOOME_ENOTSUP, "no X input method here");
    x11i.g = g, x11i.dpy = d, x11i.win = win;
    x11i.im = x11i_open_im(d);
    x11i.style = x11i.im ? x11i_pick(x11i.im) : 0;
    if (!x11i.style) {
        if (x11i.im) XI.CloseIM(x11i.im);
        x11i.im = NULL, x11i.failed = 1;
        return gm_fail(g, GOOME_ENOTSUP, "no X input method here");
    }
    x11i_point spot = {0, 0};
    void *attr = NULL;
    if (x11i.style & X11I_PRE_CALLBACKS) {
        x11i.cbs[0].fn = (void (*)(void))x11i_pre_start;
        x11i.cbs[1].fn = (void (*)(void))x11i_pre_done;
        x11i.cbs[2].fn = (void (*)(void))x11i_pre_draw;
        x11i.cbs[3].fn = (void (*)(void))x11i_pre_caret;
        attr = XI.VaCreateNestedList(0, "spotLocation", &spot, "preeditStartCallback", &x11i.cbs[0], "preeditDoneCallback", &x11i.cbs[1],
                                     "preeditDrawCallback", &x11i.cbs[2], "preeditCaretCallback", &x11i.cbs[3], NULL);
    } else if (x11i.style & X11I_PRE_POSITION)
        attr = XI.VaCreateNestedList(0, "spotLocation", &spot, NULL);
    x11_err = 0;
    if (attr) x11i.ic = XI.CreateIC(x11i.im, "inputStyle", x11i.style, "clientWindow", win, "focusWindow", win, "preeditAttributes", attr, NULL);
    else x11i.ic = XI.CreateIC(x11i.im, "inputStyle", x11i.style, "clientWindow", win, "focusWindow", win, NULL);
    if (attr) X11.Free(attr);
    if (!x11i.ic) {
        XI.CloseIM(x11i.im);
        x11i.im = NULL, x11i.failed = 1;
        return gm_fail(g, GOOME_ENOTSUP, "the X input method wouldnt make an input context");
    }
    unsigned long extra = 0;
    if (mask && !XI.GetICValues(x11i.ic, "filterEvents", &extra, NULL) && (extra & ~(unsigned long)mask)) X11.SelectInput(d, win, mask | (long)extra);
    return GOOME_OK;
}

static void x11i_spot(int x, int y)
{
    if (!(x11i.style & (X11I_PRE_CALLBACKS | X11I_PRE_POSITION))) return;
    x11i_point p = {(short)x, (short)y};
    void *attr = XI.VaCreateNestedList(0, "spotLocation", &p, NULL);
    if (!attr) return;
    XI.SetICValues(x11i.ic, "preeditAttributes", attr, NULL);
    X11.Free(attr);
}

static void x11i_reset(void)
{
    char *left = XI.utf8ResetIC(x11i.ic);
    if (left) X11.Free(left);
    if (x11i.npre) x11i.npre = x11i.pcur = 0, x11i_send_pre();
}

int gm_ime_x11(goome *g, int on, const goome_rect *caret)
{
    x11_dpy *d;
    x11_xid win;
    int x, y;
    long mask = 0;
    if (g->ov_priv) {
        x11_win *w = g->ov_priv;
        d = w->dpy, win = w->win;
        x = (int)(caret->x * w->scale + 0.5), y = (int)((caret->y + caret->h) * w->scale + 0.5);
        mask = 1L | 4L | 8L | 16L | 64L | (1L << 15) | (1L << 17) | (1L << 21) | (1L << 22);
    } else {
        if (!x11.dpy) return gm_fail(g, GOOME_EGONE, "the X display is gone");
        d = x11.dpy, win = x11.kwin ? x11.kwin : x11.root;
        x11_xid child;
        x = caret->x, y = caret->y + caret->h;
        if (win != x11.root) X11.TranslateCoordinates(d, x11.root, win, caret->x, caret->y + caret->h, &x, &y, &child);
        if (x11.kwin) mask = 1L | (1L << 21);
    }
    if (!on) {
        if (x11i.g == g && x11i.ic && x11i.focused) x11i_reset(), XI.UnsetICFocus(x11i.ic), x11i.focused = 0;
        if (x11i.g == g) X11.Flush(d);
        return GOOME_OK;
    }
    int e = x11i_make(g, d, win, mask);
    if (e != GOOME_OK) return e;
    if (x11i.win != win) XI.SetICValues(x11i.ic, "focusWindow", win, NULL), x11i.win = win;
    x11i_spot(x, y);
    if (!x11i.focused) XI.SetICFocus(x11i.ic), x11i.focused = 1;
    X11.Flush(d);
    return GOOME_OK;
}

static int x11i_lookup(goome *g, x11_input_ev *k)
{
    if (!x11i.ic || !x11i.focused || x11i.g != g) return 0;
    char small[64], *buf = small;
    unsigned long sym = 0;
    int st = 0, n = XI.utf8LookupString(x11i.ic, k, small, (int)sizeof(small) - 1, &sym, &st);
    if (st == -1 && n > 0) {
        buf = malloc((size_t)n + 1);
        if (!buf) return 1;
        n = XI.utf8LookupString(x11i.ic, k, buf, n, &sym, &st);
    }
    int done = 0;
    if ((st == 2 || st == 4) && n > 0) {
        buf[n] = 0;
        char plain[16] = {0};
        unsigned long psym = 0;
        x11_input_ev copy = *k;
        if (k->detail) X11.LookupString(&copy, plain, (int)sizeof(plain) - 1, &psym, NULL);
        char want[8] = {0};
        unsigned cp = psym >= 0x20 && psym <= 0xff ? (unsigned)psym : (psym & 0xff000000ul) == 0x01000000ul ? (unsigned)(psym & 0xffffff) : 0;
        if (cp) want[x11_utf8(want, cp)] = 0;
        if (!k->detail || strcmp(want, buf)) {
            if ((unsigned char)buf[0] >= 32 && buf[0] != 127) gm_ime_push(g, GOOME_TEXT, buf, n, 0);
            done = 1;
        }
    }
    if (buf != small) free(buf);
    return done;
}

static const gm_ov x11_wov = {"x11", x11_win_open, x11_win_canvas, x11_win_commit, x11_win_wait, x11_win_pointer, x11_win_keyboard, x11_win_hotkeys,
                              x11_win_close, GOOME_CAP_GLOBAL_POINTER | GOOME_CAP_KEYBOARD | GOOME_CAP_HOTKEYS | GOOME_CAP_WINDOW | GOOME_CAP_CLIPBOARD | GOOME_CAP_CURSOR |
                                  GOOME_CAP_GRAB,
                              x11_win_op, x11_win_cursor, x11_win_clip_set, x11_win_clip_get, x11_win_grab};

typedef struct {
    x11_dpy *dpy;
    int own;
    x11_xid root;
    x11_visual *visual;
    int depth;
    x11_shminfo shm;
    int shm_on, shm_w, shm_h;
    uint32_t *out[GM_MAX_MON];
    size_t outn[GM_MAX_MON];
} x11_capst;

static x11_capst x11c;

static x11_capst *x11_cs(goome *g) { return g && g->cap_priv ? g->cap_priv : &x11c; }

static void x11_shm_drop(x11_capst *c)
{
    if (!c->shm_on) return;
    X11.ShmDetach(c->dpy, &c->shm);
    X11.Sync(c->dpy, 0);
    shmdt(c->shm.shmaddr);
    memset(&c->shm, 0, sizeof(c->shm));
    c->shm_on = 0;
}

static int x11_shm_make(x11_capst *c, int w, int h)
{
    if (c->shm_on && c->shm_w == w && c->shm_h == h) return 1;
    x11_shm_drop(c);
    if (!x11_have_shm || !X11.ShmQueryExtension(c->dpy)) return 0;
    int id = shmget(IPC_PRIVATE, (size_t)w * h * 4, IPC_CREAT | 0600);
    if (id < 0) return 0;
    void *addr = shmat(id, NULL, 0);
    if (addr == (void *)-1) {
        shmctl(id, IPC_RMID, NULL);
        return 0;
    }
    c->shm.shmid = id, c->shm.shmaddr = addr, c->shm.read_only = 0;
    x11_err = 0;
    int ok = X11.ShmAttach(c->dpy, &c->shm);
    X11.Sync(c->dpy, 0);
    shmctl(id, IPC_RMID, NULL);
    if (!ok || x11_err) {
        shmdt(addr);
        memset(&c->shm, 0, sizeof(c->shm));
        x11_have_shm = 0;
        return 0;
    }
    c->shm_on = 1, c->shm_w = w, c->shm_h = h;
    return 1;
}

static x11_dpy *x11_win_dpy(goome *g);

static int x11_cap_open(goome *g)
{
    x11_capst *c = &x11c;
    if (g->ov_priv) {
        if (!(c = calloc(1, sizeof(*c)))) return gm_fail(g, GOOME_ENOMEM, "out of memory");
        g->cap_priv = c;
        c->dpy = x11_win_dpy(g);
    } else
        memset(c, 0, sizeof(*c));
    if (!c->dpy) c->dpy = x11.dpy;
    if (!c->dpy) {
        if (!gm_env("DISPLAY")) return gm_fail(g, GOOME_ENODISPLAY, "DISPLAY isnt set, no X server to grab");
        int e = x11_load(g);
        if (e != GOOME_OK) return e;
        c->dpy = X11.OpenDisplay(NULL);
        if (!c->dpy) return gm_fail(g, GOOME_ENODISPLAY, "cant connect to the X server on %s", gm_env("DISPLAY"));
        c->own = 1;
        x11_err_on();
    }
    int scr = X11.DefaultScreen(c->dpy);
    c->root = X11.RootWindow(c->dpy, scr);
    c->visual = X11.DefaultVisual(c->dpy, scr);
    c->depth = X11.DefaultDepth(c->dpy, scr);
    return GOOME_OK;
}

static void x11_cap_close(goome *g)
{
    x11_capst *c = x11_cs(g);
    if (c->dpy) x11_shm_drop(c);
    if (c->own && c->dpy) X11.CloseDisplay(c->dpy), x11_err_off();
    for (int i = 0; i < GM_MAX_MON; i++) free(c->out[i]);
    memset(c, 0, sizeof(*c));
    if (c != &x11c) free(c), g->cap_priv = NULL;
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

static int x11_win_rect(goome *g, goome_rect *r);

static int x11_cap_grab(goome *g, int mon, goome_rect r, goome_image *img)
{
    x11_capst *c = x11_cs(g);
    if (!c->dpy) return GOOME_EGONE;
    goome_rect want = r;
    if (g->ov_priv && !x11_win_rect(g, &r)) return gm_fail(g, GOOME_EGONE, "the window is gone");
    int rw = x11.w, rh = x11.h;
    if (c->own || !rw || g->ov_priv) x11_root_size(c->dpy, c->root, &rw, &rh);
    if (r.x < 0) r.w += r.x, r.x = 0;
    if (r.y < 0) r.h += r.y, r.y = 0;
    if (r.x + r.w > rw) r.w = rw - r.x;
    if (r.y + r.h > rh) r.h = rh - r.y;
    if (r.w <= 0 || r.h <= 0) return gm_fail(g, GOOME_EARG, "that rect is off the screen");
    size_t need = (size_t)r.w * r.h;
    if (need > c->outn[mon]) {
        uint32_t *p = realloc(c->out[mon], need * 4);
        if (!p) return gm_fail(g, GOOME_ENOMEM, "no memory for a %dx%d grab", r.w, r.h);
        c->out[mon] = p, c->outn[mon] = need;
    }
    x11_image *im = NULL;
    int shm = 0;
    if (x11_shm_make(c, rw, rh)) {
        im = X11.ShmCreateImage(c->dpy, c->visual, (unsigned)c->depth, X11_ZPIXMAP, c->shm.shmaddr, &c->shm, (unsigned)r.w, (unsigned)r.h);
        x11_err = 0;
        if (im && (X11.ShmGetImage(c->dpy, c->root, im, r.x, r.y, ~0ul) == 0 || x11_err)) {
            im->f.destroy_image(im);
            im = NULL;
        }
        shm = im != NULL;
    }
    if (!im) {
        x11_err = 0;
        im = X11.GetImage(c->dpy, c->root, r.x, r.y, (unsigned)r.w, (unsigned)r.h, ~0ul, X11_ZPIXMAP);
        if (!im || x11_err) {
            if (im) im->f.destroy_image(im);
            return gm_fail(g, GOOME_EGONE, "XGetImage failed, the screen changed under us?");
        }
    }
    int e = x11_convert(g, im, c->out[mon], r.w, r.h);
    if (shm) im->data = NULL;
    im->f.destroy_image(im);
    if (e != GOOME_OK) return e;
    img->px = c->out[mon], img->w = r.w, img->h = r.h, img->stride = r.w, img->area = g->ov_priv ? want : r;
    img->has_self = 1;
    return GOOME_OK;
}

static const gm_cap x11_cap = {"x11", NULL, x11_cap_open, x11_cap_grab, x11_cap_close, GOOME_CAP_CAPTURE};
const gm_cap *gm_cap_x11(void) { return &x11_cap; }

#endif
