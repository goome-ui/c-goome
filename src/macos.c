#include "internal.h"

#if defined(__APPLE__)

// plain c on purpose, no .m file and no -framework flags. call goome from main(), appkit wont work off it
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifndef RTLD_DEFAULT
#define RTLD_DEFAULT ((void *)-2)
#endif

typedef void *mac_id;
typedef void *mac_sel;
#if defined(__aarch64__) || defined(__arm64__)
typedef _Bool mac_bool;
#define MAC_B "B"
#else
typedef signed char mac_bool;
#define MAC_B "c"
#endif
typedef struct {
    double x, y;
} mac_pt;
typedef struct {
    double w, h;
} mac_sz;
typedef struct {
    mac_pt o;
    mac_sz s;
} mac_rect; // same layout as NSRect / CGRect, CGFloat is double on 64 bit
#define MAC_RECT_ENC "{CGRect={CGPoint=dd}{CGSize=dd}}"

typedef struct {
    unsigned long reserved, size;
    const char *sig, *layout;
} mac_blk_desc;

typedef struct {
    void *isa;
    int flags, reserved;
    void (*invoke)(void);
    const mac_blk_desc *desc;
    void *ctx;
} mac_blk;

#define MAC_BLK_GLOBAL (1 << 28)
#define MAC_BLK_SIG (1 << 30)

typedef struct {
    void *p;
    long code;
    int state, kind; // state 1 callback done, 2 we stopped waiting. kind 0 cgimage, 1 objc object
} mac_slot;

typedef struct {
    uint32_t id;
    mac_rect r; // points, global, y down from the top of the main display
    double bs;
    int pw, ph, rot;
    char name[64];
    mac_id win, view;
    int shown;
    uint32_t *px, *cap;
    size_t capn;
    goome_canvas cv;
    goome_rect hits[256];
    int nhits;
} mac_mon;

static struct {
    int loaded;
    void *objc, *appkit, *cg, *sck, *found;
    mac_id (*cls)(const char *);
    mac_sel (*sel)(const char *);
    void (*msg)(void);
    void (*msg_stret)(void);
    mac_id (*alloc_pair)(mac_id, const char *, size_t);
    void (*reg_pair)(mac_id);
    mac_bool (*add_method)(mac_id, mac_sel, void (*)(void), const char *);
    void *(*pool_push)(void);
    void (*pool_pop)(void *);
    int (*main_np)(void);
    void *stack_isa, *global_isa;
    mac_id run_mode;
    int32_t (*CGGetActiveDisplayList)(uint32_t, uint32_t *, uint32_t *);
    mac_rect (*CGDisplayBounds)(uint32_t);
    uint32_t (*CGMainDisplayID)(void);
    uint32_t (*CGDisplayMirrorsDisplay)(uint32_t);
    double (*CGDisplayRotation)(uint32_t);
    void *(*CGDisplayCopyDisplayMode)(uint32_t);
    size_t (*CGDisplayModeGetPixelWidth)(void *);
    size_t (*CGDisplayModeGetPixelHeight)(void *);
    void (*CGDisplayModeRelease)(void *);
    void *(*CGColorSpaceCreateWithName)(void *);
    void *(*CGColorSpaceCreateDeviceRGB)(void);
    void (*CGColorSpaceRelease)(void *);
    void *(*CGBitmapContextCreate)(void *, size_t, size_t, size_t, size_t, void *, uint32_t);
    void *(*CGBitmapContextCreateImage)(void *);
    void (*CGContextDrawImage)(void *, mac_rect, void *);
    void (*CGContextClearRect)(void *, mac_rect);
    void (*CGContextRelease)(void *);
    void *(*CGImageRetain)(void *);
    void (*CGImageRelease)(void *);
    _Bool (*CGPreflightScreenCaptureAccess)(void);
    _Bool (*CGRequestScreenCaptureAccess)(void);
    void **srgb_name;
    mac_id panel_cls, view_cls;
    int launched, asked;

    goome *g;
    void *rgb;
    mac_mon mon[GM_MAX_MON];
    int n, hot, held_hot, kb_on;
    long last_plug;
    double lx, ly, scroll_acc;
    mac_id prev_app, glob_mon, loc_mon;
    mac_id content;
    int stale;
} mac;

static const mac_blk_desc mac_desc_img = {0, sizeof(mac_blk), "v24@?0^v8@16", NULL};
static const mac_blk_desc mac_desc_obj = {0, sizeof(mac_blk), "v24@?0@8@16", NULL};
static const mac_blk_desc mac_desc_glob = {0, sizeof(mac_blk), "v16@?0@8", NULL};
static const mac_blk_desc mac_desc_loc = {0, sizeof(mac_blk), "@16@?0@8", NULL};
static mac_blk mac_glob_blk, mac_loc_blk;

// arm64 breaks if objc_msgSend gets called through a varargs cast
static mac_sel mac_s(const char *s) { return mac.sel(s); }
static mac_id mac_c(const char *s) { return mac.cls(s); }
static mac_id mac_i(mac_id o, const char *s) { return ((mac_id(*)(mac_id, mac_sel))mac.msg)(o, mac_s(s)); }
static mac_id mac_ii(mac_id o, const char *s, mac_id a) { return ((mac_id(*)(mac_id, mac_sel, mac_id))mac.msg)(o, mac_s(s), a); }
static void mac_v(mac_id o, const char *s) { ((void (*)(mac_id, mac_sel))mac.msg)(o, mac_s(s)); }
static void mac_vi(mac_id o, const char *s, mac_id a) { ((void (*)(mac_id, mac_sel, mac_id))mac.msg)(o, mac_s(s), a); }
static void mac_vb(mac_id o, const char *s, int b) { ((void (*)(mac_id, mac_sel, mac_bool))mac.msg)(o, mac_s(s), (mac_bool)(b != 0)); }
static void mac_vl(mac_id o, const char *s, long v) { ((void (*)(mac_id, mac_sel, long))mac.msg)(o, mac_s(s), v); }
static void mac_vu(mac_id o, const char *s, unsigned long v) { ((void (*)(mac_id, mac_sel, unsigned long))mac.msg)(o, mac_s(s), v); }
static void mac_vr(mac_id o, const char *s, mac_rect r) { ((void (*)(mac_id, mac_sel, mac_rect))mac.msg)(o, mac_s(s), r); }
static int mac_b(mac_id o, const char *s) { return o && ((mac_bool(*)(mac_id, mac_sel))mac.msg)(o, mac_s(s)); }
static long mac_l(mac_id o, const char *s) { return o ? ((long (*)(mac_id, mac_sel))mac.msg)(o, mac_s(s)) : 0; }
static unsigned long mac_u(mac_id o, const char *s) { return o ? ((unsigned long (*)(mac_id, mac_sel))mac.msg)(o, mac_s(s)) : 0; }
static uint32_t mac_u32(mac_id o, const char *s) { return o ? ((uint32_t(*)(mac_id, mac_sel))mac.msg)(o, mac_s(s)) : 0; }
static double mac_d(mac_id o, const char *s) { return o ? ((double (*)(mac_id, mac_sel))mac.msg)(o, mac_s(s)) : 0; }
static mac_pt mac_p(mac_id o, const char *s) { return ((mac_pt(*)(mac_id, mac_sel))mac.msg)(o, mac_s(s)); }
static mac_id mac_at(mac_id arr, unsigned long i) { return ((mac_id(*)(mac_id, mac_sel, unsigned long))mac.msg)(arr, mac_s("objectAtIndex:"), i); }
static int mac_responds(mac_id o, const char *s) { return o && ((mac_bool(*)(mac_id, mac_sel, mac_sel))mac.msg)(o, mac_s("respondsToSelector:"), mac_s(s)); }
static mac_id mac_str(const char *s) { return ((mac_id(*)(mac_id, mac_sel, const char *))mac.msg)(mac_c("NSString"), mac_s("stringWithUTF8String:"), s); }
static const char *mac_utf8(mac_id s) { return s ? ((const char *(*)(mac_id, mac_sel))mac.msg)(s, mac_s("UTF8String")) : NULL; }
static mac_id mac_app(void) { return mac_i(mac_c("NSApplication"), "sharedApplication"); }

static void mac_nap(int ms)
{
    struct timespec t = {0, (long)ms * 1000000L};
    nanosleep(&t, NULL);
}

static void *mac_sym(void *lib, const char *name, int *miss)
{
    void *p = lib ? dlsym(lib, name) : NULL;
    if (!p) *miss = 1;
    return p;
}

#define MAC_LOAD(lib, f) (memcpy(&mac.f, &(void *){mac_sym(lib, #f, &miss)}, sizeof(void *)))

static int mac_load(goome *g)
{
    if (mac.loaded) return GOOME_OK;
    int miss = 0;
    mac.objc = dlopen("/usr/lib/libobjc.A.dylib", RTLD_NOW | RTLD_GLOBAL);
    mac.found = dlopen("/System/Library/Frameworks/Foundation.framework/Foundation", RTLD_NOW | RTLD_GLOBAL);
    mac.cg = dlopen("/System/Library/Frameworks/CoreGraphics.framework/CoreGraphics", RTLD_NOW | RTLD_GLOBAL);
    mac.appkit = dlopen("/System/Library/Frameworks/AppKit.framework/AppKit", RTLD_NOW | RTLD_GLOBAL);
    if (!mac.objc || !mac.found || !mac.cg || !mac.appkit) return gm_fail(g, GOOME_ENODISPLAY, "cant load appkit / libobjc: %s", dlerror());
    void *p;
    p = mac_sym(mac.objc, "objc_getClass", &miss), memcpy(&mac.cls, &p, sizeof(p));
    p = mac_sym(mac.objc, "sel_registerName", &miss), memcpy(&mac.sel, &p, sizeof(p));
    p = mac_sym(mac.objc, "objc_msgSend", &miss), memcpy(&mac.msg, &p, sizeof(p));
#if !defined(__aarch64__) && !defined(__arm64__)
    p = mac_sym(mac.objc, "objc_msgSend_stret", &miss), memcpy(&mac.msg_stret, &p, sizeof(p));
#endif
    p = mac_sym(mac.objc, "objc_allocateClassPair", &miss), memcpy(&mac.alloc_pair, &p, sizeof(p));
    p = mac_sym(mac.objc, "objc_registerClassPair", &miss), memcpy(&mac.reg_pair, &p, sizeof(p));
    p = mac_sym(mac.objc, "class_addMethod", &miss), memcpy(&mac.add_method, &p, sizeof(p));
    p = mac_sym(mac.objc, "objc_autoreleasePoolPush", &miss), memcpy(&mac.pool_push, &p, sizeof(p));
    p = mac_sym(mac.objc, "objc_autoreleasePoolPop", &miss), memcpy(&mac.pool_pop, &p, sizeof(p));
    p = mac_sym(RTLD_DEFAULT, "pthread_main_np", &miss), memcpy(&mac.main_np, &p, sizeof(p));
    mac.stack_isa = mac_sym(RTLD_DEFAULT, "_NSConcreteStackBlock", &miss);
    mac.global_isa = mac_sym(RTLD_DEFAULT, "_NSConcreteGlobalBlock", &miss);
    mac_id *mode = mac_sym(mac.found, "NSDefaultRunLoopMode", &miss);
    MAC_LOAD(mac.cg, CGGetActiveDisplayList);
    MAC_LOAD(mac.cg, CGDisplayBounds);
    MAC_LOAD(mac.cg, CGMainDisplayID);
    MAC_LOAD(mac.cg, CGDisplayMirrorsDisplay);
    MAC_LOAD(mac.cg, CGDisplayRotation);
    MAC_LOAD(mac.cg, CGDisplayCopyDisplayMode);
    MAC_LOAD(mac.cg, CGDisplayModeGetPixelWidth);
    MAC_LOAD(mac.cg, CGDisplayModeGetPixelHeight);
    MAC_LOAD(mac.cg, CGDisplayModeRelease);
    MAC_LOAD(mac.cg, CGColorSpaceCreateWithName);
    MAC_LOAD(mac.cg, CGColorSpaceCreateDeviceRGB);
    MAC_LOAD(mac.cg, CGColorSpaceRelease);
    MAC_LOAD(mac.cg, CGBitmapContextCreate);
    MAC_LOAD(mac.cg, CGBitmapContextCreateImage);
    MAC_LOAD(mac.cg, CGContextDrawImage);
    MAC_LOAD(mac.cg, CGContextClearRect);
    MAC_LOAD(mac.cg, CGContextRelease);
    MAC_LOAD(mac.cg, CGImageRetain);
    MAC_LOAD(mac.cg, CGImageRelease);
    if (miss) return gm_fail(g, GOOME_ENODISPLAY, "libobjc or coregraphics is missing something goome needs");
    mac.run_mode = *mode;
    MAC_LOAD(mac.cg, CGPreflightScreenCaptureAccess);
    MAC_LOAD(mac.cg, CGRequestScreenCaptureAccess);
    mac.srgb_name = dlsym(mac.cg, "kCGColorSpaceSRGB");
    mac.sck = dlopen("/System/Library/Frameworks/ScreenCaptureKit.framework/ScreenCaptureKit", RTLD_NOW | RTLD_GLOBAL);
    mac.loaded = 1;
    return GOOME_OK;
}
#undef MAC_LOAD

