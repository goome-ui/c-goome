#include "internal.h"

#if defined(__APPLE__) && !defined(GOOME_NO_UI)
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef void *mx_id;
typedef void *mx_sel;
#if defined(__aarch64__) || defined(__arm64__)
typedef _Bool mx_bool;
#define MX_B "B"
#else
typedef signed char mx_bool;
#define MX_B "c"
#endif
typedef struct {
    double x, y;
} mx_pt;
typedef struct {
    double w, h;
} mx_sz;
typedef struct {
    mx_pt o;
    mx_sz s;
} mx_rect;
typedef struct {
    unsigned long loc, len;
} mx_range;
#define MX_RECT "{CGRect={CGPoint=dd}{CGSize=dd}}"

typedef struct {
    goome *g;
    mx_id view;
    uint32_t *ids;
    mx_id *els;
    int n, cap;
} mx_host;

static struct {
    int tried, ok, view_done;
    mx_id (*cls)(const char *);
    mx_sel (*sel)(const char *);
    void (*msg)(void);
    void (*stret)(void);
    mx_id (*alloc_pair)(mx_id, const char *, size_t);
    void (*reg_pair)(mx_id);
    mx_bool (*add)(mx_id, mx_sel, void (*)(void), const char *);
    void (*post)(mx_id, mx_id);
    mx_id *n_focus, *n_value, *n_title, *n_gone, *n_made;
    mx_id node;
    mx_host *hosts[16];
    int nh;
} mx_lib;

static mx_sel mx_s(const char *s) { return mx_lib.sel(s); }
static mx_id mx_i(mx_id o, const char *s) { return o ? ((mx_id(*)(mx_id, mx_sel))mx_lib.msg)(o, mx_s(s)) : NULL; }
static mx_id mx_ii(mx_id o, const char *s, mx_id a) { return ((mx_id(*)(mx_id, mx_sel, mx_id))mx_lib.msg)(o, mx_s(s), a); }
static void mx_v(mx_id o, const char *s) { ((void (*)(mx_id, mx_sel))mx_lib.msg)(o, mx_s(s)); }
static int mx_b(mx_id o, const char *s) { return o && ((mx_bool(*)(mx_id, mx_sel))mx_lib.msg)(o, mx_s(s)); }
static double mx_d(mx_id o, const char *s) { return o ? ((double (*)(mx_id, mx_sel))mx_lib.msg)(o, mx_s(s)) : 0; }
static mx_id mx_str(const char *s) { return ((mx_id(*)(mx_id, mx_sel, const char *))mx_lib.msg)(mx_lib.cls("NSString"), mx_s("stringWithUTF8String:"), s ? s : ""); }
static mx_id mx_num(double v) { return ((mx_id(*)(mx_id, mx_sel, double))mx_lib.msg)(mx_lib.cls("NSNumber"), mx_s("numberWithDouble:"), v); }
static mx_id mx_int(long v) { return ((mx_id(*)(mx_id, mx_sel, long))mx_lib.msg)(mx_lib.cls("NSNumber"), mx_s("numberWithLong:"), v); }
static const char *mx_utf8(mx_id s) { return s ? ((const char *(*)(mx_id, mx_sel))mx_lib.msg)(s, mx_s("UTF8String")) : NULL; }

static mx_rect mx_r(mx_id o, const char *s)
{
#if defined(__aarch64__) || defined(__arm64__)
    return ((mx_rect(*)(mx_id, mx_sel))mx_lib.msg)(o, mx_s(s));
#else
    return ((mx_rect(*)(mx_id, mx_sel))mx_lib.stret)(o, mx_s(s));
#endif
}

static mx_rect mx_rr(mx_id o, const char *s, mx_rect a)
{
#if defined(__aarch64__) || defined(__arm64__)
    return ((mx_rect(*)(mx_id, mx_sel, mx_rect))mx_lib.msg)(o, mx_s(s), a);
#else
    return ((mx_rect(*)(mx_id, mx_sel, mx_rect))mx_lib.stret)(o, mx_s(s), a);
#endif
}

