#include "internal.h"

#if defined(GM_LINUX)

#include <dlfcn.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

// no glib/gst headers, so the few types we touch are spelled out here
typedef struct {
    uint32_t domain;
    int code;
    char *message;
} gn_error;
typedef struct {
    void *memory;
    int flags;
    uint8_t *data;
    size_t size;
    size_t maxsize;
    void *user_data[4];
    void *reserved[4];
} gn_mapinfo; // has to match GstMapInfo byte for byte
typedef void (*gn_signal_cb)(void *, const char *, const char *, const char *, const char *, void *, void *);

static struct {
    void *(*bus_get_sync)(int, void *, gn_error **);
    void *(*call_sync)(void *, const char *, const char *, const char *, const char *, void *, const void *, int, int, void *,
                       gn_error **);
    void *(*call_fds)(void *, const char *, const char *, const char *, const char *, void *, const void *, int, int, void *,
                      void **, void *, gn_error **);
    unsigned (*subscribe)(void *, const char *, const char *, const char *, const char *, const char *, int, gn_signal_cb, void *,
                          void *);
    void (*unsubscribe)(void *, unsigned);
    const char *(*unique_name)(void *);
    int (*fd_list_get)(void *, int, gn_error **);
    void (*gobj_unref)(void *);
    void *(*new_parsed)(const char *, ...);
    void (*variant_get)(void *, const char *, ...);
    void *(*child)(void *, size_t);
    size_t (*n_children)(void *);
    void (*unref)(void *);
    const char *(*get_string)(void *, size_t *);
    int (*lookup)(void *, const char *, const char *, ...);
    int (*iteration)(void *, int);
    void (*error_free)(gn_error *);

    void (*gst_init)(int *, char ***);
    void *(*parse_launch)(const char *, gn_error **);
    void *(*bin_get_by_name)(void *, const char *);
    int (*set_state)(void *, int);
    void *(*try_pull_sample)(void *, uint64_t);
    void *(*sample_get_buffer)(void *);
    void *(*sample_get_caps)(void *);
    void *(*caps_get_structure)(void *, unsigned);
    int (*structure_get_int)(void *, const char *, int *);
    int (*buffer_map)(void *, gn_mapinfo *, int);
    void (*buffer_unmap)(void *, gn_mapinfo *);
    void (*mini_unref)(void *);
    void (*obj_unref)(void *);
} gn_f;

typedef struct {
    char conn[64], path[256];
    unsigned node, sub;
    int x, y, w, h; // logical spot of the stream
    void *pipe, *sink;
    uint32_t *frame; // newest frame as 0xFFRRGGBB, stride fw
    int fw, fh;
} gn_stream;

enum { GN_IDLE, GN_NODES, GN_CREATE, GN_SELECT, GN_START, GN_RUN };

static struct {
    int loaded, gst_ready, portal, step, tries, dead, opening, ns, pw_fd, fps, reqs, grabs, restart_at;
    int changed, resp_got;
    long changed_at, next_at, deadline;
    void *bus, *resp;
    unsigned mon_sub, session_sub, req_sub, resp_code, ver, cursors;
    char session[256], token[128], token_path[1100], why[256];
    gn_stream s[GM_MAX_MON];
    uint32_t *out[GM_MAX_MON];
    size_t outn[GM_MAX_MON];
} gn = {.pw_fd = -1};

#define GN_MUTTER "org.gnome.Mutter.ScreenCast"
#define GN_PORTAL "org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop"