static double mac_main_h(void) { return mac.CGDisplayBounds(mac.CGMainDisplayID()).s.h; } // cocoa y goes up from the bottom of this one

static int mac_mon_at(double x, double y)
{
    for (int i = 0; i < mac.n; i++) {
        mac_rect r = mac.mon[i].r;
        if (x >= r.o.x && y >= r.o.y && x < r.o.x + r.s.w && y < r.o.y + r.s.h) return i;
    }
    return -1;
}

static int mac_on_hit(int i, double x, double y)
{
    mac_mon *m = &mac.mon[i];
    for (int k = 0; k < m->nhits; k++)
        if (x >= m->hits[k].x && x < m->hits[k].x + m->hits[k].w && y >= m->hits[k].y && y < m->hits[k].y + m->hits[k].h) return 1;
    return 0;
}

static int mac_view_mon(mac_id v)
{
    for (int i = 0; i < mac.n; i++)
        if (mac.mon[i].view == v || mac.mon[i].win == v) return i;
    return -1;
}

static int mac_mods(mac_id e)
{
    unsigned long f = mac_u(e, "modifierFlags");
    return (f & (1ul << 17) ? GOOME_SHIFT : 0) | (f & (1ul << 18) ? GOOME_CTRL : 0) | (f & (1ul << 19) ? GOOME_ALT : 0) |
           (f & (1ul << 20) ? GOOME_SUPER : 0);
}

static int mac_char(mac_id s) // first char of an NSString as a goome key, mac backspace is 127
{
    if (!s || !mac_u(s, "length")) return 0;
    unsigned c = ((unsigned short (*)(mac_id, mac_sel, unsigned long))mac.msg)(s, mac_s("characterAtIndex:"), 0);
    static const struct {
        unsigned short ns;
        unsigned char k;
    } fk[] = {{0xF700, GOOME_K_UP}, {0xF701, GOOME_K_DOWN}, {0xF702, GOOME_K_LEFT}, {0xF703, GOOME_K_RIGHT}, {0xF727, GOOME_K_INSERT},
              {0xF728, GOOME_K_DELETE}, {0xF729, GOOME_K_HOME}, {0xF72B, GOOME_K_END}, {0xF72C, GOOME_K_PGUP}, {0xF72D, GOOME_K_PGDN}};
    if (c == 127 || c == 8) return GOOME_K_BACKSPACE;
    if (c == 3 || c == 13) return GOOME_K_ENTER;
    if (c == 9 || c == 25) return GOOME_K_TAB; // 25 = shift+tab
    if (c >= 0xF704 && c <= 0xF70F) return GOOME_K_F1 + (int)(c - 0xF704);
    for (size_t i = 0; i < sizeof(fk) / sizeof(fk[0]); i++)
        if (fk[i].ns == c) return fk[i].k;
    return c < 127 ? (int)c : 0;
}

static void mac_track(void)
{
    if (!mac.g) return;
    mac_pt p = mac_p(mac_c("NSEvent"), "mouseLocation");
    double x = p.x, y = mac_main_h() - p.y;
    int i = mac_mon_at(x, y), want = i >= 0 && mac.mon[i].win && mac.mon[i].shown && mac_on_hit(i, x, y) ? i : -1;
    if (want < 0 && mac.hot >= 0 && (mac_u(mac_c("NSEvent"), "pressedMouseButtons") & 7)) want = mac.hot; // mid drag, keep the up coming to us
    if (want != mac.hot) {
        if (mac.hot >= 0 && mac.hot < mac.n && mac.mon[mac.hot].win) mac_vb(mac.mon[mac.hot].win, "setIgnoresMouseEvents:", 1);
        if (want >= 0) mac_vb(mac.mon[want].win, "setIgnoresMouseEvents:", 0);
        mac.hot = want;
    }
    if (want >= 0 && i == want && mac_on_hit(i, x, y) && (x != mac.lx || y != mac.ly)) {
        goome_event e;
        memset(&e, 0, sizeof(e));
        e.type = GOOME_MOTION, e.x = x, e.y = y, e.mon = i;
        gm_push(mac.g, &e);
    }
    mac.lx = x, mac.ly = y;
}

static mac_bool mac_yes(mac_id self, mac_sel cmd) { return 1; }
static mac_bool mac_no(mac_id self, mac_sel cmd) { return 0; }
static mac_bool mac_yes1(mac_id self, mac_sel cmd, mac_id e) { return 1; }
static mac_bool mac_key_ok(mac_id self, mac_sel cmd) { return (mac_bool)(mac.kb_on != 0); } // only key while the keyboard is on

static void mac_draw(mac_id self, mac_sel cmd, mac_rect dirty)
{
    int i = mac_view_mon(self);
    if (i < 0 || !mac.mon[i].px) return;
    mac_mon *m = &mac.mon[i];
    void *ctx = mac_i(mac_i(mac_c("NSGraphicsContext"), "currentContext"), "CGContext");
    if (!ctx) return;
    mac_rect b = {{0, 0}, m->r.s};
    void *bmp = mac.CGBitmapContextCreate(m->px, (size_t)m->pw, (size_t)m->ph, 8, (size_t)m->pw * 4, mac.rgb, 2 | (2 << 12)); // premultiplied first, 32 little = bgra bytes
    void *img = bmp ? mac.CGBitmapContextCreateImage(bmp) : NULL;
    mac.CGContextClearRect(ctx, b);
    if (img) mac.CGContextDrawImage(ctx, b, img), mac.CGImageRelease(img);
    if (bmp) mac.CGContextRelease(bmp);
}

static void mac_mouse(mac_id self, mac_id e, int type, int button)
{
    int i = mac_view_mon(self);
    if (i < 0 || !mac.g) return;
    mac_mon *m = &mac.mon[i];
    mac_pt p = mac_p(e, "locationInWindow");
    double x = m->r.o.x + p.x, y = m->r.o.y + m->r.s.h - p.y;
    if (type == GOOME_DOWN && !mac_on_hit(i, x, y)) { // missed, cant pass it on without accessibility access
        mac_vb(m->win, "setIgnoresMouseEvents:", 1);
        mac.hot = -1;
        return;
    }
    goome_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = type, ev.x = x, ev.y = y, ev.mon = i, ev.button = button;
    gm_push(mac.g, &ev);
}

static void mac_ldown(mac_id self, mac_sel cmd, mac_id e) { mac_mouse(self, e, GOOME_DOWN, 1); }
static void mac_lup(mac_id self, mac_sel cmd, mac_id e) { mac_mouse(self, e, GOOME_UP, 1); }
static void mac_rdown(mac_id self, mac_sel cmd, mac_id e) { mac_mouse(self, e, GOOME_DOWN, 3); }
static void mac_rup(mac_id self, mac_sel cmd, mac_id e) { mac_mouse(self, e, GOOME_UP, 3); }
static void mac_odown(mac_id self, mac_sel cmd, mac_id e)
{
    if (mac_l(e, "buttonNumber") == 2) mac_mouse(self, e, GOOME_DOWN, 2);
}
static void mac_oup(mac_id self, mac_sel cmd, mac_id e)
{
    if (mac_l(e, "buttonNumber") == 2) mac_mouse(self, e, GOOME_UP, 2);
}

static void mac_scroll(mac_id self, mac_sel cmd, mac_id e)
{
    int i = mac_view_mon(self);
    if (i < 0 || !mac.g) return;
    double d = mac_d(e, "scrollingDeltaY");
    int dy;
    if (mac_b(e, "hasPreciseScrollingDeltas")) { // trackpads send tons of tiny pixel deltas, add them up
        mac.scroll_acc += d;
        if (mac.scroll_acc > -16 && mac.scroll_acc < 16) return;
        dy = mac.scroll_acc < 0 ? 1 : -1;
        mac.scroll_acc = 0;
    } else {
        if (d > -0.5 && d < 0.5) return;
        dy = d < 0 ? 1 : -1;
    }
    mac_mon *m = &mac.mon[i];
    mac_pt p = mac_p(e, "locationInWindow");
    goome_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = GOOME_SCROLL, ev.x = m->r.o.x + p.x, ev.y = m->r.o.y + m->r.s.h - p.y, ev.mon = i, ev.dy = dy;
    gm_push(mac.g, &ev);
}

static int mac_hot_index(goome *g, mac_id e)
{
    if (!g) return -1;
    int ch = mac_char(mac_i(e, "charactersIgnoringModifiers")); // keeps shift, gm_combo_key folds it back
    if (!ch) return -1;
    int c = gm_combo_key(ch, mac_mods(e));
    for (int i = 0; i < g->nhk; i++)
        if (g->hk[i].key == (c & 0xFF) && g->hk[i].mods == (c & ~0xFF)) return i;
    return -1;
}

static void mac_push_hot(goome *g, int i)
{
    goome_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = GOOME_HOTKEY, ev.id = g->hk[i].id, ev.key = g->hk[i].key, ev.mods = g->hk[i].mods;
    gm_push(g, &ev);
}

static void mac_key(goome *g, mac_id e)
{
    int mods = mac_mods(e);
    mac_id chars = mac_i(e, "characters");
    int ch = mac_char(mods & (GOOME_ALT | GOOME_CTRL | GOOME_SUPER) ? mac_i(e, "charactersIgnoringModifiers") : chars); // option+h would type a symbol
    goome_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = GOOME_KEY;
    if (ch) {
        int c = gm_combo_key(ch, mods);
        ev.key = c & 0xFF, ev.mods = c & ~0xFF;
    } else
        ev.mods = mods;
    unsigned first = chars && mac_u(chars, "length") ? ((unsigned short (*)(mac_id, mac_sel, unsigned long))mac.msg)(chars, mac_s("characterAtIndex:"), 0) : 0;
    const char *t = !(mods & (GOOME_CTRL | GOOME_SUPER)) && first >= 32 && first != 127 && (first < 0xF700 || first > 0xF8FF) ? mac_utf8(chars) : NULL;
    if (t && strlen(t) < sizeof(ev.text)) memcpy(ev.text, t, strlen(t) + 1);
    if (ev.key || ev.text[0]) gm_push(g, &ev);
}

