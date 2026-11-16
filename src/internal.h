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
    int (*win)(struct goome *g, int op, int a, int b, int *ra, int *rb);
    int (*cursor)(struct goome *g, int shape);
    int (*clip_set)(struct goome *g, const char *s);
    int (*clip_get)(struct goome *g, char **out);
    int (*grab)(struct goome *g, int on);
} gm_ov;

enum { GM_WIN_TITLE = 1, GM_WIN_SIZE, GM_WIN_POS_GET, GM_WIN_POS_SET, GM_WIN_STATE, GM_WIN_GET_STATE, GM_WIN_DRAG, GM_WIN_SHOW };

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
    int kb_on, grab_on, grab_req;
    int ctl_fd;
    char ctl_path[1100], ctl_buf[256];
    int ctl_len;
    int cursor;
    char *clip;
    char drop[4096];
    char *ime_q, *ime_text;
    size_t ime_qlen, ime_qcap;
    int ime_cur, ime_on;
    goome_rect ime_caret;
    int nhit;
    goome_rect *hit;
    goome_canvas *frame;
    goome_canvas *dcv;
    uint32_t *dpx;
    int dw, dh, dirty, dx0, dy0, dx1, dy1;
    struct gm_ax_s *ax;
};

int gm_push(struct goome *g, const goome_event *e);
int gm_fail(struct goome *g, int code, const char *fmt, ...);
void gm_set_monitors(struct goome *g, const goome_monitor *m, int n); // queues GOOME_MONITORS when it changed
int gm_combo_key(int ch, int mods); // whatever a backend got into key | mods, lowercase + shift folded
int gm_clip(const goome_monitor *m, const goome_rect *in, goome_rect *out);
void gm_canvas_reset(goome_canvas *c, uint32_t *px, int w, int h, int stride, double ox, double oy, double scale, int mon);
int gm_canvas_damage(const goome_canvas *c, int *x0, int *y0, int *x1, int *y1);
void gm_canvas_clear(goome_canvas *c);
const char *gm_env(const char *name);
int gm_num(const char **p); // hand rolled, glibc 2.38 redirects the libc ones

#ifndef GOOME_NO_DRAW
typedef struct {
    int lo, hi, rtl;
    double x0, x1;
} gm_tc;
int gm_text_rtl(goome_font *f, const char *s, int n);
int gm_text_layout(goome_font *f, double size, const char *s, int n, gm_tc **out);
int gm_text_next(const char *s, int i, int n);
int gm_text_prev(const char *s, int i);
#endif

#ifndef GOOME_NO_UI
enum { GM_AXK_EXPANDABLE = 1, GM_AXK_CHECKABLE = 2, GM_AXK_FOCUSABLE = 4, GM_AXK_EDITABLE = 8, GM_AXK_MULTILINE = 16, GM_AXK_PASSWORD = 32, GM_AXK_RANGE = 64, GM_AXK_PRESS = 128, GM_AXK_SELECTABLE = 256, GM_AXK_HIDDEN = 512 };
typedef struct {
    uint32_t press;
    int kind, text, ntext;
} gm_ax_x;
typedef struct gm_ax_s {
    goome_a11y_node *n, *pn;
    gm_ax_x *x, *px;
    char *text, *ptext;
    int count, pcount, cap, pcap, ntext, tcap, pntext, ptcap, active, pactive, synced, serial;
    uint32_t *hash;
    int hcap;
    char *long_text;
    uint32_t long_id;
    void *plat;
    int plat_state;
} gm_ax;
void gm_ax_sync(struct goome *g, const goome_a11y_node *n, const gm_ax_x *x, int count, const char *text, int ntext, int active);
int gm_ax_find(const gm_ax *a, uint32_t id);
int gm_ax_act(struct goome *g, uint32_t id, const char *verb, const char *text);
const char *gm_ax_long(struct goome *g, uint32_t id);
const char *gm_ax_str(const gm_ax *a, int i, int *len);
int gm_ax_chars(const char *s, int n);
int gm_ax_bytes(const char *s, int n, int chars);
enum { GM_AXE_FOCUS = 1, GM_AXE_VALUE, GM_AXE_NAME, GM_AXE_STATE, GM_AXE_ADD, GM_AXE_REMOVE, GM_AXE_ACTIVE, GM_AXE_TEXT, GM_AXE_CARET };
#endif
int gm_ax_pump(struct goome *g);
#if defined(__APPLE__) && !defined(GOOME_NO_UI)
void *gm_mac_view(struct goome *g);
#endif
#if defined(_WIN32) && !defined(GOOME_NO_UI)
int gm_uia_obj(struct goome *g, void *hwnd, uintptr_t wp, intptr_t lp, intptr_t *out);
#endif
void gm_ax_close(struct goome *g);
const gm_cap *gm_cap_test(void);
const gm_ov *gm_ov_test(void);
const gm_ov *gm_ov_offscreen(void);
int gm_of_resize(struct goome *g, int w, int h, double scale);
int gm_clip_keep(struct goome *g, const char *s);
int gm_clip_copy(struct goome *g, char **out);
void gm_drop(struct goome *g, double x, double y, const char *path);
int gm_ime_push(struct goome *g, int type, const char *s, int n, int cursor);
#if defined(_WIN32)
const gm_cap *gm_cap_win32(void);
const gm_ov *gm_ov_win32(void);
int gm_ime_win32(struct goome *g, int on, const goome_rect *caret);
#elif defined(__APPLE__)
const gm_cap *gm_cap_macos(void);
const gm_ov *gm_ov_macos(void);
int gm_ime_macos(struct goome *g, int on, const goome_rect *caret);
#elif defined(__linux__) || defined(__FreeBSD__)
#define GM_LINUX 1
const gm_ov *gm_ov_wayland(void);
const gm_cap *gm_cap_wlr(void);
const gm_ov *gm_ov_x11(void);
const gm_cap *gm_cap_x11(void);
const gm_cap *gm_cap_mutter(void);
const gm_cap *gm_cap_portal(void);
int gm_ime_x11(struct goome *g, int on, const goome_rect *caret);
int gm_ime_wayland(struct goome *g, int on, const goome_rect *caret);
int gm_uri_paths(const char *s, size_t n, char **out);
void gm_drop_next(struct goome *g, const char *drops, int *off, int len, double x, double y);
#endif

#endif
