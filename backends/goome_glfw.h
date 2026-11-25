#ifndef GOOME_GLFW_H
#define GOOME_GLFW_H

#include "goome.h"
#ifndef GLFW_VERSION_MAJOR
#include <GLFW/glfw3.h>
#endif

int goome_glfw_init(goome *g, GLFWwindow *win, int install_callbacks);
void goome_glfw_set_ui(goome *g, goome_ui *u);
void goome_glfw_frame(goome *g);
void goome_glfw_shutdown(goome *g);

int goome_glfw_cursor_pos(goome *g, double x, double y);
int goome_glfw_mouse_button(goome *g, int button, int action, int mods);
int goome_glfw_scroll(goome *g, double dx, double dy);
int goome_glfw_key(goome *g, int key, int scancode, int action, int mods);
int goome_glfw_char(goome *g, unsigned int cp);
int goome_glfw_focus(goome *g, int focused);

#endif

#if defined(GOOME_IMPL_GLFW_IMPLEMENTATION) && !defined(GOOME_IMPL_GLFW_DONE)
#define GOOME_IMPL_GLFW_DONE

#include <stdlib.h>
#include <string.h>

typedef struct {
    goome *g;
    goome_ui *u;
    GLFWwindow *win;
    int held, mods, lw, lh, pw, ph, shape, installed;
    double mx, my, wheel;
    char *clip;
    GLFWcursor *cur[GOOME_CURSOR_HIDDEN];
    GLFWcursorposfun prev_pos;
    GLFWmousebuttonfun prev_button;
    GLFWscrollfun prev_scroll;
    GLFWkeyfun prev_key;
    GLFWcharfun prev_char;
    GLFWwindowfocusfun prev_focus;
} gfw_state;

static gfw_state gfw;

static int gfw_ours(goome *g) { return g && g == gfw.g; }

static void gfw_push(int type, double x, double y, int button, int dy, int key, const char *text)
{
    goome_event e;
    memset(&e, 0, sizeof(e));
    e.type = type, e.x = x, e.y = y, e.button = button, e.dy = dy, e.key = key, e.mods = gfw.mods;
    if (text) memcpy(e.text, text, strlen(text) < 7 ? strlen(text) : 7);
    goome_push(gfw.g, &e);
}

static int gfw_mods(int m)
{
    return (m & GLFW_MOD_SHIFT ? GOOME_SHIFT : 0) | (m & GLFW_MOD_CONTROL ? GOOME_CTRL : 0) | (m & GLFW_MOD_ALT ? GOOME_ALT : 0) |
           (m & GLFW_MOD_SUPER ? GOOME_SUPER : 0);
}

static int gfw_wants_keys(void) { return gfw.u && (goome_ui_wants(gfw.u) & GOOME_WANT_KEYBOARD); }

static int gfw_over(void) { return gfw.held || goome_hit(gfw.g, gfw.mx, gfw.my); }

static void gfw_pull_clip(void)
{
    const char *s = glfwGetClipboardString(gfw.win);
    if (!s) return;
    goome_clipboard_set(gfw.g, s);
    free(gfw.clip);
    gfw.clip = malloc(strlen(s) + 1);
    if (gfw.clip) memcpy(gfw.clip, s, strlen(s) + 1);
}

static void gfw_push_clip(void)
{
    int n = goome_clipboard_get(gfw.g, NULL, 0);
    if (n < 0 || (n == 0 && !gfw.clip)) return;
    char *s = malloc((size_t)n + 1);
    if (!s) return;
    goome_clipboard_get(gfw.g, s, n + 1);
    if (gfw.clip && !strcmp(s, gfw.clip)) {
        free(s);
        return;
    }
    free(gfw.clip);
    gfw.clip = s;
    glfwSetClipboardString(gfw.win, s);
}

static GLFWcursor *gfw_cursor(int shape)
{
    static const int std[GOOME_CURSOR_HIDDEN] = {
        GLFW_ARROW_CURSOR, GLFW_IBEAM_CURSOR, GLFW_HAND_CURSOR,
#ifdef GLFW_RESIZE_ALL_CURSOR
        GLFW_RESIZE_ALL_CURSOR, GLFW_RESIZE_EW_CURSOR, GLFW_RESIZE_NS_CURSOR, GLFW_RESIZE_NWSE_CURSOR, GLFW_RESIZE_NESW_CURSOR,
        GLFW_ARROW_CURSOR, GLFW_NOT_ALLOWED_CURSOR,
#else
        GLFW_CROSSHAIR_CURSOR, GLFW_HRESIZE_CURSOR, GLFW_VRESIZE_CURSOR, GLFW_CROSSHAIR_CURSOR, GLFW_CROSSHAIR_CURSOR,
        GLFW_ARROW_CURSOR, GLFW_ARROW_CURSOR,
#endif
    };
    if (shape < 0 || shape >= GOOME_CURSOR_HIDDEN) shape = 0;
    if (!gfw.cur[shape]) gfw.cur[shape] = glfwCreateStandardCursor(std[shape]);
    return gfw.cur[shape];
}

