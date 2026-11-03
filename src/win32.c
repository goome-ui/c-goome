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
    HRESULT(WINAPI *dpi_for_mon)(HMONITOR, int, UINT *, UINT *);
    int aware, sysdpi, build;
    ATOM cls;
    w32_mon m[GM_MAX_MON];
    int n;
    HWND hot, before_kb;
    int fwd, ours;
    double lx, ly;
    int wheel;
    unsigned hi;
    int recheck;
    long next_poll;
    gm_hotkey hk[GM_MAX_HK];
    int nreg;
    int hidden_ok;
} w32;

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

static int w32_load_gdi(void)
{
    if (w32.gdi_dll) return 1;
    HMODULE m = LoadLibraryA("gdi32.dll");
    if (!m) return 0;
    char *slot = (char *)&w32.gdi;
    for (size_t i = 0; i < sizeof(w32_gdi_names) / sizeof(w32_gdi_names[0]); i++) {
        void *p = w32_sym(m, w32_gdi_names[i]);
        if (!p) {
            FreeLibrary(m);
            memset(&w32.gdi, 0, sizeof(w32.gdi));
            return 0;
        }
        memcpy(slot + i * sizeof(p), &p, sizeof(p));
    }
    w32.gdi_dll = m;
    return 1;
}

static void w32_dpi_aware(void)
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
    W32_LOAD(shc_set, w32.shcore, "SetProcessDpiAwareness");
    W32_LOAD(shc_get, w32.shcore, "GetProcessDpiAwareness");
    // -4 per monitor v2, -3 v1
    if (!(ctx && (ctx((HANDLE)(INT_PTR)-4) || ctx((HANDLE)(INT_PTR)-3))) && !(shc_set && shc_set(2) == S_OK)) SetProcessDPIAware();
    int a = -1;
    if (thread_ctx && ctx_awareness) a = ctx_awareness(thread_ctx());
    if (a < 0 && shc_get && shc_get(NULL, &a) != S_OK) a = -1;
    if (a < 0) a = IsProcessDPIAware() ? 1 : 0;
    w32.aware = a;
    UINT d = sys_dpi ? sys_dpi() : 0;
    if (!d && w32.gdi.caps) {
        HDC dc = GetDC(NULL);
        d = (UINT)w32.gdi.caps(dc, LOGPIXELSX);
        ReleaseDC(NULL, dc);
    }
    w32.sysdpi = d >= 48 ? (int)d : 96;
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
    int dpi = w32.aware == 0 ? 96 : w32.sysdpi;
    if (w32.aware == 2 && w32.dpi_for_mon && w32.dpi_for_mon(hm, 0, &dx, &dy) == S_OK && dx >= 48) dpi = (int)dx; // 0 = effective dpi
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

static HBITMAP w32_dib(int w, int h, uint32_t **bits)
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
    HBITMAP b = w32.gdi.dib(screen, &bi, DIB_RGB_COLORS, &p, NULL, 0);
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