static void mac_keydown(mac_id self, mac_sel cmd, mac_id e)
{
    if (mac.g && mac.kb_on) mac_key(mac.g, e);
}

static int mac_hot_all(mac_id e);

static void mac_glob_key(mac_blk *b, mac_id e) // other apps keys, needs accessibility access or it just never fires
{
    mac_hot_all(e);
}

static mac_id mac_loc_key(mac_blk *b, mac_id e) { return mac_hot_all(e) ? NULL : e; }

static void mac_method(mac_id cls, const char *sel, void (*f)(void), const char *types) { mac.add_method(cls, mac_s(sel), f, types); }

static int mac_classes(void)
{
    if (!mac.panel_cls) mac.panel_cls = mac_c("GoomePanel");
    if (!mac.panel_cls) {
        mac_id c = mac.alloc_pair(mac_c("NSPanel"), "GoomePanel", 0);
        if (!c) return 0;
        mac_method(c, "canBecomeKeyWindow", (void (*)(void))mac_key_ok, MAC_B "@:");
        mac_method(c, "canBecomeMainWindow", (void (*)(void))mac_no, MAC_B "@:");
        mac.reg_pair(c);
        mac.panel_cls = c;
    }
    if (!mac.view_cls) mac.view_cls = mac_c("GoomeView");
    if (!mac.view_cls) {
        mac_id c = mac.alloc_pair(mac_c("NSView"), "GoomeView", 0);
        if (!c) return 0;
        mac_method(c, "acceptsFirstMouse:", (void (*)(void))mac_yes1, MAC_B "@:@");
        mac_method(c, "acceptsFirstResponder", (void (*)(void))mac_yes, MAC_B "@:"); // or keyDown just beeps
        mac_method(c, "isOpaque", (void (*)(void))mac_no, MAC_B "@:");
        mac_method(c, "drawRect:", (void (*)(void))mac_draw, "v@:" MAC_RECT_ENC);
        mac_method(c, "mouseDown:", (void (*)(void))mac_ldown, "v@:@");
        mac_method(c, "mouseUp:", (void (*)(void))mac_lup, "v@:@");
        mac_method(c, "rightMouseDown:", (void (*)(void))mac_rdown, "v@:@");
        mac_method(c, "rightMouseUp:", (void (*)(void))mac_rup, "v@:@");
        mac_method(c, "otherMouseDown:", (void (*)(void))mac_odown, "v@:@");
        mac_method(c, "otherMouseUp:", (void (*)(void))mac_oup, "v@:@");
        mac_method(c, "scrollWheel:", (void (*)(void))mac_scroll, "v@:@");
        mac_method(c, "keyDown:", (void (*)(void))mac_keydown, "v@:@");
        mac.reg_pair(c);
        mac.view_cls = c;
    }
    return 1;
}

static mac_id mac_screen(uint32_t id)
{
    mac_id all = mac_i(mac_c("NSScreen"), "screens"), key = mac_str("NSScreenNumber");
    unsigned long n = mac_u(all, "count");
    for (unsigned long i = 0; i < n; i++) {
        mac_id s = mac_at(all, i);
        if (mac_u32(mac_ii(mac_i(s, "deviceDescription"), "objectForKey:", key), "unsignedIntValue") == id) return s;
    }
    return NULL;
}

static void mac_drop_win(int i)
{
    mac_mon *m = &mac.mon[i];
    if (m->win) {
        mac_vi(m->win, "orderOut:", NULL);
        mac_v(m->win, "close");
        mac_v(m->win, "release");
    }
    m->win = m->view = NULL, m->shown = 0;
    if (mac.hot == i) mac.hot = -1;
}

static void mac_add_win(int i)
{
    mac_mon *m = &mac.mon[i];
    if (m->win || !mac_screen(m->id)) return; // a new screen shows up in cg before nsscreen knows it
    mac_rect fr = {{m->r.o.x, mac_main_h() - m->r.o.y - m->r.s.h}, m->r.s};
    mac_id w = ((mac_id(*)(mac_id, mac_sel, mac_rect, unsigned long, unsigned long, mac_bool))mac.msg)(
        mac_i(mac.panel_cls, "alloc"), mac_s("initWithContentRect:styleMask:backing:defer:"), fr, 1ul << 7, 2, 0);
    if (!w) return;
    mac_vb(w, "setBecomesKeyOnlyIfNeeded:", 1);
    mac_vb(w, "setHidesOnDeactivate:", 0); // panels hide when the app loses focus otherwise
    mac_vb(w, "setReleasedWhenClosed:", 0);
    mac_vb(w, "setOpaque:", 0);
    mac_vi(w, "setBackgroundColor:", mac_i(mac_c("NSColor"), "clearColor"));
    mac_vb(w, "setHasShadow:", 0);
    mac_vl(w, "setLevel:", 1000); // NSScreenSaverWindowLevel
    mac_vu(w, "setCollectionBehavior:", 1ul << 0 | 1ul << 4 | 1ul << 6 | 1ul << 8); // all spaces, stationary, no cmd+tab, beside fullscreen apps
    mac_vb(w, "setIgnoresMouseEvents:", 1);
    mac_rect vr = {{0, 0}, m->r.s};
    mac_id v = ((mac_id(*)(mac_id, mac_sel, mac_rect))mac.msg)(mac_i(mac.view_cls, "alloc"), mac_s("initWithFrame:"), vr);
    mac_vi(w, "setContentView:", v);
    mac_v(v, "release");
    m->win = w, m->view = v, m->shown = 0;
    if (!m->px) {
        m->px = calloc((size_t)m->pw * m->ph, 4);
        gm_canvas_reset(&m->cv, m->px, m->pw, m->ph, m->pw, m->r.o.x, m->r.o.y, m->bs, i);
    }
}

static int mac_read_mons(mac_mon *out)
{
    uint32_t ids[GM_MAX_MON], n = 0, k = 0;
    if (mac.CGGetActiveDisplayList(GM_MAX_MON, ids, &n)) n = 0;
    for (uint32_t i = 0; i < n && i < GM_MAX_MON; i++) {
        if (mac.CGDisplayMirrorsDisplay(ids[i])) continue;
        mac_mon *m = &out[k];
        memset(m, 0, sizeof(*m));
        m->id = ids[i];
        m->r = mac.CGDisplayBounds(ids[i]);
        if (m->r.s.w < 1 || m->r.s.h < 1) continue;
        mac_id scr = mac_screen(ids[i]);
        m->bs = scr ? mac_d(scr, "backingScaleFactor") : 0;
        if (m->bs <= 0) {
            void *mode = mac.CGDisplayCopyDisplayMode(ids[i]);
            size_t pw = mode ? mac.CGDisplayModeGetPixelWidth(mode) : 0, ph = mode ? mac.CGDisplayModeGetPixelHeight(mode) : 0;
            if (mode) mac.CGDisplayModeRelease(mode);
            if (pw && ph && (pw > ph) != (m->r.s.w > m->r.s.h)) pw = ph; // rotated, the mode keeps the panel way round
            m->bs = pw ? (double)pw / m->r.s.w : 1;
        }
        m->pw = (int)(m->r.s.w * m->bs + 0.5), m->ph = (int)(m->r.s.h * m->bs + 0.5);
        m->rot = (int)(mac.CGDisplayRotation(ids[i]) + 0.5) % 360;
        const char *nm = scr && mac_responds(scr, "localizedName") ? mac_utf8(mac_i(scr, "localizedName")) : NULL;
        if (nm && *nm) snprintf(m->name, sizeof(m->name), "%s", nm);
        else snprintf(m->name, sizeof(m->name), "display %u", ids[i]);
        k++;
    }
    return (int)k;
}

static void mac_publish(goome *g)
{
    goome_monitor gm[GM_MAX_MON];
    memset(gm, 0, sizeof(gm));
    for (int i = 0; i < mac.n; i++) {
        mac_mon *m = &mac.mon[i];
        gm[i].id = (int)m->id;
        snprintf(gm[i].name, sizeof(gm[i].name), "%s", m->name);
        gm[i].area = (goome_rect){(int)m->r.o.x, (int)m->r.o.y, (int)m->r.s.w, (int)m->r.s.h};
        gm[i].scale = m->bs, gm[i].pw = m->pw, gm[i].ph = m->ph, gm[i].rotation = m->rot;
    }
    gm_set_monitors(g, gm, mac.n);
}

static int mac_same(const mac_mon *a, int n)
{
    if (n != mac.n) return 0;
    for (int i = 0; i < n; i++)
        if (a[i].id != mac.mon[i].id || memcmp(&a[i].r, &mac.mon[i].r, sizeof(mac_rect)) || a[i].bs != mac.mon[i].bs || a[i].rot != mac.mon[i].rot) return 0;
    return 1;
}

static void mac_free_mons(void)
{
    for (int i = 0; i < mac.n; i++) {
        mac_drop_win(i);
        free(mac.mon[i].px), free(mac.mon[i].cap);
    }
    memset(mac.mon, 0, sizeof(mac.mon));
    mac.n = 0, mac.hot = -1;
}

static void mac_plug(goome *g)
{
    long now = goome_now_ms();
    if (now - mac.last_plug < 2000) return;
    mac.last_plug = now;
    mac_mon got[GM_MAX_MON];
    int n = mac_read_mons(got);
    if (n > 0 && !mac_same(got, n)) {
        mac_free_mons();
        memcpy(mac.mon, got, sizeof(mac_mon) * (size_t)n);
        mac.n = n;
        for (int i = 0; i < n; i++) mac_add_win(i);
        mac.stale = 1;
        mac_publish(g);
        return;
    }
    for (int i = 0; i < mac.n; i++)
        if (!mac.mon[i].win) mac_add_win(i);
}

static int mac_wopen(goome *g);

static int mac_ov_open(goome *g)
{
    if (g->opts.mode == GOOME_MODE_WINDOW) return mac_wopen(g);
    if (g->opts.mode != GOOME_MODE_OVERLAY) return gm_fail(g, GOOME_ENOTSUP, "macos does overlay and window mode");
    int e = mac_load(g);
    if (e != GOOME_OK) return e;
    if (!mac.main_np()) {
        fprintf(stderr, "goome: on macos goome_open has to be called from the main thread\n");
        return gm_fail(g, GOOME_ENOTSUP, "appkit only works on the main thread, call goome from main()");
    }
    void *pool = mac.pool_push();
    mac.g = g, mac.hot = -1, mac.n = 0, mac.kb_on = 0, mac.lx = mac.ly = -1e9;
    mac_id app = mac_app();
    if (!app || !mac_classes()) {
        mac.pool_pop(pool);
        mac.g = NULL;
        return gm_fail(g, GOOME_ENODISPLAY, "cant set up NSApplication");
    }
    if (!mac.launched) {
        ((mac_bool(*)(mac_id, mac_sel, long))mac.msg)(app, mac_s("setActivationPolicy:"), 1); // accessory, no dock icon
        mac_v(app, "finishLaunching");
        mac.launched = 1;
    }
    mac.rgb = mac.srgb_name ? mac.CGColorSpaceCreateWithName(*mac.srgb_name) : NULL;
    if (!mac.rgb) mac.rgb = mac.CGColorSpaceCreateDeviceRGB();
    mac.n = mac_read_mons(mac.mon);
    for (int i = 0; i < mac.n; i++) mac_add_win(i);
    mac.last_plug = goome_now_ms();
    mac_publish(g);
    mac.pool_pop(pool);
    if (!mac.n) {
        mac.g = NULL;
        if (mac.rgb) mac.CGColorSpaceRelease(mac.rgb);
        mac.rgb = NULL;
        return gm_fail(g, GOOME_ENODISPLAY, "macos says there are no displays");
    }
    return GOOME_OK;
}