static void gfw_size(void)
{
    int lw, lh, pw, ph;
    glfwGetWindowSize(gfw.win, &lw, &lh);
    glfwGetFramebufferSize(gfw.win, &pw, &ph);
    if (lw <= 0 || lh <= 0 || pw <= 0 || ph <= 0) return;
    if (lw == gfw.lw && lh == gfw.lh && pw == gfw.pw && ph == gfw.ph) return;
    gfw.lw = lw, gfw.lh = lh, gfw.pw = pw, gfw.ph = ph;
    goome_resize(gfw.g, lw, lh, (double)pw / lw);
}

int goome_glfw_cursor_pos(goome *g, double x, double y)
{
    if (!gfw_ours(g)) return 0;
    int moved = x != gfw.mx || y != gfw.my;
    gfw.mx = x, gfw.my = y;
    if (moved) gfw_push(GOOME_MOTION, x, y, 0, 0, 0, NULL);
    return gfw_over();
}

int goome_glfw_mouse_button(goome *g, int button, int action, int mods)
{
    if (!gfw_ours(g)) return 0;
    int b = button == GLFW_MOUSE_BUTTON_LEFT ? 1 : button == GLFW_MOUSE_BUTTON_RIGHT ? 3 : button == GLFW_MOUSE_BUTTON_MIDDLE ? 2 : 0;
    if (!b) return gfw.held != 0;
    gfw.mods = gfw_mods(mods);
    int bit = 1 << b, eaten;
    if (action == GLFW_PRESS) {
        eaten = gfw.held || goome_hit(g, gfw.mx, gfw.my);
        if (eaten) gfw.held |= bit;
        gfw_push(GOOME_DOWN, gfw.mx, gfw.my, b, 0, 0, NULL);
    } else {
        eaten = (gfw.held & bit) != 0;
        gfw.held &= ~bit;
        gfw_push(GOOME_UP, gfw.mx, gfw.my, b, 0, 0, NULL);
    }
    return eaten;
}

int goome_glfw_scroll(goome *g, double dx, double dy)
{
    if (!gfw_ours(g)) return 0;
    gfw.wheel -= dy;
    int n = (int)gfw.wheel;
    gfw.wheel -= n;
    if (n) gfw_push(GOOME_SCROLL, gfw.mx, gfw.my, 0, n, 0, NULL);
    return gfw_over();
}

static int gfw_key_code(int k)
{
    if (k >= GLFW_KEY_F1 && k <= GLFW_KEY_F12) return GOOME_K_F1 + k - GLFW_KEY_F1;
    switch (k) {
    case GLFW_KEY_BACKSPACE: return GOOME_K_BACKSPACE;
    case GLFW_KEY_TAB: return GOOME_K_TAB;
    case GLFW_KEY_ENTER:
    case GLFW_KEY_KP_ENTER: return GOOME_K_ENTER;
    case GLFW_KEY_ESCAPE: return GOOME_K_ESC;
    case GLFW_KEY_DELETE: return GOOME_K_DELETE;
    case GLFW_KEY_LEFT: return GOOME_K_LEFT;
    case GLFW_KEY_RIGHT: return GOOME_K_RIGHT;
    case GLFW_KEY_UP: return GOOME_K_UP;
    case GLFW_KEY_DOWN: return GOOME_K_DOWN;
    case GLFW_KEY_HOME: return GOOME_K_HOME;
    case GLFW_KEY_END: return GOOME_K_END;
    case GLFW_KEY_PAGE_UP: return GOOME_K_PGUP;
    case GLFW_KEY_PAGE_DOWN: return GOOME_K_PGDN;
    case GLFW_KEY_INSERT: return GOOME_K_INSERT;
    }
    return 0;
}

int goome_glfw_key(goome *g, int key, int scancode, int action, int mods)
{
    if (!gfw_ours(g)) return 0;
    gfw.mods = gfw_mods(mods);
    int eaten = gfw_wants_keys();
    if (action == GLFW_RELEASE) return eaten;
    int k = gfw_key_code(key);
    if (!k && key >= 32 && key < 127 && (gfw.mods & (GOOME_CTRL | GOOME_ALT | GOOME_SUPER))) {
        k = key >= 'A' && key <= 'Z' ? key + 32 : key;
        if (k == 'v' && (gfw.mods & (GOOME_CTRL | GOOME_SUPER))) gfw_pull_clip();
    }
    if (k) gfw_push(GOOME_KEY, gfw.mx, gfw.my, 0, 0, k, NULL);
    return eaten;
}

