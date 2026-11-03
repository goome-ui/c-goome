#include "internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#endif

const char *gm_env(const char *name)
{
    const char *v = getenv(name);
    return v && *v ? v : NULL;
}

int gm_num(const char **p)
{
    int neg = **p == '-', v = 0;
    if (neg) (*p)++;
    while (**p >= '0' && **p <= '9' && v < 100000000) v = v * 10 + *(*p)++ - '0';
    return neg ? -v : v;
}

#ifdef _WIN32
long goome_now_ms(void)
{
    static LARGE_INTEGER f, t0;
    LARGE_INTEGER t;
    if (!f.QuadPart) QueryPerformanceFrequency(&f), QueryPerformanceCounter(&t0);
    QueryPerformanceCounter(&t);
    return (long)((t.QuadPart - t0.QuadPart) * 1000 / f.QuadPart);
}
#else
long goome_now_ms(void)
{
    static long t0;
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    long ms = (long)t.tv_sec * 1000 + t.tv_nsec / 1000000;
    if (!t0) t0 = ms - 1;
    return ms - t0;
}
#endif

int goome_pace(goome_pacer *p, int fps)
{
    if (fps <= 0) fps = 60;
    long now = goome_now_ms(), step = 1000 / fps;
    if (!p->next || p->fps != fps || now - p->next > step * 4) p->next = now, p->fps = fps; // way behind, dont try to catch up
    if (now < p->next) return (int)(p->next - now);
    p->next += step;
    return 0;
}

void gm_push(goome *g, const goome_event *e)
{
    if (g->qlen == GM_QLEN) { // full. drop a motion
        for (int i = 0; i < g->qlen; i++) {
            int k = (g->qhead + i) % GM_QLEN;
            if (g->q[k].type == GOOME_MOTION) {
                for (int j = i; j < g->qlen - 1; j++) g->q[(g->qhead + j) % GM_QLEN] = g->q[(g->qhead + j + 1) % GM_QLEN];
                g->qlen--;
                break;
            }
        }
        if (g->qlen == GM_QLEN) return;
    }
    g->q[(g->qhead + g->qlen++) % GM_QLEN] = *e;
}

int gm_fail(goome *g, int code, const char *fmt, ...)
{
    if (!g) return code;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(g->err, sizeof(g->err), fmt, ap);
    va_end(ap);
    return code;
}

void gm_set_monitors(goome *g, const goome_monitor *m, int n)
{
    if (n > GM_MAX_MON) n = GM_MAX_MON;
    if (n == g->nmon && !memcmp(m, g->mon, sizeof(*m) * (size_t)n)) return;
    int had = g->nmon > 0;
    memcpy(g->mon, m, sizeof(*m) * (size_t)n);
    for (int i = n; i < GM_MAX_MON; i++) g->cv[i] = NULL;
    g->nmon = n;
    if (had) {
        goome_event e = {.type = GOOME_MONITORS};
        gm_push(g, &e);
    }
}

int gm_clip(const goome_monitor *m, const goome_rect *in, goome_rect *out)
{
    goome_rect a = m->area;
    if (!in) {
        *out = a;
        return 1;
    }
    int x0 = in->x > a.x ? in->x : a.x, y0 = in->y > a.y ? in->y : a.y;
    int x1 = in->x + in->w < a.x + a.w ? in->x + in->w : a.x + a.w, y1 = in->y + in->h < a.y + a.h ? in->y + in->h : a.y + a.h;
    if (x1 <= x0 || y1 <= y0) return 0;
    *out = (goome_rect){x0, y0, x1 - x0, y1 - y0};
    return 1;
}

void gm_canvas_reset(goome_canvas *c, uint32_t *px, int w, int h, int stride, double ox, double oy, double scale, int mon)
{
    memset(c, 0, sizeof(*c));
    c->px = px, c->w = w, c->h = h, c->stride = stride, c->ox = ox, c->oy = oy, c->scale = scale > 0 ? scale : 1, c->mon = mon;
    c->cx1 = w, c->cy1 = h;
    c->kx1 = w, c->ky1 = h;
}

