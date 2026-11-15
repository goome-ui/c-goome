#ifndef GOOME_SDL2_H
#define GOOME_SDL2_H

#include "goome.h"
#ifndef SDL_MAJOR_VERSION
#include <SDL.h>
#endif

int goome_sdl2_init(goome *g, SDL_Window *win);
int goome_sdl2_event(goome *g, const SDL_Event *e);
void goome_sdl2_set_ui(goome *g, goome_ui *u);
void goome_sdl2_frame(goome *g);
void goome_sdl2_shutdown(goome *g);

#endif

#if defined(GOOME_IMPL_SDL2_IMPLEMENTATION) && !defined(GOOME_IMPL_SDL2_DONE)
#define GOOME_IMPL_SDL2_DONE

#include <stdlib.h>
#include <string.h>

typedef struct {
    goome *g;
    goome_ui *u;
    SDL_Window *win;
    Uint32 id;
    int held, lw, lh, pw, ph, shape, text_on;
    double mx, my, wheel;
    char *clip;
    SDL_Cursor *cur[GOOME_CURSOR_HIDDEN], *game_cursor;
} gs2_state;

static gs2_state gs2;

static int gs2_mods(void)
{
    SDL_Keymod m = SDL_GetModState();
    return (m & KMOD_SHIFT ? GOOME_SHIFT : 0) | (m & KMOD_CTRL ? GOOME_CTRL : 0) | (m & KMOD_ALT ? GOOME_ALT : 0) | (m & KMOD_GUI ? GOOME_SUPER : 0);
}

static void gs2_push(int type, int button, int dy, int key, int mods, const char *text, int len)
{
    goome_event e;
    memset(&e, 0, sizeof(e));
    e.type = type, e.x = gs2.mx, e.y = gs2.my, e.button = button, e.dy = dy, e.key = key, e.mods = mods;
    if (text) memcpy(e.text, text, len < 7 ? len : 7);
    goome_push(gs2.g, &e);
}

static int gs2_over(void) { return gs2.held || goome_hit(gs2.g, gs2.mx, gs2.my); }

static int gs2_wants_keys(void) { return gs2.u && (goome_ui_wants(gs2.u) & GOOME_WANT_KEYBOARD); }

static void gs2_pull_clip(void)
{
    if (!SDL_HasClipboardText()) return;
    char *s = SDL_GetClipboardText();
    if (!s) return;
    goome_clipboard_set(gs2.g, s);
    free(gs2.clip);
    gs2.clip = malloc(strlen(s) + 1);
    if (gs2.clip) memcpy(gs2.clip, s, strlen(s) + 1);
    SDL_free(s);
}

static void gs2_push_clip(void)
{
    int n = goome_clipboard_get(gs2.g, NULL, 0);
    if (n < 0 || (n == 0 && !gs2.clip)) return;
    char *s = malloc((size_t)n + 1);
    if (!s) return;
    goome_clipboard_get(gs2.g, s, n + 1);
    if (gs2.clip && !strcmp(s, gs2.clip)) {
        free(s);
        return;
    }
    free(gs2.clip);
    gs2.clip = s;
    SDL_SetClipboardText(s);
}

static void gs2_size(void)
{
    int lw, lh, pw, ph;
    SDL_GetWindowSize(gs2.win, &lw, &lh);
#if SDL_VERSION_ATLEAST(2, 26, 0)
    SDL_GetWindowSizeInPixels(gs2.win, &pw, &ph);
#else
    SDL_GL_GetDrawableSize(gs2.win, &pw, &ph);
    if (pw <= 0 || ph <= 0) pw = lw, ph = lh;
#endif
    if (lw <= 0 || lh <= 0 || pw <= 0 || ph <= 0) return;
    if (lw == gs2.lw && lh == gs2.lh && pw == gs2.pw && ph == gs2.ph) return;
    gs2.lw = lw, gs2.lh = lh, gs2.pw = pw, gs2.ph = ph;
    goome_resize(gs2.g, lw, lh, (double)pw / lw);
}

