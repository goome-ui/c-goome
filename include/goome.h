#ifndef GOOME_H
#define GOOME_H

#if (defined(GOOME_IMPLEMENTATION) || defined(GOOME_BUILDING)) && !defined(_WIN32) && !defined(_DEFAULT_SOURCE)
#define _DEFAULT_SOURCE // posix bits even under -std=c99, only works if this is the first include
#endif

#if defined(GOOME_IMPLEMENTATION) && defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS // msvc calls fopen and snprintf unsafe
#endif

#include <stddef.h>
#include <stdint.h>

#if defined(GOOME_NO_DRAW) && !defined(GOOME_NO_UI)
#define GOOME_NO_UI // the ui draws with the draw layer
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define GOOME_VERSION "0.2.0"

typedef struct goome goome;

enum { GOOME_AUTO, GOOME_WAYLAND, GOOME_GNOME, GOOME_PORTAL, GOOME_X11, GOOME_WIN32, GOOME_MACOS, GOOME_TEST, GOOME_OFFSCREEN };
enum { GOOME_MODE_OVERLAY, GOOME_MODE_WINDOW, GOOME_MODE_OFFSCREEN };
enum {
    GOOME_WINDOW_RESIZABLE = 1, GOOME_WINDOW_BORDERLESS = 2, GOOME_WINDOW_TRANSPARENT = 4,
    GOOME_WINDOW_ON_TOP = 8, GOOME_WINDOW_HIDDEN = 16, GOOME_WINDOW_CENTER = 32,
};

enum {
    GOOME_OK = 0,
    GOOME_ENOTSUP = -1,
    GOOME_EDENIED = -2,
    GOOME_ENODISPLAY = -3,
    GOOME_EGONE = -4,
    GOOME_EAGAIN = -5,
    GOOME_ENOMEM = -6,
    GOOME_EARG = -7,
};

enum {
    GOOME_CAP_CAPTURE = 1,
    GOOME_CAP_GLOBAL_POINTER = 2,
    GOOME_CAP_HOTKEYS = 4,
    GOOME_CAP_HIDE_SELF = 8,
    GOOME_CAP_KEYBOARD = 16,
    GOOME_CAP_WINDOW = 32,
    GOOME_CAP_CLIPBOARD = 64,
    GOOME_CAP_CURSOR = 128,
};

typedef struct {
    const char *title;     // NULL = "goome"
    int backend;           // GOOME_AUTO or force one, env GOOME_BACKEND wins
    const char *state_dir; // portal token + control fifo live here, NULL = neither
    int no_capture;        // overlay only, never asks for screen access
    int capture_fps;       // gnome/portal streams, 0 = 5
    int hide_self;         // keep our overlay out of grabs where the os lets us
    const char *test_frames, *test_script, *test_shots; // GOOME_TEST only, env GOOME_TEST_FRAMES/_SCRIPT/_SHOTS
    int mode;                  // GOOME_MODE_*
    int width, height;         // window / offscreen size, logical. 0 = 960x640
    int min_width, min_height; // window only, 0 = none
    int window_flags;          // GOOME_WINDOW_*
    double scale;              // offscreen only, 0 = 1
} goome_opts;

typedef struct {
    int x, y, w, h;
} goome_rect;

typedef struct {
    int id;
    char name[64];
    goome_rect area; // logical desktop coords
    double scale;    // real px per logical px
    int pw, ph;      // real px
    int rotation;    // 0 90 180 270, +360 if mirrored. grabs and canvases already undo it
} goome_monitor;

typedef struct {
    uint32_t *px; // 0xFFRRGGBB, goome owns it, good until the next grab of that monitor
    int w, h, stride; // real px, stride in pixels
    goome_rect area;
    int mon;
    int has_self;   // our overlay is in it, mask with goome_alpha_at
    int has_cursor; // the mouse got baked in at cursor (logical)
    goome_rect cursor;
} goome_image;

typedef struct {
    uint32_t *px; // premultiplied argb
    int w, h, stride; // real px, stride in pixels
    double ox, oy, scale; // logical spot of px[0], real px per logical px
    int mon;
    int dx0, dy0, dx1, dy1; // drawn since the clear
    int cx0, cy0, cx1, cy1; // what the clear wiped
    int kx0, ky0, kx1, ky1; // clip in real px, drawing never leaves it
    int nclip;
    int clips[16][4];
} goome_canvas;

