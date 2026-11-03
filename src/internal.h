#ifndef GOOME_INTERNAL_H
#define GOOME_INTERNAL_H

#ifndef GOOME_BUILDING
#define GOOME_BUILDING
#endif

#include "goome.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define GM_MAX_MON 16
#define GM_MAX_HK 32
#define GM_QLEN 128

struct goome;

typedef struct gm_cap {
    const char *name;
    int (*probe)(struct goome *g); // 1 if it can work here, cheap, no popups. runs after the overlay opened. NULL = yes
    int (*open)(struct goome *g); // called on the first grab
    int (*grab)(struct goome *g, int mon, goome_rect r, goome_image *img); // r is already clipped to the monitor
    void (*close)(struct goome *g);
    int caps;
} gm_cap;

typedef struct gm_ov {
    const char *name;
    int (*open)(struct goome *g); // fills g->mon via gm_set_monitors
    goome_canvas *(*canvas)(struct goome *g, int mon); // NULL = no surface right now
    int (*commit)(struct goome *g, int mon, goome_canvas *c, const goome_rect *hits, int n);
    int (*wait)(struct goome *g, int timeout_ms); // pumps native events into the queue, <0 = display gone
    int (*pointer)(struct goome *g, double *x, double *y, int *buttons); // can be NULL
    int (*keyboard)(struct goome *g, int on); // can be NULL
    int (*hotkeys)(struct goome *g); // grab everything in g->hk, NULL = cant
    void (*close)(struct goome *g);
    int caps;
} gm_ov;

typedef struct {
    int key, mods, id;
} gm_hotkey;

struct goome {
    goome_opts opts;
    char title[64], state_dir[1024], err[256], name[96];
    const gm_cap *cap;
    const gm_ov *ov;
    void *cap_priv, *ov_priv;
    int cap_state; // 0 not opened yet, 1 open, <0 the error it failed with
    int nmon;
    goome_monitor mon[GM_MAX_MON];
    goome_canvas *cv[GM_MAX_MON]; // last one goome_begin handed out per monitor
    goome_event q[GM_QLEN];
    int qhead, qlen;
    gm_hotkey hk[GM_MAX_HK];
    int nhk;
    int kb_on;
    int ctl_fd;
    char ctl_path[1100], ctl_buf[256];
    int ctl_len;
};

void gm_push(struct goome *g, const goome_event *e);
int gm_fail(struct goome *g, int code, const char *fmt, ...);
void gm_set_monitors(struct goome *g, const goome_monitor *m, int n); // queues GOOME_MONITORS when it changed
int gm_combo_key(int ch, int mods); // whatever a backend got into key | mods, lowercase + shift folded
int gm_clip(const goome_monitor *m, const goome_rect *in, goome_rect *out);
void gm_canvas_reset(goome_canvas *c, uint32_t *px, int w, int h, int stride, double ox, double oy, double scale, int mon);
int gm_canvas_damage(const goome_canvas *c, int *x0, int *y0, int *x1, int *y1);
void gm_canvas_clear(goome_canvas *c);
const char *gm_env(const char *name);
int gm_num(const char **p); // hand rolled, glibc 2.38 redirects the libc ones

const gm_cap *gm_cap_test(void);
const gm_ov *gm_ov_test(void);
#if defined(_WIN32)
const gm_cap *gm_cap_win32(void);
const gm_ov *gm_ov_win32(void);
#elif defined(__APPLE__)
const gm_cap *gm_cap_macos(void);
const gm_ov *gm_ov_macos(void);
#elif defined(__linux__) || defined(__FreeBSD__)
#define GM_LINUX 1
const gm_ov *gm_ov_wayland(void);
const gm_cap *gm_cap_wlr(void);
const gm_ov *gm_ov_x11(void);
const gm_cap *gm_cap_x11(void);
const gm_cap *gm_cap_mutter(void);
const gm_cap *gm_cap_portal(void);
#endif

#endif