static int gs2_key_code(SDL_Keycode k)
{
    if (k >= SDLK_F1 && k <= SDLK_F12) return GOOME_K_F1 + (int)(k - SDLK_F1);
    switch (k) {
    case SDLK_BACKSPACE: return GOOME_K_BACKSPACE;
    case SDLK_TAB: return GOOME_K_TAB;
    case SDLK_RETURN:
    case SDLK_KP_ENTER: return GOOME_K_ENTER;
    case SDLK_ESCAPE: return GOOME_K_ESC;
    case SDLK_DELETE: return GOOME_K_DELETE;
    case SDLK_LEFT: return GOOME_K_LEFT;
    case SDLK_RIGHT: return GOOME_K_RIGHT;
    case SDLK_UP: return GOOME_K_UP;
    case SDLK_DOWN: return GOOME_K_DOWN;
    case SDLK_HOME: return GOOME_K_HOME;
    case SDLK_END: return GOOME_K_END;
    case SDLK_PAGEUP: return GOOME_K_PGUP;
    case SDLK_PAGEDOWN: return GOOME_K_PGDN;
    case SDLK_INSERT: return GOOME_K_INSERT;
    }
    return 0;
}

static int gs2_text(const char *s)
{
    const unsigned char *p = (const unsigned char *)s;
    while (*p) {
        int n = *p < 0x80 ? 1 : *p < 0xE0 ? 2 : *p < 0xF0 ? 3 : 4;
        int k = *p < 0x80 ? *p : 0, mods = gs2_mods() & GOOME_SHIFT;
        if (k >= 'A' && k <= 'Z') k += 32, mods |= GOOME_SHIFT;
        if (*p >= 32 && *p != 127) gs2_push(GOOME_KEY, 0, 0, k, mods, (const char *)p, n);
        for (int i = 0; i < n && *p; i++) p++;
    }
    return gs2_wants_keys();
}

int goome_sdl2_event(goome *g, const SDL_Event *e)
{
    if (!g || g != gs2.g || !e) return 0;
    switch (e->type) {
    case SDL_MOUSEMOTION:
        if (e->motion.windowID != gs2.id || e->motion.which == SDL_TOUCH_MOUSEID) return 0;
        gs2.mx = e->motion.x, gs2.my = e->motion.y;
        gs2_push(GOOME_MOTION, 0, 0, 0, gs2_mods(), NULL, 0);
        return gs2_over();
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP: {
        if (e->button.windowID != gs2.id || e->button.which == SDL_TOUCH_MOUSEID) return 0;
        int b = e->button.button, bit = 1 << b, eaten;
        if (b < 1 || b > 3) return gs2.held != 0;
        gs2.mx = e->button.x, gs2.my = e->button.y;
        if (e->type == SDL_MOUSEBUTTONDOWN) {
            eaten = gs2.held || goome_hit(g, gs2.mx, gs2.my);
            if (eaten) gs2.held |= bit;
            gs2_push(GOOME_DOWN, b, 0, 0, gs2_mods(), NULL, 0);
            if (eaten && gs2.held == bit) SDL_CaptureMouse(SDL_TRUE);
        } else {
            eaten = (gs2.held & bit) != 0;
            gs2.held &= ~bit;
            gs2_push(GOOME_UP, b, 0, 0, gs2_mods(), NULL, 0);
            if (eaten && !gs2.held) SDL_CaptureMouse(SDL_FALSE);
        }
        return eaten;
    }
    case SDL_MOUSEWHEEL: {
        if (e->wheel.windowID != gs2.id || e->wheel.which == SDL_TOUCH_MOUSEID) return 0;
#if SDL_VERSION_ATLEAST(2, 0, 18)
        double dy = e->wheel.preciseY;
#else
        double dy = e->wheel.y;
#endif
        if (e->wheel.direction == SDL_MOUSEWHEEL_FLIPPED) dy = -dy;
        gs2.wheel -= dy;
        int n = (int)gs2.wheel;
        gs2.wheel -= n;
        if (n) gs2_push(GOOME_SCROLL, 0, n, 0, gs2_mods(), NULL, 0);
        return gs2_over();
    }
    case SDL_KEYDOWN:
    case SDL_KEYUP: {
        if (e->key.windowID != gs2.id) return 0;
        int eaten = gs2_wants_keys();
        if (e->type == SDL_KEYUP) return eaten;
        SDL_Keymod sm = (SDL_Keymod)e->key.keysym.mod;
        int mods = (sm & KMOD_SHIFT ? GOOME_SHIFT : 0) | (sm & KMOD_CTRL ? GOOME_CTRL : 0) | (sm & KMOD_ALT ? GOOME_ALT : 0) | (sm & KMOD_GUI ? GOOME_SUPER : 0);
        SDL_Keycode sym = e->key.keysym.sym;
        int k = gs2_key_code(sym);
        if (!k && sym >= 32 && sym < 127 && (mods & (GOOME_CTRL | GOOME_ALT | GOOME_SUPER))) {
            k = sym >= 'A' && sym <= 'Z' ? sym + 32 : sym;
            if (k == 'v' && (mods & (GOOME_CTRL | GOOME_SUPER))) gs2_pull_clip();
        }
        if (k) gs2_push(GOOME_KEY, 0, 0, k, mods, NULL, 0);
        return eaten;
    }
    case SDL_TEXTINPUT:
        if (e->text.windowID != gs2.id) return 0;
        return gs2_text(e->text.text);
    case SDL_WINDOWEVENT:
        if (e->window.windowID != gs2.id) return 0;
        if (e->window.event == SDL_WINDOWEVENT_FOCUS_GAINED || e->window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
            goome_event f;
            memset(&f, 0, sizeof(f));
            f.type = GOOME_FOCUS, f.id = e->window.event == SDL_WINDOWEVENT_FOCUS_GAINED;
            goome_push(g, &f);
            if (!f.id) gs2.held = 0;
        }
        if (e->window.event == SDL_WINDOWEVENT_SIZE_CHANGED) gs2_size();
        return 0;
    }
    return 0;
}

