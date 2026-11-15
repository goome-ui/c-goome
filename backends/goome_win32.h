#ifndef GOOME_WIN32_H
#define GOOME_WIN32_H

#include "goome.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

int goome_win32_init(goome *g, HWND hwnd);
LRESULT goome_win32_wndproc(goome *g, HWND h, UINT msg, WPARAM w, LPARAM l, int *eaten);
void goome_win32_set_ui(goome *g, goome_ui *u);
void goome_win32_frame(goome *g);
void goome_win32_shutdown(goome *g);

#endif

#if defined(GOOME_IMPL_WIN32_IMPLEMENTATION) && !defined(GOOME_IMPL_WIN32_DONE)
#define GOOME_IMPL_WIN32_DONE

#include <stdlib.h>
#include <string.h>

typedef struct {
    goome *g;
    goome_ui *u;
    HWND win;
    int held, pw, ph, shape, captured, wheel;
    unsigned hi;
    double s, mx, my;
    char *clip;
    UINT(WINAPI *dpi_for)(HWND);
} gw32_state;

static gw32_state gw32;

static int gw32_mods(void)
{
    return (GetKeyState(VK_SHIFT) < 0 ? GOOME_SHIFT : 0) | (GetKeyState(VK_CONTROL) < 0 ? GOOME_CTRL : 0) | (GetKeyState(VK_MENU) < 0 ? GOOME_ALT : 0) |
           (GetKeyState(VK_LWIN) < 0 || GetKeyState(VK_RWIN) < 0 ? GOOME_SUPER : 0);
}

static void gw32_push(int type, int button, int dy, int key, int mods, const char *text)
{
    goome_event e;
    memset(&e, 0, sizeof(e));
    e.type = type, e.x = gw32.mx, e.y = gw32.my, e.button = button, e.dy = dy, e.key = key, e.mods = mods;
    if (text) memcpy(e.text, text, strlen(text) < 7 ? strlen(text) : 7);
    goome_push(gw32.g, &e);
}

static int gw32_over(void) { return gw32.held || goome_hit(gw32.g, gw32.mx, gw32.my); }

static int gw32_wants_keys(void) { return gw32.u && (goome_ui_wants(gw32.u) & GOOME_WANT_KEYBOARD); }

static double gw32_scale(void)
{
    UINT dpi = gw32.dpi_for ? gw32.dpi_for(gw32.win) : 0;
    if (!dpi) {
        HDC dc = GetDC(gw32.win);
        dpi = dc ? (UINT)GetDeviceCaps(dc, LOGPIXELSX) : 96;
        if (dc) ReleaseDC(gw32.win, dc);
    }
    return dpi >= 48 ? dpi / 96.0 : 1;
}

static void gw32_size(void)
{
    RECT r;
    if (!GetClientRect(gw32.win, &r)) return;
    int pw = r.right - r.left, ph = r.bottom - r.top;
    double s = gw32_scale();
    if (pw <= 0 || ph <= 0 || (pw == gw32.pw && ph == gw32.ph && s == gw32.s)) return;
    gw32.pw = pw, gw32.ph = ph, gw32.s = s;
    int lw = (int)(pw / s + 0.5), lh = (int)(ph / s + 0.5);
    goome_resize(gw32.g, lw > 0 ? lw : 1, lh > 0 ? lh : 1, s);
}

static HCURSOR gw32_cursor(int shape)
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

static void gw32_pull_clip(void)
{
    if (!OpenClipboard(gw32.win)) return;
    HANDLE d = IsClipboardFormatAvailable(CF_UNICODETEXT) ? GetClipboardData(CF_UNICODETEXT) : NULL;
    const WCHAR *ws = d ? (const WCHAR *)GlobalLock(d) : NULL;
    if (ws) {
        int n = WideCharToMultiByte(CP_UTF8, 0, ws, -1, NULL, 0, NULL, NULL);
        char *s = n > 0 ? (char *)malloc((size_t)n) : NULL;
        if (s && WideCharToMultiByte(CP_UTF8, 0, ws, -1, s, n, NULL, NULL) > 0) {
            int k = 0;
            for (int i = 0; s[i]; i++)
                if (!(s[i] == '\r' && s[i + 1] == '\n')) s[k++] = s[i];
            s[k] = 0;
            goome_clipboard_set(gw32.g, s);
            free(gw32.clip);
            gw32.clip = s;
        } else
            free(s);
        GlobalUnlock(d);
    }
    CloseClipboard();
}