static int gn_load(goome *g)
{
    if (gn.loaded) return gn.loaded > 0 ? GOOME_OK : gm_fail(g, GOOME_ENOTSUP, "%s", gn.why);
    void *gio = dlopen("libgio-2.0.so.0", RTLD_NOW | RTLD_GLOBAL);
    void *glib = dlopen("libglib-2.0.so.0", RTLD_NOW | RTLD_GLOBAL);
    void *gst = dlopen("libgstreamer-1.0.so.0", RTLD_NOW | RTLD_GLOBAL);
    void *app = dlopen("libgstapp-1.0.so.0", RTLD_NOW | RTLD_GLOBAL);
    gn.loaded = -1;
    if (!gio || !glib) {
        snprintf(gn.why, sizeof(gn.why), "cant find libgio-2.0.so.0 / libglib-2.0.so.0");
        return gm_fail(g, GOOME_ENOTSUP, "%s", gn.why);
    }
    if (!gst || !app) {
        snprintf(gn.why, sizeof(gn.why), "cant find gstreamer, install libgstreamer-1.0 and libgstapp-1.0");
        return gm_fail(g, GOOME_ENOTSUP, "%s", gn.why);
    }
#define GN_L(lib, field, name)                                                             \
    do {                                                                                   \
        *(void **)(&gn_f.field) = dlsym(lib, name);                                        \
        if (!gn_f.field) {                                                                 \
            snprintf(gn.why, sizeof(gn.why), "%s is missing from glib or gstreamer", name); \
            return gm_fail(g, GOOME_ENOTSUP, "%s", gn.why);                                \
        }                                                                                  \
    } while (0)
    GN_L(gio, bus_get_sync, "g_bus_get_sync");
    GN_L(gio, call_sync, "g_dbus_connection_call_sync");
    GN_L(gio, call_fds, "g_dbus_connection_call_with_unix_fd_list_sync");
    GN_L(gio, subscribe, "g_dbus_connection_signal_subscribe");
    GN_L(gio, unsubscribe, "g_dbus_connection_signal_unsubscribe");
    GN_L(gio, unique_name, "g_dbus_connection_get_unique_name");
    GN_L(gio, fd_list_get, "g_unix_fd_list_get");
    GN_L(gio, gobj_unref, "g_object_unref");
    GN_L(glib, new_parsed, "g_variant_new_parsed");
    GN_L(glib, variant_get, "g_variant_get");
    GN_L(glib, child, "g_variant_get_child_value");
    GN_L(glib, n_children, "g_variant_n_children");
    GN_L(glib, unref, "g_variant_unref");
    GN_L(glib, get_string, "g_variant_get_string");
    GN_L(glib, lookup, "g_variant_lookup");
    GN_L(glib, iteration, "g_main_context_iteration");
    GN_L(glib, error_free, "g_error_free");
    GN_L(gst, gst_init, "gst_init");
    GN_L(gst, parse_launch, "gst_parse_launch");
    GN_L(gst, bin_get_by_name, "gst_bin_get_by_name");
    GN_L(gst, set_state, "gst_element_set_state");
    GN_L(gst, sample_get_buffer, "gst_sample_get_buffer");
    GN_L(gst, sample_get_caps, "gst_sample_get_caps");
    GN_L(gst, caps_get_structure, "gst_caps_get_structure");
    GN_L(gst, structure_get_int, "gst_structure_get_int");
    GN_L(gst, buffer_map, "gst_buffer_map");
    GN_L(gst, buffer_unmap, "gst_buffer_unmap");
    GN_L(gst, mini_unref, "gst_mini_object_unref");
    GN_L(gst, obj_unref, "gst_object_unref");
    GN_L(app, try_pull_sample, "gst_app_sink_try_pull_sample");
#undef GN_L
    gn_error *err = NULL;
    gn.bus = gn_f.bus_get_sync(2, NULL, &err); // G_BUS_TYPE_SESSION
    if (!gn.bus) {
        snprintf(gn.why, sizeof(gn.why), "cant reach the session bus: %s", err ? err->message : "?");
        if (err) gn_f.error_free(err);
        return gm_fail(g, GOOME_ENOTSUP, "%s", gn.why);
    }
    gn.loaded = 1;
    return GOOME_OK;
}

static void *gn_call(goome *g, const char *dest, const char *path, const char *iface, const char *method, void *args, int ms)
{
    gn_error *err = NULL;
    void *r = gn_f.call_sync(gn.bus, dest, path, iface, method, args, NULL, 0, ms, NULL, &err);
    if (!r) {
        if (g) gm_fail(g, GOOME_ENOTSUP, "%s failed: %s", method, err ? err->message : "?");
        if (err) gn_f.error_free(err);
    }
    return r;
}

static void gn_on_change(void *c, const char *sender, const char *path, const char *iface, const char *sig, void *params, void *user)
{
    gn.changed = 1;
    gn.changed_at = goome_now_ms();
}

static void gn_on_stream(void *c, const char *sender, const char *path, const char *iface, const char *sig, void *params, void *user)
{
    for (int i = 0; i < gn.ns; i++)
        if (!strcmp(gn.s[i].path, path)) gn_f.variant_get(params, "(u)", &gn.s[i].node);
}

static void gn_on_response(void *c, const char *sender, const char *path, const char *iface, const char *sig, void *params, void *user)
{
    if (gn.resp) gn_f.unref(gn.resp);
    gn.resp = NULL;
    gn_f.variant_get(params, "(u@a{sv})", &gn.resp_code, &gn.resp);
    gn.resp_got = 1;
}