int gm_canvas_damage(const goome_canvas *c, int *x0, int *y0, int *x1, int *y1)
{
    int a = c->dx1 > c->dx0, b = c->cx1 > c->cx0;
    if (!a && !b) return 0;
    *x0 = a ? c->dx0 : c->cx0, *y0 = a ? c->dy0 : c->cy0, *x1 = a ? c->dx1 : c->cx1, *y1 = a ? c->dy1 : c->cy1;
    if (a && b) {
        if (c->cx0 < *x0) *x0 = c->cx0;
        if (c->cy0 < *y0) *y0 = c->cy0;
        if (c->cx1 > *x1) *x1 = c->cx1;
        if (c->cy1 > *y1) *y1 = c->cy1;
    }
    if (*x0 < 0) *x0 = 0;
    if (*y0 < 0) *y0 = 0;
    if (*x1 > c->w) *x1 = c->w;
    if (*y1 > c->h) *y1 = c->h;
    return *x1 > *x0 && *y1 > *y0;
}

void gm_canvas_clear(goome_canvas *c)
{
    c->cx0 = c->dx0, c->cy0 = c->dy0, c->cx1 = c->dx1, c->cy1 = c->dy1; // wiped bits still go out as damage
    if (c->dx1 > c->dx0) {
        int x0 = c->dx0 < 0 ? 0 : c->dx0, x1 = c->dx1 > c->w ? c->w : c->dx1;
        int y0 = c->dy0 < 0 ? 0 : c->dy0, y1 = c->dy1 > c->h ? c->h : c->dy1;
        for (int y = y0; y < y1 && x1 > x0; y++) memset(c->px + (size_t)y * c->stride + x0, 0, (size_t)(x1 - x0) * 4);
    }
    c->dx0 = c->dy0 = c->dx1 = c->dy1 = 0;
}

void goome_damage(goome_canvas *c, const goome_rect *r)
{
    if (!c) return;
    int x0 = 0, y0 = 0, x1 = c->w, y1 = c->h;
    if (r) {
        x0 = (int)((r->x - c->ox) * c->scale), y0 = (int)((r->y - c->oy) * c->scale);
        x1 = (int)((r->x + r->w - c->ox) * c->scale + 1), y1 = (int)((r->y + r->h - c->oy) * c->scale + 1);
    }
    if (c->dx1 <= c->dx0) c->dx0 = x0, c->dy0 = y0, c->dx1 = x1, c->dy1 = y1;
    if (x0 < c->dx0) c->dx0 = x0;
    if (y0 < c->dy0) c->dy0 = y0;
    if (x1 > c->dx1) c->dx1 = x1;
    if (y1 > c->dy1) c->dy1 = y1;
}

static const char gm_shifted[] = "~!@#$%^&*()_+{}|:\"<>?", gm_plain[] = "`1234567890-=[]\\;',./"; // us layout

int gm_combo_key(int ch, int mods)
{
    mods &= GOOME_SHIFT | GOOME_CTRL | GOOME_ALT | GOOME_SUPER;
    if ((mods & GOOME_CTRL) && ch >= 1 && ch <= 26 && ch != 8 && ch != 9 && ch != 13) ch += 96; // ctrl+h shows up as backspace
    if (ch >= 'A' && ch <= 'Z') ch += 32, mods |= GOOME_SHIFT;
    for (int i = 0; gm_shifted[i]; i++)
        if (ch == gm_shifted[i]) ch = gm_plain[i], mods |= GOOME_SHIFT;
    return ch | mods;
}

static int gm_lower(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }

static int gm_word(const char *s, const char *w, int len)
{
    for (int i = 0; i < len; i++)
        if (gm_lower((unsigned char)s[i]) != w[i]) return 0;
    return 1;
}