static void gw32_push_clip(void)
{
    int n = goome_clipboard_get(gw32.g, NULL, 0);
    if (n < 0 || (n == 0 && !gw32.clip)) return;
    char *s = (char *)malloc((size_t)n + 1);
    if (!s) return;
    goome_clipboard_get(gw32.g, s, n + 1);
    if (gw32.clip && !strcmp(s, gw32.clip)) {
        free(s);
        return;
    }
    free(gw32.clip);
    gw32.clip = s;
    int wn = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    HGLOBAL mem = wn > 0 ? GlobalAlloc(GMEM_MOVEABLE, (size_t)wn * sizeof(WCHAR)) : NULL;
    if (!mem) return;
    WCHAR *w = (WCHAR *)GlobalLock(mem);
    if (w) MultiByteToWideChar(CP_UTF8, 0, s, -1, w, wn);
    GlobalUnlock(mem);
    if (!w || !OpenClipboard(gw32.win)) {
        GlobalFree(mem);
        return;
    }
    EmptyClipboard();
    if (!SetClipboardData(CF_UNICODETEXT, mem)) GlobalFree(mem);
    CloseClipboard();
}

static int gw32_navkey(WPARAM vk)
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

static void gw32_char(unsigned c)
{
    int mods = gw32_mods(), key = 0;
    char t[8] = {0};
    if ((mods & GOOME_CTRL) && (mods & GOOME_ALT) && c >= 32) mods &= ~(GOOME_CTRL | GOOME_ALT);
    if (c == 127) c = GOOME_K_BACKSPACE;
    if ((mods & GOOME_CTRL) && c >= 1 && c <= 26 && c != 8 && c != 9 && c != 13) key = 'a' + (int)c - 1;
    else if (c < 128) key = (int)c;
    if (key >= 'A' && key <= 'Z') key += 32, mods |= GOOME_SHIFT;
    if (key == 'v' && (mods & GOOME_CTRL)) gw32_pull_clip();
    if (c >= 32 && !(mods & (GOOME_CTRL | GOOME_ALT | GOOME_SUPER))) {
        if (c < 0x80) t[0] = (char)c;
        else if (c < 0x800) t[0] = (char)(0xC0 | c >> 6), t[1] = (char)(0x80 | (c & 63));
        else if (c < 0x10000) t[0] = (char)(0xE0 | c >> 12), t[1] = (char)(0x80 | ((c >> 6) & 63)), t[2] = (char)(0x80 | (c & 63));
        else t[0] = (char)(0xF0 | c >> 18), t[1] = (char)(0x80 | ((c >> 12) & 63)), t[2] = (char)(0x80 | ((c >> 6) & 63)), t[3] = (char)(0x80 | (c & 63));
    }
    if (key || t[0]) gw32_push(GOOME_KEY, 0, 0, key, mods, t);
}

static int gw32_button(UINT msg, LPARAM l)
{
    int b, down;
    switch (msg) {
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK: b = 1, down = 1; break;
    case WM_MBUTTONDOWN:
    case WM_MBUTTONDBLCLK: b = 2, down = 1; break;
    case WM_RBUTTONDOWN:
    case WM_RBUTTONDBLCLK: b = 3, down = 1; break;
    case WM_LBUTTONUP: b = 1, down = 0; break;
    case WM_MBUTTONUP: b = 2, down = 0; break;
    default: b = 3, down = 0; break;
    }
    gw32.mx = (short)LOWORD(l) / gw32.s, gw32.my = (short)HIWORD(l) / gw32.s;
    int bit = 1 << b, eaten;
    if (down) {
        eaten = gw32.held || goome_hit(gw32.g, gw32.mx, gw32.my);
        if (eaten) gw32.held |= bit;
        gw32_push(GOOME_DOWN, b, 0, 0, gw32_mods(), NULL);
        if (eaten && !GetCapture()) SetCapture(gw32.win), gw32.captured = 1;
    } else {
        eaten = (gw32.held & bit) != 0;
        gw32.held &= ~bit;
        gw32_push(GOOME_UP, b, 0, 0, gw32_mods(), NULL);
        if (!gw32.held && gw32.captured) gw32.captured = 0, ReleaseCapture();
    }
    return eaten;
}