static goome_canvas *mac_ov_canvas(goome *g, int i) { return i < mac.n && mac.mon[i].px ? &mac.mon[i].cv : NULL; }

static int mac_ov_commit(goome *g, int i, goome_canvas *c, const goome_rect *hits, int n)
{
    if (i < 0 || i >= mac.n) return gm_fail(g, GOOME_EGONE, "no monitor %d", i);
    mac_mon *m = &mac.mon[i];
    m->nhits = n < 256 ? n : 256;
    if (m->nhits) memcpy(m->hits, hits, sizeof(goome_rect) * (size_t)m->nhits);
    if (!m->win) return GOOME_OK;
    void *pool = mac.pool_push();
    int x0, y0, x1, y1, dmg = gm_canvas_damage(c, &x0, &y0, &x1, &y1);
    int want = c->dx1 > c->dx0 || m->nhits || (mac.kb_on && mac.hot == i); // empty canvas = window out, costs nothing
    if (want && !m->shown) mac_v(m->win, "orderFrontRegardless"), m->shown = 1;
    if (dmg && m->shown) {
        double bs = m->bs; // damage is canvas px from the top, cocoa wants points from the bottom
        mac_rect r = {{x0 / bs, m->r.s.h - y1 / bs}, {(x1 - x0) / bs, (y1 - y0) / bs}};
        mac_vr(m->view, "setNeedsDisplayInRect:", r);
        mac_v(m->win, "displayIfNeeded");
    }
    if (!want && m->shown) mac_vi(m->win, "orderOut:", NULL), m->shown = 0;
    mac_track();
    mac.pool_pop(pool);
    return GOOME_OK;
}

static int mac_ov_wait(goome *g, int ms)
{
    long end = goome_now_ms() + (ms > 0 ? ms : 0);
    mac_id app = mac_app();
    for (;;) {
        void *pool = mac.pool_push(); // no [NSApp run] means no pools unless we make them
        long left = end - goome_now_ms();
        if (left < 0) left = 0;
        if (left > 30) left = 30;
        for (int k = 0; k < 64; k++) {
            mac_id until = ((mac_id(*)(mac_id, mac_sel, double))mac.msg)(mac_c("NSDate"), mac_s("dateWithTimeIntervalSinceNow:"), k ? 0 : left / 1000.0);
            mac_id e = ((mac_id(*)(mac_id, mac_sel, unsigned long long, mac_id, mac_id, mac_bool))mac.msg)(
                app, mac_s("nextEventMatchingMask:untilDate:inMode:dequeue:"), ~0ull, until, mac.run_mode, 1);
            if (!e) break;
            mac_vi(app, "sendEvent:", e);
        }
        mac_track();
        mac_plug(g);
        mac.pool_pop(pool);
        if (g->qlen || goome_now_ms() >= end) return 0;
    }
}

static int mac_ov_pointer(goome *g, double *x, double *y, int *buttons)
{
    void *pool = mac.pool_push();
    mac_pt p = mac_p(mac_c("NSEvent"), "mouseLocation");
    unsigned long b = mac_u(mac_c("NSEvent"), "pressedMouseButtons"); // bit 0 left, 1 right, 2 middle
    *x = p.x, *y = mac_main_h() - p.y;
    *buttons = (int)((b & 1) | (b & 4 ? 2 : 0) | (b & 2 ? 4 : 0));
    mac.pool_pop(pool);
    return 1;
}

static void mac_give_back(void)
{
    if (!mac.prev_app) return;
    mac_id app = mac_app();
    if (mac_responds(app, "yieldActivationToApplication:")) mac_vi(app, "yieldActivationToApplication:", mac.prev_app); // 14+ wants this first
    ((mac_bool(*)(mac_id, mac_sel, unsigned long))mac.msg)(mac.prev_app, mac_s("activateWithOptions:"), 1ul << 1);
    mac_v(mac.prev_app, "release");
    mac.prev_app = NULL;
}

static int mac_ov_keyboard(goome *g, int on)
{
    void *pool = mac.pool_push();
    mac_id app = mac_app();
    if (on && !mac.kb_on) {
        mac_id front = mac_i(mac_i(mac_c("NSWorkspace"), "sharedWorkspace"), "frontmostApplication");
        int self = front && ((int (*)(mac_id, mac_sel))mac.msg)(front, mac_s("processIdentifier")) == (int)getpid();
        if (front && !self && !mac.prev_app) mac.prev_app = mac_i(front, "retain");
    }
    mac.kb_on = on;
    if (on) {
        int i = mac.hot;
        for (int k = 0; i < 0 && k < mac.n; k++)
            if (mac.mon[k].win && mac.mon[k].shown) i = k;
        if (i < 0) i = mac.n > 0 && mac.mon[0].win ? 0 : -1;
        mac_vb(app, "activateIgnoringOtherApps:", 1);
        if (i >= 0) {
            mac_mon *m = &mac.mon[i];
            if (!m->shown) mac_v(m->win, "orderFrontRegardless"), m->shown = 1;
            mac_vi(m->win, "makeKeyAndOrderFront:", NULL);
            ((mac_bool(*)(mac_id, mac_sel, mac_id))mac.msg)(m->win, mac_s("makeFirstResponder:"), m->view);
        }
    } else
        mac_give_back();
    mac.pool_pop(pool);
    return GOOME_OK;
}

static int mac_ov_hotkeys(goome *g)
{
    if (mac.glob_mon || !g->nhk) return GOOME_OK;
    void *pool = mac.pool_push();
    mac_glob_blk = (mac_blk){mac.global_isa, MAC_BLK_GLOBAL | MAC_BLK_SIG, 0, (void (*)(void))mac_glob_key, &mac_desc_glob, NULL};
    mac_loc_blk = (mac_blk){mac.global_isa, MAC_BLK_GLOBAL | MAC_BLK_SIG, 0, (void (*)(void))mac_loc_key, &mac_desc_loc, NULL};
    mac_id (*add)(mac_id, mac_sel, unsigned long long, void *) = (mac_id(*)(mac_id, mac_sel, unsigned long long, void *))mac.msg;
    mac.glob_mon = add(mac_c("NSEvent"), mac_s("addGlobalMonitorForEventsMatchingMask:handler:"), 1ull << 10, &mac_glob_blk); // key down
    mac.loc_mon = add(mac_c("NSEvent"), mac_s("addLocalMonitorForEventsMatchingMask:handler:"), 1ull << 10, &mac_loc_blk);
    if (mac.glob_mon) mac_v(mac.glob_mon, "retain");
    if (mac.loc_mon) mac_v(mac.loc_mon, "retain");
    mac.pool_pop(pool);
    return GOOME_OK; // without accessibility access the global one never fires and we cant tell
}

static void mac_hot_off(void)
{
    mac_id ev = mac_c("NSEvent");
    if (mac.glob_mon) mac_vi(ev, "removeMonitor:", mac.glob_mon), mac_v(mac.glob_mon, "release");
    if (mac.loc_mon) mac_vi(ev, "removeMonitor:", mac.loc_mon), mac_v(mac.loc_mon, "release");
    mac.glob_mon = mac.loc_mon = NULL;
}

static int mac_wcount(void);
static void mac_wclose(goome *g);

static void mac_ov_close(goome *g)
{
    if (g->opts.mode == GOOME_MODE_WINDOW) {
        mac_wclose(g);
        return;
    }
    if (!mac.loaded) return;
    void *pool = mac.pool_push();
    if (mac.kb_on) mac.kb_on = 0, mac_give_back();
    if (!mac_wcount()) mac_hot_off();
    mac_free_mons();
    if (mac.content) mac_v(mac.content, "release");
    mac.content = NULL;
    mac.pool_pop(pool);
    if (mac.rgb) mac.CGColorSpaceRelease(mac.rgb);
    mac.rgb = NULL, mac.g = NULL;
}

static int mac_ov_cursor(goome *g, int shape);
static int mac_clip_set(goome *g, const char *s);
static int mac_clip_get(goome *g, char **out);

static const gm_ov mac_ov = {"macos", mac_ov_open, mac_ov_canvas, mac_ov_commit, mac_ov_wait, mac_ov_pointer, mac_ov_keyboard, mac_ov_hotkeys, mac_ov_close,
                             GOOME_CAP_GLOBAL_POINTER | GOOME_CAP_KEYBOARD | GOOME_CAP_HOTKEYS | GOOME_CAP_CLIPBOARD | GOOME_CAP_CURSOR,
                             NULL, mac_ov_cursor, mac_clip_set, mac_clip_get};
const gm_ov *gm_ov_macos(void) { return &mac_ov; }

static void mac_slot_done(mac_slot *s, void *p, mac_id err)
{
    s->code = err ? mac_l(err, "code") : 0;
    s->p = p ? (s->kind ? mac_i(p, "retain") : mac.CGImageRetain(p)) : NULL;
    if (__atomic_exchange_n(&s->state, 1, __ATOMIC_ACQ_REL) == 2) { // we gave up on it already, last one frees
        if (s->p) {
            if (s->kind) mac_v(s->p, "release");
            else mac.CGImageRelease(s->p);
        }
        free(s);
    }
}

static void mac_got_img(mac_blk *b, void *img, mac_id err) { mac_slot_done(b->ctx, img, err); }
static void mac_got_obj(mac_blk *b, mac_id obj, mac_id err) { mac_slot_done(b->ctx, obj, err); }

static void *mac_await(mac_slot *s, int ms, long *code)
{
    for (int t = 0;; t += 2) {
        if (__atomic_load_n(&s->state, __ATOMIC_ACQUIRE) == 1) break;
        if (t >= ms) {
            if (__atomic_exchange_n(&s->state, 2, __ATOMIC_ACQ_REL) == 1) break; // came in right now
            *code = -1;
            return NULL;
        }
        mac_nap(2);
        if (t % 20 == 0) mac_track();
    }
    void *p = s->p;
    *code = s->code;
    free(s);
    return p;
}

static long mac_list(int ms)
{
    mac_slot *s = calloc(1, sizeof(*s));
    if (!s) return -1;
    s->kind = 1;
    mac_blk b = {mac.stack_isa, MAC_BLK_SIG, 0, (void (*)(void))mac_got_obj, &mac_desc_obj, s};
    ((void (*)(mac_id, mac_sel, void *))mac.msg)(mac_c("SCShareableContent"), mac_s("getShareableContentWithCompletionHandler:"), &b);
    long code;
    mac_id c = mac_await(s, ms, &code);
    if (!c) return code ? code : -1;
    if (mac.content) mac_v(mac.content, "release");
    mac.content = c, mac.stale = 0;
    return 0;
}

static int mac_cap_open(goome *g)
{
    if (!mac.loaded || (!mac.g && g->opts.mode != GOOME_MODE_WINDOW)) return gm_fail(g, GOOME_ENOTSUP, "the macos overlay isnt open");
    if (!mac.sck || !mac_c("SCScreenshotManager") || !mac.CGPreflightScreenCaptureAccess) return gm_fail(g, GOOME_ENOTSUP, "screen grabs need macos 14 or newer");
    if (!mac.CGPreflightScreenCaptureAccess()) { // its per app so the terminal gets it, not us
        if (!mac.asked && mac.CGRequestScreenCaptureAccess) mac.CGRequestScreenCaptureAccess();
        mac.asked = 1;
        return gm_fail(g, GOOME_EDENIED, "macos wont let this app record the screen: allow your terminal (or this app) in System Settings > Privacy & Security > "
                                         "Screen & System Audio Recording, then quit it and start again");
    }
    void *pool = mac.pool_push();
    long e = mac_list(5000);
    mac.pool_pop(pool);
    if (e == -3801) return gm_fail(g, GOOME_EDENIED, "screen recording got turned down (System Settings > Privacy & Security > Screen & System Audio Recording)");
    if (e) return gm_fail(g, GOOME_EAGAIN, "screencapturekit didnt list the screens (error %ld)", e);
    return GOOME_OK;
}