enum {
    GOOME_NONE, GOOME_DOWN, GOOME_UP, GOOME_MOTION, GOOME_SCROLL, GOOME_KEY,
    GOOME_HOTKEY, GOOME_CONTROL, GOOME_MONITORS, GOOME_QUIT,
    GOOME_CLOSE, // window close button / alt-f4 / cmd-w, the app decides
    GOOME_FOCUS, // id = 1 gained, 0 lost
    GOOME_DROP,  // a file dropped on the window, msg = path (cut at 63 bytes, goome_drop_path has all of it), x,y = where
};

enum {
    GOOME_CURSOR_ARROW, GOOME_CURSOR_IBEAM, GOOME_CURSOR_HAND, GOOME_CURSOR_MOVE,
    GOOME_CURSOR_RESIZE_EW, GOOME_CURSOR_RESIZE_NS, GOOME_CURSOR_RESIZE_NWSE, GOOME_CURSOR_RESIZE_NESW,
    GOOME_CURSOR_WAIT, GOOME_CURSOR_NO, GOOME_CURSOR_HIDDEN,
};
enum { GOOME_EDGE_LEFT = 1, GOOME_EDGE_RIGHT = 2, GOOME_EDGE_TOP = 4, GOOME_EDGE_BOTTOM = 8 };
enum { GOOME_STATE_NORMAL, GOOME_STATE_MINIMIZED, GOOME_STATE_MAXIMIZED, GOOME_STATE_FULLSCREEN };

enum {
    GOOME_K_BACKSPACE = 8, GOOME_K_TAB = 9, GOOME_K_ENTER = 13, GOOME_K_ESC = 27, GOOME_K_SPACE = 32, GOOME_K_DELETE = 127,
    GOOME_K_F1 = 0x81, GOOME_K_F12 = 0x8C,
    GOOME_K_LEFT = 0x90, GOOME_K_RIGHT, GOOME_K_UP, GOOME_K_DOWN, GOOME_K_HOME, GOOME_K_END, GOOME_K_PGUP, GOOME_K_PGDN, GOOME_K_INSERT,
};

enum { GOOME_SHIFT = 0x100, GOOME_CTRL = 0x200, GOOME_ALT = 0x400, GOOME_SUPER = 0x800 };

typedef struct {
    int type;
    double x, y; // logical
    int mon;
    int button; // 1 left 2 middle 3 right
    int dy;     // scroll notches, +1 = down
    int key;    // lowercase ascii or GOOME_K_
    int mods;
    char text[8]; // utf-8 it typed, can be empty
    int id;
    char msg[64]; // GOOME_CONTROL line
} goome_event;

typedef struct {
    long next;
    int fps;
} goome_pacer;

goome *goome_open(const goome_opts *o, int *err);
goome *goome_open_app(const char *title, int w, int h, int *err); // a normal resizable window, centered
void goome_close(goome *g);
const char *goome_error(goome *g);
const char *goome_strerror(int err);
const char *goome_backend_name(goome *g);
int goome_caps(goome *g);

int goome_monitors(goome *g, goome_monitor *out, int max);

int goome_capture(goome *g, int mon, const goome_rect *r, goome_image *img);
int goome_alpha_at(goome *g, double x, double y);

goome_canvas *goome_begin(goome *g, int mon);
int goome_end(goome *g, goome_canvas *c, const goome_rect *hits, int nhits);
void goome_damage(goome_canvas *c, const goome_rect *r);

int goome_poll(goome *g, goome_event *ev, int timeout_ms);
int goome_pointer(goome *g, double *x, double *y, int *buttons);
int goome_keyboard(goome *g, int on);
int goome_hotkey(goome *g, const char *combo, int id);
int goome_parse_combo(const char *combo, int *key, int *mods);
const char *goome_control_path(goome *g);
int goome_send(const char *state_dir, const char *msg);

int goome_pace(goome_pacer *p, int fps);
long goome_now_ms(void);

int goome_mode(goome *g);
int goome_window_title(goome *g, const char *title);
int goome_window_size(goome *g, int w, int h); // logical client size
int goome_window_pos(goome *g, int *x, int *y, int set); // set=1 moves it, 0 reads it. ENOTSUP where the os wont say (wayland)
int goome_window_state(goome *g, int state); // GOOME_STATE_*
int goome_window_get_state(goome *g);
int goome_window_drag(goome *g, int edges); // native move (0) or resize (GOOME_EDGE_* bits) from the press happening now
int goome_window_show(goome *g, int on);
int goome_cursor(goome *g, int shape);
int goome_cursor_shape(goome *g); // last one asked for, offscreen embedders read this
int goome_clipboard_set(goome *g, const char *utf8);
int goome_clipboard_get(goome *g, char *buf, int cap); // full length (can be > cap-1, buf gets cut + nul), <0 error. can block ~250ms on x11/wayland
const char *goome_drop_path(goome *g); // full path of the last GOOME_DROP, good until the next poll

