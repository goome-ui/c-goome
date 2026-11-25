#include "internal.h"

#if defined(GM_LINUX) && !defined(GOOME_NO_UI)
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct {
    const char *name, *message;
    void *pad[6];
} as_err;

typedef struct {
    void *p[16];
} as_it;

static struct {
    int tried, ok;
    void (*err_init)(as_err *);
    void (*err_free)(as_err *);
    void *(*bus_get)(int, as_err *);
    void *(*open)(const char *, as_err *);
    unsigned (*reg)(void *, as_err *);
    const char *(*unique)(void *);
    void (*no_exit)(void *, unsigned);
    void (*close)(void *);
    void (*unref)(void *);
    unsigned (*rw)(void *, int);
    void *(*pop)(void *);
    unsigned (*send)(void *, void *, uint32_t *);
    void (*flush)(void *);
    void *(*call)(void *, void *, int, as_err *);
    void *(*m_call)(const char *, const char *, const char *, const char *);
    void *(*m_ret)(void *);
    void *(*m_err)(void *, const char *, const char *);
    void *(*m_sig)(const char *, const char *, const char *);
    void (*m_unref)(void *);
    int (*m_type)(void *);
    const char *(*m_path)(void *);
    const char *(*m_iface)(void *);
    const char *(*m_member)(void *);
    unsigned (*it_init)(void *, as_it *);
    int (*it_type)(as_it *);
    void (*it_get)(as_it *, void *);
    unsigned (*it_next)(as_it *);
    void (*it_rec)(as_it *, as_it *);
    void (*it_app)(void *, as_it *);
    unsigned (*it_put)(as_it *, int, const void *);
    unsigned (*it_open)(as_it *, int, const char *, as_it *);
    unsigned (*it_close)(as_it *, as_it *);
} as_lib;

typedef struct {
    void *bus;
    char me[128], desk[128], dpath[160];
    int app_id;
} as_st;

#define AS_PRE "/org/a11y/atspi/accessible/"
#define AS_IF "org.a11y.atspi."

static int as_load(void)
{
    if (as_lib.tried) return as_lib.ok;
    as_lib.tried = 1;
    void *h = dlopen("libdbus-1.so.3", RTLD_NOW | RTLD_LOCAL);
    if (!h) h = dlopen("libdbus-1.so", RTLD_NOW | RTLD_LOCAL);
    if (!h) return 0;
    struct {
        void **p;
        const char *n;
    } f[] = {
        {(void **)&as_lib.err_init, "dbus_error_init"}, {(void **)&as_lib.err_free, "dbus_error_free"}, {(void **)&as_lib.bus_get, "dbus_bus_get_private"},
        {(void **)&as_lib.open, "dbus_connection_open_private"}, {(void **)&as_lib.reg, "dbus_bus_register"}, {(void **)&as_lib.unique, "dbus_bus_get_unique_name"},
        {(void **)&as_lib.no_exit, "dbus_connection_set_exit_on_disconnect"}, {(void **)&as_lib.close, "dbus_connection_close"}, {(void **)&as_lib.unref, "dbus_connection_unref"},
        {(void **)&as_lib.rw, "dbus_connection_read_write"}, {(void **)&as_lib.pop, "dbus_connection_pop_message"}, {(void **)&as_lib.send, "dbus_connection_send"},
        {(void **)&as_lib.flush, "dbus_connection_flush"}, {(void **)&as_lib.call, "dbus_connection_send_with_reply_and_block"},
        {(void **)&as_lib.m_call, "dbus_message_new_method_call"}, {(void **)&as_lib.m_ret, "dbus_message_new_method_return"}, {(void **)&as_lib.m_err, "dbus_message_new_error"},
        {(void **)&as_lib.m_sig, "dbus_message_new_signal"}, {(void **)&as_lib.m_unref, "dbus_message_unref"}, {(void **)&as_lib.m_type, "dbus_message_get_type"},
        {(void **)&as_lib.m_path, "dbus_message_get_path"}, {(void **)&as_lib.m_iface, "dbus_message_get_interface"}, {(void **)&as_lib.m_member, "dbus_message_get_member"},
        {(void **)&as_lib.it_init, "dbus_message_iter_init"}, {(void **)&as_lib.it_type, "dbus_message_iter_get_arg_type"}, {(void **)&as_lib.it_get, "dbus_message_iter_get_basic"},
        {(void **)&as_lib.it_next, "dbus_message_iter_next"}, {(void **)&as_lib.it_rec, "dbus_message_iter_recurse"}, {(void **)&as_lib.it_app, "dbus_message_iter_init_append"},
        {(void **)&as_lib.it_put, "dbus_message_iter_append_basic"}, {(void **)&as_lib.it_open, "dbus_message_iter_open_container"},
        {(void **)&as_lib.it_close, "dbus_message_iter_close_container"},
    };
    for (size_t i = 0; i < sizeof(f) / sizeof(f[0]); i++) {
        *f[i].p = dlsym(h, f[i].n);
        if (!*f[i].p) return 0;
    }
    as_lib.ok = 1;
    return 1;
}

static void as_s(as_it *it, const char *s) { as_lib.it_put(it, 's', &s); }
static void as_o(as_it *it, const char *s) { as_lib.it_put(it, 'o', &s); }
static void as_i(as_it *it, int v) { as_lib.it_put(it, 'i', &v); }
static void as_u(as_it *it, uint32_t v) { as_lib.it_put(it, 'u', &v); }
static void as_b(as_it *it, int v)
{
    uint32_t b = v != 0;
    as_lib.it_put(it, 'b', &b);
}
static void as_d(as_it *it, double v) { as_lib.it_put(it, 'd', &v); }

static void as_path(char *out, size_t cap, int i, const gm_ax *a)
{
    if (i == -2) snprintf(out, cap, AS_PRE "root");
    else if (i == -1) snprintf(out, cap, AS_PRE "1");
    else if (i >= 0 && a && i < a->count) snprintf(out, cap, AS_PRE "%u", (unsigned)a->n[i].id);
    else snprintf(out, cap, "/org/a11y/atspi/null");
}