static mx_rect mx_rri(mx_id o, const char *s, mx_rect a, mx_id b)
{
#if defined(__aarch64__) || defined(__arm64__)
    return ((mx_rect(*)(mx_id, mx_sel, mx_rect, mx_id))mx_lib.msg)(o, mx_s(s), a, b);
#else
    return ((mx_rect(*)(mx_id, mx_sel, mx_rect, mx_id))mx_lib.stret)(o, mx_s(s), a, b);
#endif
}

static mx_host *mx_host_of_view(mx_id v)
{
    for (int i = 0; i < mx_lib.nh; i++)
        if (mx_lib.hosts[i]->view == v) return mx_lib.hosts[i];
    return NULL;
}

static int mx_el_at(mx_id el, mx_host **h)
{
    for (int i = 0; i < mx_lib.nh; i++)
        for (int k = 0; k < mx_lib.hosts[i]->n; k++)
            if (mx_lib.hosts[i]->els[k] == el) {
                *h = mx_lib.hosts[i];
                gm_ax *a = mx_lib.hosts[i]->g ? mx_lib.hosts[i]->g->ax : NULL;
                return a ? gm_ax_find(a, mx_lib.hosts[i]->ids[k]) : -3;
            }
    *h = NULL;
    return -3;
}

static mx_id mx_el(mx_host *h, uint32_t id, int make)
{
    for (int k = 0; k < h->n; k++)
        if (h->ids[k] == id) return h->els[k];
    if (!make) return NULL;
    if (h->n == h->cap) {
        int nc = h->cap ? h->cap * 2 : 64;
        uint32_t *ni = realloc(h->ids, sizeof(*ni) * (size_t)nc);
        if (!ni) return NULL;
        h->ids = ni;
        mx_id *ne = realloc(h->els, sizeof(*ne) * (size_t)nc);
        if (!ne) return NULL;
        h->els = ne, h->cap = nc;
    }
    mx_id e = mx_i(mx_i(mx_lib.node, "alloc"), "init");
    if (!e) return NULL;
    h->ids[h->n] = id, h->els[h->n] = e;
    h->n++;
    return e;
}

static void mx_drop(mx_host *h, uint32_t id)
{
    for (int k = 0; k < h->n; k++)
        if (h->ids[k] == id) {
            mx_id e = h->els[k];
            h->ids[k] = h->ids[h->n - 1], h->els[k] = h->els[h->n - 1];
            h->n--;
            if (mx_lib.post && mx_lib.n_gone) mx_lib.post(e, *mx_lib.n_gone);
            mx_v(e, "release");
            return;
        }
}

static mx_id mx_node_el(mx_host *h, int i)
{
    if (i < 0) return h->view;
    return mx_el(h, h->g->ax->n[i].id, 1);
}

static mx_id mx_kids(mx_host *h, int parent)
{
    mx_id arr = mx_i(mx_lib.cls("NSMutableArray"), "array");
    gm_ax *a = h && h->g ? h->g->ax : NULL;
    if (!a) return arr;
    for (int i = 0; i < a->count; i++)
        if (a->n[i].parent == parent) {
            mx_id e = mx_node_el(h, i);
            if (e) mx_ii(arr, "addObject:", e);
        }
    return arr;
}

static mx_rect mx_frame(mx_host *h, const goome_a11y_node *n)
{
    mx_rect b = mx_r(h->view, "bounds"), r;
    r.o.x = n->x, r.s.w = n->w, r.s.h = n->h;
    r.o.y = mx_b(h->view, "isFlipped") ? n->y : b.s.h - n->y - n->h;
    r = mx_rri(h->view, "convertRect:toView:", r, NULL);
    mx_id w = mx_i(h->view, "window");
    return w ? mx_rr(w, "convertRectToScreen:", r) : r;
}

