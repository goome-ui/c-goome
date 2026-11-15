#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#define GOOME_IMPL_GLFW_IMPLEMENTATION
#include "goome_glfw.h"
#define GOOME_IMPL_GL3_IMPLEMENTATION
#include "goome_gl3.h"
#define GOOME_IMPL_GL2_IMPLEMENTATION
#include "goome_gl2.h"

#include "demo.h"
#include "gltest.h"

typedef struct {
    demo *d;
    GLFWwindow *win;
    void (*render)(goome *);
    int game;
} run;

static int game_seen;

static void game_pos(GLFWwindow *w, double x, double y) { game_seen++; }
static void game_button(GLFWwindow *w, int b, int a, int m) { game_seen++; }
static void game_key(GLFWwindow *w, int k, int s, int a, int m) { game_seen++; }
static void game_char(GLFWwindow *w, unsigned c) { game_seen++; }

static void draw(run *r)
{
    int fw, fh;
    glfwPollEvents();
    goome_glfw_frame(r->d->g);
    demo_frame(r->d);
    glfwGetFramebufferSize(r->win, &fw, &fh);
    gltest_clear(fw, fh);
    r->render(r->d->g);
}

static void tick(void *ctx)
{
    run *r = ctx;
    draw(r);
    glfwSwapBuffers(r->win);
}

static int send(void *ctx, int type, double x, double y, int ch)
{
    run *r = ctx;
    GLFWcursorposfun pos = glfwSetCursorPosCallback(r->win, NULL);
    GLFWmousebuttonfun button = glfwSetMouseButtonCallback(r->win, NULL);
    GLFWkeyfun key = glfwSetKeyCallback(r->win, NULL);
    GLFWcharfun chr = glfwSetCharCallback(r->win, NULL);
    glfwSetCursorPosCallback(r->win, pos);
    glfwSetMouseButtonCallback(r->win, button);
    glfwSetKeyCallback(r->win, key);
    glfwSetCharCallback(r->win, chr);
    int before = game_seen;
    if (type == EV_MOVE) pos(r->win, x, y);
    if (type == EV_DOWN || type == EV_UP) button(r->win, GLFW_MOUSE_BUTTON_LEFT, type == EV_DOWN ? GLFW_PRESS : GLFW_RELEASE, 0);
    if (type == EV_KEY) {
        key(r->win, ch - 'a' + GLFW_KEY_A, 0, GLFW_PRESS, 0);
        chr(r->win, (unsigned)ch);
        key(r->win, ch - 'a' + GLFW_KEY_A, 0, GLFW_RELEASE, 0);
    }
    return game_seen == before;
}

static int one(const char *name, int gl3)
{
    printf("glfw + %s\n", name);
    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    if (gl3) {
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    }
    GLFWwindow *win = glfwCreateWindow(DEMO_W, DEMO_H, "goome backend test", NULL, NULL);
    if (!win) return printf("  cant make a %s window\n", name), 1;
    glfwMakeContextCurrent(win);
    glfwSwapInterval(0);
    demo d;
    if (!gltest_load(glfwGetProcAddress) || !demo_open(&d)) return 1;
    glfwSetCursorPosCallback(win, game_pos);
    glfwSetMouseButtonCallback(win, game_button);
    glfwSetKeyCallback(win, game_key);
    glfwSetCharCallback(win, game_char);
    goome_glfw_init(d.g, win, 1);
    goome_glfw_set_ui(d.g, d.u);
    run r = {&d, win, gl3 ? goome_gl3_render : goome_gl2_render, 0};
    int e = gl3 ? goome_gl3_init(d.g, glfwGetProcAddress) : goome_gl2_init(d.g, NULL);
    demo_expect(&d, "renderer init", e, GOOME_OK);
    if (e == GOOME_OK) {
        draw(&r);
        gltest_compare(&d, "first frame matches goome_frame", DEMO_W, DEMO_H);
        glfwSwapBuffers(win);
        demo_input(&d, send, tick, &r);
        draw(&r);
        gltest_compare(&d, "dirty updates match goome_frame", DEMO_W, DEMO_H);
        glfwSwapBuffers(win);
        demo_frame(&d);
        gltest_state(&d, r.render, DEMO_W, DEMO_H);
        if (gl3) goome_gl3_shutdown(d.g);
        else goome_gl2_shutdown(d.g);
    }
    goome_glfw_shutdown(d.g);
    demo_expect(&d, "game callbacks back after shutdown", glfwSetCursorPosCallback(win, NULL) == game_pos, 1);
    int fails = d.fails;
    demo_close(&d);
    glfwDestroyWindow(win);
    return fails;
}

int main(void)
{
    if (!glfwInit()) return printf("glfw cant start (no display?)\n"), 1;
    int fails = one("gl3", 1) + one("gl2", 0);
    glfwTerminate();
    printf(fails ? "%d failed\n" : "all good\n", fails);
    return fails != 0;
}