int goome_push(goome *g, const goome_event *ev); // queue an event, any mode
int goome_resize(goome *g, int w, int h, double scale); // offscreen only
int goome_frame(goome *g, goome_image *out); // last frame goome_end sent, px = premultiplied argb, good until the next goome_begin
int goome_hit(goome *g, double x, double y); // 1 if x,y is in the hit rects of the last goome_end

int goome_save_ppm(const char *path, const uint32_t *px, int w, int h, int stride);

#ifndef GOOME_NO_DRAW
// logical coords, the canvas scales them. colors are plain 0xAARRGGBB
void goome_clear(goome_canvas *c);
void goome_draw_rect(goome_canvas *c, double x, double y, double w, double h, uint32_t argb);
void goome_draw_round_rect(goome_canvas *c, double x, double y, double w, double h, double radius, uint32_t argb);
void goome_draw_frame(goome_canvas *c, double x, double y, double w, double h, double width, uint32_t argb);
void goome_draw_line(goome_canvas *c, double x0, double y0, double x1, double y1, double width, uint32_t argb);
void goome_draw_triangle(goome_canvas *c, double x0, double y0, double x1, double y1, double x2, double y2, uint32_t argb);
void goome_draw_image(goome_canvas *c, double x, double y, double w, double h, const uint32_t *img, int iw, int ih);
void goome_erase(goome_canvas *c, double x, double y, double w, double h);
double goome_draw_text(goome_canvas *c, double x, double y, const char *s, int big, uint32_t argb);
double goome_text_width(const goome_canvas *c, const char *s, int big);
void goome_set_font(int font);
int goome_font_count(void);
void goome_canvas_blit(goome_canvas *dst, const goome_canvas *src, double alpha, double dx, double dy);

typedef struct {
    double tl, tr, br, bl;
} goome_radii;

goome_radii goome_radius(double r);
void goome_clip_push(goome_canvas *c, double x, double y, double w, double h); // snaps to whole px, 16 deep, goome_begin resets it
void goome_clip_pop(goome_canvas *c);
void goome_draw_rrect(goome_canvas *c, double x, double y, double w, double h, goome_radii r, uint32_t argb);
void goome_draw_rrect_line(goome_canvas *c, double x, double y, double w, double h, goome_radii r, double width, uint32_t argb);
void goome_draw_rrect_grad(goome_canvas *c, double x, double y, double w, double h, goome_radii r, uint32_t from, uint32_t to, int sideways);
void goome_draw_shadow(goome_canvas *c, double x, double y, double w, double h, double radius, double blur, uint32_t argb); // blur like css box-shadow
void goome_draw_circle(goome_canvas *c, double cx, double cy, double r, uint32_t argb);
void goome_draw_ring(goome_canvas *c, double cx, double cy, double r, double width, uint32_t argb); // r = middle of the stroke
void goome_draw_arc(goome_canvas *c, double cx, double cy, double r, double width, double a0, double a1, uint32_t argb); // radians, 0 = 3 oclock, clockwise, round caps
void goome_draw_poly(goome_canvas *c, const double *xy, int n, uint32_t argb); // x,y pairs, nonzero fill

typedef struct {
    uint32_t *px; // premultiplied argb
    int w, h, stride;
    int owned;
} goome_bitmap;

int goome_bitmap_load(const char *path, goome_bitmap *out); // png, jpeg, bmp, ppm, qoi. picks by content, not name
int goome_bitmap_load_mem(const void *data, size_t n, goome_bitmap *out);
const char *goome_bitmap_error(void); // why the last load failed
int goome_bitmap_from_argb(const uint32_t *argb, int w, int h, goome_bitmap *out); // plain argb in, copied
void goome_bitmap_free(goome_bitmap *b);
void goome_draw_bitmap(goome_canvas *c, const goome_bitmap *b, double x, double y, double w, double h, goome_radii r, uint32_t tint);
void goome_draw_bitmap_part(goome_canvas *c, const goome_bitmap *b, int sx, int sy, int sw, int sh, double x, double y, double w, double h, uint32_t tint);
void goome_draw_bitmap_nine(goome_canvas *c, const goome_bitmap *b, double x, double y, double w, double h, double l, double t, double r, double bm); // l t r bm = border sizes in bitmap px