int goome_parse_combo(const char *s, int *key, int *mods)
{
    static const struct {
        const char *n;
        int v;
    } mn[] = {{"alt", GOOME_ALT}, {"opt", GOOME_ALT}, {"option", GOOME_ALT}, {"ctrl", GOOME_CTRL}, {"control", GOOME_CTRL},
              {"shift", GOOME_SHIFT}, {"super", GOOME_SUPER}, {"win", GOOME_SUPER}, {"cmd", GOOME_SUPER}, {"meta", GOOME_SUPER}},
      kn[] = {{"space", ' '}, {"esc", GOOME_K_ESC}, {"escape", GOOME_K_ESC}, {"enter", GOOME_K_ENTER}, {"return", GOOME_K_ENTER},
              {"tab", GOOME_K_TAB}, {"backspace", GOOME_K_BACKSPACE}, {"delete", GOOME_K_DELETE}, {"del", GOOME_K_DELETE},
              {"left", GOOME_K_LEFT}, {"right", GOOME_K_RIGHT}, {"up", GOOME_K_UP}, {"down", GOOME_K_DOWN}, {"home", GOOME_K_HOME},
              {"end", GOOME_K_END}, {"pageup", GOOME_K_PGUP}, {"pagedown", GOOME_K_PGDN}, {"insert", GOOME_K_INSERT}, {"plus", '='}};
    if (!s) return GOOME_EARG;
    int m = 0;
    for (;;) {
        int got = 0;
        for (size_t i = 0; i < sizeof(mn) / sizeof(mn[0]) && !got; i++) {
            int l = (int)strlen(mn[i].n);
            if (gm_word(s, mn[i].n, l) && s[l] == '+' && s[l + 1]) m |= mn[i].v, s += l + 1, got = 1;
        }
        if (!got) break;
    }
    int len = 0;
    while (s[len] && s[len] != ' ' && s[len] != '\n' && s[len] != '\r') len++;
    int k = 0;
    if (len == 1) {
        k = gm_combo_key((unsigned char)s[0], m);
        m = k & ~0xFF, k &= 0xFF;
        if (k < 32 || k > 126) k = 0;
    } else if ((len == 2 || len == 3) && gm_lower((unsigned char)s[0]) == 'f' && s[1] >= '1' && s[1] <= '9') {
        int n = s[1] - '0';
        if (len == 3) n = s[2] >= '0' && s[2] <= '9' ? n * 10 + s[2] - '0' : 0;
        if (n >= 1 && n <= 12) k = GOOME_K_F1 + n - 1;
    } else {
        for (size_t i = 0; i < sizeof(kn) / sizeof(kn[0]) && !k; i++)
            if ((int)strlen(kn[i].n) == len && gm_word(s, kn[i].n, len)) k = kn[i].v;
        if (k == '=' && len == 4) m |= GOOME_SHIFT;
    }
    if (!k) return GOOME_EARG;
    if (key) *key = k;
    if (mods) *mods = m;
    return GOOME_OK;
}

const char *goome_strerror(int e)
{
    switch (e) {
    case GOOME_OK: return "ok";
    case GOOME_ENOTSUP: return "this backend cant do that";
    case GOOME_EDENIED: return "not allowed (permission or the user said no)";
    case GOOME_ENODISPLAY: return "no display to draw on";
    case GOOME_EGONE: return "that monitor is gone";
    case GOOME_EAGAIN: return "nothing yet, try again in a bit";
    case GOOME_ENOMEM: return "out of memory";
    case GOOME_EARG: return "bad argument";
    }
    return "unknown error";
}

const char *goome_error(goome *g) { return g && g->err[0] ? g->err : "no error"; }
const char *goome_backend_name(goome *g) { return g ? g->name : ""; }

int goome_caps(goome *g)
{
    if (!g) return 0;
    int c = g->ov->caps;
    if (g->cap && !g->opts.no_capture) c |= g->cap->caps;
    if (!g->ov->hotkeys) c &= ~GOOME_CAP_HOTKEYS;
    return c;
}

#ifndef _WIN32
static void gm_ctl_open(goome *g)
{
    g->ctl_fd = -1;
    if (!g->state_dir[0]) return;
    mkdir(g->state_dir, 0700);
    snprintf(g->ctl_path, sizeof(g->ctl_path), "%s/control", g->state_dir);
    struct stat st;
    if (lstat(g->ctl_path, &st) && mkfifo(g->ctl_path, 0600)) return;
    int fd = open(g->ctl_path, O_RDWR | O_NONBLOCK); // rdwr or it spins on eof after the last writer leaves
    if (fd < 0) return;
    fcntl(fd, F_SETFD, FD_CLOEXEC);
    if (fstat(fd, &st) || !S_ISFIFO(st.st_mode) || st.st_uid != getuid()) { // not ours
        close(fd);
        return;
    }
    g->ctl_fd = fd;
}