static void gn_stop(void)
{
    for (int i = 0; i < gn.ns; i++) {
        gn_stream *s = &gn.s[i];
        if (s->sub) gn_f.unsubscribe(gn.bus, s->sub);
        if (s->pipe) {
            gn_f.set_state(s->pipe, 1); // GST_STATE_NULL before the unref
            if (s->sink) gn_f.obj_unref(s->sink);
            gn_f.obj_unref(s->pipe);
        }
        free(s->frame);
    }
    if (gn.req_sub) gn_f.unsubscribe(gn.bus, gn.req_sub);
    if (gn.session_sub) gn_f.unsubscribe(gn.bus, gn.session_sub); // before Stop, or its Closed kicks off another restart
    gn.req_sub = gn.session_sub = 0;
    if (gn.session[0]) {
        void *r = gn.portal ? gn_call(NULL, "org.freedesktop.portal.Desktop", gn.session, "org.freedesktop.portal.Session", "Close", NULL, 2000)
                            : gn_call(NULL, GN_MUTTER, gn.session, GN_MUTTER ".Session", "Stop", NULL, 2000);
        if (r) gn_f.unref(r);
    }
    gn.session[0] = 0;
    if (gn.pw_fd >= 0) close(gn.pw_fd);
    gn.pw_fd = -1;
    if (gn.resp) gn_f.unref(gn.resp);
    gn.resp = NULL;
    memset(gn.s, 0, sizeof(gn.s));
    gn.ns = 0;
    gn.step = GN_IDLE;
}

static int gn_pipes(goome *g)
{
    for (int i = 0; i < gn.ns; i++) {
        gn_stream *s = &gn.s[i];
        char desc[512], fd[24] = "";
        if (gn.pw_fd >= 0) snprintf(fd, sizeof(fd), "fd=%d ", gn.pw_fd);
        snprintf(desc, sizeof(desc),
                 "pipewiresrc %spath=%u always-copy=true ! videorate drop-only=true max-rate=%d ! videoconvert ! "
                 "video/x-raw,format=BGRx ! appsink name=sink max-buffers=1 drop=true sync=false",
                 fd, s->node, gn.fps);
        gn_error *e = NULL;
        s->pipe = gn_f.parse_launch(desc, &e);
        if (!s->pipe) {
            gm_fail(g, GOOME_ENOTSUP, "cant start gstreamer: %s (is gstreamer1.0-pipewire installed?)", e ? e->message : "?");
            if (e) gn_f.error_free(e);
            return GOOME_ENOTSUP;
        }
        if (e) gn_f.error_free(e);
        s->sink = gn_f.bin_get_by_name(s->pipe, "sink");
        if (!s->sink || !gn_f.set_state(s->pipe, 4)) // GST_STATE_PLAYING, 0 = GST_STATE_CHANGE_FAILURE
            return gm_fail(g, GOOME_ENOTSUP, "gstreamer wont play the stream for %s", s->conn);
    }
    gn.step = GN_RUN;
    gn.tries = 0;
    return GOOME_OK;
}