typedef struct goome_font goome_font;

goome_font *goome_font_load(const char *path); // .ttf .otf .ttc
goome_font *goome_font_load_index(const char *path, int index); // face in a .ttc, fontconfig hands these out
goome_font *goome_font_load_mem(const void *data, size_t n, int copy);
goome_font *goome_font_system(const char *name); // NULL = the desktops usual ui font
void goome_font_free(goome_font *f);
double goome_text(goome_canvas *c, goome_font *f, double size, double x, double y, const char *utf8, uint32_t argb); // y = top, NULL font = built in
double goome_text_measure(goome_font *f, double size, const char *utf8, double *height);
double goome_text_line_height(goome_font *f, double size);
#endif

#ifndef GOOME_NO_UI
typedef struct goome_ui goome_ui;

typedef struct {
    const goome_bitmap *bm;
    double l, t, r, b;
} goome_skin;

typedef struct {
    uint32_t bg, panel, border, text, text_dim, accent, accent_text, hover, active, input_bg, shadow;
    double radius, pad, gap, border_width, font_size, shadow_blur, anim_ms;
    goome_font *font; // NULL = built in, goome_ui_new puts the system font here and frees it
    goome_skin skin_button, skin_panel, skin_input;
} goome_theme;

enum { GOOME_WIN_MOVE = 1, GOOME_WIN_RESIZE = 2, GOOME_WIN_COLLAPSE = 4, GOOME_WIN_NO_TITLE = 8, GOOME_WIN_CLOSE = 16 };

goome_ui *goome_ui_new(goome *g);
void goome_ui_free(goome_ui *u);
goome_theme *goome_ui_theme(goome_ui *u);
void goome_ui_theme_dark(goome_theme *t); // leaves font alone
void goome_ui_theme_light(goome_theme *t);
int goome_ui_feed(goome_ui *u, const goome_event *ev); // 1 if the ui used it
goome_canvas *goome_ui_begin(goome_ui *u, int mon); // NULL = nothing to draw on, skip the frame
void goome_ui_end(goome_ui *u); // sends the frame with every widgets hit area
void goome_ui_hit(goome_ui *u, double x, double y, double w, double h);
int goome_ui_animating(goome_ui *u); // 1 while something still moves, draw again soon

int goome_ui_window(goome_ui *u, const char *title, double *x, double *y, double *w, double *h, int flags, int *open);
void goome_ui_window_end(goome_ui *u);
void goome_ui_row(goome_ui *u, int cols);
void goome_ui_same_line(goome_ui *u);
void goome_ui_space(goome_ui *u, double px);
void goome_ui_separator(goome_ui *u);
void goome_ui_width(goome_ui *u, double w); // next widget, 0 = fill

void goome_ui_label(goome_ui *u, const char *fmt, ...);
void goome_ui_heading(goome_ui *u, const char *text);
void goome_ui_wrapped(goome_ui *u, const char *text);
int goome_ui_button(goome_ui *u, const char *label);
int goome_ui_image_button(goome_ui *u, const char *id, const goome_bitmap *b, double w, double h);
void goome_ui_image(goome_ui *u, const goome_bitmap *b, double w, double h, double radius);
int goome_ui_checkbox(goome_ui *u, const char *label, int *v);
int goome_ui_toggle(goome_ui *u, const char *label, int *v);
int goome_ui_radio(goome_ui *u, const char *label, int *v, int value);
int goome_ui_slider(goome_ui *u, const char *label, double *v, double min, double max, const char *fmt);
int goome_ui_slider_int(goome_ui *u, const char *label, int *v, int min, int max);
int goome_ui_input(goome_ui *u, const char *label, char *buf, int cap);
int goome_ui_combo(goome_ui *u, const char *label, int *sel, const char *const *items, int n);
int goome_ui_selectable(goome_ui *u, const char *label, int selected);
int goome_ui_tabs(goome_ui *u, const char *id, const char *const *names, int n, int *sel);
int goome_ui_collapsing(goome_ui *u, const char *label, int *open);
void goome_ui_progress(goome_ui *u, double frac, const char *text);
int goome_ui_color(goome_ui *u, const char *label, uint32_t *argb);
void goome_ui_tooltip(goome_ui *u, const char *text); // for the widget right before it
int goome_ui_hovered(goome_ui *u); // the widget right before it
int goome_ui_scroll_begin(goome_ui *u, const char *id, double h); // always call scroll_end
void goome_ui_scroll_end(goome_ui *u);
void goome_ui_push_id(goome_ui *u, const char *id); // same labels in a loop, push the index first
void goome_ui_pop_id(goome_ui *u);

