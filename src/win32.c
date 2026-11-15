#include "internal.h"

#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _MSC_VER
#pragma comment(lib, "user32.lib") // cl only links kernel32 on its own
#endif

// coords: logical is the real origin plus real px / that monitors scale, so hits stay exact on mixed dpi

typedef struct {
    DWORD size;
    HDC dst;
    const POINT *pos;
    const SIZE *sz;
    HDC src;
    const POINT *src_pos;
    COLORREF key;
    const BLENDFUNCTION *blend;
    DWORD flags;
    const RECT *dirty;
} w32_ulw_info;

typedef struct {
    HDC(WINAPI *create_dc)(HDC);
    HBITMAP(WINAPI *dib)(HDC, const BITMAPINFO *, UINT, void **, HANDLE, DWORD);
    HGDIOBJ(WINAPI *select)(HDC, HGDIOBJ);
    BOOL(WINAPI *del_dc)(HDC);
    BOOL(WINAPI *del_obj)(HGDIOBJ);
    BOOL(WINAPI *blt)(HDC, int, int, int, int, HDC, int, int, DWORD);
    BOOL(WINAPI *flush)(void);
    int(WINAPI *caps)(HDC, int);
} w32_gdi_fns;

static const char *const w32_gdi_names[] = {"CreateCompatibleDC", "CreateDIBSection", "SelectObject", "DeleteDC",
                                            "DeleteObject", "BitBlt", "GdiFlush", "GetDeviceCaps"};

typedef struct {
    HMONITOR hm;
    RECT r;
    double s;
    int primary, id, rot;
    char name[64];
    HWND win;
    HDC dc;
    HBITMAP bmp;
    uint32_t *px;
    goome_canvas cv;
    int shown;
    long topped;
    goome_rect *hits;
    int nhits, hcap;
    HDC cdc;
    HBITMAP cbmp;
    uint32_t *cpx;
} w32_mon;

static struct {
    goome *g;
    HMODULE gdi_dll, shcore, ntdll;
    w32_gdi_fns gdi;
    BOOL(WINAPI *ulw_indirect)(HWND, const w32_ulw_info *);
    BOOL(WINAPI *set_affinity)(HWND, DWORD);
    BOOL(WINAPI *get_affinity)(HWND, DWORD *);
    int build;
    ATOM cls;
    w32_mon m[GM_MAX_MON];
    int n;
    HWND hot, before_kb;
    int fwd, ours, grab;
    double lx, ly;
    int wheel;
    unsigned hi;
    int recheck;
    long next_poll;
    gm_hotkey hk[GM_MAX_HK];
    int nreg;
    int hidden_ok;
} w32;

static struct {
    int aware, sysdpi;
    HRESULT(WINAPI *dpi_for_mon)(HMONITOR, int, UINT *, UINT *);
} w32d;

static int w32_cap_open(goome *g);
static int w32_grab(goome *g, int mon, goome_rect r, goome_image *img);
static void w32_cap_close(goome *g);
static gm_cap w32_cap = {"win32", NULL, w32_cap_open, w32_grab, w32_cap_close, GOOME_CAP_CAPTURE};

static void *w32_sym(HMODULE m, const char *name)
{
    FARPROC p = m ? GetProcAddress(m, name) : NULL;
    void *v = NULL;
    if (p) memcpy(&v, &p, sizeof(v) < sizeof(p) ? sizeof(v) : sizeof(p));
    return v;
}

#define W32_LOAD(dst, mod, name) \
    do { \
        void *w32_p_ = w32_sym(mod, name); \
        memcpy(&(dst), &w32_p_, sizeof(dst)); \
    } while (0)

static int w32_load_gdi(w32_gdi_fns *f, HMODULE *dll)
{
    if (*dll) return 1;
    HMODULE m = LoadLibraryA("gdi32.dll");
    if (!m) return 0;
    char *slot = (char *)f;
    for (size_t i = 0; i < sizeof(w32_gdi_names) / sizeof(w32_gdi_names[0]); i++) {
        void *p = w32_sym(m, w32_gdi_names[i]);
        if (!p) {
            FreeLibrary(m);
            memset(f, 0, sizeof(*f));
            return 0;
        }
        memcpy(slot + i * sizeof(p), &p, sizeof(p));
    }
    *dll = m;
    return 1;
}

static void w32_dpi_aware(HMODULE shcore, const w32_gdi_fns *gdi)
{
    HMODULE u = GetModuleHandleA("user32.dll");
    BOOL(WINAPI * ctx)(HANDLE) = NULL;
    HANDLE(WINAPI * thread_ctx)(void) = NULL;
    int(WINAPI * ctx_awareness)(HANDLE) = NULL;
    UINT(WINAPI * sys_dpi)(void) = NULL;
    HRESULT(WINAPI * shc_set)(int) = NULL;
    HRESULT(WINAPI * shc_get)(HANDLE, int *) = NULL;
    W32_LOAD(ctx, u, "SetProcessDpiAwarenessContext");
    W32_LOAD(thread_ctx, u, "GetThreadDpiAwarenessContext");
    W32_LOAD(ctx_awareness, u, "GetAwarenessFromDpiAwarenessContext");
    W32_LOAD(sys_dpi, u, "GetDpiForSystem");
    W32_LOAD(shc_set, shcore, "SetProcessDpiAwareness");
    W32_LOAD(shc_get, shcore, "GetProcessDpiAwareness");
    // -4 per monitor v2, -3 v1
    if (!(ctx && (ctx((HANDLE)(INT_PTR)-4) || ctx((HANDLE)(INT_PTR)-3))) && !(shc_set && shc_set(2) == S_OK)) SetProcessDPIAware();
    int a = -1;
    if (thread_ctx && ctx_awareness) a = ctx_awareness(thread_ctx());
    if (a < 0 && shc_get && shc_get(NULL, &a) != S_OK) a = -1;
    if (a < 0) a = IsProcessDPIAware() ? 1 : 0;
    w32d.aware = a;
    UINT d = sys_dpi ? sys_dpi() : 0;
    if (!d && gdi->caps) {
        HDC dc = GetDC(NULL);
        d = (UINT)gdi->caps(dc, LOGPIXELSX);
        ReleaseDC(NULL, dc);
    }
    w32d.sysdpi = d >= 48 ? (int)d : 96;
}

static int w32_find(POINT p)
{
    for (int i = 0; i < w32.n; i++)
        if (p.x >= w32.m[i].r.left && p.x < w32.m[i].r.right && p.y >= w32.m[i].r.top && p.y < w32.m[i].r.bottom) return i;
    return -1;
}

static int w32_logical(POINT p, double *x, double *y)
{
    int i = w32_find(p);
    if (i < 0) {
        *x = p.x, *y = p.y;
        return -1;
    }
    w32_mon *m = &w32.m[i];
    *x = m->r.left + (p.x - m->r.left) / m->s, *y = m->r.top + (p.y - m->r.top) / m->s;
    return i;
}

static int w32_on_hit(const w32_mon *m, double x, double y)
{
    for (int i = 0; i < m->nhits; i++) {
        const goome_rect *h = &m->hits[i];
        if (x >= h->x && x < h->x + h->w && y >= h->y && y < h->y + h->h) return 1;
    }
    return 0;
}

static w32_mon *w32_mon_of(HWND w)
{
    for (int i = 0; w && i < w32.n; i++)
        if (w32.m[i].win == w) return &w32.m[i];
    return NULL;
}

typedef struct {
    w32_mon *out;
    int n;
} w32_enum;

static BOOL CALLBACK w32_on_mon(HMONITOR hm, HDC dc, LPRECT r, LPARAM p)
{
    w32_enum *l = (w32_enum *)p;
    if (l->n >= GM_MAX_MON) return FALSE;
    w32_mon *m = &l->out[l->n++];
    memset(m, 0, sizeof(*m));
    m->hm = hm, m->r = *r, m->id = l->n;
    struct {
        MONITORINFO mi;
        WCHAR dev[CCHDEVICENAME];
    } mi;
    memset(&mi, 0, sizeof(mi));
    mi.mi.cbSize = sizeof(mi);
    snprintf(m->name, sizeof(m->name), "screen %d", l->n);
    if (GetMonitorInfoW(hm, &mi.mi)) {
        m->primary = (mi.mi.dwFlags & MONITORINFOF_PRIMARY) != 0;
        const WCHAR *d = mi.dev;
        if (d[0] == '\\' && d[1] == '\\' && d[2] == '.' && d[3] == '\\') d += 4; // \\.\DISPLAY1
        int k = 0, num = 0;
        for (; d[k] && k < 63; k++) {
            m->name[k] = d[k] < 128 ? (char)d[k] : '?';
            if (d[k] >= '0' && d[k] <= '9') num = num * 10 + (d[k] - '0');
        }
        if (k) m->name[k] = 0;
        if (num > 0 && num < 100000) m->id = num;
        DEVMODEW dm;
        memset(&dm, 0, sizeof(dm));
        dm.dmSize = sizeof(dm);
        if (EnumDisplaySettingsExW(mi.dev, ENUM_CURRENT_SETTINGS, &dm, 0) && (dm.dmFields & DM_DISPLAYORIENTATION))
            m->rot = (int)dm.dmDisplayOrientation * 90;
    }
    UINT dx = 0, dy = 0;
    int dpi = w32d.aware == 0 ? 96 : w32d.sysdpi;
    if (w32d.aware == 2 && w32d.dpi_for_mon && w32d.dpi_for_mon(hm, 0, &dx, &dy) == S_OK && dx >= 48) dpi = (int)dx; // 0 = effective dpi
    m->s = dpi / 96.0;
    return TRUE;
}

static int w32_list(w32_mon *out)
{
    w32_enum l = {out, 0};
    EnumDisplayMonitors(NULL, NULL, w32_on_mon, (LPARAM)&l);
    for (int i = 1; i < l.n; i++)
        if (out[i].primary) {
            w32_mon t = out[i];
            memmove(out + 1, out, sizeof(*out) * (size_t)i);
            out[0] = t;
            break;
        }
    return l.n;
}

static HBITMAP w32_dib(const w32_gdi_fns *f, int w, int h, uint32_t **bits)
{
    BITMAPINFO bi;
    memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h; // top down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void *p = NULL;
    HDC screen = GetDC(NULL);
    HBITMAP b = f->dib(screen, &bi, DIB_RGB_COLORS, &p, NULL, 0);
    ReleaseDC(NULL, screen);
    *bits = b ? (uint32_t *)p : NULL;
    return b;
}

static void w32_drop_cap(w32_mon *m)
{
    if (m->cdc) w32.gdi.del_dc(m->cdc);
    if (m->cbmp) w32.gdi.del_obj(m->cbmp);
    m->cdc = NULL, m->cbmp = NULL, m->cpx = NULL;
}