int goome_glfw_char(goome *g, unsigned int c)
{
    if (!gfw_ours(g)) return 0;
    int eaten = gfw_wants_keys(), m = gfw.mods;
    if (m & GOOME_ALT) m &= ~(GOOME_CTRL | GOOME_ALT);
    if (c < 32 || c == 127 || c > 0x10FFFF || (m & (GOOME_CTRL | GOOME_SUPER))) return eaten;
    char t[8] = {0};
    if (c < 0x80) t[0] = (char)c;
    else if (c < 0x800) t[0] = (char)(0xC0 | c >> 6), t[1] = (char)(0x80 | (c & 63));
    else if (c < 0x10000) t[0] = (char)(0xE0 | c >> 12), t[1] = (char)(0x80 | ((c >> 6) & 63)), t[2] = (char)(0x80 | (c & 63));
    else t[0] = (char)(0xF0 | c >> 18), t[1] = (char)(0x80 | ((c >> 12) & 63)), t[2] = (char)(0x80 | ((c >> 6) & 63)), t[3] = (char)(0x80 | (c & 63));
    int k = c < 128 ? (int)c : 0, keep = gfw.mods;
    gfw.mods = m;
    if (k >= 'A' && k <= 'Z') k += 32, gfw.mods |= GOOME_SHIFT;
    gfw_push(GOOME_KEY, gfw.mx, gfw.my, 0, 0, k, t);
    gfw.mods = keep;
    return eaten;
}

int goome_glfw_focus(goome *g, int focused)
{
    if (!gfw_ours(g)) return 0;
    goome_event e;
    memset(&e, 0, sizeof(e));
    e.type = GOOME_FOCUS, e.id = focused != 0;
    goome_push(g, &e);
    if (!focused) gfw.held = 0;
    return 0;
}

static void gfw_cb_pos(GLFWwindow *w, double x, double y)
{
    if (w != gfw.win || !goome_glfw_cursor_pos(gfw.g, x, y))
        if (gfw.prev_pos) gfw.prev_pos(w, x, y);
}

static void gfw_cb_button(GLFWwindow *w, int b, int a, int m)
{
    if (w != gfw.win || !goome_glfw_mouse_button(gfw.g, b, a, m))
        if (gfw.prev_button) gfw.prev_button(w, b, a, m);
}

static void gfw_cb_scroll(GLFWwindow *w, double dx, double dy)
{
    if (w != gfw.win || !goome_glfw_scroll(gfw.g, dx, dy))
        if (gfw.prev_scroll) gfw.prev_scroll(w, dx, dy);
}

static void gfw_cb_key(GLFWwindow *w, int k, int s, int a, int m)
{
    if (w != gfw.win || !goome_glfw_key(gfw.g, k, s, a, m))
        if (gfw.prev_key) gfw.prev_key(w, k, s, a, m);
}

static void gfw_cb_char(GLFWwindow *w, unsigned int c)
{
    if (w != gfw.win || !goome_glfw_char(gfw.g, c))
        if (gfw.prev_char) gfw.prev_char(w, c);
}

static void gfw_cb_focus(GLFWwindow *w, int f)
{
    if (w == gfw.win) goome_glfw_focus(gfw.g, f);
    if (gfw.prev_focus) gfw.prev_focus(w, f);
}

int goome_glfw_init(goome *g, GLFWwindow *win, int install_callbacks)
{
    if (!g || !win) return GOOME_EARG;
    if (gfw.g) goome_glfw_shutdown(gfw.g);
    memset(&gfw, 0, sizeof(gfw));
    gfw.g = g, gfw.win = win, gfw.shape = -1;
    glfwGetCursorPos(win, &gfw.mx, &gfw.my);
    gfw_size();
    if (install_callbacks) {
        gfw.installed = 1;
        gfw.prev_pos = glfwSetCursorPosCallback(win, gfw_cb_pos);
        gfw.prev_button = glfwSetMouseButtonCallback(win, gfw_cb_button);
        gfw.prev_scroll = glfwSetScrollCallback(win, gfw_cb_scroll);
        gfw.prev_key = glfwSetKeyCallback(win, gfw_cb_key);
        gfw.prev_char = glfwSetCharCallback(win, gfw_cb_char);
        gfw.prev_focus = glfwSetWindowFocusCallback(win, gfw_cb_focus);
    }
    return GOOME_OK;
}

void goome_glfw_set_ui(goome *g, goome_ui *u)
{
    if (gfw_ours(g)) gfw.u = u;
}

void goome_glfw_frame(goome *g)
{
    if (!gfw_ours(g)) return;
    gfw_size();
    gfw_push_clip();
    int shape = gfw_over() ? goome_cursor_shape(g) : -1;
    if (shape == gfw.shape) return;
    gfw.shape = shape;
    glfwSetCursor(gfw.win, shape < 0 ? NULL : gfw_cursor(shape));
}

void goome_glfw_shutdown(goome *g)
{
    if (!gfw_ours(g)) return;
    if (gfw.installed) {
        glfwSetCursorPosCallback(gfw.win, gfw.prev_pos);
        glfwSetMouseButtonCallback(gfw.win, gfw.prev_button);
        glfwSetScrollCallback(gfw.win, gfw.prev_scroll);
        glfwSetKeyCallback(gfw.win, gfw.prev_key);
        glfwSetCharCallback(gfw.win, gfw.prev_char);
        glfwSetWindowFocusCallback(gfw.win, gfw.prev_focus);
    }
    if (gfw.shape >= 0) glfwSetCursor(gfw.win, NULL);
    for (int i = 0; i < GOOME_CURSOR_HIDDEN; i++)
        if (gfw.cur[i]) glfwDestroyCursor(gfw.cur[i]);
    free(gfw.clip);
    memset(&gfw, 0, sizeof(gfw));
}

#endif