static const char *mx_role_of(const gm_ax *a, int i)
{
    const goome_a11y_node *n = &a->n[i];
    int k = a->x[i].kind, p = n->parent;
    switch (n->role) {
    case GOOME_ROLE_BUTTON: return "AXButton";
    case GOOME_ROLE_CHECKBOX: return "AXCheckBox";
    case GOOME_ROLE_RADIO: return "AXRadioButton";
    case GOOME_ROLE_SLIDER: return "AXSlider";
    case GOOME_ROLE_TEXT: return "AXStaticText";
    case GOOME_ROLE_INPUT: return (k & GM_AXK_MULTILINE) ? "AXTextArea" : "AXTextField";
    case GOOME_ROLE_COMBO: return "AXPopUpButton";
    case GOOME_ROLE_LIST: return "AXList";
    case GOOME_ROLE_ITEM: return p >= 0 && a->n[p].role == GOOME_ROLE_LIST ? "AXMenuItem" : "AXStaticText";
    case GOOME_ROLE_TAB: return "AXRadioButton";
    case GOOME_ROLE_MENU: return p >= 0 && a->n[p].role == GOOME_ROLE_GROUP ? "AXMenuBarItem" : "AXMenuItem";
    case GOOME_ROLE_MENUITEM: return "AXMenuItem";
    case GOOME_ROLE_TREE: return "AXDisclosureTriangle";
    case GOOME_ROLE_PROGRESS: return n->num < 0 ? "AXBusyIndicator" : "AXProgressIndicator";
    case GOOME_ROLE_IMAGE: return "AXImage";
    case GOOME_ROLE_GROUP:
        for (int j = i + 1; j < a->count; j++)
            if (a->n[j].parent == i) return a->n[j].role == GOOME_ROLE_TAB ? "AXTabGroup" : a->n[j].role == GOOME_ROLE_MENU ? "AXMenuBar" : "AXGroup";
        return "AXGroup";
    }
    return "AXGroup";
}

static int mx_titled(int role) { return role == GOOME_ROLE_BUTTON || role == GOOME_ROLE_CHECKBOX || role == GOOME_ROLE_RADIO || role == GOOME_ROLE_TAB || role == GOOME_ROLE_MENU || role == GOOME_ROLE_MENUITEM || role == GOOME_ROLE_TREE || role == GOOME_ROLE_ITEM; }

#define MX_NODE(self)                    \
    mx_host *h;                          \
    int i = mx_el_at((self), &h);        \
    gm_ax *a = i >= 0 ? h->g->ax : NULL; \
    (void)a

static mx_id mx_n_role(mx_id self, mx_sel s)
{
    MX_NODE(self);
    return mx_str(i >= 0 ? mx_role_of(a, i) : "AXUnknown");
}

static mx_id mx_n_subrole(mx_id self, mx_sel s)
{
    MX_NODE(self);
    if (i >= 0 && (a->x[i].kind & GM_AXK_PASSWORD)) return mx_str("AXSecureTextField");
    if (i >= 0 && a->n[i].role == GOOME_ROLE_DIALOG) return mx_str("AXDialog");
    return NULL;
}

static mx_id mx_n_label(mx_id self, mx_sel s)
{
    MX_NODE(self);
    return i >= 0 && !mx_titled(a->n[i].role) && a->n[i].role != GOOME_ROLE_TEXT ? mx_str(a->n[i].label) : NULL;
}

static mx_id mx_n_title(mx_id self, mx_sel s)
{
    MX_NODE(self);
    return i >= 0 && mx_titled(a->n[i].role) ? mx_str(a->n[i].label) : NULL;
}

static mx_id mx_n_value(mx_id self, mx_sel s)
{
    MX_NODE(self);
    if (i < 0) return NULL;
    const goome_a11y_node *n = &a->n[i];
    int len;
    switch (n->role) {
    case GOOME_ROLE_CHECKBOX:
    case GOOME_ROLE_RADIO: return mx_int((n->states & GOOME_A11Y_CHECKED) != 0);
    case GOOME_ROLE_TAB: return mx_int((n->states & GOOME_A11Y_SELECTED) != 0);
    case GOOME_ROLE_TREE: return mx_int((n->states & GOOME_A11Y_EXPANDED) != 0);
    case GOOME_ROLE_TEXT: return mx_str(n->label);
    case GOOME_ROLE_SLIDER:
    case GOOME_ROLE_PROGRESS: return mx_num(n->num);
    }
    if (a->x[i].kind & GM_AXK_RANGE) return mx_num(n->num);
    const char *t = gm_ax_str(a, i, &len);
    if (!len) return n->role == GOOME_ROLE_INPUT ? mx_str("") : NULL;
    char *c = malloc((size_t)len + 1);
    if (!c) return NULL;
    memcpy(c, t, (size_t)len);
    c[len] = 0;
    mx_id r = mx_str(c);
    free(c);
    return r;
}

