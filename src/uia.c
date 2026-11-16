#include "internal.h"

#if defined(_WIN32) && !defined(GOOME_NO_UI)
#include <windows.h>
#include <ole2.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#define UA_GONE ((HRESULT)0x80040201L)

typedef struct {
    double left, top, width, height;
} ua_rect;

typedef struct {
    LONG ref;
    goome *g;
    HWND hwnd;
    int serial;
} ua_host;

typedef struct ua_el ua_el;

typedef struct {
    HRESULT(STDMETHODCALLTYPE *qi)(void *, REFIID, void **);
    ULONG(STDMETHODCALLTYPE *add)(void *);
    ULONG(STDMETHODCALLTYPE *rel)(void *);
    void *m[8];
} ua_vt;

struct ua_el {
    const void *simple, *frag, *root, *inv, *tog, *val, *rng, *sel, *seli, *exp;
    LONG ref;
    ua_host *host;
    uint32_t id;
};

static struct {
    int tried, ok;
    HMODULE core, ole;
    LRESULT(WINAPI *ret)(HWND, WPARAM, LPARAM, void *);
    HRESULT(WINAPI *host)(HWND, void **);
    HRESULT(WINAPI *raise)(void *, int);
    HRESULT(WINAPI *prop)(void *, int, VARIANT, VARIANT);
    HRESULT(WINAPI *structure)(void *, int, int *, int);
    BOOL(WINAPI *listening)(void);
    HRESULT(WINAPI *disconnect)(void *);
    BSTR(WINAPI *bstr)(const OLECHAR *);
    SAFEARRAY *(WINAPI *arr)(VARTYPE, LONG, ULONG);
    HRESULT(WINAPI *put)(SAFEARRAY *, LONG *, void *);
} ua_lib;