static void w32_char(unsigned c)
{
    goome *g = w32.g;
    if (!g->kb_on) return;
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
    if (!w32.ours) ReleaseCapture();
    w32_ev(GOOME_UP, (int)(m - w32.m), m->r.left + (short)LOWORD(lp) / m->s, m->r.top + (short)HIWORD(lp) / m->s, button);
}

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
        if ((x != w32.lx || y != w32.ly) && w32_on_hit(m, x, y)) w32.lx = x, w32.ly = y, w32_ev(GOOME_MOTION, (int)(m - w32.m), x, y, 0);
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
        if (c >= 0xD800 && c < 0xDC00) {
            w32.hi = c;
            return 0;
        }
        if (c >= 0xDC00 && c < 0xE000) {
            if (!w32.hi) return 0;
            c = 0x10000 + ((w32.hi - 0xD800) << 10) + (c - 0xDC00);
        }
        w32.hi = 0;
        w32_char(c);
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
    case WM_MOUSEACTIVATE: return w32.g->kb_on ? MA_ACTIVATE : MA_NOACTIVATE;
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
    m->bmp = m->dc ? w32_dib(w, h, &m->px) : NULL;
    if (!m->win || !m->bmp) {
        w32_drop_win(m);
        return;
    }
    w32.gdi.select(m->dc, m->bmp);
    memset(m->px, 0, (size_t)w * h * 4);
    gm_canvas_reset(&m->cv, m->px, w, h, w, m->r.left, m->r.top, m->s, i);
    if (g->kb_on) {
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

static int w32_open(goome *g)
{
    memset(&w32, 0, sizeof(w32));
    w32.g = g;
    if (!w32_load_gdi()) return gm_fail(g, GOOME_ENODISPLAY, "cant load gdi32.dll");
    HMODULE u = GetModuleHandleA("user32.dll");
    w32.shcore = LoadLibraryA("shcore.dll");
    w32.ntdll = GetModuleHandleA("ntdll.dll");
    W32_LOAD(w32.ulw_indirect, u, "UpdateLayeredWindowIndirect");
    W32_LOAD(w32.set_affinity, u, "SetWindowDisplayAffinity");
    W32_LOAD(w32.get_affinity, u, "GetWindowDisplayAffinity");
    W32_LOAD(w32.dpi_for_mon, w32.shcore, "GetDpiForMonitor");
    LONG(WINAPI * ver)(OSVERSIONINFOW *) = NULL;
    W32_LOAD(ver, w32.ntdll, "RtlGetVersion"); // GetVersionEx lies without a manifest
    OSVERSIONINFOW vi;
    memset(&vi, 0, sizeof(vi));
    vi.dwOSVersionInfoSize = sizeof(vi);
    if (ver && ver(&vi) == 0) w32.build = (int)vi.dwBuildNumber;
    void(WINAPI * noghost)(void) = NULL;
    W32_LOAD(noghost, u, "DisableProcessWindowsGhosting");
    if (noghost) noghost(); // or windows swaps in a frozen ghost copy of us when a frame is slow
    w32_dpi_aware();

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
    if (w32.ours && w32_mon_of(GetCapture())) return;
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

static int w32_keyboard(goome *g, int on)
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
        HWND t = w32.hot;
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

static void w32_close(goome *g)
{
    for (int i = 0; i < w32.nreg; i++) UnregisterHotKey(NULL, i + 1);
    if (GetCapture() && w32_mon_of(GetCapture())) ReleaseCapture();
    for (int i = 0; i < w32.n; i++) w32_drop_cap(&w32.m[i]), w32_drop_win(&w32.m[i]);
    if (w32.cls) UnregisterClassW((LPCWSTR)(UINT_PTR)w32.cls, GetModuleHandleW(NULL));
    if (w32.gdi_dll) FreeLibrary(w32.gdi_dll);
    if (w32.shcore) FreeLibrary(w32.shcore);
    memset(&w32, 0, sizeof(w32));
}

static gm_ov w32_ov = {"win32", w32_open, w32_canvas, w32_commit, w32_wait, w32_pointer, w32_keyboard, w32_hotkeys, w32_close,
                       GOOME_CAP_GLOBAL_POINTER | GOOME_CAP_KEYBOARD | GOOME_CAP_HOTKEYS};
const gm_ov *gm_ov_win32(void) { return &w32_ov; }

static int w32_cap_open(goome *g)
{
    if (!w32.gdi_dll) return gm_fail(g, GOOME_ENOTSUP, "win32 capture needs the win32 overlay open");
    return GOOME_OK;
}

static int w32_grab(goome *g, int mon, goome_rect r, goome_image *img)
{
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
        m->cbmp = m->cdc ? w32_dib(pw, ph, &m->cpx) : NULL;
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
    for (int i = 0; i < w32.n; i++) w32_drop_cap(&w32.m[i]);
}

const gm_cap *gm_cap_win32(void) { return &w32_cap; }

#undef W32_LOAD

#endif