static void as_ref(as_it *it, as_st *s, const gm_ax *a, int i)
{
    as_it r;
    char p[96];
    as_path(p, sizeof(p), i, a);
    as_lib.it_open(it, 'r', NULL, &r);
    as_s(&r, s->me);
    as_o(&r, p);
    as_lib.it_close(it, &r);
}

static void as_desk(as_it *it, as_st *s)
{
    as_it r;
    as_lib.it_open(it, 'r', NULL, &r);
    as_s(&r, s->desk[0] ? s->desk : s->me);
    as_o(&r, s->desk[0] ? s->dpath : "/org/a11y/atspi/null");
    as_lib.it_close(it, &r);
}

static int as_obj(const gm_ax *a, const char *path)
{
    if (!path || strncmp(path, AS_PRE, sizeof(AS_PRE) - 1)) return -3;
    const char *p = path + sizeof(AS_PRE) - 1;
    if (!strcmp(p, "root")) return -2;
    uint32_t id = 0;
    if (*p < '0' || *p > '9') return -3;
    while (*p >= '0' && *p <= '9') id = id * 10 + (uint32_t)(*p++ - '0');
    if (*p) return -3;
    if (id == 1) return -1;
    int i = gm_ax_find(a, id);
    return i >= 0 ? i : -3;
}

static int as_kids(const gm_ax *a, int i, int *out, int max)
{
    if (i == -2) {
        if (out && max > 0) out[0] = -1;
        return 1;
    }
    int n = 0;
    for (int k = 0; k < a->count; k++)
        if (a->n[k].parent == i) {
            if (out && n < max) out[n] = k;
            n++;
        }
    return n;
}

static int as_has_kid(const gm_ax *a, int i, int role)
{
    for (int k = i + 1; k < a->count; k++)
        if (a->n[k].parent == i && a->n[k].role == role) return 1;
    return 0;
}

static uint32_t as_role(const gm_ax *a, int i)
{
    if (i == -2) return 75;
    if (i == -1) return 23;
    const goome_a11y_node *n = &a->n[i];
    int k = a->x[i].kind;
    switch (n->role) {
    case GOOME_ROLE_WINDOW: return 28;
    case GOOME_ROLE_BUTTON: return 43;
    case GOOME_ROLE_CHECKBOX: return 7;
    case GOOME_ROLE_RADIO: return 44;
    case GOOME_ROLE_SLIDER: return 51;
    case GOOME_ROLE_TEXT: return 29;
    case GOOME_ROLE_INPUT: return (k & GM_AXK_PASSWORD) ? 40 : (k & GM_AXK_MULTILINE) ? 61 : 79;
    case GOOME_ROLE_COMBO: return 11;
    case GOOME_ROLE_LIST: return 31;
    case GOOME_ROLE_ITEM: return 32;
    case GOOME_ROLE_TAB: return 37;
    case GOOME_ROLE_MENU: return 33;
    case GOOME_ROLE_MENUITEM: return (k & GM_AXK_CHECKABLE) ? 8 : 35;
    case GOOME_ROLE_TABLE: return 55;
    case GOOME_ROLE_TREE: return 91;
    case GOOME_ROLE_PROGRESS: return 42;
    case GOOME_ROLE_IMAGE: return 27;
    case GOOME_ROLE_GROUP: return as_has_kid(a, i, GOOME_ROLE_TAB) ? 38 : as_has_kid(a, i, GOOME_ROLE_MENU) ? 34 : 39;
    case GOOME_ROLE_DIALOG: return 16;
    }
    return 39;
}

static const char *as_role_name(uint32_t r)
{
    switch (r) {
    case 75: return "application";
    case 23: return "frame";
    case 28: return "internal frame";
    case 43: return "push button";
    case 7: return "check box";
    case 44: return "radio button";
    case 51: return "slider";
    case 29: return "label";
    case 40: return "password text";
    case 61: return "text";
    case 79: return "entry";
    case 11: return "combo box";
    case 31: return "list";
    case 32: return "list item";
    case 37: return "page tab";
    case 38: return "page tab list";
    case 33: return "menu";
    case 34: return "menu bar";
    case 8: return "check menu item";
    case 35: return "menu item";
    case 55: return "table";
    case 91: return "tree item";
    case 42: return "progress bar";
    case 27: return "image";
    case 16: return "dialog";
    }
    return "panel";
}

static void as_states(const gm_ax *a, int i, uint32_t st[2])
{
    st[0] = st[1] = 0;
#define AS_ST(b) st[(b) / 32] |= 1u << ((b) % 32)
    AS_ST(8), AS_ST(24), AS_ST(30);
    if (i < 0) {
        AS_ST(25);
        if (i == -1 && a->active) AS_ST(1);
        return;
    }
    const goome_a11y_node *n = &a->n[i];
    int k = a->x[i].kind;
    if (!(k & GM_AXK_HIDDEN)) AS_ST(25);
    if (n->states & GOOME_A11Y_DISABLED) st[0] &= ~((1u << 8) | (1u << 24));
    if (k & GM_AXK_FOCUSABLE) AS_ST(11);
    if (n->states & GOOME_A11Y_FOCUSED) AS_ST(12);
    if (k & GM_AXK_CHECKABLE) AS_ST(41);
    if (n->states & GOOME_A11Y_CHECKED) AS_ST(4);
    if (k & GM_AXK_EXPANDABLE) {
        AS_ST(9);
        if (n->states & GOOME_A11Y_EXPANDED) AS_ST(10);
        else AS_ST(5);
    }
    if (k & GM_AXK_SELECTABLE) AS_ST(22);
    if (n->states & GOOME_A11Y_SELECTED) AS_ST(23);
    if (n->role == GOOME_ROLE_INPUT) {
        if (k & GM_AXK_EDITABLE) AS_ST(7);
        else AS_ST(43);
        if (k & GM_AXK_MULTILINE) AS_ST(17);
        else AS_ST(26);
    }
    if (n->role == GOOME_ROLE_SLIDER) AS_ST(14);
    if (n->role == GOOME_ROLE_PROGRESS && n->num < 0) AS_ST(3), AS_ST(32);
    if (n->role == GOOME_ROLE_DIALOG) AS_ST(16);
#undef AS_ST
}