static const GUID ua_iid_unknown = {0x00000000, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
static const GUID ua_iid_simple = {0xd6dd68d1, 0x86fd, 0x4332, {0x86, 0x66, 0x9a, 0xbe, 0xde, 0xa2, 0xd2, 0x4c}};
static const GUID ua_iid_frag = {0xf7063da8, 0x8359, 0x439c, {0x92, 0x97, 0xbb, 0xc5, 0x29, 0x9a, 0x7d, 0x87}};
static const GUID ua_iid_root = {0x620ce2a5, 0xab8f, 0x40a9, {0x86, 0xcb, 0xde, 0x3c, 0x75, 0x59, 0x9b, 0x58}};
static const GUID ua_iid_inv = {0x54fcb24b, 0xe18e, 0x47a2, {0xb4, 0xd3, 0xec, 0xcb, 0xe7, 0x75, 0x99, 0xa2}};
static const GUID ua_iid_tog = {0x56d00bd0, 0xc4f4, 0x433c, {0xa8, 0x36, 0x1a, 0x52, 0xa5, 0x7e, 0x08, 0x92}};
static const GUID ua_iid_val = {0xc7935180, 0x6fb3, 0x4201, {0xb1, 0x74, 0x7d, 0xf7, 0x3a, 0xdb, 0xf6, 0x4a}};
static const GUID ua_iid_rng = {0x36dc7aef, 0x33e6, 0x4691, {0xaf, 0xe1, 0x2b, 0xe7, 0x27, 0x4b, 0x3d, 0x33}};
static const GUID ua_iid_sel = {0xfb8b03af, 0x3bdf, 0x48d4, {0xbd, 0x36, 0x1a, 0x65, 0x79, 0x3b, 0xe1, 0x68}};
static const GUID ua_iid_seli = {0x2acad808, 0xb2d4, 0x452d, {0xa4, 0x07, 0x91, 0xff, 0x1a, 0xd1, 0x67, 0xb2}};
static const GUID ua_iid_exp = {0xd847d3a5, 0xcab0, 0x4a98, {0x8c, 0x32, 0xec, 0xb4, 0x5c, 0x59, 0xad, 0x24}};

enum { UA_INVOKE = 10000, UA_SELECTION = 10001, UA_VALUE = 10002, UA_RANGE = 10003, UA_EXPAND = 10005, UA_SELITEM = 10010, UA_TOGGLE = 10015 };

static int ua_load(void)
{
    if (ua_lib.tried) return ua_lib.ok;
    ua_lib.tried = 1;
    ua_lib.core = LoadLibraryA("uiautomationcore.dll");
    ua_lib.ole = LoadLibraryA("oleaut32.dll");
    if (!ua_lib.core || !ua_lib.ole) return 0;
    ua_lib.ret = (LRESULT(WINAPI *)(HWND, WPARAM, LPARAM, void *))(void (*)(void))GetProcAddress(ua_lib.core, "UiaReturnRawElementProvider");
    ua_lib.host = (HRESULT(WINAPI *)(HWND, void **))(void (*)(void))GetProcAddress(ua_lib.core, "UiaHostProviderFromHwnd");
    ua_lib.raise = (HRESULT(WINAPI *)(void *, int))(void (*)(void))GetProcAddress(ua_lib.core, "UiaRaiseAutomationEvent");
    ua_lib.prop = (HRESULT(WINAPI *)(void *, int, VARIANT, VARIANT))(void (*)(void))GetProcAddress(ua_lib.core, "UiaRaiseAutomationPropertyChangedEvent");
    ua_lib.structure = (HRESULT(WINAPI *)(void *, int, int *, int))(void (*)(void))GetProcAddress(ua_lib.core, "UiaRaiseStructureChangedEvent");
    ua_lib.listening = (BOOL(WINAPI *)(void))(void (*)(void))GetProcAddress(ua_lib.core, "UiaClientsAreListening");
    ua_lib.disconnect = (HRESULT(WINAPI *)(void *))(void (*)(void))GetProcAddress(ua_lib.core, "UiaDisconnectProvider");
    ua_lib.bstr = (BSTR(WINAPI *)(const OLECHAR *))(void (*)(void))GetProcAddress(ua_lib.ole, "SysAllocString");
    ua_lib.arr = (SAFEARRAY * (WINAPI *)(VARTYPE, LONG, ULONG))(void (*)(void))GetProcAddress(ua_lib.ole, "SafeArrayCreateVector");
    ua_lib.put = (HRESULT(WINAPI *)(SAFEARRAY *, LONG *, void *))(void (*)(void))GetProcAddress(ua_lib.ole, "SafeArrayPutElement");
    ua_lib.ok = ua_lib.ret && ua_lib.host && ua_lib.raise && ua_lib.prop && ua_lib.bstr && ua_lib.arr && ua_lib.put;
    return ua_lib.ok;
}

static ua_el *ua_of(void *p, size_t off) { return (ua_el *)(void *)((char *)p - off); }
#define UA_EL(p, f) ua_of((p), offsetof(ua_el, f))

static gm_ax *ua_ax(ua_el *e) { return e->host->g ? e->host->g->ax : NULL; }

static int ua_idx(ua_el *e)
{
    gm_ax *a = ua_ax(e);
    if (!a) return -3;
    if (e->id == 1) return -1;
    return gm_ax_find(a, e->id) >= 0 ? gm_ax_find(a, e->id) : -3;
}

static ua_el *ua_new(ua_host *h, uint32_t id);

static BSTR ua_bstr(const char *s, int n)
{
    WCHAR tmp[512], *w = tmp;
    if (n < 0) n = (int)strlen(s);
    int k = MultiByteToWideChar(CP_UTF8, 0, s, n, NULL, 0);
    if (k + 1 > 512) w = malloc(sizeof(WCHAR) * (size_t)(k + 1));
    if (!w) return ua_lib.bstr(L"");
    MultiByteToWideChar(CP_UTF8, 0, s, n, w, k);
    w[k] = 0;
    BSTR b = ua_lib.bstr(w);
    if (w != tmp) free(w);
    return b;
}

static char *ua_utf8(const WCHAR *w)
{
    int k = WideCharToMultiByte(CP_UTF8, 0, w ? w : L"", -1, NULL, 0, NULL, NULL);
    char *s = malloc((size_t)(k > 0 ? k : 1));
    if (!s) return NULL;
    s[0] = 0;
    if (k > 0) WideCharToMultiByte(CP_UTF8, 0, w ? w : L"", -1, s, k, NULL, NULL);
    return s;
}

static int ua_has(const gm_ax *a, int i, int pat)
{
    if (i < 0) return 0;
    int k = a->x[i].kind, r = a->n[i].role;
    switch (pat) {
    case UA_INVOKE: return (k & GM_AXK_PRESS) && r != GOOME_ROLE_CHECKBOX && r != GOOME_ROLE_RADIO && r != GOOME_ROLE_ITEM && r != GOOME_ROLE_TAB && !(k & GM_AXK_EXPANDABLE);
    case UA_TOGGLE: return r == GOOME_ROLE_CHECKBOX || (r == GOOME_ROLE_MENUITEM && (k & GM_AXK_CHECKABLE));
    case UA_SELITEM: return r == GOOME_ROLE_RADIO || r == GOOME_ROLE_ITEM || r == GOOME_ROLE_TAB;
    case UA_VALUE: return r == GOOME_ROLE_INPUT || r == GOOME_ROLE_COMBO || (r == GOOME_ROLE_BUTTON && (k & GM_AXK_EDITABLE));
    case UA_RANGE: return (k & GM_AXK_RANGE) != 0;
    case UA_EXPAND: return (k & GM_AXK_EXPANDABLE) != 0;
    case UA_SELECTION: {
        for (int j = i + 1; j < a->count; j++)
            if (a->n[j].parent == i && (a->n[j].role == GOOME_ROLE_ITEM || a->n[j].role == GOOME_ROLE_TAB)) return 1;
        return 0;
    }
    }
    return 0;
}

static int ua_type(const gm_ax *a, int i)
{
    switch (a->n[i].role) {
    case GOOME_ROLE_BUTTON: return 50000;
    case GOOME_ROLE_CHECKBOX: return 50002;
    case GOOME_ROLE_RADIO: return 50013;
    case GOOME_ROLE_SLIDER: return 50015;
    case GOOME_ROLE_TEXT: return 50020;
    case GOOME_ROLE_INPUT: return 50004;
    case GOOME_ROLE_COMBO: return 50003;
    case GOOME_ROLE_LIST: return 50008;
    case GOOME_ROLE_ITEM: return 50007;
    case GOOME_ROLE_TAB: return 50019;
    case GOOME_ROLE_MENU: return 50011;
    case GOOME_ROLE_MENUITEM: return 50011;
    case GOOME_ROLE_TABLE: return 50036;
    case GOOME_ROLE_TREE: return 50024;
    case GOOME_ROLE_PROGRESS: return 50012;
    case GOOME_ROLE_IMAGE: return 50006;
    case GOOME_ROLE_DIALOG: return 50032;
    case GOOME_ROLE_GROUP:
        for (int j = i + 1; j < a->count; j++)
            if (a->n[j].parent == i) return a->n[j].role == GOOME_ROLE_TAB ? 50018 : a->n[j].role == GOOME_ROLE_MENU ? 50010 : 50026;
        return 50026;
    }
    return 50033;
}

static void ua_screen(ua_el *e, const goome_a11y_node *n, ua_rect *r)
{
    goome *g = e->host->g;
    POINT p = {0, 0};
    ClientToScreen(e->host->hwnd, &p);
    double s = g->nmon > 0 && g->mon[0].scale > 0 ? g->mon[0].scale : 1;
    r->left = p.x + n->x * s, r->top = p.y + n->y * s, r->width = n->w * s, r->height = n->h * s;
}

static HRESULT ua_give(ua_el *e, void **out, const void *field)
{
    *out = (void *)field;
    InterlockedIncrement(&e->ref);
    return S_OK;
}

static HRESULT ua_qi_el(ua_el *e, REFIID r, void **out)
{
    int i = ua_idx(e);
    gm_ax *a = ua_ax(e);
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID(r, &ua_iid_unknown) || IsEqualGUID(r, &ua_iid_simple)) return ua_give(e, out, &e->simple);
    if (IsEqualGUID(r, &ua_iid_frag)) return ua_give(e, out, &e->frag);
    if (IsEqualGUID(r, &ua_iid_root) && e->id == 1) return ua_give(e, out, &e->root);
    if (!a || i < 0) return E_NOINTERFACE;
    if (IsEqualGUID(r, &ua_iid_inv) && ua_has(a, i, UA_INVOKE)) return ua_give(e, out, &e->inv);
    if (IsEqualGUID(r, &ua_iid_tog) && ua_has(a, i, UA_TOGGLE)) return ua_give(e, out, &e->tog);
    if (IsEqualGUID(r, &ua_iid_val) && ua_has(a, i, UA_VALUE)) return ua_give(e, out, &e->val);
    if (IsEqualGUID(r, &ua_iid_rng) && ua_has(a, i, UA_RANGE)) return ua_give(e, out, &e->rng);
    if (IsEqualGUID(r, &ua_iid_sel) && ua_has(a, i, UA_SELECTION)) return ua_give(e, out, &e->sel);
    if (IsEqualGUID(r, &ua_iid_seli) && ua_has(a, i, UA_SELITEM)) return ua_give(e, out, &e->seli);
    if (IsEqualGUID(r, &ua_iid_exp) && ua_has(a, i, UA_EXPAND)) return ua_give(e, out, &e->exp);
    return E_NOINTERFACE;
}