static int mac_grab(goome *g, mac_mon *m, goome_rect r, goome_image *img, void *rgb)
{
    mac_id disp = NULL, ours[GM_MAX_MON];
    int nours = 0, missing = 0, shown = 0;
    for (int tries = 0; tries < 2; tries++) {
        if (mac.stale || tries) mac_list(3000);
        disp = NULL, nours = 0, missing = 0, shown = 0;
        mac_id ds = mac_i(mac.content, "displays");
        for (unsigned long i = 0, n = mac_u(ds, "count"); i < n && !disp; i++)
            if (mac_u32(mac_at(ds, i), "displayID") == m->id) disp = mac_at(ds, i);
        for (int k = 0; k < mac.n; k++) {
            if (!mac.mon[k].win || !mac.mon[k].shown) continue;
            shown++;
            if (!g->opts.hide_self) continue;
            uint32_t wn = (uint32_t)mac_l(mac.mon[k].win, "windowNumber");
            mac_id ws = mac_i(mac.content, "windows"), hit = NULL;
            for (unsigned long i = 0, n = mac_u(ws, "count"); i < n && !hit; i++)
                if (mac_u32(mac_at(ws, i), "windowID") == wn) hit = mac_at(ws, i);
            if (hit) ours[nours++] = hit;
            else missing++;
        }
        if (disp && !missing) break;
    }
    if (!disp) return gm_fail(g, GOOME_EGONE, "screencapturekit doesnt know display %u", m->id);
    mac_id arr = ((mac_id(*)(mac_id, mac_sel, mac_id *, unsigned long))mac.msg)(mac_c("NSArray"), mac_s("arrayWithObjects:count:"), ours, (unsigned long)nours);
    mac_id filter = ((mac_id(*)(mac_id, mac_sel, mac_id, mac_id))mac.msg)(mac_i(mac_c("SCContentFilter"), "alloc"), mac_s("initWithDisplay:excludingWindows:"), disp, arr);
    mac_id cfg = mac_i(mac_i(mac_c("SCStreamConfiguration"), "alloc"), "init");
    int w = (int)(r.w * m->bs + 0.5), h = (int)(r.h * m->bs + 0.5);
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    ((void (*)(mac_id, mac_sel, size_t))mac.msg)(cfg, mac_s("setWidth:"), (size_t)w);
    ((void (*)(mac_id, mac_sel, size_t))mac.msg)(cfg, mac_s("setHeight:"), (size_t)h);
    mac_vb(cfg, "setShowsCursor:", 0);
    if (r.x != (int)m->r.o.x || r.y != (int)m->r.o.y || r.w != (int)m->r.s.w || r.h != (int)m->r.s.h) {
        mac_rect src = {{r.x - m->r.o.x, r.y - m->r.o.y}, {r.w, r.h}}; // points, from the display top left
        mac_vr(cfg, "setSourceRect:", src);
    }
    mac_slot *s = calloc(1, sizeof(*s));
    if (!s || !filter || !cfg) {
        free(s);
        if (filter) mac_v(filter, "release");
        if (cfg) mac_v(cfg, "release");
        return gm_fail(g, GOOME_ENOMEM, "cant set up the screenshot");
    }
    mac_blk b = {mac.stack_isa, MAC_BLK_SIG, 0, (void (*)(void))mac_got_img, &mac_desc_img, s};
    ((void (*)(mac_id, mac_sel, mac_id, mac_id, void *))mac.msg)(mac_c("SCScreenshotManager"), mac_s("captureImageWithFilter:configuration:completionHandler:"), filter, cfg, &b);
    mac_v(filter, "release");
    mac_v(cfg, "release");
    long code;
    void *shot = mac_await(s, 2000, &code);
    if (!shot) {
        if (code == -3801) return gm_fail(g, GOOME_EDENIED, "screen recording got turned down");
        return gm_fail(g, GOOME_EAGAIN, code == -1 ? "the screenshot took over 2 s" : "the screenshot failed (error %ld)", code);
    }
    size_t need = (size_t)w * h;
    if (need > m->capn) {
        uint32_t *p = realloc(m->cap, need * 4);
        if (!p) {
            mac.CGImageRelease(shot);
            return gm_fail(g, GOOME_ENOMEM, "no memory for a %dx%d grab", w, h);
        }
        m->cap = p, m->capn = need;
    }
    void *ctx = mac.CGBitmapContextCreate(m->cap, (size_t)w, (size_t)h, 8, (size_t)w * 4, rgb, 6 | (2 << 12)); // skip first + 32 little = bgrx = 0xXXRRGGBB words
    if (ctx) {
        mac_rect all = {{0, 0}, {w, h}};
        mac.CGContextDrawImage(ctx, all, shot);
        mac.CGContextRelease(ctx);
    }
    mac.CGImageRelease(shot);
    if (!ctx) return gm_fail(g, GOOME_ENOMEM, "cant make a %dx%d bitmap", w, h);
    for (size_t k = 0; k < need; k++) m->cap[k] |= 0xFF000000u;
    img->px = m->cap, img->w = w, img->h = h, img->stride = w, img->area = r;
    img->has_self = g->opts.hide_self ? missing > 0 : shown > 0;
    img->has_cursor = 0;
    return GOOME_OK;
}

static int mac_wgrab(goome *g, goome_rect r, goome_image *img);

static int mac_cap_grab(goome *g, int i, goome_rect r, goome_image *img)
{
    if (g->opts.mode == GOOME_MODE_WINDOW) {
        void *pool = mac.pool_push();
        int e = mac_wgrab(g, r, img);
        mac.pool_pop(pool);
        return e;
    }
    if (i < 0 || i >= mac.n) return gm_fail(g, GOOME_EGONE, "no monitor %d", i);
    void *pool = mac.pool_push();
    int e = mac_grab(g, &mac.mon[i], r, img, mac.rgb);
    mac.pool_pop(pool);
    return e;
}

static void mac_cap_close(goome *g)
{
    if (!mac.loaded) return;
    void *pool = mac.pool_push();
    if (mac.content) mac_v(mac.content, "release");
    mac.content = NULL;
    mac.pool_pop(pool);
}

static const gm_cap mac_cap = {"screencapturekit", NULL, mac_cap_open, mac_cap_grab, mac_cap_close, GOOME_CAP_CAPTURE | GOOME_CAP_HIDE_SELF};
const gm_cap *gm_cap_macos(void) { return &mac_cap; }

typedef struct {
    char *path;
    double x, y;
} mac_drop_item;

typedef struct {
    goome *g;
    mac_id win, view, down, rgb;
    int pw, ph, lw, lh, trans, popup, hid, mzoom, mfs;
    double bs, vw, vh, scroll_acc;
    mac_rect keep;
    uint32_t *px;
    goome_canvas cv;
    mac_mon grab;
    mac_drop_item *drops;
    int ndrop;
} mac_win;

static mac_win *mac_wins[64];
static int mac_nwin;

static int mac_wcount(void) { return mac_nwin; }

static int mac_hot_all(mac_id e)
{
    int n = 0, i = mac_hot_index(mac.g, e);
    if (i >= 0) mac_push_hot(mac.g, i), n++;
    for (int k = 0; k < mac_nwin; k++)
        if ((i = mac_hot_index(mac_wins[k]->g, e)) >= 0) mac_push_hot(mac_wins[k]->g, i), n++;
    return n;
}
static mac_id mac_wincls, mac_appview;

static mac_rect mac_r(mac_id o, const char *s)
{
#if defined(__aarch64__) || defined(__arm64__)
    return ((mac_rect(*)(mac_id, mac_sel))mac.msg)(o, mac_s(s));
#else
    return ((mac_rect(*)(mac_id, mac_sel))mac.msg_stret)(o, mac_s(s));
#endif
}

static mac_rect mac_rr(mac_id o, const char *s, mac_rect a)
{
#if defined(__aarch64__) || defined(__arm64__)
    return ((mac_rect(*)(mac_id, mac_sel, mac_rect))mac.msg)(o, mac_s(s), a);
#else
    return ((mac_rect(*)(mac_id, mac_sel, mac_rect))mac.msg_stret)(o, mac_s(s), a);
#endif
}

static mac_win *mac_win_of(mac_id o)
{
    for (int i = 0; o && i < mac_nwin; i++)
        if (mac_wins[i]->win == o || mac_wins[i]->view == o) return mac_wins[i];
    return NULL;
}

static void mac_wpush(mac_win *w, int type, int id)
{
    goome_event e;
    memset(&e, 0, sizeof(e));
    e.type = type, e.id = id;
    gm_push(w->g, &e);
}

static void mac_wpublish(mac_win *w)
{
    goome_monitor m;
    memset(&m, 0, sizeof(m));
    snprintf(m.name, sizeof(m.name), "%s", w->g->title);
    m.area = (goome_rect){0, 0, w->lw, w->lh};
    m.scale = w->bs, m.pw = w->pw, m.ph = w->ph;
    if (w->cv.px != w->px || w->cv.w != w->pw || w->cv.h != w->ph || w->cv.scale != w->bs)
        gm_canvas_reset(&w->cv, w->px, w->pw, w->ph, w->pw, 0, 0, w->bs, 0);
    gm_set_monitors(w->g, &m, 1);
}

static void mac_wsized(mac_win *w)
{
    if (!w->view) return;
    mac_rect b = mac_r(w->view, "bounds");
    double bs = mac_d(w->win, "backingScaleFactor");
    if (bs <= 0) bs = 1;
    if (b.s.w < 1 || b.s.h < 1) return;
    int pw = (int)(b.s.w * bs + 0.5), ph = (int)(b.s.h * bs + 0.5);
    if (pw != w->pw || ph != w->ph || !w->px) {
        uint32_t *px = calloc((size_t)pw * ph, 4);
        if (!px) return;
        if (w->px) {
            int cw = pw < w->pw ? pw : w->pw, ch = ph < w->ph ? ph : w->ph;
            for (int y = 0; y < ch; y++) memcpy(px + (size_t)y * pw, w->px + (size_t)y * w->pw, (size_t)cw * 4);
        }
        free(w->px);
        w->px = px, w->pw = pw, w->ph = ph;
    }
    w->bs = bs, w->vw = b.s.w, w->vh = b.s.h;
    w->lw = (int)(b.s.w + 0.5), w->lh = (int)(b.s.h + 0.5);
    mac_wpublish(w);
}

static mac_pt mac_wpoint(mac_win *w, mac_pt in_win)
{
    mac_pt p = ((mac_pt(*)(mac_id, mac_sel, mac_pt, mac_id))mac.msg)(w->view, mac_s("convertPoint:fromView:"), in_win, NULL);
    p.y = w->vh - p.y;
    return p;
}

static mac_id mac_nscursor(int shape)
{
    static const char *const sel[] = {"arrowCursor", "IBeamCursor", "pointingHandCursor", "openHandCursor", "resizeLeftRightCursor", "resizeUpDownCursor",
                                      "_windowResizeNorthWestSouthEastCursor", "_windowResizeNorthEastSouthWestCursor", "busyButClickableCursor",
                                      "operationNotAllowedCursor"};
    mac_id c = mac_c("NSCursor");
    const char *s = shape >= 0 && shape < (int)(sizeof(sel) / sizeof(sel[0])) ? sel[shape] : "arrowCursor";
    if (!mac_responds(c, s)) s = "arrowCursor";
    return mac_i(c, s);
}