static mx_id mx_n_min(mx_id self, mx_sel s)
{
    MX_NODE(self);
    return i >= 0 && (a->x[i].kind & GM_AXK_RANGE) ? mx_num(a->n[i].min) : NULL;
}

static mx_id mx_n_max(mx_id self, mx_sel s)
{
    MX_NODE(self);
    return i >= 0 && (a->x[i].kind & GM_AXK_RANGE) ? mx_num(a->n[i].max) : NULL;
}

static mx_rect mx_n_frame(mx_id self, mx_sel s)
{
    MX_NODE(self);
    mx_rect z = {{0, 0}, {0, 0}};
    return i >= 0 ? mx_frame(h, &a->n[i]) : z;
}

static mx_id mx_n_parent(mx_id self, mx_sel s)
{
    MX_NODE(self);
    if (i < 0) return NULL;
    return mx_node_el(h, a->n[i].parent);
}

static mx_id mx_n_kids(mx_id self, mx_sel s)
{
    MX_NODE(self);
    return i >= 0 ? mx_kids(h, i) : mx_i(mx_lib.cls("NSArray"), "array");
}

static mx_id mx_n_window(mx_id self, mx_sel s)
{
    MX_NODE(self);
    return h ? mx_i(h->view, "window") : NULL;
}

static mx_id mx_n_ident(mx_id self, mx_sel s)
{
    MX_NODE(self);
    char t[16];
    if (i < 0) return NULL;
    snprintf(t, sizeof(t), "%u", (unsigned)a->n[i].id);
    return mx_str(t);
}

static mx_bool mx_yes(mx_id self, mx_sel s) { return 1; }

static mx_bool mx_n_state(mx_id self, mx_sel s, int bit)
{
    MX_NODE(self);
    return i >= 0 && (a->n[i].states & bit) != 0;
}

static mx_bool mx_n_focused(mx_id self, mx_sel s) { return mx_n_state(self, s, GOOME_A11Y_FOCUSED); }
static mx_bool mx_n_selected(mx_id self, mx_sel s) { return mx_n_state(self, s, GOOME_A11Y_SELECTED); }
static mx_bool mx_n_expanded(mx_id self, mx_sel s) { return mx_n_state(self, s, GOOME_A11Y_EXPANDED); }
static mx_bool mx_n_enabled(mx_id self, mx_sel s) { return !mx_n_state(self, s, GOOME_A11Y_DISABLED); }

static mx_bool mx_n_press(mx_id self, mx_sel s)
{
    MX_NODE(self);
    if (i < 0 || !(a->x[i].kind & (GM_AXK_PRESS | GM_AXK_EXPANDABLE))) return 0;
    if (a->x[i].kind & GM_AXK_PRESS) gm_ax_act(h->g, a->n[i].id, "press", NULL);
    else gm_ax_act(h->g, a->n[i].id, (a->n[i].states & GOOME_A11Y_EXPANDED) ? "collapse" : "expand", NULL);
    return 1;
}

static mx_bool mx_n_step(mx_id self, int dir)
{
    MX_NODE(self);
    if (i < 0 || !(a->x[i].kind & GM_AXK_RANGE) || !(a->x[i].kind & GM_AXK_EDITABLE)) return 0;
    const goome_a11y_node *n = &a->n[i];
    double span = n->max - n->min, st = span > 0 ? (span >= 20 ? span / 20 : span >= 2 ? 1 : span / 20) : 1;
    char t[40];
    snprintf(t, sizeof(t), "%.17g", n->num + dir * st);
    gm_ax_act(h->g, n->id, "set", t);
    return 1;
}

static mx_bool mx_n_inc(mx_id self, mx_sel s) { return mx_n_step(self, 1); }
static mx_bool mx_n_dec(mx_id self, mx_sel s) { return mx_n_step(self, -1); }