static ULONG ua_add_el(ua_el *e) { return (ULONG)InterlockedIncrement(&e->ref); }

static ULONG ua_rel_el(ua_el *e)
{
    LONG r = InterlockedDecrement(&e->ref);
    if (r == 0) {
        if (InterlockedDecrement(&e->host->ref) == 0) free(e->host);
        free(e);
    }
    return (ULONG)r;
}

#define UA_IUNK(name, field)                                                                                                   \
    static HRESULT STDMETHODCALLTYPE name##_qi(void *t, REFIID r, void **o) { return ua_qi_el(UA_EL(t, field), r, o); } \
    static ULONG STDMETHODCALLTYPE name##_add(void *t) { return ua_add_el(UA_EL(t, field)); }                           \
    static ULONG STDMETHODCALLTYPE name##_rel(void *t) { return ua_rel_el(UA_EL(t, field)); }

UA_IUNK(ua_s, simple)
UA_IUNK(ua_f, frag)
UA_IUNK(ua_r, root)
UA_IUNK(ua_i, inv)
UA_IUNK(ua_t, tog)
UA_IUNK(ua_v, val)
UA_IUNK(ua_g, rng)
UA_IUNK(ua_l, sel)
UA_IUNK(ua_e, seli)
UA_IUNK(ua_x, exp)