static void gm_ctl_read(goome *g)
{
    if (g->ctl_fd < 0) return;
    for (;;) {
        ssize_t n = read(g->ctl_fd, g->ctl_buf + g->ctl_len, sizeof(g->ctl_buf) - 1 - (size_t)g->ctl_len);
        if (n <= 0) break;
        g->ctl_len += (int)n;
        if (g->ctl_len >= (int)sizeof(g->ctl_buf) - 1) g->ctl_buf[g->ctl_len++ - 1] = '\n';
    }
    char *nl;
    while (g->ctl_len > 0 && (nl = memchr(g->ctl_buf, '\n', (size_t)g->ctl_len))) {
        goome_event e = {.type = GOOME_CONTROL};
        int l = (int)(nl - g->ctl_buf);
        snprintf(e.msg, sizeof(e.msg), "%.*s", l < 63 ? l : 63, g->ctl_buf);
        if (e.msg[0]) gm_push(g, &e);
        memmove(g->ctl_buf, nl + 1, (size_t)(g->ctl_len - l - 1));
        g->ctl_len -= l + 1;
    }
}

int goome_send(const char *dir, const char *msg)
{
    if (!dir || !msg) return GOOME_EARG;
    char path[1100];
    snprintf(path, sizeof(path), "%s/control", dir);
    int fd = open(path, O_WRONLY | O_NONBLOCK); // nobody reading = ENXIO right away instead of hanging
    if (fd < 0) return GOOME_EGONE;
    struct stat st;
    size_t len = strlen(msg);
    int ok = !fstat(fd, &st) && S_ISFIFO(st.st_mode) && write(fd, msg, len) == (ssize_t)len && write(fd, "\n", 1) == 1;
    close(fd);
    return ok ? GOOME_OK : GOOME_EGONE;
}
#else
static void gm_ctl_open(goome *g) { g->ctl_fd = -1; }
static void gm_ctl_read(goome *g) {}
int goome_send(const char *dir, const char *msg) { return GOOME_ENOTSUP; }
#endif

const char *goome_control_path(goome *g) { return g && g->ctl_fd >= 0 ? g->ctl_path : NULL; }

static int gm_try(goome *g, const gm_ov *ov, const gm_cap *cap)
{
    if (!ov) return 0;
    g->ov = ov, g->cap = cap;
    g->nmon = 0;
    int r = ov->open(g);
    int bad = r != GOOME_OK || g->nmon < 1 || (cap && !g->opts.no_capture && cap->probe && !cap->probe(g));
    if (bad) {
        if (r == GOOME_OK) ov->close(g);
        g->ov = NULL, g->cap = NULL, g->ov_priv = NULL;
        return 0;
    }
    snprintf(g->name, sizeof(g->name), "%s%s%s", ov->name, cap && !g->opts.no_capture ? " + " : "", cap && !g->opts.no_capture ? cap->name : "");
    return 1;
}


#ifdef GM_LINUX
static int gm_has(const char *hay, const char *needle)
{
    for (; hay && *hay; hay++) {
        int i = 0;
        while (needle[i] && gm_lower((unsigned char)hay[i]) == needle[i]) i++;
        if (!needle[i]) return 1;
    }
    return 0;
}
#endif

static int gm_pick(goome *g, int want)
{
    if (want == GOOME_TEST) return gm_try(g, gm_ov_test(), gm_cap_test());
#if defined(_WIN32)
    if (want == GOOME_AUTO || want == GOOME_WIN32) return gm_try(g, gm_ov_win32(), gm_cap_win32());
#elif defined(__APPLE__)
    if (want == GOOME_AUTO || want == GOOME_MACOS) return gm_try(g, gm_ov_macos(), gm_cap_macos());
#elif defined(GM_LINUX)
    const char *desk = gm_env("XDG_CURRENT_DESKTOP"), *stype = gm_env("XDG_SESSION_TYPE");
    int wl = gm_env("WAYLAND_DISPLAY") != NULL, x = gm_env("DISPLAY") != NULL;
    int gnome = gm_has(desk, "gnome"), x11_session = (stype && !strcmp(stype, "x11")) || (!wl && x);
    if (want == GOOME_X11) return gm_try(g, gm_ov_x11(), gm_cap_x11());
    if (want == GOOME_WAYLAND) return gm_try(g, gm_ov_wayland(), gm_cap_wlr());
    if (want == GOOME_PORTAL) return gm_try(g, gm_ov_wayland(), gm_cap_portal()) || gm_try(g, gm_ov_x11(), gm_cap_portal());
    if (want == GOOME_GNOME) return gm_try(g, gm_ov_x11(), gm_cap_mutter());
    if (want != GOOME_AUTO) return 0;
    if (x11_session) return gm_try(g, gm_ov_x11(), gm_cap_x11());
    if (!wl) return 0;
    if (gnome) return gm_try(g, gm_ov_x11(), gm_cap_mutter()) || gm_try(g, gm_ov_x11(), gm_cap_portal()) || gm_try(g, gm_ov_x11(), NULL);
    if (gm_try(g, gm_ov_wayland(), gm_cap_wlr())) return 1;
    if (gm_try(g, gm_ov_wayland(), gm_cap_portal())) return 1; // kde, cosmic: layer-shell but no screencopy
    if (gm_try(g, gm_ov_x11(), gm_cap_portal())) return 1; // no layer-shell either, xwayland window
    return gm_try(g, gm_ov_wayland(), NULL) || gm_try(g, gm_ov_x11(), NULL); // still draw, grabs just say no
#endif
    return 0;
}

