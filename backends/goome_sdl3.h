#ifndef GOOME_SDL3_H
#define GOOME_SDL3_H

#include "goome.h"
#ifndef SDL_MAJOR_VERSION
#include <SDL3/SDL.h>
#endif

int goome_sdl3_init(goome *g, SDL_Window *win);
int goome_sdl3_event(goome *g, const SDL_Event *e);
void goome_sdl3_set_ui(goome *g, goome_ui *u);
void goome_sdl3_frame(goome *g);
void goome_sdl3_shutdown(goome *g);

#endif

#if defined(GOOME_IMPL_SDL3_IMPLEMENTATION) && !defined(GOOME_IMPL_SDL3_DONE)
#define GOOME_IMPL_SDL3_DONE

#include <stdlib.h>
#include <string.h>

typedef struct {
    goome *g;
    goome_ui *u;
    SDL_Window *win;
    SDL_WindowID id;
    int held, lw, lh, pw, ph, shape, text_on;
    double mx, my, wheel;
    char *clip;
    SDL_Cursor *cur[GOOME_CURSOR_HIDDEN], *game_cursor;
} gs3_state;

static gs3_state gs3;

static int gs3_kmods(SDL_Keymod m)
{
    return (m & SDL_KMOD_SHIFT ? GOOME_SHIFT : 0) | (m & SDL_KMOD_CTRL ? GOOME_CTRL : 0) | (m & SDL_KMOD_ALT ? GOOME_ALT : 0) |
           (m & SDL_KMOD_GUI ? GOOME_SUPER : 0);
}

static void gs3_push(int type, int button, int dy, int key, int mods, const char *text, int len)
{
    goome_event e;
    memset(&e, 0, sizeof(e));
    e.type = type, e.x = gs3.mx, e.y = gs3.my, e.button = button, e.dy = dy, e.key = key, e.mods = mods;
    if (text) memcpy(e.text, text, len < 7 ? len : 7);
    goome_push(gs3.g, &e);
}

static int gs3_over(void) { return gs3.held || goome_hit(gs3.g, gs3.mx, gs3.my); }

static int gs3_wants_keys(void) { return gs3.u && (goome_ui_wants(gs3.u) & GOOME_WANT_KEYBOARD); }

static void gs3_pull_clip(void)
{
    if (!SDL_HasClipboardText()) return;
    char *s = SDL_GetClipboardText();
    if (!s) return;
    goome_clipboard_set(gs3.g, s);
    free(gs3.clip);
    gs3.clip = malloc(strlen(s) + 1);
    if (gs3.clip) memcpy(gs3.clip, s, strlen(s) + 1);
    SDL_free(s);
}

static void gs3_push_clip(void)
{
    int n = goome_clipboard_get(gs3.g, NULL, 0);
    if (n < 0 || (n == 0 && !gs3.clip)) return;
    char *s = malloc((size_t)n + 1);
    if (!s) return;
    goome_clipboard_get(gs3.g, s, n + 1);
    if (gs3.clip && !strcmp(s, gs3.clip)) {
        free(s);
        return;
    }
    free(gs3.clip);
    gs3.clip = s;
    SDL_SetClipboardText(s);
}

static void gs3_size(void)
{
    int lw = 0, lh = 0, pw = 0, ph = 0;
    SDL_GetWindowSize(gs3.win, &lw, &lh);
    SDL_GetWindowSizeInPixels(gs3.win, &pw, &ph);
    if (lw <= 0 || lh <= 0 || pw <= 0 || ph <= 0) return;
    if (lw == gs3.lw && lh == gs3.lh && pw == gs3.pw && ph == gs3.ph) return;
    gs3.lw = lw, gs3.lh = lh, gs3.pw = pw, gs3.ph = ph;
    goome_resize(gs3.g, lw, lh, (double)pw / lw);
}

static int gs3_key_code(SDL_Keycode k)
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