static int gn_mutter_begin(goome *g)
{
    void *st = gn_call(g, "org.gnome.Mutter.DisplayConfig", "/org/gnome/Mutter/DisplayConfig", "org.gnome.Mutter.DisplayConfig",
                       "GetCurrentState", NULL, 5000);
    if (!st) return GOOME_ENOTSUP;
    void *mons = gn_f.child(st, 1); // (serial, monitors, logical monitors, props)
    size_t nm = gn_f.n_children(mons);
    for (size_t i = 0; i < nm && gn.ns < GM_MAX_MON; i++) {
        void *m = gn_f.child(mons, i), *spec = gn_f.child(m, 0), *conn = gn_f.child(spec, 0);
        snprintf(gn.s[gn.ns].conn, sizeof(gn.s[0].conn), "%s", gn_f.get_string(conn, NULL));
        gn_f.unref(conn);
        gn_f.unref(spec);
        void *modes = gn_f.child(m, 1);
        int current = 0;
        size_t nmodes = gn_f.n_children(modes);
        for (size_t k = 0; k < nmodes; k++) {
            void *mode = gn_f.child(modes, k), *props = gn_f.child(mode, 6);
            int is_cur = 0;
            if (gn_f.lookup(props, "is-current", "b", &is_cur) && is_cur) current = 1;
            gn_f.unref(props);
            gn_f.unref(mode);
        }
        gn_f.unref(modes);
        gn_f.unref(m);
        if (current) gn.ns++; // no current mode = plugged in but off
    }
    gn_f.unref(mons);
    gn_f.unref(st);
    if (!gn.ns) return gm_fail(g, GOOME_ENOTSUP, "mutter says no monitor is on");

    void *r = gn_call(g, GN_MUTTER, "/org/gnome/Mutter/ScreenCast", GN_MUTTER, "CreateSession", gn_f.new_parsed("(@a{sv} {},)"), 5000);
    if (!r) return GOOME_ENOTSUP;
    const char *sp = NULL;
    gn_f.variant_get(r, "(&o)", &sp);
    snprintf(gn.session, sizeof(gn.session), "%s", sp ? sp : "");
    gn_f.unref(r);
    for (int i = 0; i < gn.ns; i++) {
        gn_stream *s = &gn.s[i];
        r = gn_call(g, GN_MUTTER, gn.session, GN_MUTTER ".Session", "RecordMonitor",
                    gn_f.new_parsed("(%s, {'cursor-mode': <uint32 0>})", s->conn), 5000); // 0 = hidden
        if (!r) return GOOME_ENOTSUP;
        gn_f.variant_get(r, "(&o)", &sp);
        snprintf(s->path, sizeof(s->path), "%s", sp ? sp : "");
        gn_f.unref(r);
        // before Start or the node id can go by before we listen
        s->sub = gn_f.subscribe(gn.bus, GN_MUTTER, GN_MUTTER ".Stream", "PipeWireStreamAdded", s->path, NULL, 0, gn_on_stream, NULL, NULL);
    }
    gn.session_sub = gn_f.subscribe(gn.bus, GN_MUTTER, GN_MUTTER ".Session", "Closed", gn.session, NULL, 0, gn_on_change, NULL, NULL);
    r = gn_call(g, GN_MUTTER, gn.session, GN_MUTTER ".Session", "Start", NULL, 5000);
    if (!r) return GOOME_ENOTSUP;
    gn_f.unref(r);
    gn.step = GN_NODES;
    gn.deadline = goome_now_ms() + 5000;
    return GOOME_OK;
}

static int gn_mutter_finish(goome *g)
{
    for (int i = 0; i < gn.ns; i++) {
        gn_stream *s = &gn.s[i];
        void *r = gn_call(NULL, GN_MUTTER, s->path, "org.freedesktop.DBus.Properties", "Get",
                          gn_f.new_parsed("('org.gnome.Mutter.ScreenCast.Stream', 'Parameters')"), 5000);
        if (!r) continue;
        void *v = gn_f.child(r, 0), *dict = NULL;
        gn_f.variant_get(v, "v", &dict);
        if (dict) {
            gn_f.lookup(dict, "position", "(ii)", &s->x, &s->y);
            gn_f.lookup(dict, "size", "(ii)", &s->w, &s->h);
            gn_f.unref(dict);
        }
        gn_f.unref(v);
        gn_f.unref(r);
    }
    return gn_pipes(g);
}

// portal calls answer later with a Response signal on a path built from our bus name
static int gn_ask(goome *g, const char *method, void *args, const char *tok, int wait_ms)
{
    char path[256], sender[64];
    snprintf(sender, sizeof(sender), "%s", gn_f.unique_name(gn.bus) + 1); // ":1.42" -> "1_42"
    for (char *q = sender; *q; q++)
        if (*q == '.') *q = '_';
    snprintf(path, sizeof(path), "/org/freedesktop/portal/desktop/request/%s/%s", sender, tok);
    if (gn.req_sub) gn_f.unsubscribe(gn.bus, gn.req_sub);
    if (gn.resp) gn_f.unref(gn.resp);
    gn.resp = NULL;
    gn.resp_got = 0;
    gn.req_sub = gn_f.subscribe(gn.bus, "org.freedesktop.portal.Desktop", "org.freedesktop.portal.Request", "Response", path, NULL, 0,
                                gn_on_response, NULL, NULL);
    void *r = gn_call(g, GN_PORTAL, "org.freedesktop.portal.ScreenCast", method, args, 30000);
    if (!r) return GOOME_ENOTSUP;
    gn_f.unref(r);
    gn.deadline = goome_now_ms() + wait_ms;
    return GOOME_OK;
}

// 0 still waiting, 1 got a yes (gn.resp), <0 error
static int gn_answer(goome *g, const char *method)
{
    if (!gn.resp_got) {
        if (goome_now_ms() > gn.deadline) return gm_fail(g, GOOME_ENOTSUP, "the screen sharing portal never answered %s", method);
        return 0;
    }
    gn_f.unsubscribe(gn.bus, gn.req_sub);
    gn.req_sub = 0;
    gn.resp_got = 0;
    if (gn.resp_code == 1) return gm_fail(g, GOOME_EDENIED, "screen sharing was turned down in the portal popup");
    if (gn.resp_code || !gn.resp) return gm_fail(g, GOOME_ENOTSUP, "the screen sharing portal gave up on %s (code %u)", method, gn.resp_code);
    return 1;
}