static int as_acts(const gm_ax *a, int i, const char **names, int *verbs)
{
    if (i < 0) return 0;
    int k = a->x[i].kind, n = 0;
    if (k & GM_AXK_PRESS) names[n] = a->n[i].role == GOOME_ROLE_CHECKBOX || a->n[i].role == GOOME_ROLE_RADIO ? "toggle" : "click", verbs[n++] = 'p';
    else if (k & GM_AXK_EXPANDABLE) names[n] = "expand or contract", verbs[n++] = 'e';
    return n;
}

static int as_is_text(const gm_ax *a, int i) { return i >= 0 && (a->n[i].role == GOOME_ROLE_INPUT || a->n[i].role == GOOME_ROLE_TEXT); }

static const char *as_text(const gm_ax *a, int i, int *len)
{
    if (a->n[i].role == GOOME_ROLE_TEXT) {
        *len = (int)strlen(a->n[i].label);
        return a->n[i].label;
    }
    return gm_ax_str(a, i, len);
}

static void as_ifaces(as_it *it, const gm_ax *a, int i)
{
    as_it r;
    const char *names[2] = {NULL, NULL};
    int verbs[2] = {0, 0};
    as_lib.it_open(it, 'a', "s", &r);
    as_s(&r, AS_IF "Accessible");
    if (i == -2) as_s(&r, AS_IF "Application");
    else as_s(&r, AS_IF "Component");
    if (as_acts(a, i, names, verbs)) as_s(&r, AS_IF "Action");
    if (i >= 0 && (a->x[i].kind & GM_AXK_RANGE)) as_s(&r, AS_IF "Value");
    if (as_is_text(a, i)) as_s(&r, AS_IF "Text");
    if (i >= 0 && a->n[i].role == GOOME_ROLE_INPUT && (a->x[i].kind & GM_AXK_EDITABLE)) as_s(&r, AS_IF "EditableText");
    as_lib.it_close(it, &r);
}

static void as_origin(goome *g, double *ox, double *oy, double *sc)
{
    int x = 0, y = 0;
    *sc = g->nmon > 0 && g->mon[0].scale > 0 ? g->mon[0].scale : 1;
    if (g->opts.mode == GOOME_MODE_WINDOW && goome_window_pos(g, &x, &y, 0) == GOOME_OK) *ox = x, *oy = y;
    else *ox = 0, *oy = 0;
}

static void as_box(goome *g, const gm_ax *a, int i, uint32_t coord, int r[4])
{
    double ox, oy, sc, x = 0, y = 0, w, h;
    as_origin(g, &ox, &oy, &sc);
    if (i >= 0) x = a->n[i].x, y = a->n[i].y, w = a->n[i].w, h = a->n[i].h;
    else w = g->nmon > 0 ? g->mon[0].area.w : 0, h = g->nmon > 0 ? g->mon[0].area.h : 0;
    if (coord == 2 && i >= 0 && a->n[i].parent >= 0) x -= a->n[a->n[i].parent].x, y -= a->n[a->n[i].parent].y;
    r[0] = (int)(x * sc + (coord == 0 ? ox : 0) + 0.5), r[1] = (int)(y * sc + (coord == 0 ? oy : 0) + 0.5);
    r[2] = (int)(w * sc + 0.5), r[3] = (int)(h * sc + 0.5);
}

static int as_at(goome *g, const gm_ax *a, int x, int y, uint32_t coord)
{
    int best = -3;
    for (int k = 0; k < a->count; k++) {
        if (a->x[k].kind & GM_AXK_HIDDEN) continue;
        int r[4];
        as_box(g, a, k, coord == 2 ? 1 : coord, r);
        if (x >= r[0] && y >= r[1] && x < r[0] + r[2] && y < r[1] + r[3]) best = k;
    }
    return best;
}

static void as_var_s(as_it *it, const char *s)
{
    as_it v;
    as_lib.it_open(it, 'v', "s", &v);
    as_s(&v, s);
    as_lib.it_close(it, &v);
}

static void as_name(const gm_ax *a, int i, goome *g, char *out, size_t cap)
{
    if (i == -2 || i == -1) snprintf(out, cap, "%s", g->title);
    else snprintf(out, cap, "%s", a->n[i].label);
}