static void w32_drop_win(w32_mon *m)
{
    if (m->win) DestroyWindow(m->win);
    if (m->dc) w32.gdi.del_dc(m->dc);
    if (m->bmp) w32.gdi.del_obj(m->bmp);
    if (w32.hot == m->win) w32.hot = NULL;
    free(m->hits);
    m->win = NULL, m->dc = NULL, m->bmp = NULL, m->px = NULL, m->shown = 0, m->hits = NULL, m->nhits = m->hcap = 0;
}

static void w32_clicky(HWND w, int on)
{
    LONG_PTR ex = GetWindowLongPtrW(w, GWL_EXSTYLE);
    LONG_PTR want = on ? ex & ~(LONG_PTR)WS_EX_TRANSPARENT : ex | WS_EX_TRANSPARENT;
    if (want != ex) SetWindowLongPtrW(w, GWL_EXSTYLE, want);
}

static int w32_mods(void)
{
    return (GetKeyState(VK_SHIFT) < 0 ? GOOME_SHIFT : 0) | (GetKeyState(VK_CONTROL) < 0 ? GOOME_CTRL : 0) |
           (GetKeyState(VK_MENU) < 0 ? GOOME_ALT : 0) | (GetKeyState(VK_LWIN) < 0 || GetKeyState(VK_RWIN) < 0 ? GOOME_SUPER : 0);
}

static void w32_ev(int type, int mon, double x, double y, int button)
{
    goome_event e;
    memset(&e, 0, sizeof(e));
    e.type = type, e.mon = mon, e.x = x, e.y = y, e.button = button, e.mods = w32_mods();
    gm_push(w32.g, &e);
}

static int w32_utf8(unsigned c, char *o)
{
    if (c < 0x80) return o[0] = (char)c, o[1] = 0, 1;
    if (c < 0x800) return o[0] = (char)(0xC0 | c >> 6), o[1] = (char)(0x80 | (c & 63)), o[2] = 0, 2;
    if (c < 0x10000) return o[0] = (char)(0xE0 | c >> 12), o[1] = (char)(0x80 | ((c >> 6) & 63)), o[2] = (char)(0x80 | (c & 63)), o[3] = 0, 3;
    o[0] = (char)(0xF0 | c >> 18), o[1] = (char)(0x80 | ((c >> 12) & 63)), o[2] = (char)(0x80 | ((c >> 6) & 63)), o[3] = (char)(0x80 | (c & 63)), o[4] = 0;
    return 4;
}

static void w32_char(goome *g, unsigned c)
{
    goome_event e;
    memset(&e, 0, sizeof(e));
    e.type = GOOME_KEY;
    int mods = w32_mods();
    if ((mods & GOOME_CTRL) && (mods & GOOME_ALT) && c >= 32) mods &= ~(GOOME_CTRL | GOOME_ALT); // altgr is ctrl+alt to windows
    if (c == 127) c = GOOME_K_BACKSPACE; // ctrl+backspace
    if (c < 128) {
        int k = gm_combo_key((int)c, mods);
        e.key = k & 0xFF, e.mods = k & ~0xFF;
    } else
        e.mods = mods;
    if (c >= 32 && !(e.mods & (GOOME_CTRL | GOOME_ALT | GOOME_SUPER))) w32_utf8(c, e.text);
    gm_push(g, &e);
}

static int w32_pair(unsigned *hi, unsigned *c)
{
    if (*c >= 0xD800 && *c < 0xDC00) {
        *hi = *c;
        return 0;
    }
    if (*c >= 0xDC00 && *c < 0xE000) {
        if (!*hi) return 0;
        *c = 0x10000 + ((*hi - 0xD800) << 10) + (*c - 0xDC00);
    }
    *hi = 0;
    return 1;
}

static int w32_navkey(WPARAM vk)
{
    if (vk >= VK_F1 && vk <= VK_F12) return GOOME_K_F1 + (int)(vk - VK_F1);
    switch (vk) {
    case VK_LEFT: return GOOME_K_LEFT;
    case VK_RIGHT: return GOOME_K_RIGHT;
    case VK_UP: return GOOME_K_UP;
    case VK_DOWN: return GOOME_K_DOWN;
    case VK_HOME: return GOOME_K_HOME;
    case VK_END: return GOOME_K_END;
    case VK_PRIOR: return GOOME_K_PGUP;
    case VK_NEXT: return GOOME_K_PGDN;
    case VK_INSERT: return GOOME_K_INSERT;
    case VK_DELETE: return GOOME_K_DELETE;
    }
    return 0;
}

static void w32_send_button(int button, int down, int up)
{
    int sw = GetSystemMetrics(SM_SWAPBUTTON); // left handed mice swap them, sendinput wants physical
    DWORD d = MOUSEEVENTF_MIDDLEDOWN, u = MOUSEEVENTF_MIDDLEUP;
    if (button != 2) {
        int left = (button == 1) != (sw != 0);
        d = left ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_RIGHTDOWN, u = left ? MOUSEEVENTF_LEFTUP : MOUSEEVENTF_RIGHTUP;
    }
    INPUT in[2];
    memset(in, 0, sizeof(in));
    in[0].type = in[1].type = INPUT_MOUSE;
    int n = 0;
    if (down) in[n++].mi.dwFlags = d;
    if (up) in[n++].mi.dwFlags = u;
    if (n) SendInput((UINT)n, in, sizeof(INPUT));
}

static int w32_held_vk(int button)
{
    int sw = GetSystemMetrics(SM_SWAPBUTTON);
    if (button == 2) return VK_MBUTTON;
    return (button == 1) != (sw != 0) ? VK_LBUTTON : VK_RBUTTON;
}

static void w32_down(HWND w, w32_mon *m, int button, LPARAM lp)
{
    int i = (int)(m - w32.m), bit = 1 << (button - 1);
    double x = m->r.left + (short)LOWORD(lp) / m->s, y = m->r.top + (short)HIWORD(lp) / m->s;
    if (!w32_on_hit(m, x, y)) {
        w32_clicky(w, 0);
        w32.hot = NULL;
        if (GetAsyncKeyState(w32_held_vk(button)) & 0x8000) {
            w32_send_button(button, 1, 0);
            w32.fwd |= bit;
        } else
            w32_send_button(button, 1, 1);
        return;
    }
    w32.ours |= bit;
    SetCapture(w);
    w32_ev(GOOME_DOWN, i, x, y, button);
}

static void w32_up(w32_mon *m, int button, LPARAM lp)
{
    int bit = 1 << (button - 1);
    if (w32.fwd & bit) {
        w32.fwd &= ~bit;
        w32_send_button(button, 0, 1);
        return;
    }
    if (!(w32.ours & bit)) return;
    w32.ours &= ~bit;
    if (!w32.ours && !w32.grab) ReleaseCapture();
    w32_ev(GOOME_UP, (int)(m - w32.m), m->r.left + (short)LOWORD(lp) / m->s, m->r.top + (short)HIWORD(lp) / m->s, button);
}

static HCURSOR w32_cursor(int shape);

static LRESULT CALLBACK w32_proc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    w32_mon *m = w32.g ? w32_mon_of(w) : NULL;
    if (!m) return DefWindowProcW(w, msg, wp, lp);
    switch (msg) {
    case WM_LBUTTONDOWN: w32_down(w, m, 1, lp); return 0;
    case WM_MBUTTONDOWN: w32_down(w, m, 2, lp); return 0;
    case WM_RBUTTONDOWN: w32_down(w, m, 3, lp); return 0;
    case WM_LBUTTONUP: w32_up(m, 1, lp); return 0;
    case WM_MBUTTONUP: w32_up(m, 2, lp); return 0;
    case WM_RBUTTONUP: w32_up(m, 3, lp); return 0;
    case WM_MOUSEMOVE: {
        double x = m->r.left + (short)LOWORD(lp) / m->s, y = m->r.top + (short)HIWORD(lp) / m->s;
        if ((x != w32.lx || y != w32.ly) && (w32.grab || w32_on_hit(m, x, y))) w32.lx = x, w32.ly = y, w32_ev(GOOME_MOTION, (int)(m - w32.m), x, y, 0);
        return 0;
    }
    case WM_MOUSEWHEEL: {
        POINT p = {(short)LOWORD(lp), (short)HIWORD(lp)};
        double x, y;
        int i = w32_logical(p, &x, &y);
        w32.wheel += (short)HIWORD(wp);
        int notches = w32.wheel / WHEEL_DELTA;
        w32.wheel -= notches * WHEEL_DELTA;
        if (notches && i >= 0 && w32_on_hit(&w32.m[i], x, y)) {
            goome_event e;
            memset(&e, 0, sizeof(e));
            e.type = GOOME_SCROLL, e.mon = i, e.x = x, e.y = y, e.dy = -notches, e.mods = w32_mods();
            gm_push(w32.g, &e);
        }
        return 0;
    }
    case WM_CHAR:
    case WM_SYSCHAR: {
        unsigned c = (unsigned)wp;
        if (w32_pair(&w32.hi, &c) && w32.g->kb_on) w32_char(w32.g, c);
        return 0;
    }
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN: {
        int k = w32_navkey(wp);
        if (!k) break;
        if (w32.g->kb_on) {
            goome_event e;
            memset(&e, 0, sizeof(e));
            e.type = GOOME_KEY, e.key = k, e.mods = w32_mods();
            gm_push(w32.g, &e);
        }
        return 0;
    }
    case WM_MOUSEACTIVATE: return w32.g->kb_on || w32.grab ? MA_ACTIVATE : MA_NOACTIVATE;
    case WM_SETCURSOR:
        if (LOWORD(lp) == HTCLIENT) {
            SetCursor(w32_cursor(w32.g->cursor));
            return TRUE;
        }
        break;
    case WM_DISPLAYCHANGE:
    case 0x02E0: // WM_DPICHANGED, older headers dont have it
    case WM_SETTINGCHANGE: w32.recheck = 1; break;
    case WM_CLOSE: return 0;
    }
    return DefWindowProcW(w, msg, wp, lp);
}

static void w32_hide_self(w32_mon *m)
{
    if (!w32.g->opts.hide_self || !w32.set_affinity || w32.build < 19041) { // 0x11 is 2004+, before that it blacks us out
        w32.hidden_ok = 0;
        return;
    }
    DWORD got = 0;
    if (!w32.set_affinity(m->win, 0x11) || (w32.get_affinity && (!w32.get_affinity(m->win, &got) || got != 0x11))) w32.hidden_ok = 0;
}