static void ua_act(ua_el *e, const char *verb, const char *text)
{
    if (e->host->g) gm_ax_act(e->host->g, e->id, verb, text);
}

static HRESULT STDMETHODCALLTYPE ua_s_opts(void *t, int *o)
{
    *o = 2;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE ua_s_pattern(void *t, int pat, IUnknown **o)
{
    ua_el *e = UA_EL(t, simple);
    *o = NULL;
    int i = ua_idx(e);
    gm_ax *a = ua_ax(e);
    if (i == -3) return UA_GONE;
    if (!ua_has(a, i, pat)) return S_OK;
    const void *f = pat == UA_INVOKE ? (const void *)&e->inv : pat == UA_TOGGLE ? (const void *)&e->tog : pat == UA_VALUE ? (const void *)&e->val : pat == UA_RANGE ? (const void *)&e->rng
                  : pat == UA_SELECTION ? (const void *)&e->sel : pat == UA_SELITEM ? (const void *)&e->seli : (const void *)&e->exp;
    return ua_give(e, (void **)o, f);
}

static void ua_vbool(VARIANT *v, int on)
{
    V_VT(v) = VT_BOOL;
    V_BOOL(v) = on ? VARIANT_TRUE : VARIANT_FALSE;
}

static HRESULT STDMETHODCALLTYPE ua_s_prop(void *t, int prop, VARIANT *v)
{
    ua_el *e = UA_EL(t, simple);
    V_VT(v) = VT_EMPTY;
    int i = ua_idx(e);
    gm_ax *a = ua_ax(e);
    if (i == -3) return UA_GONE;
    if (i == -1) {
        if (prop == 30024) V_VT(v) = VT_BSTR, V_BSTR(v) = ua_bstr("goome", -1);
        return S_OK;
    }
    const goome_a11y_node *n = &a->n[i];
    int k = a->x[i].kind;
    char id[16];
    switch (prop) {
    case 30003: V_VT(v) = VT_I4, V_I4(v) = ua_type(a, i); break;
    case 30005: V_VT(v) = VT_BSTR, V_BSTR(v) = ua_bstr(n->label, -1); break;
    case 30011:
        snprintf(id, sizeof(id), "%u", (unsigned)n->id);
        V_VT(v) = VT_BSTR, V_BSTR(v) = ua_bstr(id, -1);
        break;
    case 30012: V_VT(v) = VT_BSTR, V_BSTR(v) = ua_bstr("goome", -1); break;
    case 30024: V_VT(v) = VT_BSTR, V_BSTR(v) = ua_bstr("goome", -1); break;
    case 30002: V_VT(v) = VT_I4, V_I4(v) = (LONG)GetCurrentProcessId(); break;
    case 30008: ua_vbool(v, (n->states & GOOME_A11Y_FOCUSED) != 0); break;
    case 30009: ua_vbool(v, (k & GM_AXK_FOCUSABLE) != 0); break;
    case 30010: ua_vbool(v, !(n->states & GOOME_A11Y_DISABLED)); break;
    case 30016: ua_vbool(v, 1); break;
    case 30017: ua_vbool(v, n->role != GOOME_ROLE_GROUP && n->role != GOOME_ROLE_WINDOW); break;
    case 30019: ua_vbool(v, (k & GM_AXK_PASSWORD) != 0); break;
    case 30022: ua_vbool(v, (k & GM_AXK_HIDDEN) != 0); break;
    }
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE ua_s_host(void *t, void **o)
{
    ua_el *e = UA_EL(t, simple);
    *o = NULL;
    if (e->id == 1 && e->host->g) return ua_lib.host(e->host->hwnd, o);
    return S_OK;
}

static ua_el *ua_at(ua_el *e, int i)
{
    gm_ax *a = ua_ax(e);
    if (i == -1) return ua_new(e->host, 1);
    if (!a || i < 0 || i >= a->count) return NULL;
    return ua_new(e->host, a->n[i].id);
}

static HRESULT STDMETHODCALLTYPE ua_f_nav(void *t, int dir, void **o)
{
    ua_el *e = UA_EL(t, frag);
    *o = NULL;
    int i = ua_idx(e);
    gm_ax *a = ua_ax(e);
    if (i == -3) return UA_GONE;
    int pick = -3;
    if (dir == 0) pick = i == -1 ? -3 : a->n[i].parent;
    else if (dir == 3 || dir == 4) {
        for (int j = i + 1; j < a->count; j++)
            if (a->n[j].parent == i) {
                pick = j;
                if (dir == 3) break;
            }
    } else if (i >= 0 && (dir == 1 || dir == 2)) {
        int par = a->n[i].parent;
        if (dir == 1) {
            for (int j = i + 1; j < a->count && pick == -3; j++)
                if (a->n[j].parent == par) pick = j;
        } else
            for (int j = i - 1; j >= 0 && pick == -3; j--)
                if (a->n[j].parent == par) pick = j;
    }
    if (pick == -3) return S_OK;
    ua_el *r = ua_at(e, pick);
    if (r) *o = (void *)&r->frag;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE ua_f_rid(void *t, SAFEARRAY **o)
{
    ua_el *e = UA_EL(t, frag);
    *o = NULL;
    if (e->id == 1) return S_OK;
    if (ua_idx(e) == -3) return UA_GONE;
    SAFEARRAY *s = ua_lib.arr(VT_I4, 0, 2);
    if (!s) return E_OUTOFMEMORY;
    LONG k = 0, v = 3;
    ua_lib.put(s, &k, &v);
    k = 1, v = (LONG)e->id;
    ua_lib.put(s, &k, &v);
    *o = s;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE ua_f_box(void *t, ua_rect *r)
{
    ua_el *e = UA_EL(t, frag);
    memset(r, 0, sizeof(*r));
    int i = ua_idx(e);
    if (i == -3) return UA_GONE;
    if (i >= 0) ua_screen(e, &ua_ax(e)->n[i], r);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE ua_f_embedded(void *t, SAFEARRAY **o)
{
    *o = NULL;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE ua_f_focus(void *t)
{
    ua_el *e = UA_EL(t, frag);
    int i = ua_idx(e);
    if (i == -3) return UA_GONE;
    if (i >= 0) ua_act(e, "focus", NULL);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE ua_f_root(void *t, void **o)
{
    ua_el *e = UA_EL(t, frag), *r = ua_new(e->host, 1);
    *o = r ? (void *)&r->root : NULL;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE ua_r_point(void *t, double x, double y, void **o)
{
    ua_el *e = UA_EL(t, root);
    gm_ax *a = ua_ax(e);
    *o = NULL;
    if (!a) return UA_GONE;
    int best = -3;
    for (int i = 0; i < a->count; i++) {
        ua_rect r;
        if (a->x[i].kind & GM_AXK_HIDDEN) continue;
        ua_screen(e, &a->n[i], &r);
        if (x >= r.left && y >= r.top && x < r.left + r.width && y < r.top + r.height) best = i;
    }
    if (best == -3) return S_OK;
    ua_el *r = ua_at(e, best);
    if (r) *o = (void *)&r->frag;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE ua_r_focus(void *t, void **o)
{
    ua_el *e = UA_EL(t, root);
    gm_ax *a = ua_ax(e);
    *o = NULL;
    if (!a) return UA_GONE;
    for (int i = 0; i < a->count; i++)
        if (a->n[i].states & GOOME_A11Y_FOCUSED) {
            ua_el *r = ua_at(e, i);
            if (r) *o = (void *)&r->frag;
            break;
        }
    return S_OK;
}

static const goome_a11y_node *ua_node(ua_el *e)
{
    int i = ua_idx(e);
    return i >= 0 ? &ua_ax(e)->n[i] : NULL;
}

static HRESULT STDMETHODCALLTYPE ua_i_invoke(void *t)
{
    ua_el *e = UA_EL(t, inv);
    if (!ua_node(e)) return UA_GONE;
    ua_act(e, "press", NULL);
    if (ua_lib.raise) ua_lib.raise(&e->simple, 20009);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE ua_t_toggle(void *t)
{
    ua_el *e = UA_EL(t, tog);
    if (!ua_node(e)) return UA_GONE;
    ua_act(e, "press", NULL);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE ua_t_state(void *t, int *o)
{
    const goome_a11y_node *n = ua_node(UA_EL(t, tog));
    if (!n) return UA_GONE;
    *o = (n->states & GOOME_A11Y_CHECKED) ? 1 : 0;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE ua_v_set(void *t, const WCHAR *w)
{
    ua_el *e = UA_EL(t, val);
    if (!ua_node(e)) return UA_GONE;
    char *s = ua_utf8(w);
    if (!s) return E_OUTOFMEMORY;
    ua_act(e, "set", s);
    free(s);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE ua_v_get(void *t, BSTR *o)
{
    ua_el *e = UA_EL(t, val);
    int i = ua_idx(e), len;
    if (i < 0) return UA_GONE;
    const char *s = gm_ax_str(ua_ax(e), i, &len);
    *o = ua_bstr(s, len);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE ua_v_ro(void *t, BOOL *o)
{
    ua_el *e = UA_EL(t, val);
    int i = ua_idx(e);
    if (i < 0) return UA_GONE;
    *o = !(ua_ax(e)->x[i].kind & GM_AXK_EDITABLE);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE ua_g_set(void *t, double d)
{
    ua_el *e = UA_EL(t, rng);
    char s[40];
    if (!ua_node(e)) return UA_GONE;
    snprintf(s, sizeof(s), "%.17g", d);
    ua_act(e, "set", s);
    return S_OK;
}

static HRESULT ua_g_num(void *t, double *o, int what)
{
    ua_el *e = UA_EL(t, rng);
    const goome_a11y_node *n = ua_node(e);
    if (!n) return UA_GONE;
    double span = n->max - n->min;
    *o = what == 0 ? n->num : what == 1 ? n->max : what == 2 ? n->min : what == 3 ? span / 10 : span >= 2 ? 1 : span / 100;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE ua_g_val(void *t, double *o) { return ua_g_num(t, o, 0); }
static HRESULT STDMETHODCALLTYPE ua_g_max(void *t, double *o) { return ua_g_num(t, o, 1); }
static HRESULT STDMETHODCALLTYPE ua_g_min(void *t, double *o) { return ua_g_num(t, o, 2); }
static HRESULT STDMETHODCALLTYPE ua_g_large(void *t, double *o) { return ua_g_num(t, o, 3); }
static HRESULT STDMETHODCALLTYPE ua_g_small(void *t, double *o) { return ua_g_num(t, o, 4); }

static HRESULT STDMETHODCALLTYPE ua_g_ro(void *t, BOOL *o)
{
    ua_el *e = UA_EL(t, rng);
    int i = ua_idx(e);
    if (i < 0) return UA_GONE;
    *o = !(ua_ax(e)->x[i].kind & GM_AXK_EDITABLE);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE ua_l_get(void *t, SAFEARRAY **o)
{
    ua_el *e = UA_EL(t, sel);
    gm_ax *a = ua_ax(e);
    int i = ua_idx(e), n = 0;
    *o = NULL;
    if (i < 0) return UA_GONE;
    for (int j = i + 1; j < a->count; j++)
        if (a->n[j].parent == i && (a->n[j].states & GOOME_A11Y_SELECTED)) n++;
    SAFEARRAY *s = ua_lib.arr(VT_UNKNOWN, 0, (ULONG)n);
    if (!s) return E_OUTOFMEMORY;
    LONG k = 0;
    for (int j = i + 1; j < a->count; j++)
        if (a->n[j].parent == i && (a->n[j].states & GOOME_A11Y_SELECTED)) {
            ua_el *c = ua_new(e->host, a->n[j].id);
            if (!c) continue;
            ua_lib.put(s, &k, &c->simple);
            ua_rel_el(c);
            k++;
        }
    *o = s;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE ua_l_multi(void *t, BOOL *o)
{
    *o = FALSE;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE ua_e_select(void *t)
{
    ua_el *e = UA_EL(t, seli);
    const goome_a11y_node *n = ua_node(e);
    if (!n) return UA_GONE;
    if (!(n->states & (GOOME_A11Y_SELECTED | GOOME_A11Y_CHECKED))) ua_act(e, "press", NULL);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE ua_e_remove(void *t) { return S_OK; }

static HRESULT STDMETHODCALLTYPE ua_e_is(void *t, BOOL *o)
{
    const goome_a11y_node *n = ua_node(UA_EL(t, seli));
    if (!n) return UA_GONE;
    *o = (n->states & (GOOME_A11Y_SELECTED | GOOME_A11Y_CHECKED)) != 0;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE ua_e_box(void *t, void **o)
{
    ua_el *e = UA_EL(t, seli);
    const goome_a11y_node *n = ua_node(e);
    *o = NULL;
    if (!n) return UA_GONE;
    ua_el *p = ua_at(e, n->parent);
    if (p) *o = (void *)&p->simple;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE ua_x_expand(void *t)
{
    ua_el *e = UA_EL(t, exp);
    if (!ua_node(e)) return UA_GONE;
    ua_act(e, "expand", NULL);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE ua_x_collapse(void *t)
{
    ua_el *e = UA_EL(t, exp);
    if (!ua_node(e)) return UA_GONE;
    ua_act(e, "collapse", NULL);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE ua_x_state(void *t, int *o)
{
    const goome_a11y_node *n = ua_node(UA_EL(t, exp));
    if (!n) return UA_GONE;
    *o = (n->states & GOOME_A11Y_EXPANDED) ? 1 : 0;
    return S_OK;
}

#define UA_VT(name, ...) static const ua_vt name = {name##_qi, name##_add, name##_rel, {__VA_ARGS__}}

UA_VT(ua_s, (void *)ua_s_opts, (void *)ua_s_pattern, (void *)ua_s_prop, (void *)ua_s_host);
UA_VT(ua_f, (void *)ua_f_nav, (void *)ua_f_rid, (void *)ua_f_box, (void *)ua_f_embedded, (void *)ua_f_focus, (void *)ua_f_root);
UA_VT(ua_r, (void *)ua_r_point, (void *)ua_r_focus);
UA_VT(ua_i, (void *)ua_i_invoke);
UA_VT(ua_t, (void *)ua_t_toggle, (void *)ua_t_state);
UA_VT(ua_v, (void *)ua_v_set, (void *)ua_v_get, (void *)ua_v_ro);
UA_VT(ua_g, (void *)ua_g_set, (void *)ua_g_val, (void *)ua_g_ro, (void *)ua_g_max, (void *)ua_g_min, (void *)ua_g_large, (void *)ua_g_small);
UA_VT(ua_l, (void *)ua_l_get, (void *)ua_l_multi, (void *)ua_l_multi);
UA_VT(ua_e, (void *)ua_e_select, (void *)ua_e_select, (void *)ua_e_remove, (void *)ua_e_is, (void *)ua_e_box);
UA_VT(ua_x, (void *)ua_x_expand, (void *)ua_x_collapse, (void *)ua_x_state);

static ua_el *ua_new(ua_host *h, uint32_t id)
{
    ua_el *e = calloc(1, sizeof(*e));
    if (!e) return NULL;
    e->simple = &ua_s, e->frag = &ua_f, e->root = &ua_r, e->inv = &ua_i, e->tog = &ua_t, e->val = &ua_v, e->rng = &ua_g, e->sel = &ua_l, e->seli = &ua_e, e->exp = &ua_x;
    e->ref = 1, e->host = h, e->id = id;
    InterlockedIncrement(&h->ref);
    return e;
}

int gm_axp_open(goome *g)
{
    if (!ua_load()) return -1;
    ua_host *h = calloc(1, sizeof(*h));
    if (!h) return -1;
    h->ref = 1, h->g = g;
    g->ax->plat = h;
    return 0;
}

int gm_uia_obj(goome *g, void *hwnd, uintptr_t wp, intptr_t lp, intptr_t *out)
{
    if (!g || !g->ax || g->ax->plat_state <= 0 || (LONG)lp != -25) return 0;
    ua_host *h = g->ax->plat;
    h->hwnd = (HWND)hwnd;
    ua_el *r = ua_new(h, 1);
    if (!r) return 0;
    *out = (intptr_t)ua_lib.ret((HWND)hwnd, (WPARAM)wp, (LPARAM)lp, &r->simple);
    ua_rel_el(r);
    return 1;
}

static void ua_changed(ua_host *h, uint32_t id, int prop, VARIANT now)
{
    ua_el *e = ua_new(h, id);
    if (!e) return;
    VARIANT old;
    V_VT(&old) = VT_EMPTY;
    ua_lib.prop(&e->simple, prop, old, now);
    ua_rel_el(e);
}

void gm_axp_event(goome *g, int ev, int i, int old)
{
    ua_host *h = g->ax ? g->ax->plat : NULL;
    if (!h || !h->hwnd || (ua_lib.listening && !ua_lib.listening())) return;
    gm_ax *a = g->ax;
    VARIANT v;
    switch (ev) {
    case GM_AXE_FOCUS: {
        ua_el *e = ua_new(h, a->n[i].id);
        if (e) ua_lib.raise(&e->simple, 20005), ua_rel_el(e);
        break;
    }
    case GM_AXE_NAME:
        V_VT(&v) = VT_BSTR, V_BSTR(&v) = ua_bstr(a->n[i].label, -1);
        ua_changed(h, a->n[i].id, 30005, v);
        break;
    case GM_AXE_VALUE:
        if (a->x[i].kind & GM_AXK_RANGE) {
            V_VT(&v) = VT_R8, V_R8(&v) = a->n[i].num;
            ua_changed(h, a->n[i].id, 30047, v);
        } else if (ua_has(a, i, UA_VALUE)) {
            int len;
            const char *s = gm_ax_str(a, i, &len);
            V_VT(&v) = VT_BSTR, V_BSTR(&v) = ua_bstr(s, len);
            ua_changed(h, a->n[i].id, 30045, v);
        }
        break;
    case GM_AXE_STATE: {
        int st = a->n[i].states, d = st ^ old;
        if ((d & GOOME_A11Y_CHECKED) && ua_has(a, i, UA_TOGGLE)) V_VT(&v) = VT_I4, V_I4(&v) = (st & GOOME_A11Y_CHECKED) ? 1 : 0, ua_changed(h, a->n[i].id, 30086, v);
        if (d & GOOME_A11Y_EXPANDED) V_VT(&v) = VT_I4, V_I4(&v) = (st & GOOME_A11Y_EXPANDED) ? 1 : 0, ua_changed(h, a->n[i].id, 30070, v);
        if ((d & (GOOME_A11Y_SELECTED | GOOME_A11Y_CHECKED)) && ua_has(a, i, UA_SELITEM)) ua_vbool(&v, (st & (GOOME_A11Y_SELECTED | GOOME_A11Y_CHECKED)) != 0), ua_changed(h, a->n[i].id, 30079, v);
        break;
    }
    case GM_AXE_ADD:
    case GM_AXE_REMOVE:
        if (ua_lib.structure && h->serial != a->serial) {
            h->serial = a->serial;
            ua_el *e = ua_new(h, 1);
            if (e) ua_lib.structure(&e->simple, 2, NULL, 0), ua_rel_el(e);
        }
        break;
    }
}

int gm_axp_pump(goome *g) { return -1; }

void gm_axp_close(goome *g)
{
    ua_host *h = g->ax->plat;
    if (!h) return;
    if (h->hwnd && ua_lib.ret) ua_lib.ret(h->hwnd, 0, 0, NULL);
    h->g = NULL;
    if (InterlockedDecrement(&h->ref) == 0) free(h);
    g->ax->plat = NULL;
}
#endif