static void mx_n_set_value(mx_id self, mx_sel s, mx_id v)
{
    MX_NODE(self);
    if (i < 0 || !(a->x[i].kind & GM_AXK_EDITABLE) || !v) return;
    char t[40];
    const char *txt;
    if (((mx_bool(*)(mx_id, mx_sel, mx_id))mx_lib.msg)(v, mx_s("isKindOfClass:"), mx_lib.cls("NSNumber"))) {
        snprintf(t, sizeof(t), "%.17g", mx_d(v, "doubleValue"));
        txt = t;
    } else
        txt = mx_utf8(mx_i(v, "description"));
    gm_ax_act(h->g, a->n[i].id, "set", txt ? txt : "");
}

static void mx_n_set_focus(mx_id self, mx_sel s, mx_bool on)
{
    MX_NODE(self);
    if (i >= 0 && on && (a->x[i].kind & GM_AXK_FOCUSABLE)) gm_ax_act(h->g, a->n[i].id, "focus", NULL);
}

static long mx_n_chars(mx_id self, mx_sel s)
{
    MX_NODE(self);
    int len;
    if (i < 0) return 0;
    const char *t = gm_ax_str(a, i, &len);
    return gm_ax_chars(t, len);
}

static mx_range mx_n_sel_range(mx_id self, mx_sel s)
{
    MX_NODE(self);
    mx_range r = {0, 0};
    if (i >= 0) r.loc = (unsigned long)a->n[i].caret;
    return r;
}

static mx_bool mx_n_allowed(mx_id self, mx_sel s, mx_sel q)
{
    MX_NODE(self);
    if (i < 0) return 1;
    int k = a->x[i].kind, r = a->n[i].role;
    if (q == mx_s("accessibilityPerformPress")) return (k & (GM_AXK_PRESS | GM_AXK_EXPANDABLE)) != 0;
    if (q == mx_s("accessibilityPerformIncrement") || q == mx_s("accessibilityPerformDecrement")) return (k & GM_AXK_RANGE) && (k & GM_AXK_EDITABLE);
    if (q == mx_s("setAccessibilityValue:")) return (k & GM_AXK_EDITABLE) != 0;
    if (q == mx_s("accessibilityMinValue") || q == mx_s("accessibilityMaxValue")) return (k & GM_AXK_RANGE) != 0;
    if (q == mx_s("accessibilityNumberOfCharacters") || q == mx_s("accessibilitySelectedTextRange")) return r == GOOME_ROLE_INPUT;
    if (q == mx_s("setAccessibilityFocused:")) return (k & GM_AXK_FOCUSABLE) != 0;
    if (q == mx_s("isAccessibilityExpanded")) return (k & GM_AXK_EXPANDABLE) != 0;
    return 1;
}

static mx_id mx_v_kids(mx_id self, mx_sel s) { return mx_kids(mx_host_of_view(self), -1); }
static mx_id mx_v_role(mx_id self, mx_sel s) { return mx_str("AXGroup"); }

static mx_id mx_v_label(mx_id self, mx_sel s)
{
    mx_host *h = mx_host_of_view(self);
    return h && h->g ? mx_str(h->g->title) : NULL;
}

static mx_id mx_v_hit(mx_id self, mx_sel s, mx_pt p)
{
    mx_host *h = mx_host_of_view(self);
    gm_ax *a = h && h->g ? h->g->ax : NULL;
    if (!a) return self;
    int best = -1;
    for (int i = 0; i < a->count; i++) {
        if (a->x[i].kind & GM_AXK_HIDDEN) continue;
        mx_rect r = mx_frame(h, &a->n[i]);
        if (p.x >= r.o.x && p.y >= r.o.y && p.x < r.o.x + r.s.w && p.y < r.o.y + r.s.h) best = i;
    }
    return best >= 0 ? mx_node_el(h, best) : self;
}

static mx_id mx_v_focus(mx_id self, mx_sel s)
{
    mx_host *h = mx_host_of_view(self);
    gm_ax *a = h && h->g ? h->g->ax : NULL;
    for (int i = 0; a && i < a->count; i++)
        if (a->n[i].states & GOOME_A11Y_FOCUSED) return mx_node_el(h, i);
    return self;
}