static int gm_backend_from(const char *s)
{
    static const char *names[] = {"auto", "wayland", "gnome", "portal", "x11", "win32", "macos", "test"};
    for (int i = 0; s && i < 8; i++)
        if (!strcmp(s, names[i])) return i;
    return -1;
}

goome *goome_open(const goome_opts *o, int *err)
{
    goome *g = calloc(1, sizeof(*g));
    if (!g) {
        if (err) *err = GOOME_ENOMEM;
        return NULL;
    }
    if (o) g->opts = *o;
    snprintf(g->title, sizeof(g->title), "%s", g->opts.title ? g->opts.title : "goome");
    if (g->opts.state_dir) snprintf(g->state_dir, sizeof(g->state_dir), "%s", g->opts.state_dir);
    g->opts.title = g->title;
    g->opts.state_dir = g->state_dir[0] ? g->state_dir : NULL;
    if (gm_env("GOOME_TEST_FRAMES") || gm_env("GOOME_TEST_SCRIPT") || gm_env("GOOME_TEST_SHOTS")) g->opts.backend = GOOME_TEST;
    int want = g->opts.backend, env = gm_backend_from(gm_env("GOOME_BACKEND"));
    if (env >= 0) want = env;
    if (!gm_pick(g, want)) {
        if (err) *err = GOOME_ENODISPLAY;
        free(g);
        return NULL;
    }
    gm_ctl_open(g);
    if (err) *err = GOOME_OK;
    return g;
}

void goome_close(goome *g)
{
    if (!g) return;
    if (g->cap && g->cap_state == 1 && g->cap->close) g->cap->close(g);
    if (g->ov) g->ov->close(g);
#ifndef _WIN32
    if (g->ctl_fd >= 0) close(g->ctl_fd);
#endif
    free(g);
}

int goome_monitors(goome *g, goome_monitor *out, int max)
{
    if (!g) return 0;
    for (int i = 0; out && i < g->nmon && i < max; i++) out[i] = g->mon[i];
    return g->nmon;
}

int goome_capture(goome *g, int mon, const goome_rect *r, goome_image *img)
{
    if (!g || !img) return GOOME_EARG;
    memset(img, 0, sizeof(*img));
    if (g->opts.no_capture || !g->cap) return gm_fail(g, GOOME_ENOTSUP, "capture is off (no_capture) or this desktop has no way to grab");
    if (mon < 0 || mon >= g->nmon) return gm_fail(g, GOOME_EGONE, "no monitor %d", mon);
    if (g->cap_state == 0) {
        int e = g->cap->open(g);
        g->cap_state = e == GOOME_OK ? 1 : e;
    }
    if (g->cap_state < 0) return g->cap_state;
    goome_rect cr;
    if (!gm_clip(&g->mon[mon], r, &cr)) return gm_fail(g, GOOME_EARG, "that rect is off monitor %d", mon);
    int e = g->cap->grab(g, mon, cr, img);
    if (e == GOOME_OK) img->mon = mon;
    return e;
}

int goome_alpha_at(goome *g, double x, double y)
{
    if (!g) return 0;
    for (int i = 0; i < g->nmon; i++) {
        goome_canvas *c = g->cv[i];
        if (!c) continue;
        int px = (int)((x - c->ox) * c->scale), py = (int)((y - c->oy) * c->scale);
        if (px >= 0 && py >= 0 && px < c->w && py < c->h) return (int)(c->px[(size_t)py * c->stride + px] >> 24);
    }
    return 0;
}