int goome_sdl2_init(goome *g, SDL_Window *win)
{
    if (!g || !win) return GOOME_EARG;
    if (gs2.g) goome_sdl2_shutdown(gs2.g);
    memset(&gs2, 0, sizeof(gs2));
    gs2.g = g, gs2.win = win, gs2.id = SDL_GetWindowID(win), gs2.shape = -1;
    int x = 0, y = 0;
    if (SDL_GetMouseFocus() == win) SDL_GetMouseState(&x, &y);
    gs2.mx = x, gs2.my = y;
    gs2_size();
    return GOOME_OK;
}

void goome_sdl2_set_ui(goome *g, goome_ui *u)
{
    if (g && g == gs2.g) gs2.u = u;
}

void goome_sdl2_frame(goome *g)
{
    if (!g || g != gs2.g) return;
    gs2_size();
    gs2_push_clip();
    int keys = gs2_wants_keys();
    if (keys && !SDL_IsTextInputActive()) SDL_StartTextInput(), gs2.text_on = 1;
    if (!keys && gs2.text_on) SDL_StopTextInput(), gs2.text_on = 0;
    int shape = gs2_over() ? goome_cursor_shape(g) : -1;
    if (shape == gs2.shape) return;
    if (gs2.shape < 0) gs2.game_cursor = SDL_GetCursor();
    gs2.shape = shape;
    if (shape == GOOME_CURSOR_HIDDEN) {
        SDL_ShowCursor(SDL_DISABLE);
        return;
    }
    SDL_ShowCursor(SDL_ENABLE);
    if (shape < 0) {
        SDL_SetCursor(gs2.game_cursor ? gs2.game_cursor : SDL_GetDefaultCursor());
        return;
    }
    static const SDL_SystemCursor sys[GOOME_CURSOR_HIDDEN] = {
        SDL_SYSTEM_CURSOR_ARROW, SDL_SYSTEM_CURSOR_IBEAM, SDL_SYSTEM_CURSOR_HAND, SDL_SYSTEM_CURSOR_SIZEALL, SDL_SYSTEM_CURSOR_SIZEWE,
        SDL_SYSTEM_CURSOR_SIZENS, SDL_SYSTEM_CURSOR_SIZENWSE, SDL_SYSTEM_CURSOR_SIZENESW, SDL_SYSTEM_CURSOR_WAIT, SDL_SYSTEM_CURSOR_NO,
    };
    if (!gs2.cur[shape]) gs2.cur[shape] = SDL_CreateSystemCursor(sys[shape]);
    if (gs2.cur[shape]) SDL_SetCursor(gs2.cur[shape]);
}

void goome_sdl2_shutdown(goome *g)
{
    if (!g || g != gs2.g) return;
    if (gs2.shape >= 0) {
        SDL_ShowCursor(SDL_ENABLE);
        SDL_SetCursor(gs2.game_cursor ? gs2.game_cursor : SDL_GetDefaultCursor());
    }
    if (gs2.held) SDL_CaptureMouse(SDL_FALSE);
    if (gs2.text_on) SDL_StopTextInput();
    for (int i = 0; i < GOOME_CURSOR_HIDDEN; i++)
        if (gs2.cur[i]) SDL_FreeCursor(gs2.cur[i]);
    free(gs2.clip);
    memset(&gs2, 0, sizeof(gs2));
}

#endif