static void mx_m(mx_id c, const char *sel, void (*f)(void), const char *types) { mx_lib.add(c, mx_s(sel), f, types); }

static int mx_load(void)
{
    if (mx_lib.tried) return mx_lib.ok;
    mx_lib.tried = 1;
    void *objc = dlopen("/usr/lib/libobjc.A.dylib", RTLD_NOW | RTLD_GLOBAL);
    void *ak = dlopen("/System/Library/Frameworks/AppKit.framework/AppKit", RTLD_NOW | RTLD_GLOBAL);
    if (!objc || !ak) return 0;
    void *p;
#define MX_SYM(lib, name, f)                \
    do {                                    \
        p = dlsym(lib, name);               \
        memcpy(&mx_lib.f, &p, sizeof(p));   \
        if (!p) ok = 0;                     \
    } while (0)
    int ok = 1;
    MX_SYM(objc, "objc_getClass", cls);
    MX_SYM(objc, "sel_registerName", sel);
    MX_SYM(objc, "objc_msgSend", msg);
    MX_SYM(objc, "objc_allocateClassPair", alloc_pair);
    MX_SYM(objc, "objc_registerClassPair", reg_pair);
    MX_SYM(objc, "class_addMethod", add);
    MX_SYM(ak, "NSAccessibilityPostNotification", post);
#if !defined(__aarch64__) && !defined(__arm64__)
    MX_SYM(objc, "objc_msgSend_stret", stret);
#endif
#undef MX_SYM
    mx_lib.n_focus = dlsym(ak, "NSAccessibilityFocusedUIElementChangedNotification");
    mx_lib.n_value = dlsym(ak, "NSAccessibilityValueChangedNotification");
    mx_lib.n_title = dlsym(ak, "NSAccessibilityTitleChangedNotification");
    mx_lib.n_gone = dlsym(ak, "NSAccessibilityUIElementDestroyedNotification");
    mx_lib.n_made = dlsym(ak, "NSAccessibilityCreatedNotification");
    if (!ok || !mx_lib.cls("NSAccessibilityElement")) return 0;
    mx_id c = mx_lib.cls("GoomeAxNode");
    if (!c) {
        c = mx_lib.alloc_pair(mx_lib.cls("NSAccessibilityElement"), "GoomeAxNode", 0);
        if (!c) return 0;
        mx_m(c, "accessibilityRole", (void (*)(void))mx_n_role, "@@:");
        mx_m(c, "accessibilitySubrole", (void (*)(void))mx_n_subrole, "@@:");
        mx_m(c, "accessibilityLabel", (void (*)(void))mx_n_label, "@@:");
        mx_m(c, "accessibilityTitle", (void (*)(void))mx_n_title, "@@:");
        mx_m(c, "accessibilityValue", (void (*)(void))mx_n_value, "@@:");
        mx_m(c, "accessibilityMinValue", (void (*)(void))mx_n_min, "@@:");
        mx_m(c, "accessibilityMaxValue", (void (*)(void))mx_n_max, "@@:");
        mx_m(c, "accessibilityFrame", (void (*)(void))mx_n_frame, MX_RECT "@:");
        mx_m(c, "accessibilityParent", (void (*)(void))mx_n_parent, "@@:");
        mx_m(c, "accessibilityChildren", (void (*)(void))mx_n_kids, "@@:");
        mx_m(c, "accessibilityWindow", (void (*)(void))mx_n_window, "@@:");
        mx_m(c, "accessibilityTopLevelUIElement", (void (*)(void))mx_n_window, "@@:");
        mx_m(c, "accessibilityIdentifier", (void (*)(void))mx_n_ident, "@@:");
        mx_m(c, "isAccessibilityElement", (void (*)(void))mx_yes, MX_B "@:");
        mx_m(c, "isAccessibilityFocused", (void (*)(void))mx_n_focused, MX_B "@:");
        mx_m(c, "isAccessibilitySelected", (void (*)(void))mx_n_selected, MX_B "@:");
        mx_m(c, "isAccessibilityExpanded", (void (*)(void))mx_n_expanded, MX_B "@:");
        mx_m(c, "isAccessibilityEnabled", (void (*)(void))mx_n_enabled, MX_B "@:");
        mx_m(c, "accessibilityPerformPress", (void (*)(void))mx_n_press, MX_B "@:");
        mx_m(c, "accessibilityPerformIncrement", (void (*)(void))mx_n_inc, MX_B "@:");
        mx_m(c, "accessibilityPerformDecrement", (void (*)(void))mx_n_dec, MX_B "@:");
        mx_m(c, "setAccessibilityValue:", (void (*)(void))mx_n_set_value, "v@:@");
        mx_m(c, "setAccessibilityFocused:", (void (*)(void))mx_n_set_focus, "v@:" MX_B);
        mx_m(c, "accessibilityNumberOfCharacters", (void (*)(void))mx_n_chars, "q@:");
        mx_m(c, "accessibilitySelectedTextRange", (void (*)(void))mx_n_sel_range, "{_NSRange=QQ}@:");
        mx_m(c, "isAccessibilitySelectorAllowed:", (void (*)(void))mx_n_allowed, MX_B "@::");
        mx_lib.reg_pair(c);
    }
    mx_lib.node = c;
    mx_lib.ok = 1;
    return 1;
}