LRESULT goome_win32_wndproc(goome *g, HWND h, UINT msg, WPARAM w, LPARAM l, int *eaten)
{
    int dummy;
    if (!eaten) eaten = &dummy;
    *eaten = 0;
    if (!g || g != gw32.g || h != gw32.win) return 0;
    switch (msg) {
    case WM_MOUSEMOVE: {
        double x = (short)LOWORD(l) / gw32.s, y = (short)HIWORD(l) / gw32.s;
        if (x != gw32.mx || y != gw32.my) gw32.mx = x, gw32.my = y, gw32_push(GOOME_MOTION, 0, 0, 0, gw32_mods(), NULL);
        *eaten = gw32_over();
        return 0;
    }
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONDBLCLK:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONDBLCLK:
    case WM_LBUTTONUP:
    case WM_MBUTTONUP:
    case WM_RBUTTONUP: *eaten = gw32_button(msg, l); return 0;
    case WM_MOUSEWHEEL: {
        POINT p = {(short)LOWORD(l), (short)HIWORD(l)};
        ScreenToClient(h, &p);
        gw32.mx = p.x / gw32.s, gw32.my = p.y / gw32.s;
        gw32.wheel += (short)HIWORD(w);
        int n = gw32.wheel / WHEEL_DELTA;
        gw32.wheel -= n * WHEEL_DELTA;
        if (n) gw32_push(GOOME_SCROLL, 0, -n, 0, gw32_mods(), NULL);
        *eaten = gw32_over();
        return 0;
    }
    case WM_CAPTURECHANGED:
        if ((HWND)l != h) gw32.captured = 0;
        return 0;
    case WM_SETCURSOR:
        if (LOWORD(l) != HTCLIENT || !gw32_over()) return 0;
        SetCursor(gw32_cursor(goome_cursor_shape(g)));
        *eaten = 1;
        return TRUE;
    case WM_CHAR:
    case WM_SYSCHAR: {
        if (msg == WM_SYSCHAR && w == ' ') return 0;
        *eaten = gw32_wants_keys();
        unsigned c = (unsigned)w;
        if (c >= 0xD800 && c < 0xDC00) {
            gw32.hi = c;
            return 0;
        }
        if (c >= 0xDC00 && c < 0xE000) {
            if (!gw32.hi) return 0;
            c = 0x10000 + ((gw32.hi - 0xD800) << 10) + (c - 0xDC00);
        }
        gw32.hi = 0;
        gw32_char(c);
        return 0;
    }
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN: {
        if (msg == WM_SYSKEYDOWN && w == VK_F4) return 0;
        int k = gw32_navkey(w);
        if (k) gw32_push(GOOME_KEY, 0, 0, k, gw32_mods(), NULL);
        *eaten = gw32_wants_keys();
        return 0;
    }
    case WM_KEYUP:
    case WM_SYSKEYUP: *eaten = gw32_wants_keys(); return 0;
    case WM_INPUT: {
        RAWINPUTHEADER hd;
        UINT n = sizeof(hd);
        if (GetRawInputData((HRAWINPUT)l, RID_HEADER, &hd, &n, sizeof(RAWINPUTHEADER)) != sizeof(hd)) return 0;
        *eaten = hd.dwType == RIM_TYPEMOUSE ? gw32_over() : hd.dwType == RIM_TYPEKEYBOARD ? gw32_wants_keys() : 0;
        return *eaten ? DefWindowProcW(h, msg, w, l) : 0;
    }
    case WM_SETFOCUS:
    case WM_KILLFOCUS: {
        goome_event e;
        memset(&e, 0, sizeof(e));
        e.type = GOOME_FOCUS, e.id = msg == WM_SETFOCUS;
        goome_push(g, &e);
        if (!e.id) gw32.held = 0;
        return 0;
    }
    case WM_SIZE:
    case 0x02E0: gw32_size(); return 0;
    }
    return 0;
}

int goome_win32_init(goome *g, HWND hwnd)
{
    if (!g || !hwnd) return GOOME_EARG;
    if (gw32.g) goome_win32_shutdown(gw32.g);
    memset(&gw32, 0, sizeof(gw32));
    gw32.g = g, gw32.win = hwnd, gw32.shape = -1, gw32.s = 1;
    HMODULE u = GetModuleHandleA("user32.dll");
    FARPROC f = u ? GetProcAddress(u, "GetDpiForWindow") : NULL;
    memcpy(&gw32.dpi_for, &f, sizeof(f));
    gw32_size();
    POINT p;
    if (GetCursorPos(&p) && ScreenToClient(hwnd, &p)) gw32.mx = p.x / gw32.s, gw32.my = p.y / gw32.s;
    return GOOME_OK;
}

void goome_win32_set_ui(goome *g, goome_ui *u)
{
    if (g && g == gw32.g) gw32.u = u;
}

void goome_win32_frame(goome *g)
{
    if (!g || g != gw32.g) return;
    gw32_size();
    gw32_push_clip();
    int shape = gw32_over() ? goome_cursor_shape(g) : -1;
    if (shape == gw32.shape) return;
    gw32.shape = shape;
    if (shape >= 0 && GetForegroundWindow() == gw32.win) SetCursor(gw32_cursor(shape));
}

void goome_win32_shutdown(goome *g)
{
    if (!g || g != gw32.g) return;
    if (gw32.captured) ReleaseCapture();
    free(gw32.clip);
    memset(&gw32, 0, sizeof(gw32));
}

#endif