static void w32_add_win(goome *g, int i)
{
    w32_mon *m = &w32.m[i];
    int w = m->r.right - m->r.left, h = m->r.bottom - m->r.top;
    WCHAR title[64];
    if (!MultiByteToWideChar(CP_UTF8, 0, g->title, -1, title, 64)) title[0] = 0;
    title[63] = 0;
    m->win = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, (LPCWSTR)(UINT_PTR)w32.cls,
                             title, WS_POPUP, m->r.left, m->r.top, w, h, NULL, NULL, GetModuleHandleW(NULL), NULL);
    m->dc = w32.gdi.create_dc(NULL);
    m->bmp = m->dc ? w32_dib(&w32.gdi, w, h, &m->px) : NULL;
    if (!m->win || !m->bmp) {
        w32_drop_win(m);
        return;
    }
    w32.gdi.select(m->dc, m->bmp);
    memset(m->px, 0, (size_t)w * h * 4);
    gm_canvas_reset(&m->cv, m->px, w, h, w, m->r.left, m->r.top, m->s, i);
    if (g->kb_on || w32.grab) {
        LONG_PTR ex = GetWindowLongPtrW(m->win, GWL_EXSTYLE);
        SetWindowLongPtrW(m->win, GWL_EXSTYLE, ex & ~(LONG_PTR)WS_EX_NOACTIVATE);
    }
    w32_hide_self(m);
}

static void w32_publish(goome *g)
{
    goome_monitor out[GM_MAX_MON];
    memset(out, 0, sizeof(out));
    for (int i = 0; i < w32.n; i++) {
        w32_mon *m = &w32.m[i];
        goome_monitor *o = &out[i];
        o->id = m->id;
        snprintf(o->name, sizeof(o->name), "%s", m->name);
        o->pw = m->r.right - m->r.left, o->ph = m->r.bottom - m->r.top;
        o->area = (goome_rect){m->r.left, m->r.top, (int)(o->pw / m->s + 0.5), (int)(o->ph / m->s + 0.5)};
        o->scale = m->s, o->rotation = m->rot;
    }
    gm_set_monitors(g, out, w32.n);
    for (int i = 0; i < GM_MAX_MON; i++) g->cv[i] = NULL;
    w32_cap.caps = GOOME_CAP_CAPTURE | (w32.hidden_ok ? GOOME_CAP_HIDE_SELF : 0);
}

static void w32_make(goome *g, const w32_mon *now, int n)
{
    for (int i = 0; i < w32.n; i++) w32_drop_cap(&w32.m[i]), w32_drop_win(&w32.m[i]);
    memset(w32.m, 0, sizeof(w32.m));
    memcpy(w32.m, now, sizeof(*now) * (size_t)n);
    w32.n = n, w32.hot = NULL, w32.ours = w32.fwd = 0;
    w32.hidden_ok = 1;
    for (int i = 0; i < n; i++) w32_add_win(g, i);
    if (!n) w32.hidden_ok = 0;
    w32_publish(g);
}

static void w32_refresh(goome *g)
{
    w32_mon now[GM_MAX_MON];
    int n = w32_list(now), same = n == w32.n;
    for (int i = 0; same && i < n; i++)
        same = !memcmp(&now[i].r, &w32.m[i].r, sizeof(RECT)) && now[i].s == w32.m[i].s && now[i].rot == w32.m[i].rot && now[i].hm == w32.m[i].hm;
    if (!same && n > 0) w32_make(g, now, n);
}

static void w32_close(goome *g);
static int w32w_open(goome *g);

static int w32_open(goome *g)
{
    if (g->opts.mode == GOOME_MODE_WINDOW) return w32w_open(g);
    if (g->opts.mode != GOOME_MODE_OVERLAY) return gm_fail(g, GOOME_ENOTSUP, "win32 does overlay and window mode");
    memset(&w32, 0, sizeof(w32));
    w32.g = g;
    if (!w32_load_gdi(&w32.gdi, &w32.gdi_dll)) return gm_fail(g, GOOME_ENODISPLAY, "cant load gdi32.dll");
    HMODULE u = GetModuleHandleA("user32.dll");
    w32.shcore = LoadLibraryA("shcore.dll");
    w32.ntdll = GetModuleHandleA("ntdll.dll");
    W32_LOAD(w32.ulw_indirect, u, "UpdateLayeredWindowIndirect");
    W32_LOAD(w32.set_affinity, u, "SetWindowDisplayAffinity");
    W32_LOAD(w32.get_affinity, u, "GetWindowDisplayAffinity");
    W32_LOAD(w32d.dpi_for_mon, w32.shcore, "GetDpiForMonitor");
    LONG(WINAPI * ver)(OSVERSIONINFOW *) = NULL;
    W32_LOAD(ver, w32.ntdll, "RtlGetVersion"); // GetVersionEx lies without a manifest
    OSVERSIONINFOW vi;
    memset(&vi, 0, sizeof(vi));
    vi.dwOSVersionInfoSize = sizeof(vi);
    if (ver && ver(&vi) == 0) w32.build = (int)vi.dwBuildNumber;
    void(WINAPI * noghost)(void) = NULL;
    W32_LOAD(noghost, u, "DisableProcessWindowsGhosting");
    if (noghost) noghost(); // or windows swaps in a frozen ghost copy of us when a frame is slow
    w32_dpi_aware(w32.shcore, &w32.gdi);

    WNDCLASSEXW wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = w32_proc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = L"goome_overlay";
    w32.cls = RegisterClassExW(&wc);
    if (!w32.cls) {
        int e = gm_fail(g, GOOME_ENODISPLAY, "cant register the overlay window class (error %lu)", GetLastError());
        w32_close(g);
        return e;
    }
    w32_mon now[GM_MAX_MON];
    int n = w32_list(now);
    if (n < 1) {
        w32_close(g);
        return gm_fail(g, GOOME_ENODISPLAY, "windows says there are no monitors");
    }
    w32_make(g, now, n);
    for (int i = 0; i < w32.n; i++)
        if (!w32.m[i].win) {
            int e = gm_fail(g, GOOME_ENODISPLAY, "cant make the overlay window on %s (error %lu)", w32.m[i].name, GetLastError());
            w32_close(g);
            return e;
        }
    w32.next_poll = goome_now_ms() + 2000;
    return GOOME_OK;
}

static goome_canvas *w32_canvas(goome *g, int mon) { return mon >= 0 && mon < w32.n && w32.m[mon].px ? &w32.m[mon].cv : NULL; }

static void w32_track(goome *g)
{
    POINT p;
    if (!GetCursorPos(&p)) return;
    if ((w32.ours || w32.grab) && w32_mon_of(GetCapture())) return;
    double x, y;
    int i = w32_logical(p, &x, &y);
    HWND want = NULL;
    if (i >= 0 && w32.m[i].win && w32.m[i].shown && w32_on_hit(&w32.m[i], x, y)) want = w32.m[i].win;
    if (want == w32.hot) return;
    if (w32.hot) w32_clicky(w32.hot, 0);
    if (want) w32_clicky(want, 1);
    w32.hot = want;
}

static int w32_px_rect(const w32_mon *m, const goome_rect *h, int *x0, int *y0, int *x1, int *y1)
{
    int w = m->r.right - m->r.left, hh = m->r.bottom - m->r.top;
    double a = (h->x - m->r.left) * m->s, b = (h->y - m->r.top) * m->s, c = (h->x + h->w - m->r.left) * m->s, d = (h->y + h->h - m->r.top) * m->s;
    *x0 = a < 0 ? 0 : (int)a, *y0 = b < 0 ? 0 : (int)b;
    *x1 = c > w ? w : (int)c + (c > (int)c), *y1 = d > hh ? hh : (int)d + (d > (int)d);
    return *x1 > *x0 && *y1 > *y0;
}

static void w32_grow(int *x0, int *y0, int *x1, int *y1, int *has, int a, int b, int c, int d)
{
    if (!*has) {
        *x0 = a, *y0 = b, *x1 = c, *y1 = d, *has = 1;
        return;
    }
    if (a < *x0) *x0 = a;
    if (b < *y0) *y0 = b;
    if (c > *x1) *x1 = c;
    if (d > *y1) *y1 = d;
}

// layered windows hit test by alpha, so empty px inside hits go up as 1 and get set back to 0 after
static void w32_solid_hits(w32_mon *m, uint32_t from, uint32_t to)
{
    int w = m->r.right - m->r.left;
    for (int i = 0; i < m->nhits; i++) {
        int x0, y0, x1, y1;
        if (!w32_px_rect(m, &m->hits[i], &x0, &y0, &x1, &y1)) continue;
        for (int y = y0; y < y1; y++) {
            uint32_t *row = m->px + (size_t)y * w;
            for (int x = x0; x < x1; x++)
                if (row[x] == from) row[x] = to;
        }
    }
}