static int as_prop(goome *g, as_st *s, const gm_ax *a, int i, const char *iface, const char *prop, as_it *it)
{
    as_it v;
    char t[128];
    if (!strcmp(iface, AS_IF "Accessible")) {
        if (!strcmp(prop, "Name")) {
            as_name(a, i, g, t, sizeof(t));
            as_var_s(it, t);
        } else if (!strcmp(prop, "Description") || !strcmp(prop, "Locale") || !strcmp(prop, "HelpText"))
            as_var_s(it, "");
        else if (!strcmp(prop, "AccessibleId")) {
            snprintf(t, sizeof(t), "%u", i >= 0 ? (unsigned)a->n[i].id : 0u);
            as_var_s(it, i >= 0 ? t : "");
        } else if (!strcmp(prop, "ChildCount")) {
            as_lib.it_open(it, 'v', "i", &v);
            as_i(&v, as_kids(a, i, NULL, 0));
            as_lib.it_close(it, &v);
        } else if (!strcmp(prop, "Parent")) {
            as_lib.it_open(it, 'v', "(so)", &v);
            if (i == -2) as_desk(&v, s);
            else as_ref(&v, s, a, i == -1 ? -2 : a->n[i].parent);
            as_lib.it_close(it, &v);
        } else
            return 0;
        return 1;
    }
    if (!strcmp(iface, AS_IF "Application") && i == -2) {
        if (!strcmp(prop, "ToolkitName")) as_var_s(it, "goome");
        else if (!strcmp(prop, "Version")) as_var_s(it, GOOME_VERSION);
        else if (!strcmp(prop, "AtspiVersion")) as_var_s(it, "2.1");
        else if (!strcmp(prop, "Id")) {
            as_lib.it_open(it, 'v', "i", &v);
            as_i(&v, s->app_id);
            as_lib.it_close(it, &v);
        } else
            return 0;
        return 1;
    }
    if (!strcmp(iface, AS_IF "Value") && i >= 0) {
        const goome_a11y_node *n = &a->n[i];
        if (!strcmp(prop, "Text")) {
            as_var_s(it, n->value);
            return 1;
        }
        double d = !strcmp(prop, "MinimumValue") ? n->min : !strcmp(prop, "MaximumValue") ? n->max : !strcmp(prop, "CurrentValue") ? n->num : 0;
        if (!strcmp(prop, "MinimumIncrement")) d = n->max > n->min && n->max - n->min >= 2 ? 1 : 0;
        else if (strcmp(prop, "MinimumValue") && strcmp(prop, "MaximumValue") && strcmp(prop, "CurrentValue")) return 0;
        as_lib.it_open(it, 'v', "d", &v);
        as_d(&v, d);
        as_lib.it_close(it, &v);
        return 1;
    }
    if (!strcmp(iface, AS_IF "Action") && i >= 0 && !strcmp(prop, "NActions")) {
        const char *names[2] = {NULL, NULL};
        int verbs[2] = {0, 0};
        as_lib.it_open(it, 'v', "i", &v);
        as_i(&v, as_acts(a, i, names, verbs));
        as_lib.it_close(it, &v);
        return 1;
    }
    if (!strcmp(iface, AS_IF "Text") && as_is_text(a, i) && (!strcmp(prop, "CharacterCount") || !strcmp(prop, "CaretOffset"))) {
        int len;
        const char *x = as_text(a, i, &len);
        as_lib.it_open(it, 'v', "i", &v);
        as_i(&v, !strcmp(prop, "CharacterCount") ? gm_ax_chars(x, len) : a->n[i].role == GOOME_ROLE_INPUT ? a->n[i].caret : 0);
        as_lib.it_close(it, &v);
        return 1;
    }
    return 0;
}

static void as_all(goome *g, as_st *s, const gm_ax *a, int i, const char *iface, as_it *it)
{
    static const char *const acc[] = {"Name", "Description", "Parent", "ChildCount", "Locale", "AccessibleId", "HelpText", NULL};
    static const char *const app[] = {"ToolkitName", "Version", "AtspiVersion", "Id", NULL};
    static const char *const val[] = {"MinimumValue", "MaximumValue", "MinimumIncrement", "CurrentValue", "Text", NULL};
    static const char *const act[] = {"NActions", NULL};
    static const char *const txt[] = {"CharacterCount", "CaretOffset", NULL};
    const char *const *l = !strcmp(iface, AS_IF "Accessible") ? acc : !strcmp(iface, AS_IF "Application") ? app : !strcmp(iface, AS_IF "Value") ? val
                         : !strcmp(iface, AS_IF "Action") ? act : !strcmp(iface, AS_IF "Text") ? txt : NULL;
    as_it r;
    as_lib.it_open(it, 'a', "{sv}", &r);
    for (int k = 0; l && l[k]; k++) {
        as_it e;
        as_lib.it_open(&r, 'e', NULL, &e);
        as_s(&e, l[k]);
        if (!as_prop(g, s, a, i, iface, l[k], &e)) as_var_s(&e, "");
        as_lib.it_close(&r, &e);
    }
    as_lib.it_close(it, &r);
}

static int as_arg(void *m, int idx, int type, void *out)
{
    as_it it;
    if (!as_lib.it_init(m, &it)) return 0;
    for (int k = 0; k < idx; k++)
        if (!as_lib.it_next(&it)) return 0;
    if (as_lib.it_type(&it) == 'v') {
        as_it v;
        as_lib.it_rec(&it, &v);
        if (as_lib.it_type(&v) != type) return 0;
        as_lib.it_get(&v, out);
        return 1;
    }
    if (as_lib.it_type(&it) != type) return 0;
    as_lib.it_get(&it, out);
    return 1;
}

static int as_int(void *m, int idx)
{
    int v = 0;
    if (as_arg(m, idx, 'i', &v)) return v;
    uint32_t u = 0;
    as_arg(m, idx, 'u', &u);
    return (int)u;
}

static void as_set_text(goome *g, const gm_ax *a, int i, const char *s)
{
    gm_ax_act(g, a->n[i].id, "set", s);
}

static void as_edit(goome *g, const gm_ax *a, int i, int pos, int del, const char *ins)
{
    int len;
    const char *t = gm_ax_str(a, i, &len);
    int b0 = gm_ax_bytes(t, len, pos), b1 = gm_ax_bytes(t, len, pos + del);
    if (b1 < b0) b1 = b0;
    size_t il = ins ? strlen(ins) : 0;
    char *o = malloc((size_t)len + il + 1);
    if (!o) return;
    memcpy(o, t, (size_t)b0);
    if (il) memcpy(o + b0, ins, il);
    memcpy(o + b0 + il, t + b1, (size_t)(len - b1));
    o[b0 + il + (size_t)(len - b1)] = 0;
    as_set_text(g, a, i, o);
    free(o);
}

static void as_ranged(as_it *it, const char *t, int len, int a0, int a1)
{
    int n = gm_ax_chars(t, len);
    if (a1 < 0 || a1 > n) a1 = n;
    if (a0 < 0) a0 = 0;
    if (a0 > a1) a0 = a1;
    int b0 = gm_ax_bytes(t, len, a0), b1 = gm_ax_bytes(t, len, a1);
    char *s = malloc((size_t)(b1 - b0) + 1);
    if (!s) {
        as_s(it, "");
        return;
    }
    memcpy(s, t + b0, (size_t)(b1 - b0));
    s[b1 - b0] = 0;
    as_s(it, s);
    free(s);
}