static void mac_wcursor_apply(mac_win *w)
{
    int hide = w->g->cursor == GOOME_CURSOR_HIDDEN;
    if (hide != w->hid) {
        mac_v(mac_c("NSCursor"), hide ? "hide" : "unhide");
        w->hid = hide;
    }
    if (!hide) mac_v(mac_nscursor(w->g->cursor), "set");
}

static int mac_winside(mac_win *w)
{
    mac_pt p = mac_wpoint(w, mac_p(w->win, "mouseLocationOutsideOfEventStream"));
    return p.x >= 0 && p.y >= 0 && p.x < w->vw && p.y < w->vh;
}

static void mac_wmouse(mac_id self, mac_id e, int type, int button)
{
    mac_win *w = mac_win_of(self);
    if (!w) return;
    mac_pt p = mac_wpoint(w, mac_p(e, "locationInWindow"));
    if (type == GOOME_DOWN && button == 1) {
        if (w->down) mac_v(w->down, "release");
        w->down = mac_i(e, "retain");
    }
    if (type == GOOME_MOTION) mac_wcursor_apply(w);
    goome_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = type, ev.x = p.x, ev.y = p.y, ev.button = button, ev.mods = mac_mods(e);
    gm_push(w->g, &ev);
}

static void mac_wldown(mac_id self, mac_sel cmd, mac_id e) { mac_wmouse(self, e, GOOME_DOWN, 1); }
static void mac_wlup(mac_id self, mac_sel cmd, mac_id e) { mac_wmouse(self, e, GOOME_UP, 1); }
static void mac_wrdown(mac_id self, mac_sel cmd, mac_id e) { mac_wmouse(self, e, GOOME_DOWN, 3); }
static void mac_wrup(mac_id self, mac_sel cmd, mac_id e) { mac_wmouse(self, e, GOOME_UP, 3); }
static void mac_wmove(mac_id self, mac_sel cmd, mac_id e) { mac_wmouse(self, e, GOOME_MOTION, 0); }
static void mac_wodown(mac_id self, mac_sel cmd, mac_id e)
{
    if (mac_l(e, "buttonNumber") == 2) mac_wmouse(self, e, GOOME_DOWN, 2);
}
static void mac_woup(mac_id self, mac_sel cmd, mac_id e)
{
    if (mac_l(e, "buttonNumber") == 2) mac_wmouse(self, e, GOOME_UP, 2);
}

static void mac_wcursor_update(mac_id self, mac_sel cmd, mac_id e)
{
    mac_win *w = mac_win_of(self);
    if (w) mac_wcursor_apply(w);
}

static void mac_wscroll(mac_id self, mac_sel cmd, mac_id e)
{
    mac_win *w = mac_win_of(self);
    if (!w) return;
    double d = mac_d(e, "scrollingDeltaY");
    int dy;
    if (mac_b(e, "hasPreciseScrollingDeltas")) {
        w->scroll_acc += d;
        if (w->scroll_acc > -16 && w->scroll_acc < 16) return;
        dy = w->scroll_acc < 0 ? 1 : -1;
        w->scroll_acc = 0;
    } else {
        if (d > -0.5 && d < 0.5) return;
        dy = d < 0 ? 1 : -1;
    }
    mac_pt p = mac_wpoint(w, mac_p(e, "locationInWindow"));
    goome_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = GOOME_SCROLL, ev.x = p.x, ev.y = p.y, ev.dy = dy, ev.mods = mac_mods(e);
    gm_push(w->g, &ev);
}

static void mac_wkeydown(mac_id self, mac_sel cmd, mac_id e)
{
    mac_win *w = mac_win_of(self);
    if (!w) return;
    int mods = mac_mods(e);
    if ((mods & (GOOME_SUPER | GOOME_CTRL | GOOME_ALT)) == GOOME_SUPER && mac_char(mac_i(e, "charactersIgnoringModifiers")) == 'w') {
        mac_wpush(w, GOOME_CLOSE, 0);
        return;
    }
    mac_key(w->g, e);
}

static void mac_wdraw(mac_id self, mac_sel cmd, mac_rect dirty)
{
    mac_win *w = mac_win_of(self);
    if (!w || !w->px) return;
    void *ctx = mac_i(mac_i(mac_c("NSGraphicsContext"), "currentContext"), "CGContext");
    if (!ctx) return;
    mac_rect b = {{0, 0}, {w->pw / w->bs, w->ph / w->bs}};
    void *bmp = mac.CGBitmapContextCreate(w->px, (size_t)w->pw, (size_t)w->ph, 8, (size_t)w->pw * 4, w->rgb, (w->trans ? 2u : 6u) | (2u << 12));
    void *img = bmp ? mac.CGBitmapContextCreateImage(bmp) : NULL;
    mac_rect all = {{0, 0}, {w->vw, w->vh}};
    if (w->trans) mac.CGContextClearRect(ctx, all);
    b.o.y = w->vh - b.s.h;
    if (img) mac.CGContextDrawImage(ctx, b, img), mac.CGImageRelease(img);
    if (bmp) mac.CGContextRelease(bmp);
}

static mac_bool mac_wopaque(mac_id self, mac_sel cmd)
{
    mac_win *w = mac_win_of(self);
    return (mac_bool)(w && !w->trans);
}

static mac_bool mac_wshould_close(mac_id self, mac_sel cmd, mac_id sender)
{
    mac_win *w = mac_win_of(self);
    if (w) mac_wpush(w, GOOME_CLOSE, 0);
    return 0;
}

static void mac_wresized(mac_id self, mac_sel cmd, mac_id note)
{
    mac_win *w = mac_win_of(self);
    if (w) mac_wsized(w);
}

static void mac_wkey_on(mac_id self, mac_sel cmd, mac_id note)
{
    mac_win *w = mac_win_of(self);
    if (w) mac_wpush(w, GOOME_FOCUS, 1);
}

static void mac_wkey_off(mac_id self, mac_sel cmd, mac_id note)
{
    mac_win *w = mac_win_of(self);
    if (w) mac_wpush(w, GOOME_FOCUS, 0);
}

static unsigned long mac_wdrag_in(mac_id self, mac_sel cmd, mac_id info) { return 1; }

static mac_bool mac_wdrop(mac_id self, mac_sel cmd, mac_id info)
{
    mac_win *w = mac_win_of(self);
    if (!w) return 0;
    mac_id pb = mac_i(info, "draggingPasteboard");
    mac_id cls = mac_c("NSURL");
    mac_id want = ((mac_id(*)(mac_id, mac_sel, mac_id *, unsigned long))mac.msg)(mac_c("NSArray"), mac_s("arrayWithObjects:count:"), &cls, 1ul);
    mac_id urls = ((mac_id(*)(mac_id, mac_sel, mac_id, mac_id))mac.msg)(pb, mac_s("readObjectsForClasses:options:"), want, NULL);
    mac_pt p = mac_wpoint(w, mac_p(info, "draggingLocation"));
    for (unsigned long i = 0, n = mac_u(urls, "count"); i < n; i++) {
        mac_id u = mac_at(urls, i);
        const char *path = mac_b(u, "isFileURL") ? mac_utf8(mac_i(u, "path")) : NULL;
        mac_drop_item *nd = path ? realloc(w->drops, sizeof(*nd) * (size_t)(w->ndrop + 1)) : NULL;
        char *s = nd ? malloc(strlen(path) + 1) : NULL;
        if (nd) w->drops = nd;
        if (!s) continue;
        memcpy(s, path, strlen(path) + 1);
        mac_drop_item it = {s, p.x, p.y};
        w->drops[w->ndrop++] = it;
    }
    return 1;
}

static int mac_wclasses(void)
{
    if (!mac_wincls) mac_wincls = mac_c("GoomeWindow");
    if (!mac_wincls) {
        mac_id c = mac.alloc_pair(mac_c("NSWindow"), "GoomeWindow", 0);
        if (!c) return 0;
        mac_method(c, "canBecomeKeyWindow", (void (*)(void))mac_yes, MAC_B "@:");
        mac_method(c, "canBecomeMainWindow", (void (*)(void))mac_yes, MAC_B "@:");
        mac_method(c, "windowShouldClose:", (void (*)(void))mac_wshould_close, MAC_B "@:@");
        mac_method(c, "windowDidResize:", (void (*)(void))mac_wresized, "v@:@");
        mac_method(c, "windowDidChangeBackingProperties:", (void (*)(void))mac_wresized, "v@:@");
        mac_method(c, "windowDidBecomeKey:", (void (*)(void))mac_wkey_on, "v@:@");
        mac_method(c, "windowDidResignKey:", (void (*)(void))mac_wkey_off, "v@:@");
        mac.reg_pair(c);
        mac_wincls = c;
    }
    if (!mac_appview) mac_appview = mac_c("GoomeAppView");
    if (!mac_appview) {
        mac_id c = mac.alloc_pair(mac_c("NSView"), "GoomeAppView", 0);
        if (!c) return 0;
        mac_method(c, "acceptsFirstMouse:", (void (*)(void))mac_yes1, MAC_B "@:@");
        mac_method(c, "acceptsFirstResponder", (void (*)(void))mac_yes, MAC_B "@:");
        mac_method(c, "isOpaque", (void (*)(void))mac_wopaque, MAC_B "@:");
        mac_method(c, "drawRect:", (void (*)(void))mac_wdraw, "v@:" MAC_RECT_ENC);
        mac_method(c, "mouseDown:", (void (*)(void))mac_wldown, "v@:@");
        mac_method(c, "mouseUp:", (void (*)(void))mac_wlup, "v@:@");
        mac_method(c, "rightMouseDown:", (void (*)(void))mac_wrdown, "v@:@");
        mac_method(c, "rightMouseUp:", (void (*)(void))mac_wrup, "v@:@");
        mac_method(c, "otherMouseDown:", (void (*)(void))mac_wodown, "v@:@");
        mac_method(c, "otherMouseUp:", (void (*)(void))mac_woup, "v@:@");
        mac_method(c, "mouseMoved:", (void (*)(void))mac_wmove, "v@:@");
        mac_method(c, "mouseDragged:", (void (*)(void))mac_wmove, "v@:@");
        mac_method(c, "rightMouseDragged:", (void (*)(void))mac_wmove, "v@:@");
        mac_method(c, "otherMouseDragged:", (void (*)(void))mac_wmove, "v@:@");
        mac_method(c, "cursorUpdate:", (void (*)(void))mac_wcursor_update, "v@:@");
        mac_method(c, "scrollWheel:", (void (*)(void))mac_wscroll, "v@:@");
        mac_method(c, "keyDown:", (void (*)(void))mac_wkeydown, "v@:@");
        mac_method(c, "draggingEntered:", (void (*)(void))mac_wdrag_in, "Q@:@");
        mac_method(c, "performDragOperation:", (void (*)(void))mac_wdrop, MAC_B "@:@");
        mac.reg_pair(c);
        mac_appview = c;
    }
    return 1;
}

static void mac_wclose(goome *g);
static const gm_ov *mac_wtable(void);