static int w32_commit(goome *g, int mon, goome_canvas *c, const goome_rect *hits, int n)
{
    if (mon < 0 || mon >= w32.n || c != &w32.m[mon].cv) return GOOME_EGONE;
    w32_mon *m = &w32.m[mon];
    if (!m->win) return GOOME_EGONE;
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0, dmg = gm_canvas_damage(c, &x0, &y0, &x1, &y1);
    int changed = n != m->nhits || (n && memcmp(m->hits, hits, sizeof(*hits) * (size_t)n));
    for (int i = 0; changed && i < m->nhits; i++) {
        int a, b, cc, d;
        if (w32_px_rect(m, &m->hits[i], &a, &b, &cc, &d)) w32_grow(&x0, &y0, &x1, &y1, &dmg, a, b, cc, d);
    }
    if (n > m->hcap) {
        goome_rect *nh = realloc(m->hits, sizeof(*nh) * (size_t)n);
        if (!nh) return gm_fail(g, GOOME_ENOMEM, "no memory for %d hit rects", n);
        m->hits = nh, m->hcap = n;
    }
    if (n) memcpy(m->hits, hits, sizeof(*hits) * (size_t)n);
    m->nhits = n;
    for (int i = 0; changed && i < n; i++) {
        int a, b, cc, d;
        if (w32_px_rect(m, &hits[i], &a, &b, &cc, &d)) w32_grow(&x0, &y0, &x1, &y1, &dmg, a, b, cc, d);
    }
    int empty = !(c->dx1 > c->dx0) && !n;
    if (empty && !m->shown) return GOOME_OK;
    long now = goome_now_ms();
    if (dmg || !m->shown) {
        w32_solid_hits(m, 0, 0x01000000u);
        w32.gdi.flush();
        POINT pos = {m->r.left, m->r.top}, zero = {0, 0};
        SIZE sz = {m->r.right - m->r.left, m->r.bottom - m->r.top};
        BLENDFUNCTION bf = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
        RECT dirty = {x0, y0, x1, y1};
        w32_ulw_info info = {sizeof(info), NULL, &pos, &sz, m->dc, &zero, 0, &bf, ULW_ALPHA, m->shown && dmg ? &dirty : NULL};
        if (!w32.ulw_indirect || !w32.ulw_indirect(m->win, &info)) UpdateLayeredWindow(m->win, NULL, &pos, &sz, m->dc, &zero, 0, &bf, ULW_ALPHA);
        w32_solid_hits(m, 0x01000000u, 0);
    }
    if (empty) {
        ShowWindow(m->win, SW_HIDE), m->shown = 0;
        w32_track(g);
        return GOOME_OK;
    }
    if (!m->shown) ShowWindow(m->win, SW_SHOWNOACTIVATE), m->shown = 1;
    if (dmg || now - m->topped > 500) { // browsers going fullscreen push us under
        SetWindowPos(m->win, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
        m->topped = now;
    }
    w32_track(g);
    return GOOME_OK;
}

static int w32_wait(goome *g, int ms)
{
    long end = goome_now_ms() + (ms > 0 ? ms : 0);
    for (;;) {
        MSG msg;
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) return -1;
            if (msg.message == WM_HOTKEY && !msg.hwnd) {
                int k = (int)msg.wParam - 1;
                if (k >= 0 && k < w32.nreg) {
                    goome_event e;
                    memset(&e, 0, sizeof(e));
                    e.type = GOOME_HOTKEY, e.id = w32.hk[k].id, e.key = w32.hk[k].key, e.mods = w32.hk[k].mods;
                    gm_push(g, &e);
                }
                continue;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        long now = goome_now_ms();
        if (w32.recheck || now >= w32.next_poll) {
            w32.recheck = 0, w32.next_poll = now + 2000;
            w32_refresh(g);
        }
        w32_track(g);
        if (g->qlen) return 0;
        long left = end - goome_now_ms();
        if (left <= 0) return 0;
        MsgWaitForMultipleObjectsEx(0, NULL, left < 15 ? (DWORD)left : 15, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
    }
}

static int w32_pointer(goome *g, double *x, double *y, int *buttons)
{
    POINT p;
    if (!GetCursorPos(&p)) return 0;
    w32_logical(p, x, y);
    int b = 0;
    for (int k = 1; k <= 3; k++)
        if (GetAsyncKeyState(w32_held_vk(k)) & 0x8000) b |= 1 << (k - 1);
    *buttons = b;
    return 1;
}

static int w32_hold(goome *g, int on, HWND t)
{
    HWND fg = GetForegroundWindow();
    if (on && !w32_mon_of(fg)) w32.before_kb = fg;
    for (int i = 0; i < w32.n; i++) {
        if (!w32.m[i].win) continue;
        LONG_PTR ex = GetWindowLongPtrW(w32.m[i].win, GWL_EXSTYLE);
        SetWindowLongPtrW(w32.m[i].win, GWL_EXSTYLE, on ? ex & ~(LONG_PTR)WS_EX_NOACTIVATE : ex | WS_EX_NOACTIVATE);
    }
    w32.hi = 0;
    if (on) {
        if (!t) t = w32.hot;
        for (int i = 0; !t && i < w32.n; i++)
            if (w32.m[i].win && w32.m[i].shown) t = w32.m[i].win;
        if (!t) t = w32.m[0].win;
        if (t && !SetForegroundWindow(t)) return gm_fail(g, GOOME_EDENIED, "windows wont hand us the keyboard right now (focus lock), click the overlay first");
        if (t) SetFocus(t);
        return GOOME_OK;
    }
    if (w32.before_kb && IsWindow(w32.before_kb) && w32_mon_of(GetForegroundWindow())) SetForegroundWindow(w32.before_kb);
    w32.before_kb = NULL;
    return GOOME_OK;
}

static int w32_keyboard(goome *g, int on)
{
    if (w32.grab) return GOOME_OK;
    return w32_hold(g, on, NULL);
}

static int w32_ov_grab(goome *g, int on)
{
    if (on) {
        HWND t = GetCapture();
        if (!w32_mon_of(t)) t = w32.hot;
        int e = g->kb_on || w32.grab ? GOOME_OK : w32_hold(g, 1, t);
        if (!t || !w32_mon_of(t)) t = GetForegroundWindow();
        w32.grab = 1;
        if (w32_mon_of(t)) {
            w32_clicky(t, 1);
            w32.hot = t;
            SetCapture(t);
        } else if (e == GOOME_OK)
            e = gm_fail(g, GOOME_EDENIED, "no overlay window to hold the pointer");
        return e;
    }
    if (!w32.grab) return GOOME_OK;
    w32.grab = 0;
    if (!w32.ours && w32_mon_of(GetCapture())) ReleaseCapture();
    return g->kb_on ? GOOME_OK : w32_hold(g, 0, NULL);
}

static UINT w32_vk(int key)
{
    if (key >= GOOME_K_F1 && key <= GOOME_K_F12) return VK_F1 + (UINT)(key - GOOME_K_F1);
    switch (key) {
    case GOOME_K_LEFT: return VK_LEFT;
    case GOOME_K_RIGHT: return VK_RIGHT;
    case GOOME_K_UP: return VK_UP;
    case GOOME_K_DOWN: return VK_DOWN;
    case GOOME_K_HOME: return VK_HOME;
    case GOOME_K_END: return VK_END;
    case GOOME_K_PGUP: return VK_PRIOR;
    case GOOME_K_PGDN: return VK_NEXT;
    case GOOME_K_INSERT: return VK_INSERT;
    case GOOME_K_DELETE: return VK_DELETE;
    case GOOME_K_BACKSPACE: return VK_BACK;
    case GOOME_K_TAB: return VK_TAB;
    case GOOME_K_ENTER: return VK_RETURN;
    case GOOME_K_ESC: return VK_ESCAPE;
    case GOOME_K_SPACE: return VK_SPACE;
    }
    if (key < 33 || key > 126) return 0;
    SHORT v = VkKeyScanW((WCHAR)key);
    return (v & 0xFF) == 0xFF ? 0 : (UINT)(v & 0xFF); // 0xff = not on this keyboard
}

static int w32_hotkeys(goome *g)
{
    for (int i = 0; i < w32.nreg; i++) UnregisterHotKey(NULL, i + 1);
    w32.nreg = g->nhk;
    memcpy(w32.hk, g->hk, sizeof(gm_hotkey) * (size_t)g->nhk);
    int bad = -1;
    for (int i = 0; i < g->nhk; i++) {
        int k = g->hk[i].key, md = g->hk[i].mods;
        UINT vk = w32_vk(k);
        UINT mods = 0x4000 | (md & GOOME_SHIFT ? MOD_SHIFT : 0) | (md & GOOME_CTRL ? MOD_CONTROL : 0) | (md & GOOME_ALT ? MOD_ALT : 0) |
                    (md & GOOME_SUPER ? MOD_WIN : 0); // 0x4000 = MOD_NOREPEAT
        if (!vk || !RegisterHotKey(NULL, i + 1, mods, vk)) bad = i;
    }
    if (bad >= 0) {
        int vk = (int)w32_vk(g->hk[bad].key);
        return gm_fail(g, GOOME_EDENIED, vk ? "another app already has the hotkey with id %d" : "the key for hotkey id %d isnt on this keyboard", g->hk[bad].id);
    }
    return GOOME_OK;
}

typedef struct {
    char *path;
    double x, y;
} w32_drop_item;

typedef struct {
    goome *g;
    HWND win;
    HDC dc;
    HBITMAP bmp;
    uint32_t *px;
    int pw, ph, lw, lh, ready;
    double s;
    goome_canvas cv;
    DWORD style, ex;
    int popup, trans, fs, ours, wheel;
    unsigned hi;
    double lx, ly;
    WINDOWPLACEMENT keep;
    goome_rect *hits;
    int nhits, hcap;
    HDC cdc;
    HBITMAP cbmp;
    uint32_t *cpx;
    int cw, ch;
    w32_drop_item *drops;
    int ndrop;
    gm_hotkey hk[GM_MAX_HK];
    int nreg, grab;
} w32_win;

static struct {
    int users;
    HMODULE gdi_dll, shcore, shell;
    w32_gdi_fns gdi;
    ATOM cls;
    UINT(WINAPI *dpi_for_win)(HWND);
    BOOL(WINAPI *adjust)(RECT *, DWORD, BOOL, DWORD, UINT);
    BOOL(WINAPI *ulw_indirect)(HWND, const w32_ulw_info *);
    UINT(WINAPI *drag_query)(HANDLE, UINT, WCHAR *, UINT);
    BOOL(WINAPI *drag_point)(HANDLE, POINT *);
    void(WINAPI *drag_finish)(HANDLE);
} w32x;

static HCURSOR w32_cursor(int shape)
{
    switch (shape) {
    case GOOME_CURSOR_IBEAM: return LoadCursor(NULL, IDC_IBEAM);
    case GOOME_CURSOR_HAND: return LoadCursor(NULL, IDC_HAND);
    case GOOME_CURSOR_MOVE: return LoadCursor(NULL, IDC_SIZEALL);
    case GOOME_CURSOR_RESIZE_EW: return LoadCursor(NULL, IDC_SIZEWE);
    case GOOME_CURSOR_RESIZE_NS: return LoadCursor(NULL, IDC_SIZENS);
    case GOOME_CURSOR_RESIZE_NWSE: return LoadCursor(NULL, IDC_SIZENWSE);
    case GOOME_CURSOR_RESIZE_NESW: return LoadCursor(NULL, IDC_SIZENESW);
    case GOOME_CURSOR_WAIT: return LoadCursor(NULL, IDC_WAIT);
    case GOOME_CURSOR_NO: return LoadCursor(NULL, IDC_NO);
    case GOOME_CURSOR_HIDDEN: return NULL;
    }
    return LoadCursor(NULL, IDC_ARROW);
}

static void w32_cursor_now(HWND h, int shape)
{
    POINT p;
    RECT r;
    if (!h || !GetCursorPos(&p) || WindowFromPoint(p) != h) return;
    ScreenToClient(h, &p);
    GetClientRect(h, &r);
    if (p.x >= 0 && p.y >= 0 && p.x < r.right && p.y < r.bottom) SetCursor(w32_cursor(shape));
}

static int w32_clip_open(HWND h)
{
    for (int i = 0; i < 25; i++) {
        if (OpenClipboard(h)) return 1;
        Sleep(4);
    }
    return 0;
}

static int w32_clip_put(goome *g, HWND h, const char *s)
{
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    if (n <= 0) return gm_fail(g, GOOME_EARG, "cant turn that text into utf-16");
    WCHAR *tmp = malloc(sizeof(WCHAR) * (size_t)n);
    if (!tmp) return gm_fail(g, GOOME_ENOMEM, "no memory for the clipboard");
    MultiByteToWideChar(CP_UTF8, 0, s, -1, tmp, n);
    int extra = 0;
    for (int i = 0; i < n; i++)
        if (tmp[i] == '\n' && (i == 0 || tmp[i - 1] != '\r')) extra++;
    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, sizeof(WCHAR) * (size_t)(n + extra));
    WCHAR *d = mem ? GlobalLock(mem) : NULL;
    if (!d) {
        free(tmp);
        if (mem) GlobalFree(mem);
        return gm_fail(g, GOOME_ENOMEM, "no memory for the clipboard");
    }
    for (int i = 0, k = 0; i < n; i++) {
        if (tmp[i] == '\n' && (i == 0 || tmp[i - 1] != '\r')) d[k++] = '\r';
        d[k++] = tmp[i];
    }
    GlobalUnlock(mem);
    free(tmp);
    if (!w32_clip_open(h)) {
        GlobalFree(mem);
        return gm_fail(g, GOOME_EAGAIN, "another app has the clipboard open");
    }
    EmptyClipboard();
    if (!SetClipboardData(CF_UNICODETEXT, mem)) {
        CloseClipboard();
        GlobalFree(mem);
        return gm_fail(g, GOOME_EDENIED, "windows wont take the clipboard text (error %lu)", GetLastError());
    }
    CloseClipboard();
    return GOOME_OK;
}

static int w32_clip_take(goome *g, HWND h, char **out)
{
    *out = NULL;
    if (!w32_clip_open(h)) return gm_fail(g, GOOME_EAGAIN, "another app has the clipboard open");
    HANDLE d = IsClipboardFormatAvailable(CF_UNICODETEXT) ? GetClipboardData(CF_UNICODETEXT) : NULL;
    const WCHAR *ws = d ? GlobalLock(d) : NULL;
    int e = GOOME_OK;
    if (ws) {
        int n = WideCharToMultiByte(CP_UTF8, 0, ws, -1, NULL, 0, NULL, NULL);
        char *s = n > 0 ? malloc((size_t)n) : NULL;
        if (s && WideCharToMultiByte(CP_UTF8, 0, ws, -1, s, n, NULL, NULL) > 0) {
            int k = 0;
            for (int i = 0; s[i]; i++)
                if (!(s[i] == '\r' && s[i + 1] == '\n')) s[k++] = s[i];
            s[k] = 0;
            *out = s;
        } else {
            free(s);
            if (n > 0) e = gm_fail(g, GOOME_ENOMEM, "no memory for the clipboard");
        }
        GlobalUnlock(d);
    }
    CloseClipboard();
    return e;
}

static int w32_ov_cursor(goome *g, int shape)
{
    w32_cursor_now(w32.hot, shape);
    return GOOME_OK;
}

static int w32_ov_clip_set(goome *g, const char *s) { return w32_clip_put(g, w32.n ? w32.m[0].win : NULL, s); }
static int w32_ov_clip_get(goome *g, char **out) { return w32_clip_take(g, w32.n ? w32.m[0].win : NULL, out); }

static void w32w_outer(const w32_win *w, int cw, int ch, UINT dpi, int *ow, int *oh)
{
    RECT r = {0, 0, cw, ch};
    if (!w->popup && !w->fs && !(w32x.adjust && w32x.adjust(&r, w->style, FALSE, w->ex, dpi))) AdjustWindowRectEx(&r, w->style, FALSE, w->ex);
    *ow = r.right - r.left, *oh = r.bottom - r.top;
}

static double w32w_scale(HWND h)
{
    UINT d = w32x.dpi_for_win && h ? w32x.dpi_for_win(h) : 0;
    if (!d && w32d.aware == 2 && w32d.dpi_for_mon) {
        UINT dx = 0, dy = 0;
        if (w32d.dpi_for_mon(MonitorFromWindow(h, MONITOR_DEFAULTTOPRIMARY), 0, &dx, &dy) == S_OK) d = dx;
    }
    if (d < 48) d = w32d.aware == 0 ? 96 : (UINT)w32d.sysdpi;
    return d / 96.0;
}

static UINT w32w_dpi(const w32_win *w) { return (UINT)(w->s * 96 + 0.5); }

static void w32w_ev(w32_win *w, int type, double x, double y, int button)
{
    goome_event e;
    memset(&e, 0, sizeof(e));
    e.type = type, e.x = x, e.y = y, e.button = button, e.mods = w32_mods();
    gm_push(w->g, &e);
}

static int w32w_buffer(w32_win *w, int pw, int ph)
{
    if (pw < 1) pw = 1;
    if (ph < 1) ph = 1;
    if (w->bmp && pw == w->pw && ph == w->ph) return 1;
    uint32_t *px = NULL;
    HBITMAP b = w32_dib(&w32x.gdi, pw, ph, &px);
    if (!b) return 0;
    w32x.gdi.flush();
    memset(px, 0, (size_t)pw * ph * 4);
    if (w->px) {
        int cw = pw < w->pw ? pw : w->pw, ch = ph < w->ph ? ph : w->ph;
        for (int y = 0; y < ch; y++) memcpy(px + (size_t)y * pw, w->px + (size_t)y * w->pw, (size_t)cw * 4);
    }
    w32x.gdi.select(w->dc, b);
    if (w->bmp) w32x.gdi.del_obj(w->bmp);
    w->bmp = b, w->px = px, w->pw = pw, w->ph = ph;
    return 1;
}

static void w32w_publish(w32_win *w)
{
    goome_monitor m;
    memset(&m, 0, sizeof(m));
    snprintf(m.name, sizeof(m.name), "%s", w->g->title);
    m.area = (goome_rect){0, 0, w->lw, w->lh};
    m.scale = w->s, m.pw = w->pw, m.ph = w->ph;
    if (w->cv.px != w->px || w->cv.w != w->pw || w->cv.h != w->ph || w->cv.scale != w->s)
        gm_canvas_reset(&w->cv, w->px, w->pw, w->ph, w->pw, 0, 0, w->s, 0);
    gm_set_monitors(w->g, &m, 1);
}

static void w32w_sized(w32_win *w)
{
    RECT r;
    if (!w->ready || !GetClientRect(w->win, &r) || IsIconic(w->win) || r.right < 1 || r.bottom < 1) return;
    if (!w32w_buffer(w, r.right, r.bottom)) return;
    w->lw = (int)(w->pw / w->s + 0.5), w->lh = (int)(w->ph / w->s + 0.5);
    w32w_publish(w);
}

static void w32w_down(w32_win *w, int button, LPARAM lp)
{
    if (!w->ours) SetCapture(w->win);
    w->ours |= 1 << (button - 1);
    w32w_ev(w, GOOME_DOWN, (short)LOWORD(lp) / w->s, (short)HIWORD(lp) / w->s, button);
}

static void w32w_up(w32_win *w, int button, LPARAM lp)
{
    int bit = 1 << (button - 1);
    if (!(w->ours & bit)) return;
    w->ours &= ~bit;
    if (!w->ours && !w->grab) ReleaseCapture();
    w32w_ev(w, GOOME_UP, (short)LOWORD(lp) / w->s, (short)HIWORD(lp) / w->s, button);
}

static void w32w_dropped(w32_win *w, HANDLE d)
{
    if (!w32x.shell) {
        w32x.shell = LoadLibraryA("shell32.dll");
        W32_LOAD(w32x.drag_query, w32x.shell, "DragQueryFileW");
        W32_LOAD(w32x.drag_point, w32x.shell, "DragQueryPoint");
        W32_LOAD(w32x.drag_finish, w32x.shell, "DragFinish");
    }
    if (!w32x.drag_query || !w32x.drag_finish) return;
    POINT p = {0, 0};
    if (w32x.drag_point) w32x.drag_point(d, &p);
    UINT n = w32x.drag_query(d, 0xFFFFFFFFu, NULL, 0);
    for (UINT i = 0; i < n; i++) {
        UINT len = w32x.drag_query(d, i, NULL, 0);
        WCHAR *ws = malloc(sizeof(WCHAR) * (len + 1));
        w32_drop_item *nd = realloc(w->drops, sizeof(*nd) * (size_t)(w->ndrop + 1));
        if (nd) w->drops = nd;
        if (!ws || !nd || !w32x.drag_query(d, i, ws, len + 1)) {
            free(ws);
            continue;
        }
        int u = WideCharToMultiByte(CP_UTF8, 0, ws, -1, NULL, 0, NULL, NULL);
        char *s = u > 0 ? malloc((size_t)u) : NULL;
        if (s && WideCharToMultiByte(CP_UTF8, 0, ws, -1, s, u, NULL, NULL) > 0) {
            w32_drop_item it = {s, p.x / w->s, p.y / w->s};
            w->drops[w->ndrop++] = it;
        } else
            free(s);
        free(ws);
    }
    w32x.drag_finish(d);
}

static void w32w_next_drop(w32_win *w)
{
    if (!w->ndrop || w->g->qlen) return;
    w32_drop_item it = w->drops[0];
    memmove(w->drops, w->drops + 1, sizeof(*w->drops) * (size_t)(w->ndrop - 1));
    w->ndrop--;
    gm_drop(w->g, it.x, it.y, it.path);
    free(it.path);
}

static LRESULT CALLBACK w32w_proc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_NCCREATE) SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)((CREATESTRUCTW *)lp)->lpCreateParams);
    w32_win *w = (w32_win *)GetWindowLongPtrW(h, GWLP_USERDATA);
    if (!w) return DefWindowProcW(h, msg, wp, lp);
    goome *g = w->g;
    switch (msg) {
    case WM_NCCALCSIZE:
        if (wp && w->popup) return 0;
        break;
    case WM_NCHITTEST:
        if (w->popup) return HTCLIENT;
        break;
    case WM_GETMINMAXINFO: {
        MINMAXINFO *mm = (MINMAXINFO *)lp;
        if (g->opts.min_width > 0 || g->opts.min_height > 0) {
            int ow, oh;
            w32w_outer(w, (int)(g->opts.min_width * w->s + 0.5), (int)(g->opts.min_height * w->s + 0.5), w32w_dpi(w), &ow, &oh);
            if (g->opts.min_width > 0) mm->ptMinTrackSize.x = ow;
            if (g->opts.min_height > 0) mm->ptMinTrackSize.y = oh;
        }
        MONITORINFO mi;
        memset(&mi, 0, sizeof(mi));
        mi.cbSize = sizeof(mi);
        if (w->popup && !w->fs && GetMonitorInfoW(MonitorFromWindow(h, MONITOR_DEFAULTTONEAREST), &mi)) {
            mm->ptMaxPosition.x = mi.rcWork.left - mi.rcMonitor.left, mm->ptMaxPosition.y = mi.rcWork.top - mi.rcMonitor.top;
            mm->ptMaxSize.x = mi.rcWork.right - mi.rcWork.left, mm->ptMaxSize.y = mi.rcWork.bottom - mi.rcWork.top;
        }
        return 0;
    }
    case 0x02E4: {
        SIZE *sz = (SIZE *)lp;
        double ns = (UINT)wp / 96.0;
        int ow, oh;
        w32w_outer(w, (int)(w->lw * ns + 0.5), (int)(w->lh * ns + 0.5), (UINT)wp, &ow, &oh);
        sz->cx = ow, sz->cy = oh;
        return TRUE;
    }
    case 0x02E0: {
        const RECT *r = (const RECT *)lp;
        w->s = LOWORD(wp) >= 48 ? LOWORD(wp) / 96.0 : w->s;
        SetWindowPos(h, NULL, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
        w32w_sized(w);
        return 0;
    }
    case WM_SIZE: w32w_sized(w); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        if (dc && !w->trans && w->px) {
            RECT r = ps.rcPaint;
            w32x.gdi.blt(dc, r.left, r.top, r.right - r.left, r.bottom - r.top, w->dc, r.left, r.top, SRCCOPY);
        }
        EndPaint(h, &ps);
        return 0;
    }
    case WM_LBUTTONDOWN: w32w_down(w, 1, lp); return 0;
    case WM_MBUTTONDOWN: w32w_down(w, 2, lp); return 0;
    case WM_RBUTTONDOWN: w32w_down(w, 3, lp); return 0;
    case WM_LBUTTONUP: w32w_up(w, 1, lp); return 0;
    case WM_MBUTTONUP: w32w_up(w, 2, lp); return 0;
    case WM_RBUTTONUP: w32w_up(w, 3, lp); return 0;
    case WM_CAPTURECHANGED: w->ours = 0, w->grab = 0; break;
    case WM_MOUSEMOVE: {
        double x = (short)LOWORD(lp) / w->s, y = (short)HIWORD(lp) / w->s;
        if (x != w->lx || y != w->ly) w->lx = x, w->ly = y, w32w_ev(w, GOOME_MOTION, x, y, 0);
        return 0;
    }
    case WM_MOUSEWHEEL: {
        POINT p = {(short)LOWORD(lp), (short)HIWORD(lp)};
        ScreenToClient(h, &p);
        w->wheel += (short)HIWORD(wp);
        int notches = w->wheel / WHEEL_DELTA;
        w->wheel -= notches * WHEEL_DELTA;
        if (notches) {
            goome_event e;
            memset(&e, 0, sizeof(e));
            e.type = GOOME_SCROLL, e.x = p.x / w->s, e.y = p.y / w->s, e.dy = -notches, e.mods = w32_mods();
            gm_push(g, &e);
        }
        return 0;
    }
    case WM_SETCURSOR:
        if (LOWORD(lp) == HTCLIENT) {
            SetCursor(w32_cursor(g->cursor));
            return TRUE;
        }
        break;
    case WM_SYSCHAR:
    case WM_CHAR: {
        if (msg == WM_SYSCHAR && wp == ' ') break;
        unsigned c = (unsigned)wp;
        if (w32_pair(&w->hi, &c)) w32_char(g, c);
        return 0;
    }
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN: {
        int k = w32_navkey(wp);
        if (k) {
            goome_event e;
            memset(&e, 0, sizeof(e));
            e.type = GOOME_KEY, e.key = k, e.mods = w32_mods();
            gm_push(g, &e);
        }
        if (msg == WM_SYSKEYDOWN && (wp == VK_F4 || !k)) break;
        if (k) return 0;
        break;
    }
    case WM_SYSCOMMAND:
        if ((wp & 0xFFF0) == SC_KEYMENU && lp == 0) return 0;
        break;
    case WM_SETFOCUS:
    case WM_KILLFOCUS: {
        goome_event e;
        memset(&e, 0, sizeof(e));
        e.type = GOOME_FOCUS, e.id = msg == WM_SETFOCUS;
        gm_push(g, &e);
        break;
    }
    case WM_CLOSE: {
        goome_event e;
        memset(&e, 0, sizeof(e));
        e.type = GOOME_CLOSE;
        gm_push(g, &e);
        return 0;
    }
    case WM_HOTKEY: {
        int k = (int)wp - 1;
        if (k >= 0 && k < w->nreg) {
            goome_event e;
            memset(&e, 0, sizeof(e));
            e.type = GOOME_HOTKEY, e.id = w->hk[k].id, e.key = w->hk[k].key, e.mods = w->hk[k].mods;
            gm_push(g, &e);
        }
        return 0;
    }
    case WM_DROPFILES: w32w_dropped(w, (HANDLE)wp); return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

static void w32w_close(goome *g);

static int w32w_load(goome *g)
{
    if (w32x.users) return GOOME_OK;
    memset(&w32x, 0, sizeof(w32x));
    if (!w32_load_gdi(&w32x.gdi, &w32x.gdi_dll)) return gm_fail(g, GOOME_ENODISPLAY, "cant load gdi32.dll");
    HMODULE u = GetModuleHandleA("user32.dll");
    w32x.shcore = LoadLibraryA("shcore.dll");
    W32_LOAD(w32x.dpi_for_win, u, "GetDpiForWindow");
    W32_LOAD(w32x.adjust, u, "AdjustWindowRectExForDpi");
    W32_LOAD(w32x.ulw_indirect, u, "UpdateLayeredWindowIndirect");
    W32_LOAD(w32d.dpi_for_mon, w32x.shcore, "GetDpiForMonitor");
    w32_dpi_aware(w32x.shcore, &w32x.gdi);
    WNDCLASSEXW wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = w32w_proc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.hCursor = NULL;
    wc.hIcon = LoadIconW(wc.hInstance, MAKEINTRESOURCEW(1));
    if (!wc.hIcon) wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.lpszClassName = L"goome_window";
    w32x.cls = RegisterClassExW(&wc);
    if (!w32x.cls) {
        int e = gm_fail(g, GOOME_ENODISPLAY, "cant register the window class (error %lu)", GetLastError());
        if (w32x.gdi_dll) FreeLibrary(w32x.gdi_dll);
        if (w32x.shcore) FreeLibrary(w32x.shcore);
        memset(&w32x, 0, sizeof(w32x));
        return e;
    }
    return GOOME_OK;
}

static void w32w_title(w32_win *w)
{
    WCHAR t[64];
    if (!MultiByteToWideChar(CP_UTF8, 0, w->g->title, -1, t, 64)) t[0] = 0;
    t[63] = 0;
    SetWindowTextW(w->win, t);
}

static const gm_ov *w32w_table(void);

static int w32w_open(goome *g)
{
    int e = w32w_load(g);
    if (e != GOOME_OK) return e;
    w32x.users++;
    w32_win *w = calloc(1, sizeof(*w));
    if (!w) {
        g->ov_priv = NULL;
        w32w_close(g);
        return gm_fail(g, GOOME_ENOMEM, "no memory for the window");
    }
    g->ov_priv = w;
    w->g = g, w->lx = w->ly = -1e9;
    int f = g->opts.window_flags;
    w->trans = (f & GOOME_WINDOW_TRANSPARENT) != 0;
    w->popup = w->trans || (f & GOOME_WINDOW_BORDERLESS);
    if (w->popup) w->style = WS_POPUP | WS_SYSMENU | WS_MINIMIZEBOX | (f & GOOME_WINDOW_RESIZABLE ? WS_THICKFRAME | WS_MAXIMIZEBOX : 0);
    else w->style = f & GOOME_WINDOW_RESIZABLE ? WS_OVERLAPPEDWINDOW : WS_OVERLAPPEDWINDOW & ~(DWORD)(WS_THICKFRAME | WS_MAXIMIZEBOX);
    w->ex = WS_EX_APPWINDOW | WS_EX_ACCEPTFILES | (w->trans ? WS_EX_LAYERED : 0) | (f & GOOME_WINDOW_ON_TOP ? WS_EX_TOPMOST : 0);
    w->dc = w32x.gdi.create_dc(NULL);
    int at = w->popup ? 0 : CW_USEDEFAULT;
    if (w->dc) w->win = CreateWindowExW(w->ex, (LPCWSTR)(UINT_PTR)w32x.cls, L"", w->style, at, at, 200, 200, NULL, NULL, GetModuleHandleW(NULL), w);
    if (!w->win) {
        e = gm_fail(g, GOOME_ENODISPLAY, "cant make the window (error %lu)", GetLastError());
        w32w_close(g);
        return e;
    }
    w32w_title(w);
    w->s = w32w_scale(w->win);
    int ow, oh;
    w32w_outer(w, (int)(g->opts.width * w->s + 0.5), (int)(g->opts.height * w->s + 0.5), w32w_dpi(w), &ow, &oh);
    RECT wr;
    GetWindowRect(w->win, &wr);
    int x = wr.left, y = wr.top;
    MONITORINFO mi;
    memset(&mi, 0, sizeof(mi));
    mi.cbSize = sizeof(mi);
    if (((f & GOOME_WINDOW_CENTER) || w->popup) && GetMonitorInfoW(MonitorFromWindow(w->win, MONITOR_DEFAULTTOPRIMARY), &mi)) {
        x = (mi.rcWork.left + mi.rcWork.right - ow) / 2, y = (mi.rcWork.top + mi.rcWork.bottom - oh) / 2;
        if (y < mi.rcWork.top) y = mi.rcWork.top;
    }
    SetWindowPos(w->win, NULL, x, y, ow, oh, SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    w->ready = 1;
    w32w_sized(w);
    if (!w->px || g->nmon < 1) {
        e = gm_fail(g, GOOME_ENOMEM, "cant make a %dx%d bitmap for the window", g->opts.width, g->opts.height);
        w32w_close(g);
        return e;
    }
    if (!(f & GOOME_WINDOW_HIDDEN)) {
        ShowWindow(w->win, SW_SHOW);
        UpdateWindow(w->win);
    }
    g->ov = w32w_table();
    return GOOME_OK;
}

static goome_canvas *w32w_canvas(goome *g, int mon)
{
    w32_win *w = g->ov_priv;
    return mon == 0 && w->px ? &w->cv : NULL;
}

static void w32w_solid(w32_win *w, uint32_t from, uint32_t to)
{
    for (int i = 0; i < w->nhits; i++) {
        const goome_rect *r = &w->hits[i];
        int x0 = (int)(r->x * w->s), y0 = (int)(r->y * w->s), x1 = (int)((r->x + r->w) * w->s + 0.999), y1 = (int)((r->y + r->h) * w->s + 0.999);
        if (x0 < 0) x0 = 0;
        if (y0 < 0) y0 = 0;
        if (x1 > w->pw) x1 = w->pw;
        if (y1 > w->ph) y1 = w->ph;
        for (int y = y0; y < y1; y++) {
            uint32_t *row = w->px + (size_t)y * w->pw;
            for (int x = x0; x < x1; x++)
                if (row[x] == from) row[x] = to;
        }
    }
}

static int w32w_commit(goome *g, int mon, goome_canvas *c, const goome_rect *hits, int n)
{
    w32_win *w = g->ov_priv;
    if (mon != 0 || c != &w->cv || !w->win) return GOOME_EGONE;
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0, dmg = gm_canvas_damage(c, &x0, &y0, &x1, &y1);
    if (!w->trans) {
        if (!dmg) return GOOME_OK;
        HDC dc = GetDC(w->win);
        if (dc) {
            w32x.gdi.blt(dc, x0, y0, x1 - x0, y1 - y0, w->dc, x0, y0, SRCCOPY);
            ReleaseDC(w->win, dc);
        }
        w32x.gdi.flush();
        return GOOME_OK;
    }
    int changed = n != w->nhits || (n && memcmp(w->hits, hits, sizeof(*hits) * (size_t)n));
    if (n > w->hcap) {
        goome_rect *nh = realloc(w->hits, sizeof(*nh) * (size_t)n);
        if (!nh) return gm_fail(g, GOOME_ENOMEM, "no memory for %d hit rects", n);
        w->hits = nh, w->hcap = n;
    }
    if (n) memcpy(w->hits, hits, sizeof(*hits) * (size_t)n);
    w->nhits = n;
    if (!dmg && !changed) return GOOME_OK;
    w32w_solid(w, 0, 0x01000000u);
    w32x.gdi.flush();
    POINT zero = {0, 0};
    SIZE sz = {w->pw, w->ph};
    BLENDFUNCTION bf = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    w32_ulw_info info = {sizeof(info), NULL, NULL, &sz, w->dc, &zero, 0, &bf, ULW_ALPHA, NULL};
    if (!w32x.ulw_indirect || !w32x.ulw_indirect(w->win, &info)) UpdateLayeredWindow(w->win, NULL, NULL, &sz, w->dc, &zero, 0, &bf, ULW_ALPHA);
    w32w_solid(w, 0x01000000u, 0);
    return GOOME_OK;
}

static int w32w_wait(goome *g, int ms)
{
    w32_win *w = g->ov_priv;
    long end = goome_now_ms() + (ms > 0 ? ms : 0);
    for (;;) {
        MSG msg;
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                PostQuitMessage((int)msg.wParam);
                return -1;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        w32w_next_drop(w);
        if (g->qlen) return 0;
        long left = end - goome_now_ms();
        if (left <= 0) return 0;
        MsgWaitForMultipleObjectsEx(0, NULL, left < 50 ? (DWORD)left : 50, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
    }
}

static int w32w_pointer(goome *g, double *x, double *y, int *buttons)
{
    w32_win *w = g->ov_priv;
    POINT p;
    if (!GetCursorPos(&p) || !ScreenToClient(w->win, &p)) return 0;
    *x = p.x / w->s, *y = p.y / w->s;
    int b = 0;
    for (int k = 1; k <= 3; k++)
        if (GetAsyncKeyState(w32_held_vk(k)) & 0x8000) b |= 1 << (k - 1);
    *buttons = b;
    return 1;
}

static int w32w_keyboard(goome *g, int on)
{
    w32_win *w = g->ov_priv;
    if (!on || GetForegroundWindow() == w->win) return GOOME_OK;
    if (!SetForegroundWindow(w->win)) return gm_fail(g, GOOME_EDENIED, "windows wont hand us the keyboard right now (focus lock)");
    SetFocus(w->win);
    return GOOME_OK;
}

static int w32w_hold(goome *g, int on)
{
    w32_win *w = g->ov_priv;
    if (on) {
        if (GetCapture() != w->win) SetCapture(w->win);
        w->grab = 1;
        return GOOME_OK;
    }
    w->grab = 0;
    if (!w->ours && GetCapture() == w->win) ReleaseCapture();
    return GOOME_OK;
}

static int w32w_hotkeys(goome *g)
{
    w32_win *w = g->ov_priv;
    for (int i = 0; i < w->nreg; i++) UnregisterHotKey(w->win, i + 1);
    w->nreg = g->nhk;
    memcpy(w->hk, g->hk, sizeof(gm_hotkey) * (size_t)g->nhk);
    int bad = -1;
    for (int i = 0; i < g->nhk; i++) {
        int md = g->hk[i].mods;
        UINT vk = w32_vk(g->hk[i].key);
        UINT mods = 0x4000 | (md & GOOME_SHIFT ? MOD_SHIFT : 0) | (md & GOOME_CTRL ? MOD_CONTROL : 0) | (md & GOOME_ALT ? MOD_ALT : 0) | (md & GOOME_SUPER ? MOD_WIN : 0);
        if (!vk || !RegisterHotKey(w->win, i + 1, mods, vk)) bad = i;
    }
    if (bad >= 0) return gm_fail(g, GOOME_EDENIED, w32_vk(g->hk[bad].key) ? "another app already has the hotkey with id %d" : "the key for hotkey id %d isnt on this keyboard", g->hk[bad].id);
    return GOOME_OK;
}

static void w32w_fullscreen(w32_win *w, int on)
{
    if (on == w->fs) return;
    if (on) {
        MONITORINFO mi;
        memset(&mi, 0, sizeof(mi));
        mi.cbSize = sizeof(mi);
        if (!GetMonitorInfoW(MonitorFromWindow(w->win, MONITOR_DEFAULTTONEAREST), &mi)) return;
        memset(&w->keep, 0, sizeof(w->keep));
        w->keep.length = sizeof(w->keep);
        GetWindowPlacement(w->win, &w->keep);
        w->fs = 1;
        SetWindowLongPtrW(w->win, GWL_STYLE, (LONG_PTR)(WS_POPUP | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE));
        SetWindowPos(w->win, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top,
                     SWP_FRAMECHANGED | SWP_NOOWNERZORDER);
        return;
    }
    w->fs = 0;
    SetWindowLongPtrW(w->win, GWL_STYLE, (LONG_PTR)(w->style | WS_VISIBLE));
    SetWindowPlacement(w->win, &w->keep);
    SetWindowPos(w->win, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
}

static void w32w_to_logical(POINT p, int *x, int *y)
{
    w32_mon all[GM_MAX_MON];
    int n = w32_list(all);
    *x = p.x, *y = p.y;
    for (int i = 0; i < n; i++)
        if (p.x >= all[i].r.left && p.x < all[i].r.right && p.y >= all[i].r.top && p.y < all[i].r.bottom) {
            *x = all[i].r.left + (int)((p.x - all[i].r.left) / all[i].s + 0.5), *y = all[i].r.top + (int)((p.y - all[i].r.top) / all[i].s + 0.5);
            return;
        }
}

static POINT w32w_to_real(int x, int y)
{
    w32_mon all[GM_MAX_MON];
    int n = w32_list(all);
    POINT p = {x, y};
    for (int i = 0; i < n; i++) {
        const RECT *r = &all[i].r;
        double s = all[i].s;
        if (x >= r->left && y >= r->top && x < r->left + (r->right - r->left) / s && y < r->top + (r->bottom - r->top) / s) {
            p.x = r->left + (LONG)((x - r->left) * s + 0.5), p.y = r->top + (LONG)((y - r->top) * s + 0.5);
            break;
        }
    }
    return p;
}

static int w32w_drag(goome *g, w32_win *w, int edges)
{
    static const unsigned char ht[16] = {HTCAPTION, HTLEFT, HTRIGHT, 0, HTTOP, HTTOPLEFT, HTTOPRIGHT, 0, HTBOTTOM, HTBOTTOMLEFT, HTBOTTOMRIGHT, 0, 0, 0, 0, 0};
    int code = ht[edges & 15];
    if (!code) return gm_fail(g, GOOME_EARG, "edges %d dont make a side or a corner", edges);
    if (edges && !(w->style & WS_THICKFRAME)) return gm_fail(g, GOOME_ENOTSUP, "this window isnt resizable");
    if (!(GetAsyncKeyState(w32_held_vk(1)) & 0x8000)) return gm_fail(g, GOOME_EAGAIN, "the left button isnt down anymore");
    POINT p;
    GetCursorPos(&p);
    ReleaseCapture();
    w->ours = 0;
    SendMessageW(w->win, WM_NCLBUTTONDOWN, (WPARAM)code, MAKELPARAM(p.x, p.y));
    GetCursorPos(&p);
    ScreenToClient(w->win, &p);
    w32w_ev(w, GOOME_UP, p.x / w->s, p.y / w->s, 1);
    return GOOME_OK;
}

static int w32w_win(goome *g, int op, int a, int b, int *ra, int *rb)
{
    w32_win *w = g->ov_priv;
    switch (op) {
    case GM_WIN_TITLE:
        w32w_title(w);
        w32w_publish(w);
        return GOOME_OK;
    case GM_WIN_SIZE: {
        int ow, oh;
        if (w->fs) w32w_fullscreen(w, 0);
        if (IsZoomed(w->win) || IsIconic(w->win)) ShowWindow(w->win, SW_RESTORE);
        w32w_outer(w, (int)(a * w->s + 0.5), (int)(b * w->s + 0.5), w32w_dpi(w), &ow, &oh);
        SetWindowPos(w->win, NULL, 0, 0, ow, oh, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        return GOOME_OK;
    }
    case GM_WIN_POS_GET: {
        POINT p = {0, 0};
        if (!ClientToScreen(w->win, &p)) return gm_fail(g, GOOME_EGONE, "the window is gone");
        w32w_to_logical(p, ra, rb);
        return GOOME_OK;
    }
    case GM_WIN_POS_SET: {
        POINT o = {0, 0}, p = w32w_to_real(a, b);
        RECT r;
        if (!ClientToScreen(w->win, &o) || !GetWindowRect(w->win, &r)) return gm_fail(g, GOOME_EGONE, "the window is gone");
        SetWindowPos(w->win, NULL, p.x + r.left - o.x, p.y + r.top - o.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        return GOOME_OK;
    }
    case GM_WIN_STATE:
        if (a != GOOME_STATE_FULLSCREEN) w32w_fullscreen(w, 0);
        if (a == GOOME_STATE_MINIMIZED) ShowWindow(w->win, SW_MINIMIZE);
        else if (a == GOOME_STATE_MAXIMIZED) ShowWindow(w->win, SW_MAXIMIZE);
        else if (a == GOOME_STATE_FULLSCREEN) {
            if (IsIconic(w->win)) ShowWindow(w->win, SW_RESTORE);
            w32w_fullscreen(w, 1);
        } else if (IsIconic(w->win) || IsZoomed(w->win))
            ShowWindow(w->win, SW_RESTORE);
        return GOOME_OK;
    case GM_WIN_GET_STATE:
        *ra = IsIconic(w->win) ? GOOME_STATE_MINIMIZED : w->fs ? GOOME_STATE_FULLSCREEN : IsZoomed(w->win) ? GOOME_STATE_MAXIMIZED : GOOME_STATE_NORMAL;
        return GOOME_OK;
    case GM_WIN_DRAG: return w32w_drag(g, w, a);
    case GM_WIN_SHOW:
        ShowWindow(w->win, a ? SW_SHOW : SW_HIDE);
        return GOOME_OK;
    }
    return gm_fail(g, GOOME_ENOTSUP, "win32 cant do window op %d", op);
}

static int w32w_cursor_set(goome *g, int shape)
{
    w32_win *w = g->ov_priv;
    w32_cursor_now(w->win, shape);
    return GOOME_OK;
}

static int w32w_clip_set(goome *g, const char *s) { return w32_clip_put(g, ((w32_win *)g->ov_priv)->win, s); }
static int w32w_clip_get(goome *g, char **out) { return w32_clip_take(g, ((w32_win *)g->ov_priv)->win, out); }

static void w32w_close(goome *g)
{
    w32_win *w = g->ov_priv;
    if (w) {
        if (w->win) {
            for (int i = 0; i < w->nreg; i++) UnregisterHotKey(w->win, i + 1);
            if (GetCapture() == w->win) ReleaseCapture();
            SetWindowLongPtrW(w->win, GWLP_USERDATA, 0);
            DestroyWindow(w->win);
        }
        if (w->dc) w32x.gdi.del_dc(w->dc);
        if (w->bmp) w32x.gdi.del_obj(w->bmp);
        if (w->cdc) w32x.gdi.del_dc(w->cdc);
        if (w->cbmp) w32x.gdi.del_obj(w->cbmp);
        for (int i = 0; i < w->ndrop; i++) free(w->drops[i].path);
        free(w->drops);
        free(w->hits);
        free(w);
        g->ov_priv = NULL;
    }
    if (w32x.users > 0 && --w32x.users == 0) {
        if (w32x.cls) UnregisterClassW((LPCWSTR)(UINT_PTR)w32x.cls, GetModuleHandleW(NULL));
        if (w32x.gdi_dll) FreeLibrary(w32x.gdi_dll);
        if (w32x.shcore) FreeLibrary(w32x.shcore);
        if (w32x.shell) FreeLibrary(w32x.shell);
        memset(&w32x, 0, sizeof(w32x));
    }
}

static int w32w_grab(goome *g, goome_rect r, goome_image *img)
{
    w32_win *w = g->ov_priv;
    int x0 = (int)(r.x * w->s + 0.5), y0 = (int)(r.y * w->s + 0.5), x1 = (int)((r.x + r.w) * w->s + 0.5), y1 = (int)((r.y + r.h) * w->s + 0.5);
    if (x1 > w->pw) x1 = w->pw;
    if (y1 > w->ph) y1 = w->ph;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 <= x0 || y1 <= y0) return gm_fail(g, GOOME_EARG, "that rect is off the window");
    if (!w->cdc || w->cw != w->pw || w->ch != w->ph) {
        if (w->cdc) w32x.gdi.del_dc(w->cdc);
        if (w->cbmp) w32x.gdi.del_obj(w->cbmp);
        w->cdc = w32x.gdi.create_dc(NULL);
        w->cbmp = w->cdc ? w32_dib(&w32x.gdi, w->pw, w->ph, &w->cpx) : NULL;
        if (!w->cbmp) {
            if (w->cdc) w32x.gdi.del_dc(w->cdc);
            w->cdc = NULL, w->cbmp = NULL, w->cpx = NULL;
            return gm_fail(g, GOOME_ENOMEM, "cant make a %dx%d bitmap to grab into", w->pw, w->ph);
        }
        w32x.gdi.select(w->cdc, w->cbmp);
        w->cw = w->pw, w->ch = w->ph;
    }
    POINT o = {0, 0};
    ClientToScreen(w->win, &o);
    int cw = x1 - x0, ch = y1 - y0;
    HDC screen = GetDC(NULL);
    BOOL ok = screen && w32x.gdi.blt(w->cdc, 0, 0, cw, ch, screen, o.x + x0, o.y + y0, SRCCOPY | CAPTUREBLT);
    if (screen) ReleaseDC(NULL, screen);
    if (!ok) return gm_fail(g, GOOME_EDENIED, "BitBlt from the screen failed (error %lu, locked or secure desktop?)", GetLastError());
    w32x.gdi.flush();
    for (int y = 0; y < ch; y++) {
        uint32_t *row = w->cpx + (size_t)y * w->cw;
        for (int x = 0; x < cw; x++) row[x] |= 0xFF000000u;
    }
    img->px = w->cpx, img->w = cw, img->h = ch, img->stride = w->cw, img->area = r;
    img->has_self = 1;
    return GOOME_OK;
}

static const gm_ov w32w_ov = {"win32", w32w_open, w32w_canvas, w32w_commit, w32w_wait, w32w_pointer, w32w_keyboard, w32w_hotkeys, w32w_close,
                              GOOME_CAP_GLOBAL_POINTER | GOOME_CAP_KEYBOARD | GOOME_CAP_HOTKEYS | GOOME_CAP_WINDOW | GOOME_CAP_CLIPBOARD | GOOME_CAP_CURSOR | GOOME_CAP_GRAB,
                              w32w_win, w32w_cursor_set, w32w_clip_set, w32w_clip_get, w32w_hold};
static const gm_ov *w32w_table(void) { return &w32w_ov; }

static void w32_close(goome *g)
{
    if (g->opts.mode == GOOME_MODE_WINDOW) {
        w32w_close(g);
        return;
    }
    for (int i = 0; i < w32.nreg; i++) UnregisterHotKey(NULL, i + 1);
    if (GetCapture() && w32_mon_of(GetCapture())) ReleaseCapture();
    for (int i = 0; i < w32.n; i++) w32_drop_cap(&w32.m[i]), w32_drop_win(&w32.m[i]);
    if (w32.cls) UnregisterClassW((LPCWSTR)(UINT_PTR)w32.cls, GetModuleHandleW(NULL));
    if (w32.gdi_dll) FreeLibrary(w32.gdi_dll);
    if (w32.shcore) FreeLibrary(w32.shcore);
    memset(&w32, 0, sizeof(w32));
}

static gm_ov w32_ov = {"win32", w32_open, w32_canvas, w32_commit, w32_wait, w32_pointer, w32_keyboard, w32_hotkeys, w32_close,
                       GOOME_CAP_GLOBAL_POINTER | GOOME_CAP_KEYBOARD | GOOME_CAP_HOTKEYS | GOOME_CAP_CLIPBOARD | GOOME_CAP_CURSOR | GOOME_CAP_GRAB,
                       NULL, w32_ov_cursor, w32_ov_clip_set, w32_ov_clip_get, w32_ov_grab};
const gm_ov *gm_ov_win32(void) { return &w32_ov; }

static int w32_cap_open(goome *g)
{
    if (g->opts.mode == GOOME_MODE_WINDOW) return g->ov_priv ? GOOME_OK : gm_fail(g, GOOME_ENOTSUP, "the window isnt open");
    if (!w32.gdi_dll) return gm_fail(g, GOOME_ENOTSUP, "win32 capture needs the win32 overlay open");
    return GOOME_OK;
}

static int w32_grab(goome *g, int mon, goome_rect r, goome_image *img)
{
    if (g->opts.mode == GOOME_MODE_WINDOW) return w32w_grab(g, r, img);
    if (mon < 0 || mon >= w32.n) return gm_fail(g, GOOME_EGONE, "no monitor %d", mon);
    w32_mon *m = &w32.m[mon];
    int pw = m->r.right - m->r.left, ph = m->r.bottom - m->r.top;
    int x0 = (int)((r.x - m->r.left) * m->s + 0.5), y0 = (int)((r.y - m->r.top) * m->s + 0.5);
    int x1 = (int)((r.x + r.w - m->r.left) * m->s + 0.5), y1 = (int)((r.y + r.h - m->r.top) * m->s + 0.5);
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > pw) x1 = pw;
    if (y1 > ph) y1 = ph;
    if (x1 <= x0 || y1 <= y0) return gm_fail(g, GOOME_EARG, "that rect is off monitor %d", mon);
    if (!m->cdc) {
        m->cdc = w32.gdi.create_dc(NULL);
        m->cbmp = m->cdc ? w32_dib(&w32.gdi, pw, ph, &m->cpx) : NULL;
        if (!m->cbmp) {
            w32_drop_cap(m);
            return gm_fail(g, GOOME_ENOMEM, "cant make a %dx%d bitmap to grab %s into", pw, ph, m->name);
        }
        w32.gdi.select(m->cdc, m->cbmp);
    }
    int w = x1 - x0, h = y1 - y0;
    HDC screen = GetDC(NULL);
    BOOL ok = screen && w32.gdi.blt(m->cdc, 0, 0, w, h, screen, m->r.left + x0, m->r.top + y0, SRCCOPY | CAPTUREBLT);
    if (screen) ReleaseDC(NULL, screen);
    if (!ok) return gm_fail(g, GOOME_EDENIED, "BitBlt from the screen failed on %s (error %lu, locked or secure desktop?)", m->name, GetLastError());
    w32.gdi.flush();
    for (int y = 0; y < h; y++) {
        uint32_t *row = m->cpx + (size_t)y * pw;
        for (int x = 0; x < w; x++) row[x] |= 0xFF000000u;
    }
    img->px = m->cpx, img->w = w, img->h = h, img->stride = pw, img->area = r;
    img->has_self = !w32.hidden_ok;
    return GOOME_OK;
}

static void w32_cap_close(goome *g)
{
    if (g->opts.mode == GOOME_MODE_WINDOW) return;
    for (int i = 0; i < w32.n; i++) w32_drop_cap(&w32.m[i]);
}

const gm_cap *gm_cap_win32(void) { return &w32_cap; }

#undef W32_LOAD

#endif