static void *as_handle(goome *g, as_st *s, void *m)
{
    gm_ax *a = g->ax;
    const char *iface = as_lib.m_iface(m), *mem = as_lib.m_member(m);
    if (!iface || !mem) return as_lib.m_err(m, "org.freedesktop.DBus.Error.UnknownMethod", "no interface");
    if (!strcmp(iface, "org.freedesktop.DBus.Peer")) return as_lib.m_ret(m);
    const char *path = as_lib.m_path(m);
    if (path && !strcmp(path, "/org/a11y/atspi/cache") && !strcmp(mem, "GetItems")) {
        void *r = as_lib.m_ret(m);
        as_it it, c;
        if (!r) return NULL;
        as_lib.it_app(r, &it);
        as_lib.it_open(&it, 'a', "((so)(so)(so)iiassusau)", &c);
        as_lib.it_close(&it, &c);
        return r;
    }
    int i = as_obj(a, path);
    if (i == -3) return as_lib.m_err(m, "org.freedesktop.DBus.Error.UnknownObject", "that one is gone");
    void *r = as_lib.m_ret(m);
    if (!r) return NULL;
    as_it it, c;
    as_lib.it_app(r, &it);
    char t[160];
    if (!strcmp(iface, "org.freedesktop.DBus.Properties")) {
        const char *pi = NULL, *pn = NULL;
        as_arg(m, 0, 's', (void *)&pi);
        if (!strcmp(mem, "Get") && pi && as_arg(m, 1, 's', (void *)&pn) && as_prop(g, s, a, i, pi, pn, &it)) return r;
        if (!strcmp(mem, "GetAll") && pi) {
            as_all(g, s, a, i, pi, &it);
            return r;
        }
        if (!strcmp(mem, "Set") && pi && as_arg(m, 1, 's', (void *)&pn)) {
            if (!strcmp(pi, AS_IF "Application") && !strcmp(pn, "Id")) s->app_id = as_int(m, 2);
            double d;
            if (i >= 0 && !strcmp(pi, AS_IF "Value") && !strcmp(pn, "CurrentValue") && as_arg(m, 2, 'd', &d) && (a->x[i].kind & GM_AXK_EDITABLE)) {
                snprintf(t, sizeof(t), "%.17g", d);
                gm_ax_act(g, a->n[i].id, "set", t);
            }
            return r;
        }
    } else if (!strcmp(iface, AS_IF "Accessible")) {
        if (!strcmp(mem, "GetChildAtIndex")) {
            int k = as_int(m, 0), n = as_kids(a, i, NULL, 0);
            int pick = -3;
            if (k >= 0 && k < n) {
                if (i == -2) pick = -1;
                else
                    for (int j = 0, c2 = 0; j < a->count; j++)
                        if (a->n[j].parent == i && c2++ == k) pick = j;
            }
            as_ref(&it, s, a, pick);
            return r;
        }
        if (!strcmp(mem, "GetChildren")) {
            as_lib.it_open(&it, 'a', "(so)", &c);
            if (i == -2) as_ref(&c, s, a, -1);
            else
                for (int j = 0; j < a->count; j++)
                    if (a->n[j].parent == i) as_ref(&c, s, a, j);
            as_lib.it_close(&it, &c);
            return r;
        }
        if (!strcmp(mem, "GetIndexInParent")) {
            int k = 0;
            if (i >= 0)
                for (int j = 0; j < i; j++)
                    if (a->n[j].parent == a->n[i].parent) k++;
            as_i(&it, i == -2 ? -1 : k);
            return r;
        }
        if (!strcmp(mem, "GetRelationSet")) {
            as_lib.it_open(&it, 'a', "(ua(so))", &c);
            as_lib.it_close(&it, &c);
            return r;
        }
        if (!strcmp(mem, "GetRole")) {
            as_u(&it, as_role(a, i));
            return r;
        }
        if (!strcmp(mem, "GetRoleName") || !strcmp(mem, "GetLocalizedRoleName")) {
            as_s(&it, as_role_name(as_role(a, i)));
            return r;
        }
        if (!strcmp(mem, "GetState")) {
            uint32_t st[2];
            as_states(a, i, st);
            as_lib.it_open(&it, 'a', "u", &c);
            as_u(&c, st[0]), as_u(&c, st[1]);
            as_lib.it_close(&it, &c);
            return r;
        }
        if (!strcmp(mem, "GetAttributes")) {
            as_it e;
            as_lib.it_open(&it, 'a', "{ss}", &c);
            as_lib.it_open(&c, 'e', NULL, &e);
            as_s(&e, "toolkit");
            as_s(&e, "goome");
            as_lib.it_close(&c, &e);
            as_lib.it_close(&it, &c);
            return r;
        }
        if (!strcmp(mem, "GetApplication")) {
            as_ref(&it, s, a, -2);
            return r;
        }
        if (!strcmp(mem, "GetInterfaces")) {
            as_ifaces(&it, a, i);
            return r;
        }
    } else if (!strcmp(iface, AS_IF "Application") && i == -2) {
        if (!strcmp(mem, "GetLocale") || !strcmp(mem, "GetApplicationBusAddress")) {
            as_s(&it, "");
            return r;
        }
    } else if (!strcmp(iface, AS_IF "Component") && i != -2) {
        int box[4];
        if (!strcmp(mem, "GetExtents")) {
            as_box(g, a, i, (uint32_t)as_int(m, 0), box);
            as_lib.it_open(&it, 'r', NULL, &c);
            for (int k = 0; k < 4; k++) as_i(&c, box[k]);
            as_lib.it_close(&it, &c);
            return r;
        }
        if (!strcmp(mem, "GetPosition") || !strcmp(mem, "GetSize")) {
            as_box(g, a, i, mem[3] == 'P' ? (uint32_t)as_int(m, 0) : 1, box);
            int o = mem[3] == 'P' ? 0 : 2;
            as_i(&it, box[o]), as_i(&it, box[o + 1]);
            return r;
        }
        if (!strcmp(mem, "Contains")) {
            int x = as_int(m, 0), y = as_int(m, 1);
            as_box(g, a, i, (uint32_t)as_int(m, 2), box);
            as_b(&it, x >= box[0] && y >= box[1] && x < box[0] + box[2] && y < box[1] + box[3]);
            return r;
        }
        if (!strcmp(mem, "GetAccessibleAtPoint")) {
            as_ref(&it, s, a, as_at(g, a, as_int(m, 0), as_int(m, 1), (uint32_t)as_int(m, 2)));
            return r;
        }
        if (!strcmp(mem, "GetLayer")) {
            as_u(&it, i == -1 ? 7 : 3);
            return r;
        }
        if (!strcmp(mem, "GetMDIZOrder")) {
            int16_t z = 0;
            as_lib.it_put(&it, 'n', &z);
            return r;
        }
        if (!strcmp(mem, "GetAlpha")) {
            as_d(&it, 1);
            return r;
        }
        if (!strcmp(mem, "GrabFocus")) {
            if (i >= 0 && (a->x[i].kind & GM_AXK_FOCUSABLE)) gm_ax_act(g, a->n[i].id, "focus", NULL);
            as_b(&it, i >= 0 && (a->x[i].kind & GM_AXK_FOCUSABLE));
            return r;
        }
        if (!strncmp(mem, "Set", 3) || !strncmp(mem, "ScrollTo", 8)) {
            as_b(&it, 0);
            return r;
        }
    } else if (!strcmp(iface, AS_IF "Action") && i >= 0) {
        const char *names[2] = {NULL, NULL};
        int verbs[2] = {0, 0}, n = as_acts(a, i, names, verbs), k = as_int(m, 0);
        if (!strcmp(mem, "GetActions")) {
            as_it e;
            as_lib.it_open(&it, 'a', "(sss)", &c);
            for (int j = 0; j < n; j++) {
                as_lib.it_open(&c, 'r', NULL, &e);
                as_s(&e, names[j]), as_s(&e, names[j]), as_s(&e, "");
                as_lib.it_close(&c, &e);
            }
            as_lib.it_close(&it, &c);
            return r;
        }
        if (!strcmp(mem, "GetName") || !strcmp(mem, "GetLocalizedName") || !strcmp(mem, "GetDescription")) {
            as_s(&it, k >= 0 && k < n ? names[k] : "");
            return r;
        }
        if (!strcmp(mem, "GetKeyBinding")) {
            as_s(&it, "");
            return r;
        }
        if (!strcmp(mem, "DoAction")) {
            int ok = k >= 0 && k < n;
            if (ok && verbs[k] == 'p') gm_ax_act(g, a->n[i].id, "press", NULL);
            else if (ok) gm_ax_act(g, a->n[i].id, (a->n[i].states & GOOME_A11Y_EXPANDED) ? "collapse" : "expand", NULL);
            as_b(&it, ok);
            return r;
        }
    } else if (!strcmp(iface, AS_IF "Text") && as_is_text(a, i)) {
        int len;
        const char *x = as_text(a, i, &len);
        int n = gm_ax_chars(x, len);
        if (!strcmp(mem, "GetText")) {
            as_ranged(&it, x, len, as_int(m, 0), as_int(m, 1));
            return r;
        }
        if (!strcmp(mem, "GetCharacterAtOffset")) {
            int o = gm_ax_bytes(x, len, as_int(m, 0));
            const unsigned char *p = (const unsigned char *)x + o;
            int cp = 0;
            if (o < len) {
                int extra = *p >= 0xF0 ? 3 : *p >= 0xE0 ? 2 : *p >= 0xC0 ? 1 : 0;
                cp = extra ? *p & (0x3F >> extra) : *p;
                for (int k = 1; k <= extra && o + k < len; k++) cp = cp << 6 | (p[k] & 0x3F);
            }
            as_i(&it, cp);
            return r;
        }
        if (!strcmp(mem, "GetTextAtOffset") || !strcmp(mem, "GetTextBeforeOffset") || !strcmp(mem, "GetTextAfterOffset") || !strcmp(mem, "GetStringAtOffset")) {
            as_ranged(&it, x, len, 0, n);
            as_i(&it, 0), as_i(&it, n);
            return r;
        }
        if (!strcmp(mem, "GetNSelections")) {
            as_i(&it, 0);
            return r;
        }
        if (!strcmp(mem, "GetSelection")) {
            as_i(&it, 0), as_i(&it, 0);
            return r;
        }
        if (!strcmp(mem, "SetCaretOffset") || !strcmp(mem, "AddSelection") || !strcmp(mem, "RemoveSelection") || !strcmp(mem, "SetSelection")) {
            as_b(&it, 0);
            return r;
        }
        if (!strcmp(mem, "GetCharacterExtents") || !strcmp(mem, "GetRangeExtents")) {
            int box[4];
            as_box(g, a, i, (uint32_t)as_int(m, mem[3] == 'C' ? 1 : 2), box);
            for (int k = 0; k < 4; k++) as_i(&it, box[k]);
            return r;
        }
        if (!strcmp(mem, "GetAttributes") || !strcmp(mem, "GetAttributeRun") || !strcmp(mem, "GetDefaultAttributes") || !strcmp(mem, "GetDefaultAttributeSet")) {
            as_lib.it_open(&it, 'a', "{ss}", &c);
            as_lib.it_close(&it, &c);
            if (mem[3] == 'A') as_i(&it, 0), as_i(&it, n);
            return r;
        }
    } else if (!strcmp(iface, AS_IF "EditableText") && i >= 0 && a->n[i].role == GOOME_ROLE_INPUT && (a->x[i].kind & GM_AXK_EDITABLE)) {
        const char *v = NULL;
        if (!strcmp(mem, "SetTextContents") && as_arg(m, 0, 's', (void *)&v)) {
            as_set_text(g, a, i, v);
            as_b(&it, 1);
            return r;
        }
        if (!strcmp(mem, "InsertText") && as_arg(m, 1, 's', (void *)&v)) {
            as_edit(g, a, i, as_int(m, 0), 0, v);
            as_b(&it, 1);
            return r;
        }
        if (!strcmp(mem, "DeleteText")) {
            int p0 = as_int(m, 0), p1 = as_int(m, 1);
            as_edit(g, a, i, p0, p1 - p0, NULL);
            as_b(&it, 1);
            return r;
        }
    }
    as_lib.m_unref(r);
    snprintf(t, sizeof(t), "goome has no %s.%s here", iface, mem);
    return as_lib.m_err(m, "org.freedesktop.DBus.Error.UnknownMethod", t);
}

