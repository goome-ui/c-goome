#include <SDL.h>

#define GOOME_IMPL_SDL2_IMPLEMENTATION
#include "goome_sdl2.h"
#define GOOME_IMPL_GL3_IMPLEMENTATION
#include "goome_gl3.h"

#include "demo.h"
#include "gltest.h"

typedef struct {
    demo *d;
    SDL_Window *win;
    Uint32 id;
} run;

static goome_gl_proc load(const char *name)
{
    void *p = SDL_GL_GetProcAddress(name);
    goome_gl_proc f;
    memcpy(&f, &p, sizeof(f));
    return f;
}

static int pump(run *r)
{
    SDL_Event e;
    int game = 0;
    while (SDL_PollEvent(&e)) {
        int input = e.type == SDL_MOUSEMOTION || e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP || e.type == SDL_KEYDOWN ||
                    e.type == SDL_KEYUP || e.type == SDL_TEXTINPUT;
        if (!goome_sdl2_event(r->d->g, &e) && input) game++;
    }
    return game;
}

static void draw(run *r)
{
    int fw, fh;
    pump(r);
    goome_sdl2_frame(r->d->g);
    demo_frame(r->d);
    SDL_GL_GetDrawableSize(r->win, &fw, &fh);
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
    run *r = ctx;
    SDL_Event e;
    memset(&e, 0, sizeof(e));
    if (type == EV_MOVE) {
        e.type = SDL_MOUSEMOTION, e.motion.windowID = r->id, e.motion.x = (int)x, e.motion.y = (int)y;
        SDL_PushEvent(&e);
    } else if (type == EV_DOWN || type == EV_UP) {
        e.type = type == EV_DOWN ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
        e.button.windowID = r->id, e.button.button = SDL_BUTTON_LEFT, e.button.x = (int)x, e.button.y = (int)y, e.button.clicks = 1;
        e.button.state = type == EV_DOWN ? SDL_PRESSED : SDL_RELEASED;
        SDL_PushEvent(&e);
    } else {
        e.type = SDL_KEYDOWN, e.key.windowID = r->id, e.key.keysym.sym = ch, e.key.state = SDL_PRESSED;
        SDL_PushEvent(&e);
        memset(&e, 0, sizeof(e));
        e.type = SDL_TEXTINPUT, e.text.windowID = r->id, e.text.text[0] = (char)ch;
        SDL_PushEvent(&e);
        memset(&e, 0, sizeof(e));
        e.type = SDL_KEYUP, e.key.windowID = r->id, e.key.keysym.sym = ch, e.key.state = SDL_RELEASED;
        SDL_PushEvent(&e);
    }
    return pump(r) == 0;
}

int main(void)
{
    printf("sdl2 + gl3\n");
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return printf("  sdl cant start: %s\n", SDL_GetError()), 1;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_Window *win = SDL_CreateWindow("goome backend test", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, DEMO_W, DEMO_H, SDL_WINDOW_OPENGL);
    SDL_GLContext ctx = win ? SDL_GL_CreateContext(win) : NULL;
    if (!ctx) return printf("  no gl 3.3 window: %s\n", SDL_GetError()), 1;
    SDL_GL_SetSwapInterval(0);
    demo d;
    if (!gltest_load(load) || !demo_open(&d)) return 1;
    run r = {&d, win, SDL_GetWindowID(win)};
    goome_sdl2_init(d.g, win);
    goome_sdl2_set_ui(d.g, d.u);
    int e = goome_gl3_init(d.g, load);
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
    goome_sdl2_shutdown(d.g);
    int fails = d.fails;
    demo_close(&d);
    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
    printf(fails ? "%d failed\n" : "all good\n", fails);
    return fails != 0;
}