int gm_axp_open(goome *g)
{
    mx_id view = gm_mac_view(g);
    if (!view || !mx_load() || mx_lib.nh >= 16) return -1;
    if (!mx_lib.view_done) {
        mx_id c = mx_lib.cls("GoomeAppView");
        if (!c) return -1;
        mx_m(c, "accessibilityChildren", (void (*)(void))mx_v_kids, "@@:");
        mx_m(c, "isAccessibilityElement", (void (*)(void))mx_yes, MX_B "@:");
        mx_m(c, "accessibilityRole", (void (*)(void))mx_v_role, "@@:");
        mx_m(c, "accessibilityLabel", (void (*)(void))mx_v_label, "@@:");
        mx_m(c, "accessibilityHitTest:", (void (*)(void))mx_v_hit, "@@:{CGPoint=dd}");
        mx_m(c, "accessibilityFocusedUIElement", (void (*)(void))mx_v_focus, "@@:");
        mx_lib.view_done = 1;
    }
    mx_host *h = calloc(1, sizeof(*h));
    if (!h) return -1;
    h->g = g, h->view = view;
    mx_lib.hosts[mx_lib.nh++] = h;
    g->ax->plat = h;
    return 0;
}

static void mx_post(mx_id e, mx_id *name)
{
    if (e && name && *name) mx_lib.post(e, *name);
}

void gm_axp_event(goome *g, int ev, int i, int old)
{
    mx_host *h = g->ax ? g->ax->plat : NULL;
    if (!h) return;
    gm_ax *a = g->ax;
    switch (ev) {
    case GM_AXE_FOCUS: mx_post(mx_node_el(h, i), mx_lib.n_focus); break;
    case GM_AXE_VALUE:
    case GM_AXE_STATE: mx_post(mx_el(h, a->n[i].id, 0), mx_lib.n_value); break;
    case GM_AXE_NAME: mx_post(mx_el(h, a->n[i].id, 0), mx_lib.n_title); break;
    case GM_AXE_ADD: mx_post(mx_node_el(h, i), mx_lib.n_made); break;
    case GM_AXE_REMOVE: mx_drop(h, a->pn[i].id); break;
    }
}

int gm_axp_pump(goome *g) { return -1; }

void gm_axp_close(goome *g)
{
    mx_host *h = g->ax->plat;
    if (!h) return;
    for (int k = 0; k < h->n; k++) mx_v(h->els[k], "release");
    free(h->ids), free(h->els);
    for (int i = 0; i < mx_lib.nh; i++)
        if (mx_lib.hosts[i] == h) mx_lib.hosts[i] = mx_lib.hosts[--mx_lib.nh];
    free(h);
    g->ax->plat = NULL;
}

#undef MX_B
#undef MX_RECT
#undef MX_NODE
#endif