static void as_emit(as_st *s, const gm_ax *a, int i, const char *iface, const char *member, const char *detail, int d1, int d2, int kind, const void *any)
{
    char p[96];
    as_path(p, sizeof(p), i, a);
    void *m = as_lib.m_sig(p, iface, member);
    if (!m) return;
    as_it it, v, e;
    as_lib.it_app(m, &it);
    as_s(&it, detail);
    as_i(&it, d1), as_i(&it, d2);
    if (kind == 's') {
        as_lib.it_open(&it, 'v', "s", &v);
        as_s(&v, (const char *)any);
    } else if (kind == 'r') {
        as_lib.it_open(&it, 'v', "(so)", &v);
        as_lib.it_open(&v, 'r', NULL, &e);
        as_s(&e, s->me);
        as_o(&e, (const char *)any);
        as_lib.it_close(&v, &e);
    } else {
        as_lib.it_open(&it, 'v', "i", &v);
        as_i(&v, 0);
    }
    as_lib.it_close(&it, &v);
    as_lib.it_open(&it, 'a', "{sv}", &v);
    as_lib.it_close(&it, &v);
    as_lib.send(s->bus, m, NULL);
    as_lib.m_unref(m);
}

static void as_state(as_st *s, const gm_ax *a, int i, const char *what, int on) { as_emit(s, a, i, AS_IF "Event.Object", "StateChanged", what, on, 0, 0, NULL); }