static unsigned gn_portal_prop(const char *name, int ms)
{
    unsigned v = 0;
    void *r = gn_call(NULL, GN_PORTAL, "org.freedesktop.DBus.Properties", "Get",
                      gn_f.new_parsed("('org.freedesktop.portal.ScreenCast', %s)", name), ms);
    if (!r) return 0;
    void *box = gn_f.child(r, 0), *in = NULL;
    gn_f.variant_get(box, "v", &in);
    if (in) {
        gn_f.variant_get(in, "u", &v);
        gn_f.unref(in);
    }
    gn_f.unref(box);
    gn_f.unref(r);
    return v;
}

static void gn_tok(char *out, size_t n)
{
    snprintf(out, n, "goome%d_%d", (int)getpid(), ++gn.reqs);
}

static int gn_portal_begin(goome *g)
{
    gn.ver = gn_portal_prop("version", 30000);
    gn.cursors = gn_portal_prop("AvailableCursorModes", 30000);
    if (!gn.ver) return gm_fail(g, GOOME_ENOTSUP, "no screencast portal, is xdg-desktop-portal running?");
    if (gn.ver >= 4 && !gn.token[0] && gn.token_path[0]) {
        int fd = open(gn.token_path, O_RDONLY | O_CLOEXEC);
        if (fd >= 0) {
            ssize_t n = read(fd, gn.token, sizeof(gn.token) - 1);
            gn.token[n > 0 ? n : 0] = 0;
            close(fd);
        }
    }
    for (char *q = gn.token; *q; q++) // its a uuid, anything else could break the gvariant text
        if (!((*q >= 'a' && *q <= 'z') || (*q >= 'A' && *q <= 'Z') || (*q >= '0' && *q <= '9') || *q == '-' || *q == '_')) *q = 0;
    char tok[48], stok[48];
    gn_tok(tok, sizeof(tok));
    gn_tok(stok, sizeof(stok));
    int e = gn_ask(g, "CreateSession", gn_f.new_parsed("({'handle_token': <%s>, 'session_handle_token': <%s>},)", tok, stok), tok, 30000);
    if (e) return e;
    gn.step = GN_CREATE;
    return GOOME_OK;
}

static int gn_portal_select(goome *g)
{
    const char *sp = NULL;
    gn_f.lookup(gn.resp, "session_handle", "&s", &sp);
    snprintf(gn.session, sizeof(gn.session), "%s", sp ? sp : "");
    if (!gn.session[0]) return gm_fail(g, GOOME_ENOTSUP, "the portal made no screencast session");
    gn.session_sub = gn_f.subscribe(gn.bus, "org.freedesktop.portal.Desktop", "org.freedesktop.portal.Session", "Closed", gn.session, NULL,
                                    0, gn_on_change, NULL, NULL);
    char opts[512], fmt[700], tok[48];
    int n = snprintf(opts, sizeof(opts), "'types': <uint32 1>, 'multiple': <true>"); // 1 = monitors
    if (gn.cursors & 1) n += snprintf(opts + n, sizeof(opts) - n, ", 'cursor_mode': <uint32 1>"); // hidden
    if (gn.ver >= 4) n += snprintf(opts + n, sizeof(opts) - n, ", 'persist_mode': <uint32 2>"); // until revoked
    if (gn.ver >= 4 && gn.token[0]) snprintf(opts + n, sizeof(opts) - n, ", 'restore_token': <'%s'>", gn.token);
    snprintf(fmt, sizeof(fmt), "(%%o, {'handle_token': <%%s>, %s})", opts);
    gn_tok(tok, sizeof(tok));
    int e = gn_ask(g, "SelectSources", gn_f.new_parsed(fmt, gn.session, tok), tok, 30000);
    if (e) return e;
    gn.step = GN_SELECT;
    return GOOME_OK;
}

static int gn_portal_start(goome *g)
{
    char tok[48];
    gn_tok(tok, sizeof(tok));
    int e = gn_ask(g, "Start", gn_f.new_parsed("(%o, '', {'handle_token': <%s>})", gn.session, tok), tok, 300000); // a person has to click
    if (e) return e;
    gn.step = GN_START;
    return GOOME_OK;
}