static int mac_wopen(goome *g)
{
    int e = mac_load(g);
    if (e != GOOME_OK) return e;
    if (!mac.main_np()) {
        fprintf(stderr, "goome: on macos goome_open has to be called from the main thread\n");
        return gm_fail(g, GOOME_ENOTSUP, "appkit only works on the main thread, call goome from main()");
    }
    if (mac_nwin >= (int)(sizeof(mac_wins) / sizeof(mac_wins[0]))) return gm_fail(g, GOOME_ENOMEM, "too many goome windows");
    void *pool = mac.pool_push();
    mac_id app = mac_app();
    if (!app || !mac_classes() || !mac_wclasses()) {
        mac.pool_pop(pool);
        return gm_fail(g, GOOME_ENODISPLAY, "cant set up NSApplication");
    }
    ((mac_bool(*)(mac_id, mac_sel, long))mac.msg)(app, mac_s("setActivationPolicy:"), 0);
    if (!mac.launched) {
        mac_v(app, "finishLaunching");
        mac.launched = 1;
    }
    mac_win *w = calloc(1, sizeof(*w));
    if (!w) {
        mac.pool_pop(pool);
        return gm_fail(g, GOOME_ENOMEM, "no memory for the window");
    }
    w->g = g;
    g->ov_priv = w;
    mac_wins[mac_nwin++] = w;
    int f = g->opts.window_flags;
    w->trans = (f & GOOME_WINDOW_TRANSPARENT) != 0;
    w->popup = (f & GOOME_WINDOW_BORDERLESS) != 0;
    w->rgb = mac.srgb_name ? mac.CGColorSpaceCreateWithName(*mac.srgb_name) : NULL;
    if (!w->rgb) w->rgb = mac.CGColorSpaceCreateDeviceRGB();
    unsigned long mask = (w->popup ? 0ul : 1ul | 2ul) | 4ul | (f & GOOME_WINDOW_RESIZABLE ? 8ul : 0ul);
    mac_rect fr = {{0, 0}, {g->opts.width, g->opts.height}};
    w->win = ((mac_id(*)(mac_id, mac_sel, mac_rect, unsigned long, unsigned long, mac_bool))mac.msg)(
        mac_i(mac_wincls, "alloc"), mac_s("initWithContentRect:styleMask:backing:defer:"), fr, mask, 2, 0);
    if (!w->win) {
        mac.pool_pop(pool);
        mac_wclose(g);
        return gm_fail(g, GOOME_ENODISPLAY, "cant make the NSWindow");
    }
    mac_vb(w->win, "setReleasedWhenClosed:", 0);
    mac_vi(w->win, "setDelegate:", w->win);
    mac_vb(w->win, "setAcceptsMouseMovedEvents:", 1);
    mac_vi(w->win, "setTitle:", mac_str(g->title));
    if (f & GOOME_WINDOW_RESIZABLE && !w->popup) mac_vu(w->win, "setCollectionBehavior:", 1ul << 7);
    if (f & GOOME_WINDOW_ON_TOP) mac_vl(w->win, "setLevel:", 3);
    if (w->trans) {
        mac_vb(w->win, "setOpaque:", 0);
        mac_vi(w->win, "setBackgroundColor:", mac_i(mac_c("NSColor"), "clearColor"));
        mac_vb(w->win, "setHasShadow:", 0);
    }
    if (g->opts.min_width > 0 || g->opts.min_height > 0) {
        mac_sz ms = {g->opts.min_width > 0 ? g->opts.min_width : 1, g->opts.min_height > 0 ? g->opts.min_height : 1};
        ((void (*)(mac_id, mac_sel, mac_sz))mac.msg)(w->win, mac_s("setContentMinSize:"), ms);
    }
    w->view = ((mac_id(*)(mac_id, mac_sel, mac_rect))mac.msg)(mac_i(mac_appview, "alloc"), mac_s("initWithFrame:"), fr);
    mac_vi(w->win, "setContentView:", w->view);
    mac_v(w->view, "release");
    mac_id area = ((mac_id(*)(mac_id, mac_sel, mac_rect, unsigned long, mac_id, mac_id))mac.msg)(
        mac_i(mac_c("NSTrackingArea"), "alloc"), mac_s("initWithRect:options:owner:userInfo:"), fr, 0x02ul | 0x04ul | 0x40ul | 0x200ul, w->view, NULL);
    if (area) mac_vi(w->view, "addTrackingArea:", area), mac_v(area, "release");
    mac_id *ft = dlsym(mac.appkit, "NSPasteboardTypeFileURL");
    if (ft && *ft) {
        mac_id t = *ft;
        mac_vi(w->view, "registerForDraggedTypes:", ((mac_id(*)(mac_id, mac_sel, mac_id *, unsigned long))mac.msg)(mac_c("NSArray"), mac_s("arrayWithObjects:count:"), &t, 1ul));
    }
    ((mac_bool(*)(mac_id, mac_sel, mac_id))mac.msg)(w->win, mac_s("makeFirstResponder:"), w->view);
    if (f & GOOME_WINDOW_CENTER) mac_v(w->win, "center");
    else {
        mac_rect vf = mac_r(mac_i(mac_c("NSScreen"), "mainScreen"), "visibleFrame");
        mac_pt tl = {vf.o.x + 40, vf.o.y + vf.s.h - 40};
        ((void (*)(mac_id, mac_sel, mac_pt))mac.msg)(w->win, mac_s("setFrameTopLeftPoint:"), tl);
    }
    mac_wsized(w);
    if (!w->px || g->nmon < 1) {
        mac.pool_pop(pool);
        mac_wclose(g);
        return gm_fail(g, GOOME_ENOMEM, "cant make the window bitmap");
    }
    if (!(f & GOOME_WINDOW_HIDDEN)) {
        mac_vb(app, "activateIgnoringOtherApps:", 1);
        mac_vi(w->win, "makeKeyAndOrderFront:", NULL);
    }
    g->ov = mac_wtable();
    mac.pool_pop(pool);
    return GOOME_OK;
}

static goome_canvas *mac_wcanvas(goome *g, int mon)
{
    mac_win *w = g->ov_priv;
    return mon == 0 && w->px ? &w->cv : NULL;
}

static int mac_wcommit(goome *g, int mon, goome_canvas *c, const goome_rect *hits, int n)
{
    mac_win *w = g->ov_priv;
    if (mon != 0 || c != &w->cv) return GOOME_EGONE;
    int x0, y0, x1, y1;
    if (!gm_canvas_damage(c, &x0, &y0, &x1, &y1)) return GOOME_OK;
    void *pool = mac.pool_push();
    double bs = w->bs;
    mac_rect r = {{x0 / bs, w->vh - y1 / bs}, {(x1 - x0) / bs, (y1 - y0) / bs}};
    mac_vr(w->view, "setNeedsDisplayInRect:", r);
    mac_v(w->win, "displayIfNeeded");
    mac.pool_pop(pool);
    return GOOME_OK;
}

static void mac_wnext_drop(mac_win *w)
{
    if (!w->ndrop || w->g->qlen) return;
    mac_drop_item it = w->drops[0];
    memmove(w->drops, w->drops + 1, sizeof(*w->drops) * (size_t)(w->ndrop - 1));
    w->ndrop--;
    gm_drop(w->g, it.x, it.y, it.path);
    free(it.path);
}

static void mac_pump(int ms)
{
    mac_id app = mac_app();
    for (int k = 0; k < 64; k++) {
        mac_id until = ((mac_id(*)(mac_id, mac_sel, double))mac.msg)(mac_c("NSDate"), mac_s("dateWithTimeIntervalSinceNow:"), k ? 0 : ms / 1000.0);
        mac_id e = ((mac_id(*)(mac_id, mac_sel, unsigned long long, mac_id, mac_id, mac_bool))mac.msg)(
            app, mac_s("nextEventMatchingMask:untilDate:inMode:dequeue:"), ~0ull, until, mac.run_mode, 1);
        if (!e) break;
        mac_vi(app, "sendEvent:", e);
    }
}

static int mac_wwait(goome *g, int ms)
{
    mac_win *w = g->ov_priv;
    long end = goome_now_ms() + (ms > 0 ? ms : 0);
    for (;;) {
        void *pool = mac.pool_push();
        long left = end - goome_now_ms();
        if (left < 0) left = 0;
        if (left > 30) left = 30;
        mac_wnext_drop(w);
        if (!g->qlen) mac_pump((int)left);
        mac_track();
        mac_wnext_drop(w);
        mac.pool_pop(pool);
        if (g->qlen || goome_now_ms() >= end) return 0;
    }
}

static int mac_wpointer(goome *g, double *x, double *y, int *buttons)
{
    mac_win *w = g->ov_priv;
    void *pool = mac.pool_push();
    mac_pt p = mac_wpoint(w, mac_p(w->win, "mouseLocationOutsideOfEventStream"));
    unsigned long b = mac_u(mac_c("NSEvent"), "pressedMouseButtons");
    *x = p.x, *y = p.y;
    *buttons = (int)((b & 1) | (b & 4 ? 2 : 0) | (b & 2 ? 4 : 0));
    mac.pool_pop(pool);
    return 1;
}

static int mac_wkeyboard(goome *g, int on)
{
    mac_win *w = g->ov_priv;
    if (!on) return GOOME_OK;
    void *pool = mac.pool_push();
    mac_vb(mac_app(), "activateIgnoringOtherApps:", 1);
    mac_vi(w->win, "makeKeyAndOrderFront:", NULL);
    mac.pool_pop(pool);
    return GOOME_OK;
}

static mac_rect mac_wcontent(mac_win *w) { return mac_rr(w->win, "contentRectForFrameRect:", mac_r(w->win, "frame")); }

static int mac_wresize_loop(mac_win *w, int edges)
{
    mac_id app = mac_app(), until = mac_i(mac_c("NSDate"), "distantPast");
    mac_id *mode = dlsym(mac.appkit, "NSEventTrackingRunLoopMode");
    mac_rect f0 = mac_r(w->win, "frame");
    mac_pt m0 = mac_p(mac_c("NSEvent"), "mouseLocation");
    double minw = (w->g->opts.min_width > 0 ? w->g->opts.min_width : 50) + f0.s.w - w->vw, minh = (w->g->opts.min_height > 0 ? w->g->opts.min_height : 50) + f0.s.h - w->vh;
    while (mac_u(mac_c("NSEvent"), "pressedMouseButtons") & 1) {
        void *pool = mac.pool_push();
        mac_id e = ((mac_id(*)(mac_id, mac_sel, unsigned long long, mac_id, mac_id, mac_bool))mac.msg)(
            app, mac_s("nextEventMatchingMask:untilDate:inMode:dequeue:"), (1ull << 6) | (1ull << 2), until, mode && *mode ? *mode : mac.run_mode, 1);
        if (e && mac_u(e, "type") == 2) {
            mac.pool_pop(pool);
            break;
        }
        mac_pt m = mac_p(mac_c("NSEvent"), "mouseLocation");
        double dx = m.x - m0.x, dy = m.y - m0.y;
        mac_rect f = f0;
        if (!edges) f.o.x += dx, f.o.y += dy;
        if (edges & GOOME_EDGE_LEFT) f.o.x += dx, f.s.w -= dx;
        if (edges & GOOME_EDGE_RIGHT) f.s.w += dx;
        if (edges & GOOME_EDGE_TOP) f.s.h += dy;
        if (edges & GOOME_EDGE_BOTTOM) f.o.y += dy, f.s.h -= dy;
        if (f.s.w < minw) {
            if (edges & GOOME_EDGE_LEFT) f.o.x -= minw - f.s.w;
            f.s.w = minw;
        }
        if (f.s.h < minh) {
            if (edges & GOOME_EDGE_BOTTOM) f.o.y -= minh - f.s.h;
            f.s.h = minh;
        }
        ((void (*)(mac_id, mac_sel, mac_rect, mac_bool))mac.msg)(w->win, mac_s("setFrame:display:"), f, 1);
        mac.pool_pop(pool);
        mac_nap(8);
    }
    return GOOME_OK;
}