static void as_kid_event(as_st *s, const gm_ax *a, int i, int add)
{
    const goome_a11y_node *nodes = add ? a->n : a->pn;
    int count = add ? a->count : a->pcount, par = nodes[i].parent, idx = 0;
    for (int j = 0; j < i; j++)
        if (nodes[j].parent == par) idx++;
    char kid[96];
    snprintf(kid, sizeof(kid), AS_PRE "%u", (unsigned)nodes[i].id);
    int pi = -1;
    if (par >= 0 && par < count) pi = gm_ax_find(a, nodes[par].id);
    if (par >= 0 && pi < 0) return;
    as_emit(s, a, pi, AS_IF "Event.Object", "ChildrenChanged", add ? "add" : "remove", idx, 0, 'r', kid);
}

static void as_text_event(as_st *s, const gm_ax *a, int i, int j)
{
    int an, bn;
    const char *t = gm_ax_str(a, i, &an);
    const char *o = a->px[j].ntext > 0 ? a->ptext + a->px[j].text : a->pn[j].value;
    bn = a->px[j].ntext > 0 ? a->px[j].ntext : (int)strlen(o);
    int pre = 0;
    while (pre < an && pre < bn && t[pre] == o[pre]) pre++;
    while (pre > 0 && ((unsigned char)t[pre] & 0xC0) == 0x80) pre--;
    int suf = 0;
    while (suf < an - pre && suf < bn - pre && t[an - 1 - suf] == o[bn - 1 - suf]) suf++;
    while (suf > 0 && ((unsigned char)t[an - suf] & 0xC0) == 0x80) suf--;
    int at = gm_ax_chars(t, pre);
    char *buf = malloc((size_t)(an > bn ? an : bn) + 1);
    if (!buf) return;
    if (bn - pre - suf > 0) {
        memcpy(buf, o + pre, (size_t)(bn - pre - suf));
        buf[bn - pre - suf] = 0;
        as_emit(s, a, i, AS_IF "Event.Object", "TextChanged", "delete", at, gm_ax_chars(buf, bn - pre - suf), 's', buf);
    }
    if (an - pre - suf > 0) {
        memcpy(buf, t + pre, (size_t)(an - pre - suf));
        buf[an - pre - suf] = 0;
        as_emit(s, a, i, AS_IF "Event.Object", "TextChanged", "insert", at, gm_ax_chars(buf, an - pre - suf), 's', buf);
    }
    free(buf);
}