enum { GOOME_APP_TITLEBAR = 1, GOOME_APP_MENUBAR = 2, GOOME_APP_STATUSBAR = 4, GOOME_APP_NO_PAD = 8 };
enum { GOOME_EDIT_READONLY = 1, GOOME_EDIT_LINE_NUMBERS = 2, GOOME_EDIT_WRAP = 4, GOOME_EDIT_MONO = 8 };
enum { GOOME_TABLE_SORTABLE = 1, GOOME_TABLE_RESIZABLE = 2, GOOME_TABLE_STRIPES = 4, GOOME_TABLE_BORDERS = 8 };

int goome_ui_app(goome_ui *u, const char *title, int flags); // root panel filling monitor 0, after goome_ui_begin
void goome_ui_app_end(goome_ui *u);
int goome_ui_menubar_begin(goome_ui *u);
void goome_ui_menubar_end(goome_ui *u);
int goome_ui_menu_begin(goome_ui *u, const char *label); // 1 = open, nest for submenus
void goome_ui_menu_end(goome_ui *u);
int goome_ui_menu_item(goome_ui *u, const char *label, const char *shortcut, int *checked); // 1 = clicked, checked can be NULL
void goome_ui_menu_separator(goome_ui *u);
int goome_ui_context_begin(goome_ui *u, const char *id); // right click on the widget before it (or empty space) opens it
void goome_ui_context_end(goome_ui *u);
void goome_ui_popup_open(goome_ui *u, const char *id);
int goome_ui_popup_begin(goome_ui *u, const char *id);
void goome_ui_popup_end(goome_ui *u);
void goome_ui_popup_close(goome_ui *u); // closes the innermost open popup, menu or modal
int goome_ui_modal_begin(goome_ui *u, const char *title, int *open); // centered, dims the rest, esc closes
void goome_ui_modal_end(goome_ui *u);
void goome_ui_toast(goome_ui *u, const char *text, int ms);
void goome_ui_statusbar(goome_ui *u, const char *text); // with GOOME_APP_STATUSBAR
int goome_ui_focused(goome_ui *u); // the widget right before it has keyboard focus
void goome_ui_set_focus(goome_ui *u); // focus goes to the next widget

int goome_ui_text_edit(goome_ui *u, const char *id, char *buf, int cap, double h, int flags); // multiline, GOOME_EDIT_*, 1 = changed
int goome_ui_table_begin(goome_ui *u, const char *id, int cols, double h, int flags); // GOOME_TABLE_*
void goome_ui_table_column(goome_ui *u, const char *name, double width); // 0 = share the rest
int goome_ui_table_sort(goome_ui *u, int *col, int *desc); // 1 = sort changed this frame, after the columns
void goome_ui_table_row(goome_ui *u);
void goome_ui_table_next(goome_ui *u);
void goome_ui_table_end(goome_ui *u);
int goome_ui_tree(goome_ui *u, const char *label, int *open); // 1 = open, then goome_ui_tree_end
void goome_ui_tree_end(goome_ui *u);
int goome_ui_list_clip(goome_ui *u, int count, double row_h, int *first, int *last); // in a scroll area, only draw first..last
int goome_ui_drag_float(goome_ui *u, const char *label, double *v, double speed, double min, double max, const char *fmt);
int goome_ui_drag_int(goome_ui *u, const char *label, int *v, double speed, int min, int max);
int goome_ui_number(goome_ui *u, const char *label, double *v, double step, const char *fmt);
void goome_ui_plot_lines(goome_ui *u, const char *label, const float *v, int n, double min, double max, double h);
void goome_ui_plot_bars(goome_ui *u, const char *label, const float *v, int n, double min, double max, double h);
int goome_ui_split_begin(goome_ui *u, const char *id, int vertical, double *frac);
void goome_ui_split_next(goome_ui *u);
void goome_ui_split_end(goome_ui *u);
int goome_ui_drag_source(goome_ui *u, const char *type, const void *data, int len); // the widget before it can be dragged
int goome_ui_drop_target(goome_ui *u, const char *type, void *buf, int cap); // widget before it, bytes dropped or 0
void goome_ui_spinner(goome_ui *u, double size);
int goome_ui_link(goome_ui *u, const char *text); // 1 = clicked
void goome_ui_badge(goome_ui *u, const char *text, uint32_t argb);
#endif

#ifdef __cplusplus
}
#endif

#endif