static void gn_save_token(const char *t)
{
    if (!t || !*t || strlen(t) >= sizeof(gn.token) || !strcmp(t, gn.token)) return;
    snprintf(gn.token, sizeof(gn.token), "%s", t); // kept for restarts even without state_dir
    if (!gn.token_path[0]) return;
    char tmp[1110];
    snprintf(tmp, sizeof(tmp), "%s.tmp", gn.token_path);
    int fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
    if (fd < 0) return;
    size_t n = strlen(t);
    int ok = write(fd, t, n) == (ssize_t)n;
    if (close(fd) || !ok || rename(tmp, gn.token_path)) unlink(tmp);
}

static int gn_portal_finish(goome *g)
{
    const char *nt = NULL;
    if (gn_f.lookup(gn.resp, "restore_token", "&s", &nt)) gn_save_token(nt);
    void *list = NULL;
    gn_f.lookup(gn.resp, "streams", "@a(ua{sv})", &list);
    size_t ns = list ? gn_f.n_children(list) : 0;
    for (size_t i = 0; i < ns && gn.ns < GM_MAX_MON; i++) {
        gn_stream *s = &gn.s[gn.ns];
        void *st = gn_f.child(list, i), *props = NULL;
        gn_f.variant_get(st, "(u@a{sv})", &s->node, &props);
        if (props) {
            gn_f.lookup(props, "position", "(ii)", &s->x, &s->y);
            gn_f.lookup(props, "size", "(ii)", &s->w, &s->h);
            gn_f.unref(props);
        }
        gn_f.unref(st);
        snprintf(s->conn, sizeof(s->conn), "screen %d", gn.ns + 1);
        gn.ns++;
    }
    if (list) gn_f.unref(list);
    if (!gn.ns) return gm_fail(g, GOOME_EDENIED, "the portal shared no screens");

    void *fds = NULL;
    gn_error *err = NULL;
    void *r = gn_f.call_fds(gn.bus, GN_PORTAL, "org.freedesktop.portal.ScreenCast", "OpenPipeWireRemote",
                            gn_f.new_parsed("(%o, @a{sv} {})", gn.session), NULL, 0, 30000, NULL, &fds, NULL, &err);
    if (!r) {
        gm_fail(g, GOOME_ENOTSUP, "OpenPipeWireRemote failed: %s", err ? err->message : "?");
        if (err) gn_f.error_free(err);
        return GOOME_ENOTSUP;
    }
    int idx = -1;
    gn_f.variant_get(r, "(h)", &idx);
    gn_f.unref(r);
    if (fds) {
        gn.pw_fd = idx >= 0 ? gn_f.fd_list_get(fds, idx, NULL) : -1;
        gn_f.gobj_unref(fds);
    }
    if (gn.pw_fd < 0) return gm_fail(g, GOOME_ENOTSUP, "the portal gave no pipewire connection");
    return gn_pipes(g);
}

static int gn_failed(goome *g, int e)
{
    gn_stop();
    if (gn.opening || e == GOOME_EDENIED) return e;
    // mutter can still be mid reconfigure, 3 quick tries then one ~4.7s out
    gn.next_at = goome_now_ms() + (++gn.tries < 3 ? 300 : 4700);
    if (gn.tries >= 3) gn.tries = 0;
    return GOOME_OK;
}

// moves the session along without blocking, <0 = broken for good
static int gn_step(goome *g)
{
    while (gn_f.iteration(NULL, 0)) {} // dbus signals only run from the main context
    long now = goome_now_ms();
    if (gn.changed && now - gn.changed_at > 700) { // monitor changes come in bursts, let it settle
        gn.changed = 0;
        gn_stop();
        gn.tries = 0;
        gn.next_at = now;
    }
    int e = GOOME_OK, a;
    switch (gn.step) {
    case GN_IDLE:
        if (now >= gn.next_at) e = gn.portal ? gn_portal_begin(g) : gn_mutter_begin(g);
        break;
    case GN_NODES: {
        int missing = 0;
        for (int i = 0; i < gn.ns; i++)
            if (!gn.s[i].node) missing = 1;
        if (!missing) e = gn_mutter_finish(g);
        else if (now > gn.deadline) e = gm_fail(g, GOOME_ENOTSUP, "mutter never sent a pipewire stream");
        break;
    }
    case GN_CREATE:
        if ((a = gn_answer(g, "CreateSession"))) e = a < 0 ? a : gn_portal_select(g);
        break;
    case GN_SELECT:
        if ((a = gn_answer(g, "SelectSources"))) e = a < 0 ? a : gn_portal_start(g);
        break;
    case GN_START:
        if ((a = gn_answer(g, "Start"))) e = a < 0 ? a : gn_portal_finish(g);
        break;
    }
    if (e < 0) {
        e = gn_failed(g, e);
        if (e < 0 && !gn.opening) {
            gn.dead = e;
            snprintf(gn.why, sizeof(gn.why), "%s", g->err);
        }
    }
    return e;
}