static int gs3_text(const char *s)
{
    const unsigned char *p = (const unsigned char *)s;
    while (p && *p) {
        int n = *p < 0x80 ? 1 : *p < 0xE0 ? 2 : *p < 0xF0 ? 3 : 4;
        int k = *p < 0x80 ? *p : 0, mods = gs3_kmods(SDL_GetModState()) & GOOME_SHIFT;
        if (k >= 'A' && k <= 'Z') k += 32, mods |= GOOME_SHIFT;
        if (*p >= 32 && *p != 127) gs3_push(GOOME_KEY, 0, 0, k, mods, (const char *)p, n);
        for (int i = 0; i < n && *p; i++) p++;
    }
    return gs3_wants_keys();
}

int goome_sdl3_event(goome *g, const SDL_Event *e)
{
    if (!g || g != gs3.g || !e) return 0;
    switch (e->type) {
    case SDL_EVENT_MOUSE_MOTION:
        if (e->motion.windowID != gs3.id || e->motion.which == SDL_TOUCH_MOUSEID) return 0;
        gs3.mx = e->motion.x, gs3.my = e->motion.y;
        gs3_push(GOOME_MOTION, 0, 0, 0, gs3_kmods(SDL_GetModState()), NULL, 0);
        return gs3_over();
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP: {
        if (e->button.windowID != gs3.id || e->button.which == SDL_TOUCH_MOUSEID) return 0;
        int b = e->button.button, bit = 1 << b, eaten;
        if (b < 1 || b > 3) return gs3.held != 0;
        gs3.mx = e->button.x, gs3.my = e->button.y;
        if (e->type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
            eaten = gs3.held || goome_hit(g, gs3.mx, gs3.my);
            if (eaten) gs3.held |= bit;
            gs3_push(GOOME_DOWN, b, 0, 0, gs3_kmods(SDL_GetModState()), NULL, 0);
            if (eaten && gs3.held == bit) SDL_CaptureMouse(true);
        } else {
            eaten = (gs3.held & bit) != 0;
            gs3.held &= ~bit;
            gs3_push(GOOME_UP, b, 0, 0, gs3_kmods(SDL_GetModState()), NULL, 0);
            if (eaten && !gs3.held) SDL_CaptureMouse(false);
        }
        return eaten;
    }
    case SDL_EVENT_MOUSE_WHEEL: {
        if (e->wheel.windowID != gs3.id || e->wheel.which == SDL_TOUCH_MOUSEID) return 0;
        double dy = e->wheel.y;
        if (e->wheel.direction == SDL_MOUSEWHEEL_FLIPPED) dy = -dy;
        gs3.mx = e->wheel.mouse_x, gs3.my = e->wheel.mouse_y;
        gs3.wheel -= dy;
        int n = (int)gs3.wheel;
        gs3.wheel -= n;
        if (n) gs3_push(GOOME_SCROLL, 0, n, 0, gs3_kmods(SDL_GetModState()), NULL, 0);
        return gs3_over();
    }
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP: {
        if (e->key.windowID != gs3.id) return 0;
        int eaten = gs3_wants_keys();
        if (e->type == SDL_EVENT_KEY_UP) return eaten;
        int mods = gs3_kmods(e->key.mod);
        SDL_Keycode sym = e->key.key;
        int k = gs3_key_code(sym);
        if (!k && sym >= 32 && sym < 127 && (mods & (GOOME_CTRL | GOOME_ALT | GOOME_SUPER))) {
            k = sym >= 'A' && sym <= 'Z' ? (int)sym + 32 : (int)sym;
            if (k == 'v' && (mods & (GOOME_CTRL | GOOME_SUPER))) gs3_pull_clip();
        }
        if (k) gs3_push(GOOME_KEY, 0, 0, k, mods, NULL, 0);
        return eaten;
    }
    case SDL_EVENT_TEXT_INPUT:
        if (e->text.windowID != gs3.id) return 0;
        return gs3_text(e->text.text);
    case SDL_EVENT_WINDOW_FOCUS_GAINED:
    case SDL_EVENT_WINDOW_FOCUS_LOST: {
        if (e->window.windowID != gs3.id) return 0;
        goome_event f;
        memset(&f, 0, sizeof(f));
        f.type = GOOME_FOCUS, f.id = e->type == SDL_EVENT_WINDOW_FOCUS_GAINED;
        goome_push(g, &f);
        if (!f.id) gs3.held = 0;
        return 0;
    }
    case SDL_EVENT_WINDOW_RESIZED:
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        if (e->window.windowID == gs3.id) gs3_size();
        return 0;
    }
    return 0;
}

