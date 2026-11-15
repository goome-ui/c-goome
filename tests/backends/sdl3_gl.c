#include <SDL3/SDL.h>

#define GOOME_IMPL_SDL3_IMPLEMENTATION
#include "goome_sdl3.h"
#define GOOME_IMPL_GL3_IMPLEMENTATION
#include "goome_gl3.h"

#include "demo.h"
#include "gltest.h"

typedef struct {
    demo *d;
    SDL_Window *win;
    SDL_WindowID id;
} run;

static goome_gl_proc load(const char *name) { return (goome_gl_proc)SDL_GL_GetProcAddress(name); }

static int pump(run *r)
{
    SDL_Event e;
    int game = 0;
    while (SDL_PollEvent(&e)) {
        int input = e.type == SDL_EVENT_MOUSE_MOTION || e.type == SDL_EVENT_MOUSE_BUTTON_DOWN || e.type == SDL_EVENT_MOUSE_BUTTON_UP ||
                    e.type == SDL_EVENT_KEY_DOWN || e.type == SDL_EVENT_KEY_UP || e.type == SDL_EVENT_TEXT_INPUT;
        if (!goome_sdl3_event(r->d->g, &e) && input) game++;
    }
    return game;
}

static void draw(run *r)
{
    int fw, fh;
    pump(r);
    goome_sdl3_frame(r->d->g);
    demo_frame(r->d);
    SDL_GetWindowSizeInPixels(r->win, &fw, &fh);
    gltest_clear(fw, fh);
    goome_gl3_render(r->d->g);
}

static void tick(void *ctx)
{
    run *r = ctx;
    draw(r);
    SDL_GL_SwapWindow(r->win);
}

static int send(void *ctx, int type, double x, double y, int ch)
{
    static char text[2][2] = {"x", "x"};
    static int flip;
    run *r = ctx;
    SDL_Event e;
    memset(&e, 0, sizeof(e));
    if (type == EV_MOVE) {
        e.type = SDL_EVENT_MOUSE_MOTION, e.motion.windowID = r->id, e.motion.x = (float)x, e.motion.y = (float)y;
        SDL_PushEvent(&e);
    } else if (type == EV_DOWN || type == EV_UP) {
        e.type = type == EV_DOWN ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
        e.button.windowID = r->id, e.button.button = SDL_BUTTON_LEFT, e.button.x = (float)x, e.button.y = (float)y, e.button.clicks = 1;
        e.button.down = type == EV_DOWN;
        SDL_PushEvent(&e);
    } else {
        e.type = SDL_EVENT_KEY_DOWN, e.key.windowID = r->id, e.key.key = (SDL_Keycode)ch, e.key.down = true;
        SDL_PushEvent(&e);
        flip ^= 1;
        text[flip][0] = (char)ch;
        memset(&e, 0, sizeof(e));
        e.type = SDL_EVENT_TEXT_INPUT, e.text.windowID = r->id, e.text.text = text[flip];
        SDL_PushEvent(&e);
        memset(&e, 0, sizeof(e));
        e.type = SDL_EVENT_KEY_UP, e.key.windowID = r->id, e.key.key = (SDL_Keycode)ch;
        SDL_PushEvent(&e);
    }
    return pump(r) == 0;
}

static int one(int es)
{
    printf("sdl3 + %s\n", es ? "gles3" : "gl3");
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, es ? 0 : 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, es ? SDL_GL_CONTEXT_PROFILE_ES : SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_Window *win = SDL_CreateWindow("goome backend test", DEMO_W, DEMO_H, SDL_WINDOW_OPENGL);
    SDL_GLContext ctx = win ? SDL_GL_CreateContext(win) : NULL;
    if (!ctx) return printf("  no %s window: %s\n", es ? "gles 3" : "gl 3.3", SDL_GetError()), 1;
    SDL_GL_SetSwapInterval(0);
    demo d;
    if (!gltest_load(load) || !demo_open(&d)) return 1;
    run r = {&d, win, SDL_GetWindowID(win)};
    goome_sdl3_init(d.g, win);
    goome_sdl3_set_ui(d.g, d.u);
    int e = goome_gl3_init(d.g, es ? load : NULL);
    demo_expect(&d, "renderer init", e, GOOME_OK);
    if (e == GOOME_OK) {
        draw(&r);
        gltest_compare(&d, "first frame matches goome_frame", DEMO_W, DEMO_H);
        SDL_GL_SwapWindow(win);
        demo_input(&d, send, tick, &r);
        draw(&r);
        gltest_compare(&d, "dirty updates match goome_frame", DEMO_W, DEMO_H);
        SDL_GL_SwapWindow(win);
        demo_frame(&d);
        gltest_state(&d, goome_gl3_render, DEMO_W, DEMO_H);
        goome_gl3_shutdown(d.g);
    }
    goome_sdl3_shutdown(d.g);
    int fails = d.fails;
    demo_close(&d);
    SDL_GL_DestroyContext(ctx);
    SDL_DestroyWindow(win);
    return fails;
}

int main(void)
{
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "x11");
    SDL_SetHint(SDL_HINT_VIDEO_FORCE_EGL, "1");
    if (!SDL_Init(SDL_INIT_VIDEO)) return printf("sdl cant start: %s\n", SDL_GetError()), 1;
    int fails = one(0) + one(1);
    SDL_Quit();
    printf(fails ? "%d failed\n" : "all good\n", fails);
    return fails != 0;
}
