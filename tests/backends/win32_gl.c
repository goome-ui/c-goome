#define GOOME_IMPL_WIN32_IMPLEMENTATION
#include "goome_win32.h"
#define GOOME_IMPL_GL3_IMPLEMENTATION
#include "goome_gl3.h"
#define GOOME_IMPL_GL2_IMPLEMENTATION
#include "goome_gl2.h"

#include "demo.h"
#include "gltest.h"

typedef struct {
    demo *d;
    HWND win;
    HDC dc;
    void (*render)(goome *);
} run;

static goome *cur;
static int game_seen;

static LRESULT CALLBACK game_proc(HWND h, UINT msg, WPARAM w, LPARAM l)
{
    int eaten = 0;
    LRESULT r = goome_win32_wndproc(cur, h, msg, w, l, &eaten);
    if (eaten) return r;
    if ((msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST) || (msg >= WM_KEYFIRST && msg <= WM_KEYLAST)) game_seen++;
    return DefWindowProcW(h, msg, w, l);
}

static HWND make_window(void)
{
    WNDCLASSW wc;
    memset(&wc, 0, sizeof(wc));
    wc.style = CS_OWNDC, wc.lpfnWndProc = game_proc, wc.hInstance = GetModuleHandleW(NULL), wc.lpszClassName = L"goome_backend_test";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassW(&wc);
    RECT r = {0, 0, DEMO_W, DEMO_H};
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
    HWND h = CreateWindowW(L"goome_backend_test", L"goome backend test", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, r.right - r.left, r.bottom - r.top, NULL, NULL,
                           wc.hInstance, NULL);
    RECT c;
    if (h && GetClientRect(h, &c) && (c.right != DEMO_W || c.bottom != DEMO_H)) SetWindowPos(h, NULL, 0, 0, r.right - r.left + DEMO_W - c.right, r.bottom - r.top + DEMO_H - c.bottom, SWP_NOMOVE | SWP_NOZORDER);
    return h;
}

static void pump(void)
{
    MSG m;
    while (PeekMessageW(&m, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
}

static void draw(run *r)
{
    pump();
    goome_win32_frame(r->d->g);
    demo_frame(r->d);
    if (!r->render) return;
    gltest_clear(DEMO_W, DEMO_H);
    r->render(r->d->g);
}

static void tick(void *ctx)
{
    run *r = ctx;
    draw(r);
    if (r->render) SwapBuffers(r->dc);
}

static int send(void *ctx, int type, double x, double y, int ch)
{
    run *r = ctx;
    int before = game_seen;
    LPARAM at = MAKELPARAM((int)x, (int)y);
    if (type == EV_MOVE) SendMessageW(r->win, WM_MOUSEMOVE, 0, at);
    if (type == EV_DOWN) SendMessageW(r->win, WM_LBUTTONDOWN, MK_LBUTTON, at);
    if (type == EV_UP) SendMessageW(r->win, WM_LBUTTONUP, 0, at);
    if (type == EV_KEY) {
        SendMessageW(r->win, WM_KEYDOWN, (WPARAM)(ch - 32), 1);
        SendMessageW(r->win, WM_CHAR, (WPARAM)ch, 1);
        SendMessageW(r->win, WM_KEYUP, (WPARAM)(ch - 32), (LPARAM)0xC0000001u);
    }
    return game_seen == before;
}

static int one(const char *what, int gl)
{
    printf("win32 + %s\n", what);
    demo d;
    if (!demo_open(&d)) return 1;
    cur = d.g;
    run r = {&d, make_window(), NULL, NULL};
    if (!r.win) return printf("  cant make a window\n"), 1;
    HGLRC rc = NULL;
    if (gl) {
        r.dc = GetDC(r.win);
        PIXELFORMATDESCRIPTOR pfd;
        memset(&pfd, 0, sizeof(pfd));
        pfd.nSize = sizeof(pfd), pfd.nVersion = 1, pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.iPixelType = PFD_TYPE_RGBA, pfd.cColorBits = 32, pfd.cAlphaBits = 8;
        int pf = ChoosePixelFormat(r.dc, &pfd);
        if (pf) SetPixelFormat(r.dc, pf, &pfd);
        rc = pf ? wglCreateContext(r.dc) : NULL;
        if (!rc || !wglMakeCurrent(r.dc, rc)) return printf("  no gl context\n"), 1;
        if (!gltest_load(gogl_default_proc)) return 1;
        const char *v = (const char *)gt.GetString(0x1F02);
        int ok3 = gl == 3 && v && (v[0] > '3' || (v[0] == '3' && v[2] >= '3'));
        if (gl == 3 && !ok3) {
            printf("  gl %s is older than 3.3, skipped\n", v ? v : "?");
            wglMakeCurrent(NULL, NULL);
            wglDeleteContext(rc);
            DestroyWindow(r.win);
            demo_close(&d);
            return 0;
        }
        r.render = gl == 3 ? goome_gl3_render : goome_gl2_render;
    }
    goome_win32_init(d.g, r.win);
    goome_win32_set_ui(d.g, d.u);
    int e = gl == 3 ? goome_gl3_init(d.g, NULL) : gl == 2 ? goome_gl2_init(d.g, NULL) : GOOME_OK;
    demo_expect(&d, "init", e, GOOME_OK);
    goome_monitor m;
    demo_expect(&d, "goome got the client size", goome_monitors(d.g, &m, 1) == 1 && m.area.w == DEMO_W && m.area.h == DEMO_H, 1);
    if (e == GOOME_OK) {
        if (gl) {
            draw(&r);
            gltest_compare(&d, "first frame matches goome_frame", DEMO_W, DEMO_H);
            SwapBuffers(r.dc);
        }
        demo_input(&d, send, tick, &r);
        if (gl) {
            draw(&r);
            gltest_compare(&d, "dirty updates match goome_frame", DEMO_W, DEMO_H);
            SwapBuffers(r.dc);
            demo_frame(&d);
            gltest_state(&d, r.render, DEMO_W, DEMO_H);
            if (gl == 3) goome_gl3_shutdown(d.g);
            else goome_gl2_shutdown(d.g);
        }
    }
    goome_win32_shutdown(d.g);
    if (rc) {
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(rc);
    }
    DestroyWindow(r.win);
    int fails = d.fails;
    demo_close(&d);
    return fails;
}

int main(void)
{
    int fails = one("plain window", 0) + one("gl2", 2) + one("gl3", 3);
    printf(fails ? "%d failed\n" : "all good\n", fails);
    return fails != 0;
}