static int gn_open(goome *g, int portal)
{
    int e = gn_load(g);
    if (e) return e;
    if (!gn.gst_ready) gn_f.gst_init(NULL, NULL);
    gn.gst_ready = 1;
    gn.portal = portal;
    gn.dead = gn.changed = gn.tries = gn.grabs = 0;
    gn.fps = g->opts.capture_fps > 0 ? g->opts.capture_fps : 5;
    gn.token[0] = gn.token_path[0] = 0;
    if (g->state_dir[0]) snprintf(gn.token_path, sizeof(gn.token_path), "%s/portal-token", g->state_dir);
    const char *at = gm_env("GOOME_GNOME_RESTART_AT"); // test hook, fakes a monitor change after n grabs
    gn.restart_at = at ? gm_num(&at) : 0;
    if (!portal && !gn.mon_sub)
        gn.mon_sub = gn_f.subscribe(gn.bus, "org.gnome.Mutter.DisplayConfig", "org.gnome.Mutter.DisplayConfig", "MonitorsChanged",
                                    "/org/gnome/Mutter/DisplayConfig", NULL, 0, gn_on_change, NULL, NULL);
    gn.opening = 1;
    gn.step = GN_IDLE;
    gn.next_at = 0;
    while (gn.step != GN_RUN) {
        e = gn_step(g);
        if (e < 0) break;
        if (gn.step != GN_RUN) nanosleep(&(struct timespec){0, 5000000L}, NULL);
    }
    gn.opening = 0;
    if (e < 0 && gn.mon_sub) {
        gn_f.unsubscribe(gn.bus, gn.mon_sub);
        gn.mon_sub = 0;
    }
    return e < 0 ? e : GOOME_OK;
}

static void gn_close(goome *g)
{
    if (gn.loaded <= 0) return;
    gn_stop();
    if (gn.mon_sub) gn_f.unsubscribe(gn.bus, gn.mon_sub);
    gn.mon_sub = 0;
    for (int i = 0; i < GM_MAX_MON; i++) {
        free(gn.out[i]);
        gn.out[i] = NULL;
        gn.outn[i] = 0;
    }
}

static int gn_pull(gn_stream *s, uint64_t timeout_ns)
{
    void *smp = gn_f.try_pull_sample(s->sink, timeout_ns);
    if (!smp) return 0;
    void *caps = gn_f.sample_get_caps(smp), *st = caps ? gn_f.caps_get_structure(caps, 0) : NULL;
    int w = 0, h = 0;
    if (st) {
        gn_f.structure_get_int(st, "width", &w);
        gn_f.structure_get_int(st, "height", &h);
    }
    void *buf = gn_f.sample_get_buffer(smp);
    gn_mapinfo mi;
    if (w > 0 && h > 0 && buf && gn_f.buffer_map(buf, &mi, 1)) { // GST_MAP_READ
        size_t stride = (size_t)w * 4;
        if (mi.size % (size_t)h == 0 && mi.size / (size_t)h > stride) stride = mi.size / (size_t)h; // padded rows from the gpu
        if (mi.size >= stride * (size_t)(h - 1) + (size_t)w * 4) {
            if (w != s->fw || h != s->fh) {
                free(s->frame);
                s->frame = malloc((size_t)w * h * 4);
                s->fw = s->frame ? w : 0;
                s->fh = s->frame ? h : 0;
            }
            for (int y = 0; s->frame && y < h; y++) {
                const uint8_t *p = mi.data + (size_t)y * stride;
                uint32_t *o = s->frame + (size_t)y * w;
                for (int x = 0; x < w; x++, p += 4) o[x] = 0xFF000000u | (uint32_t)p[2] << 16 | (uint32_t)p[1] << 8 | p[0]; // BGRx
            }
        }
        gn_f.buffer_unmap(buf, &mi);
    }
    gn_f.mini_unref(smp);
    return 1;
}

static double gn_iou(goome_rect a, double x, double y, double w, double h)
{
    double ix0 = a.x > x ? a.x : x, iy0 = a.y > y ? a.y : y;
    double ix1 = a.x + a.w < x + w ? a.x + a.w : x + w, iy1 = a.y + a.h < y + h ? a.y + a.h : y + h;
    if (ix1 <= ix0 || iy1 <= iy0) return 0;
    double in = (ix1 - ix0) * (iy1 - iy0);
    return in / ((double)a.w * a.h + w * h - in);
}