static void mac_wsave(mac_win *w) { w->keep = mac_r(w->win, "frame"); }
static void mac_wrestore(mac_win *w) { ((void (*)(mac_id, mac_sel, mac_rect, mac_bool))mac.msg)(w->win, mac_s("setFrame:display:"), w->keep, 1); }

static int mac_wfull(mac_win *w) { return (mac_u(w->win, "styleMask") & (1ul << 14)) != 0 || w->mfs; }

static void mac_wstate(mac_win *w, int st)
{
    if (st != GOOME_STATE_MINIMIZED && mac_b(w->win, "isMiniaturized")) mac_vi(w->win, "deminiaturize:", NULL);
    if (st != GOOME_STATE_FULLSCREEN && mac_wfull(w)) {
        if (w->mfs) {
            w->mfs = 0;
            ((void (*)(mac_id, mac_sel, unsigned long))mac.msg)(mac_app(), mac_s("setPresentationOptions:"), 0ul);
            mac_wrestore(w);
        } else
            mac_vi(w->win, "toggleFullScreen:", NULL);
    }
    if (st != GOOME_STATE_MAXIMIZED && st != GOOME_STATE_MINIMIZED) {
        if (w->mzoom) w->mzoom = 0, mac_wrestore(w);
        else if (!w->popup && mac_b(w->win, "isZoomed")) mac_vi(w->win, "zoom:", NULL);
    }
    if (st == GOOME_STATE_MINIMIZED) mac_vi(w->win, "miniaturize:", NULL);
    if (st == GOOME_STATE_MAXIMIZED) {
        if (w->popup) {
            if (!w->mzoom) {
                mac_wsave(w);
                w->mzoom = 1;
                mac_rect vf = mac_r(mac_i(w->win, "screen"), "visibleFrame");
                ((void (*)(mac_id, mac_sel, mac_rect, mac_bool))mac.msg)(w->win, mac_s("setFrame:display:"), vf, 1);
            }
        } else if (!mac_b(w->win, "isZoomed"))
            mac_vi(w->win, "zoom:", NULL);
    }
    if (st == GOOME_STATE_FULLSCREEN && !mac_wfull(w)) {
        if (w->popup) {
            if (!w->mzoom) mac_wsave(w);
            w->mfs = 1;
            ((void (*)(mac_id, mac_sel, unsigned long))mac.msg)(mac_app(), mac_s("setPresentationOptions:"), (1ul << 0) | (1ul << 2));
            mac_rect sf = mac_r(mac_i(w->win, "screen"), "frame");
            ((void (*)(mac_id, mac_sel, mac_rect, mac_bool))mac.msg)(w->win, mac_s("setFrame:display:"), sf, 1);
        } else
            mac_vi(w->win, "toggleFullScreen:", NULL);
    }
}

static int mac_wwin(goome *g, int op, int a, int b, int *ra, int *rb)
{
    mac_win *w = g->ov_priv;
    int e = GOOME_OK;
    void *pool = mac.pool_push();
    switch (op) {
    case GM_WIN_TITLE:
        mac_vi(w->win, "setTitle:", mac_str(g->title));
        mac_wpublish(w);
        break;
    case GM_WIN_SIZE: {
        mac_sz s = {a, b};
        ((void (*)(mac_id, mac_sel, mac_sz))mac.msg)(w->win, mac_s("setContentSize:"), s);
        mac_wsized(w);
        break;
    }
    case GM_WIN_POS_GET: {
        mac_rect c = mac_wcontent(w);
        *ra = (int)(c.o.x + 0.5), *rb = (int)(mac_main_h() - c.o.y - c.s.h + 0.5);
        break;
    }
    case GM_WIN_POS_SET: {
        mac_rect c = mac_wcontent(w);
        c.o.x = a, c.o.y = mac_main_h() - b - c.s.h;
        mac_rect f = mac_rr(w->win, "frameRectForContentRect:", c);
        ((void (*)(mac_id, mac_sel, mac_pt))mac.msg)(w->win, mac_s("setFrameOrigin:"), f.o);
        break;
    }
    case GM_WIN_STATE: mac_wstate(w, a); break;
    case GM_WIN_GET_STATE:
        *ra = mac_b(w->win, "isMiniaturized") ? GOOME_STATE_MINIMIZED
              : mac_wfull(w)                  ? GOOME_STATE_FULLSCREEN
              : (w->popup ? w->mzoom : mac_b(w->win, "isZoomed")) ? GOOME_STATE_MAXIMIZED
                                                                 : GOOME_STATE_NORMAL;
        break;
    case GM_WIN_DRAG:
        if (!(mac_u(mac_c("NSEvent"), "pressedMouseButtons") & 1)) {
            e = gm_fail(g, GOOME_EAGAIN, "the left button isnt down anymore");
            break;
        }
        if (a && !(g->opts.window_flags & GOOME_WINDOW_RESIZABLE)) {
            e = gm_fail(g, GOOME_ENOTSUP, "this window isnt resizable");
            break;
        }
        if (!a && w->down && mac_responds(w->win, "performWindowDragWithEvent:")) mac_vi(w->win, "performWindowDragWithEvent:", w->down);
        else mac_wresize_loop(w, a);
        {
            mac_pt p = mac_wpoint(w, mac_p(w->win, "mouseLocationOutsideOfEventStream"));
            goome_event ev;
            memset(&ev, 0, sizeof(ev));
            ev.type = GOOME_UP, ev.x = p.x, ev.y = p.y, ev.button = 1;
            gm_push(g, &ev);
        }
        break;
    case GM_WIN_SHOW:
        if (a) {
            mac_vb(mac_app(), "activateIgnoringOtherApps:", 1);
            mac_vi(w->win, "makeKeyAndOrderFront:", NULL);
        } else
            mac_vi(w->win, "orderOut:", NULL);
        break;
    default: e = gm_fail(g, GOOME_ENOTSUP, "macos cant do window op %d", op);
    }
    mac.pool_pop(pool);
    return e;
}

static int mac_wcursor(goome *g, int shape)
{
    mac_win *w = g->ov_priv;
    void *pool = mac.pool_push();
    if (mac_winside(w)) mac_wcursor_apply(w);
    mac.pool_pop(pool);
    return GOOME_OK;
}

static void mac_wclose(goome *g)
{
    mac_win *w = g->ov_priv;
    if (!w) return;
    void *pool = mac.pool_push();
    if (w->hid) mac_v(mac_c("NSCursor"), "unhide");
    if (w->win) {
        mac_vi(w->win, "setDelegate:", NULL);
        mac_vi(w->win, "orderOut:", NULL);
        mac_v(w->win, "close");
        mac_v(w->win, "release");
    }
    if (w->down) mac_v(w->down, "release");
    mac.pool_pop(pool);
    for (int i = 0; i < mac_nwin; i++)
        if (mac_wins[i] == w) mac_wins[i] = mac_wins[--mac_nwin];
    if (w->rgb) mac.CGColorSpaceRelease(w->rgb);
    for (int i = 0; i < w->ndrop; i++) free(w->drops[i].path);
    free(w->drops), free(w->px), free(w->grab.cap);
    free(w);
    g->ov_priv = NULL;
    if (!mac_nwin && !mac.g) mac_hot_off();
}

static int mac_wgrab(goome *g, goome_rect r, goome_image *img)
{
    mac_win *w = g->ov_priv;
    mac_rect c = mac_wcontent(w);
    double cx = c.o.x, cy = mac_main_h() - c.o.y - c.s.h;
    uint32_t ids[GM_MAX_MON], n = 0, id = 0;
    if (mac.CGGetActiveDisplayList(GM_MAX_MON, ids, &n)) n = 0;
    double mx = cx + c.s.w / 2, my = cy + c.s.h / 2;
    for (uint32_t i = 0; i < n && !id; i++) {
        mac_rect d = mac.CGDisplayBounds(ids[i]);
        if (mx >= d.o.x && my >= d.o.y && mx < d.o.x + d.s.w && my < d.o.y + d.s.h) id = ids[i];
    }
    if (!id) return gm_fail(g, GOOME_EGONE, "the window isnt on any display");
    mac_mon *m = &w->grab;
    m->id = id, m->r = mac.CGDisplayBounds(id);
    mac_id scr = mac_screen(id);
    m->bs = scr ? mac_d(scr, "backingScaleFactor") : 1;
    if (m->bs <= 0) m->bs = 1;
    goome_rect gr = {(int)(cx + r.x + 0.5), (int)(cy + r.y + 0.5), r.w, r.h};
    int e = mac_grab(g, m, gr, img, w->rgb);
    if (e == GOOME_OK) img->area = r, img->has_self = 1;
    return e;
}

static int mac_clip_set(goome *g, const char *s)
{
    if (!mac.loaded) return gm_fail(g, GOOME_ENOTSUP, "appkit isnt loaded");
    mac_id *type = dlsym(mac.appkit, "NSPasteboardTypeString");
    if (!type || !*type) return gm_fail(g, GOOME_ENOTSUP, "no NSPasteboardTypeString here");
    void *pool = mac.pool_push();
    mac_id str = mac_str(s), pb = mac_i(mac_c("NSPasteboard"), "generalPasteboard");
    int ok = 0;
    if (str && pb) {
        mac_l(pb, "clearContents");
        ok = ((mac_bool(*)(mac_id, mac_sel, mac_id, mac_id))mac.msg)(pb, mac_s("setString:forType:"), str, *type) != 0;
    }
    mac.pool_pop(pool);
    if (!str) return gm_fail(g, GOOME_EARG, "that text isnt utf-8");
    return ok ? GOOME_OK : gm_fail(g, GOOME_EDENIED, "the pasteboard didnt take it");
}

static int mac_clip_get(goome *g, char **out)
{
    *out = NULL;
    if (!mac.loaded) return gm_fail(g, GOOME_ENOTSUP, "appkit isnt loaded");
    mac_id *type = dlsym(mac.appkit, "NSPasteboardTypeString");
    if (!type || !*type) return gm_fail(g, GOOME_ENOTSUP, "no NSPasteboardTypeString here");
    void *pool = mac.pool_push();
    mac_id pb = mac_i(mac_c("NSPasteboard"), "generalPasteboard");
    const char *s = mac_utf8(mac_ii(pb, "stringForType:", *type));
    int e = GOOME_OK;
    if (s) {
        size_t n = strlen(s);
        *out = malloc(n + 1);
        if (*out) memcpy(*out, s, n + 1);
        else e = gm_fail(g, GOOME_ENOMEM, "no memory for the clipboard");
    }
    mac.pool_pop(pool);
    return e;
}

static int mac_ov_cursor(goome *g, int shape)
{
    if (mac.hot < 0 || shape == GOOME_CURSOR_HIDDEN) return GOOME_OK;
    void *pool = mac.pool_push();
    mac_v(mac_nscursor(shape), "set");
    mac.pool_pop(pool);
    return GOOME_OK;
}

static const gm_ov mac_wov = {"macos", mac_wopen, mac_wcanvas, mac_wcommit, mac_wwait, mac_wpointer, mac_wkeyboard, mac_ov_hotkeys, mac_wclose,
                              GOOME_CAP_GLOBAL_POINTER | GOOME_CAP_KEYBOARD | GOOME_CAP_HOTKEYS | GOOME_CAP_WINDOW | GOOME_CAP_CLIPBOARD | GOOME_CAP_CURSOR,
                              mac_wwin, mac_wcursor, mac_clip_set, mac_clip_get};
static const gm_ov *mac_wtable(void) { return &mac_wov; }

#undef MAC_B
#undef MAC_RECT_ENC
#undef MAC_BLK_GLOBAL
#undef MAC_BLK_SIG

#endif