void gm_axp_event(goome *g, int ev, int i, int old)
{
    as_st *s = g->ax ? g->ax->plat : NULL;
    if (!s) return;
    gm_ax *a = g->ax;
    switch (ev) {
    case GM_AXE_FOCUS: {
        int k = old ? gm_ax_find(a, (uint32_t)old) : -1;
        if (k >= 0) as_state(s, a, k, "focused", 0);
        as_state(s, a, i, "focused", 1);
        break;
    }
    case GM_AXE_ACTIVE:
        as_emit(s, a, -1, AS_IF "Event.Window", a->active ? "Activate" : "Deactivate", "", 0, 0, 0, NULL);
        as_state(s, a, -1, "active", a->active);
        break;
    case GM_AXE_NAME: as_emit(s, a, i, AS_IF "Event.Object", "PropertyChange", "accessible-name", 0, 0, 's', a->n[i].label); break;
    case GM_AXE_VALUE: as_emit(s, a, i, AS_IF "Event.Object", "PropertyChange", "accessible-value", 0, 0, 0, NULL); break;
    case GM_AXE_TEXT: as_text_event(s, a, i, old); break;
    case GM_AXE_CARET: as_emit(s, a, i, AS_IF "Event.Object", "TextCaretMoved", "", a->n[i].caret, 0, 0, NULL); break;
    case GM_AXE_STATE: {
        int st = a->n[i].states, d = st ^ old;
        if (d & GOOME_A11Y_CHECKED) as_state(s, a, i, "checked", (st & GOOME_A11Y_CHECKED) != 0);
        if (d & GOOME_A11Y_EXPANDED) as_state(s, a, i, "expanded", (st & GOOME_A11Y_EXPANDED) != 0);
        if (d & GOOME_A11Y_SELECTED) as_state(s, a, i, "selected", (st & GOOME_A11Y_SELECTED) != 0);
        if (d & GOOME_A11Y_DISABLED) as_state(s, a, i, "enabled", !(st & GOOME_A11Y_DISABLED));
        break;
    }
    case GM_AXE_ADD: as_kid_event(s, a, i, 1); break;
    case GM_AXE_REMOVE: as_kid_event(s, a, i, 0); break;
    }
}

static char *as_addr(void)
{
    const char *e = gm_env("AT_SPI_BUS_ADDRESS");
    if (e) return strdup(e);
    const char *rt = gm_env("XDG_RUNTIME_DIR");
    char bus[1100];
    snprintf(bus, sizeof(bus), "%s/bus", rt ? rt : "");
    if (!gm_env("DBUS_SESSION_BUS_ADDRESS") && (!rt || access(bus, F_OK) != 0)) return NULL;
    as_err err;
    as_lib.err_init(&err);
    void *sb = as_lib.bus_get(0, &err);
    if (!sb) {
        as_lib.err_free(&err);
        return NULL;
    }
    as_lib.no_exit(sb, 0);
    char *out = NULL;
    void *m = as_lib.m_call("org.a11y.Bus", "/org/a11y/bus", "org.a11y.Bus", "GetAddress");
    void *r = m ? as_lib.call(sb, m, 1500, &err) : NULL;
    const char *s = NULL;
    if (r && as_arg(r, 0, 's', (void *)&s) && s && *s) out = strdup(s);
    if (r) as_lib.m_unref(r);
    if (m) as_lib.m_unref(m);
    as_lib.err_free(&err);
    as_lib.close(sb);
    as_lib.unref(sb);
    return out;
}

int gm_axp_open(goome *g)
{
    if (!as_load()) return -1;
    char *addr = as_addr();
    if (!addr) return -1;
    as_err err;
    as_lib.err_init(&err);
    void *bus = as_lib.open(addr, &err);
    free(addr);
    if (!bus) {
        as_lib.err_free(&err);
        return -1;
    }
    as_lib.no_exit(bus, 0);
    if (!as_lib.reg(bus, &err)) {
        as_lib.err_free(&err);
        as_lib.close(bus);
        as_lib.unref(bus);
        return -1;
    }
    as_st *s = calloc(1, sizeof(*s));
    if (!s) {
        as_lib.close(bus);
        as_lib.unref(bus);
        return -1;
    }
    s->bus = bus;
    snprintf(s->me, sizeof(s->me), "%s", as_lib.unique(bus) ? as_lib.unique(bus) : "");
    g->ax->plat = s;
    void *m = as_lib.m_call("org.a11y.atspi.Registry", AS_PRE "root", AS_IF "Socket", "Embed");
    if (m) {
        as_it it, r;
        as_lib.it_app(m, &it);
        as_lib.it_open(&it, 'r', NULL, &r);
        as_s(&r, s->me);
        as_o(&r, AS_PRE "root");
        as_lib.it_close(&it, &r);
        void *rep = as_lib.call(bus, m, 1500, &err);
        as_lib.m_unref(m);
        if (rep) {
            as_it ri, st;
            if (as_lib.it_init(rep, &ri) && as_lib.it_type(&ri) == 'r') {
                const char *dn = NULL, *dp = NULL;
                as_lib.it_rec(&ri, &st);
                if (as_lib.it_type(&st) == 's') as_lib.it_get(&st, (void *)&dn);
                as_lib.it_next(&st);
                if (as_lib.it_type(&st) == 'o') as_lib.it_get(&st, (void *)&dp);
                if (dn && dp) snprintf(s->desk, sizeof(s->desk), "%s", dn), snprintf(s->dpath, sizeof(s->dpath), "%s", dp);
            }
            as_lib.m_unref(rep);
        }
    }
    as_lib.err_free(&err);
    as_lib.flush(bus);
    return 0;
}

int gm_axp_pump(goome *g)
{
    as_st *s = g->ax->plat;
    if (!s) return -1;
    as_lib.rw(s->bus, 0);
    void *m;
    int n = 0;
    while (n++ < 256 && (m = as_lib.pop(s->bus))) {
        if (as_lib.m_type(m) == 1) {
            void *r = as_handle(g, s, m);
            if (r) as_lib.send(s->bus, r, NULL), as_lib.m_unref(r);
        }
        as_lib.m_unref(m);
    }
    as_lib.flush(s->bus);
    return 30;
}

void gm_axp_close(goome *g)
{
    as_st *s = g->ax->plat;
    if (!s) return;
    if (s->desk[0]) {
        void *m = as_lib.m_call("org.a11y.atspi.Registry", AS_PRE "root", AS_IF "Socket", "Unembed");
        if (m) {
            as_it it, r;
            as_lib.it_app(m, &it);
            as_lib.it_open(&it, 'r', NULL, &r);
            as_s(&r, s->me);
            as_o(&r, AS_PRE "root");
            as_lib.it_close(&it, &r);
            as_lib.send(s->bus, m, NULL);
            as_lib.m_unref(m);
        }
    }
    as_lib.flush(s->bus);
    as_lib.close(s->bus);
    as_lib.unref(s->bus);
    free(s);
    g->ax->plat = NULL;
}
#endif