// overlay areas can be in a scaled space (xwayland), so also try the stream scaled to the monitor width
static gn_stream *gn_match(goome_rect a)
{
    gn_stream *best = NULL;
    double bs = 0.5;
    for (int i = 0; i < gn.ns; i++) {
        gn_stream *s = &gn.s[i];
        int w = s->w > 0 ? s->w : s->fw, h = s->h > 0 ? s->h : s->fh;
        if (w <= 0 || h <= 0) continue;
        double k = (double)a.w / w;
        double v = gn_iou(a, s->x, s->y, w, h), v2 = gn_iou(a, s->x * k, s->y * k, w * k, h * k);
        if (v2 > v) v = v2;
        if (v > bs) bs = v, best = s;
    }
    return best;
}

static int gn_grab(goome *g, int mon, goome_rect r, goome_image *img)
{
    if (gn.dead) return gm_fail(g, gn.dead, "%s", gn.why);
    if (gn.restart_at && ++gn.grabs == gn.restart_at) gn.changed = 1, gn.changed_at = goome_now_ms();
    int e = gn_step(g);
    if (e < 0) return e;
    if (gn.step != GN_RUN) return gm_fail(g, GOOME_EAGAIN, "the screen streams are restarting");
    goome_rect a = g->mon[mon].area;
    gn_stream *s = gn_match(a);
    if (!s) {
        if (gn.changed) return gm_fail(g, GOOME_EAGAIN, "the screen streams are restarting");
        return gm_fail(g, GOOME_EGONE, "no screen stream covers monitor %d (%s)", mon, g->mon[mon].name);
    }
    uint64_t timeout = s->frame ? 0 : 20000000ULL;
    while (gn_pull(s, timeout)) timeout = 0;
    if (!s->frame) return gm_fail(g, GOOME_EAGAIN, "no frame from %s yet", s->conn);
    double sx = (double)s->fw / a.w, sy = (double)s->fh / a.h;
    int x0 = (int)((r.x - a.x) * sx + 0.5), y0 = (int)((r.y - a.y) * sy + 0.5);
    int x1 = (int)((r.x + r.w - a.x) * sx + 0.5), y1 = (int)((r.y + r.h - a.y) * sy + 0.5);
    if (x1 > s->fw) x1 = s->fw;
    if (y1 > s->fh) y1 = s->fh;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    int w = x1 - x0, h = y1 - y0;
    if (w <= 0 || h <= 0) return gm_fail(g, GOOME_EARG, "that rect is too small to grab");
    size_t need = (size_t)w * h;
    if (need > gn.outn[mon]) {
        uint32_t *p = realloc(gn.out[mon], need * 4);
        if (!p) return gm_fail(g, GOOME_ENOMEM, "no memory for a %dx%d grab", w, h);
        gn.out[mon] = p;
        gn.outn[mon] = need;
    }
    for (int y = 0; y < h; y++) memcpy(gn.out[mon] + (size_t)y * w, s->frame + (size_t)(y0 + y) * s->fw + x0, (size_t)w * 4);
    img->px = gn.out[mon];
    img->w = w;
    img->h = h;
    img->stride = w;
    img->area = r;
    img->has_self = 1;
    img->has_cursor = 0;
    return GOOME_OK;
}

static int gn_name_owned(const char *name)
{
    void *r = gn_call(NULL, "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "NameHasOwner",
                      gn_f.new_parsed("(%s,)", name), 2000);
    int has = 0;
    if (r) {
        gn_f.variant_get(r, "(b)", &has);
        gn_f.unref(r);
    }
    return has;
}

static int gn_mutter_probe(goome *g) { return gn_load(g) == GOOME_OK && gn_name_owned(GN_MUTTER); }
static int gn_portal_probe(goome *g) { return gn_load(g) == GOOME_OK && gn_portal_prop("version", 5000) > 0; }
static int gn_mutter_open(goome *g) { return gn_open(g, 0); }
static int gn_portal_open(goome *g) { return gn_open(g, 1); }

static const gm_cap gn_mutter_cap = {"mutter", gn_mutter_probe, gn_mutter_open, gn_grab, gn_close, GOOME_CAP_CAPTURE};
static const gm_cap gn_portal_cap = {"portal", gn_portal_probe, gn_portal_open, gn_grab, gn_close, GOOME_CAP_CAPTURE};
const gm_cap *gm_cap_mutter(void) { return &gn_mutter_cap; }
const gm_cap *gm_cap_portal(void) { return &gn_portal_cap; }

#undef GN_MUTTER
#undef GN_PORTAL

#endif