int goome_sdl3_init(goome *g, SDL_Window *win)
{
    if (!g || !win) return GOOME_EARG;
    if (gs3.g) goome_sdl3_shutdown(gs3.g);
    memset(&gs3, 0, sizeof(gs3));
    gs3.g = g, gs3.win = win, gs3.id = SDL_GetWindowID(win), gs3.shape = -1;
    float x = 0, y = 0;
    if (SDL_GetMouseFocus() == win) SDL_GetMouseState(&x, &y);
    gs3.mx = x, gs3.my = y;
    gs3_size();
    return GOOME_OK;
}

void goome_sdl3_set_ui(goome *g, goome_ui *u)
{
    if (g && g == gs3.g) gs3.u = u;
}

void goome_sdl3_frame(goome *g)
{
    if (!g || g != gs3.g) return;
    gs3_size();
    gs3_push_clip();
    int keys = gs3_wants_keys();
    if (keys && !SDL_TextInputActive(gs3.win)) SDL_StartTextInput(gs3.win), gs3.text_on = 1;
    if (!keys && gs3.text_on) SDL_StopTextInput(gs3.win), gs3.text_on = 0;
    int shape = gs3_over() ? goome_cursor_shape(g) : -1;
    if (shape == gs3.shape) return;
    if (gs3.shape < 0) gs3.game_cursor = SDL_GetCursor();
    gs3.shape = shape;
    if (shape == GOOME_CURSOR_HIDDEN) {
        SDL_HideCursor();
        return;
    }
    SDL_ShowCursor();
    if (shape < 0) {
        SDL_SetCursor(gs3.game_cursor ? gs3.game_cursor : SDL_GetDefaultCursor());
        return;
    }
    static const SDL_SystemCursor sys[GOOME_CURSOR_HIDDEN] = {
        SDL_SYSTEM_CURSOR_DEFAULT, SDL_SYSTEM_CURSOR_TEXT, SDL_SYSTEM_CURSOR_POINTER, SDL_SYSTEM_CURSOR_MOVE, SDL_SYSTEM_CURSOR_EW_RESIZE,
        SDL_SYSTEM_CURSOR_NS_RESIZE, SDL_SYSTEM_CURSOR_NWSE_RESIZE, SDL_SYSTEM_CURSOR_NESW_RESIZE, SDL_SYSTEM_CURSOR_WAIT, SDL_SYSTEM_CURSOR_NOT_ALLOWED,
    };
    if (!gs3.cur[shape]) gs3.cur[shape] = SDL_CreateSystemCursor(sys[shape]);
    if (gs3.cur[shape]) SDL_SetCursor(gs3.cur[shape]);
}

void goome_sdl3_shutdown(goome *g)
{
    if (!g || g != gs3.g) return;
    if (gs3.shape >= 0) {
        SDL_ShowCursor();
        SDL_SetCursor(gs3.game_cursor ? gs3.game_cursor : SDL_GetDefaultCursor());
    }
    if (gs3.held) SDL_CaptureMouse(false);
    if (gs3.text_on) SDL_StopTextInput(gs3.win);
    for (int i = 0; i < GOOME_CURSOR_HIDDEN; i++)
        if (gs3.cur[i]) SDL_DestroyCursor(gs3.cur[i]);
    free(gs3.clip);
    memset(&gs3, 0, sizeof(gs3));
}

#endif