goome_canvas *goome_begin(goome *g, int mon)
{
    if (!g || mon < 0 || mon >= g->nmon) return NULL;
    goome_canvas *c = g->ov->canvas(g, mon);
    g->cv[mon] = c;
    if (c) {
        gm_canvas_clear(c);
        c->nclip = 0, c->kx0 = c->ky0 = 0, c->kx1 = c->w, c->ky1 = c->h; // a missed clip pop cant leak into the next frame
    }
    return c;
}

int goome_end(goome *g, goome_canvas *c, const goome_rect *hits, int n)
{
    if (!g || !c) return GOOME_EARG;
    if (n < 0 || (n && !hits)) n = 0;
    return g->ov->commit(g, c->mon, c, hits, n);
}

int goome_poll(goome *g, goome_event *ev, int timeout_ms)
{
    if (!g || !ev) return GOOME_EARG;
    long end = goome_now_ms() + (timeout_ms > 0 ? timeout_ms : 0);
    for (;;) {
        gm_ctl_read(g);
        if (g->qlen) {
            *ev = g->q[g->qhead];
            g->qhead = (g->qhead + 1) % GM_QLEN, g->qlen--;
            return 1;
        }
        long left = end - goome_now_ms();
        if (left < 0) left = 0;
        if (g->ctl_fd >= 0 && left > 100) left = 100;
        int r = g->ov->wait(g, (int)left);
        if (r < 0) {
            goome_event q = {.type = GOOME_QUIT};
            *ev = q;
            return 1;
        }
        if (!g->qlen && goome_now_ms() >= end) {
            gm_ctl_read(g);
            if (!g->qlen) return 0;
        }
    }
}

int goome_pointer(goome *g, double *x, double *y, int *buttons)
{
    double a = 0, b = 0;
    int k = 0;
    if (!g || !g->ov->pointer) return 0;
    int r = g->ov->pointer(g, &a, &b, &k);
    if (x) *x = a;
    if (y) *y = b;
    if (buttons) *buttons = k;
    return r;
}

int goome_keyboard(goome *g, int on)
{
    if (!g) return GOOME_EARG;
    if (!g->ov->keyboard) return gm_fail(g, GOOME_ENOTSUP, "%s cant take the keyboard", g->ov->name);
    g->kb_on = !!on;
    return g->ov->keyboard(g, g->kb_on);
}

int goome_hotkey(goome *g, const char *combo, int id)
{
    if (!g) return GOOME_EARG;
    if (!g->ov->hotkeys)
        return gm_fail(g, GOOME_ENOTSUP, "%s cant grab keys system wide, bind a key to send a line to %s instead", g->ov->name,
                       g->ctl_fd >= 0 ? g->ctl_path : "the control fifo (set state_dir)");
    for (int i = 0; i < g->nhk; i++)
        if (g->hk[i].id == id) g->hk[i--] = g->hk[--g->nhk];
    if (combo) {
        int k, m;
        if (goome_parse_combo(combo, &k, &m) != GOOME_OK) return gm_fail(g, GOOME_EARG, "cant read the combo '%s'", combo);
        if (g->nhk == GM_MAX_HK) return gm_fail(g, GOOME_ENOMEM, "only %d hotkeys at once", GM_MAX_HK);
        g->hk[g->nhk++] = (gm_hotkey){k, m, id};
    }
    return g->ov->hotkeys(g);
}

int goome_save_ppm(const char *path, const uint32_t *px, int w, int h, int stride)
{
    FILE *f = fopen(path, "wb");
    if (!f) return GOOME_EARG;
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    unsigned char *row = malloc((size_t)w * 3);
    if (!row) {
        fclose(f);
        return GOOME_ENOMEM;
    }
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            uint32_t v = px[(size_t)y * stride + x];
            row[x * 3] = (unsigned char)(v >> 16), row[x * 3 + 1] = (unsigned char)(v >> 8), row[x * 3 + 2] = (unsigned char)v;
        }
        fwrite(row, 3, (size_t)w, f);
    }
    free(row);
    return fclose(f) ? GOOME_EARG : GOOME_OK;
}
