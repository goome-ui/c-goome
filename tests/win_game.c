#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>

static int nraw;

static LRESULT CALLBACK game_proc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_INPUT: {
        RAWINPUT ri;
        UINT n = sizeof(ri);
        if (GetRawInputData((HRAWINPUT)lp, RID_INPUT, &ri, &n, sizeof(RAWINPUTHEADER)) != (UINT)-1 && ri.header.dwType == RIM_TYPEMOUSE &&
            (ri.data.mouse.lLastX || ri.data.mouse.lLastY))
            printf("raw %d\n", ++nraw);
        break;
    }
    case WM_MOUSEMOVE: printf("move %d,%d\n", (short)LOWORD(lp), (short)HIWORD(lp)); break;
    case WM_SETFOCUS: printf("focus in\n"); break;
    case WM_KILLFOCUS: printf("focus out\n"); break;
    case WM_CLOSE: PostQuitMessage(0); return 0;
    }
    fflush(stdout);
    return DefWindowProcW(w, msg, wp, lp);
}

int main(int argc, char **argv)
{
    int v[5] = {0, 0, 640, 480, 20};
    for (int i = 1; i < argc && i <= 5; i++) {
        int n = 0;
        for (const char *p = argv[i]; *p >= '0' && *p <= '9'; p++) n = n * 10 + *p - '0';
        v[i - 1] = n;
    }
    SetProcessDPIAware();
    WNDCLASSW wc;
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = game_proc, wc.hInstance = GetModuleHandleW(NULL), wc.lpszClassName = L"goome_game";
    wc.hCursor = LoadCursorW(NULL, MAKEINTRESOURCEW(32512)), wc.hbrBackground = (HBRUSH)(COLOR_HIGHLIGHT + 1);
    RegisterClassW(&wc);
    HWND w = CreateWindowExW(WS_EX_TOPMOST, L"goome_game", L"game", WS_POPUP | WS_VISIBLE, v[0], v[1], v[2], v[3], NULL, NULL, wc.hInstance, NULL);
    RAWINPUTDEVICE rid = {1, 2, 0, w};
    printf("game %s\n", RegisterRawInputDevices(&rid, 1, sizeof(rid)) ? "raw" : "noraw");
    SetForegroundWindow(w);
    SetFocus(w);
    fflush(stdout);
    DWORD end = GetTickCount() + (DWORD)v[4] * 1000;
    MSG m;
    while (GetTickCount() < end) {
        while (PeekMessageW(&m, NULL, 0, 0, PM_REMOVE)) {
            if (m.message == WM_QUIT) return 0;
            TranslateMessage(&m);
            DispatchMessageW(&m);
        }
        MsgWaitForMultipleObjects(0, NULL, FALSE, 20, QS_ALLINPUT);
    }
    return 0;
}
