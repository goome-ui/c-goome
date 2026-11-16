#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "goome.h"

static goome *g;

static void pump(int n)
{
    for (int i = 0; i < n; i++) {
        goome_event ev;
        while (goome_poll(g, &ev, 20) > 0) {
            if (ev.type == GOOME_TEXT) printf("text %s\n", goome_event_text(g));
            if (ev.type == GOOME_PREEDIT) printf("preedit '%s' %d\n", goome_event_text(g), goome_preedit_cursor(g));
            if (ev.type == GOOME_KEY && ev.text[0]) printf("key %s\n", ev.text);
        }
        goome_canvas *c = goome_begin(g, 0);
        if (c) {
            goome_clear(c);
            goome_end(g, c, NULL, 0);
        }
    }
}

int main(void)
{
    int err;
    setvbuf(stdout, NULL, _IONBF, 0);
    g = goome_open_app("goome ime", 320, 160, &err);
    if (!g) {
        printf("open failed: %s\n", goome_strerror(err));
        return 1;
    }
    pump(5);
    printf("ime rc %d\n", goome_text_input(g, 1, &(goome_rect){20, 40, 1, 18}));
    HMODULE imm = LoadLibraryA("imm32.dll");
    void *(WINAPI *get)(HWND) = NULL;
    BOOL(WINAPI *release)(HWND, void *) = NULL;
    BOOL(WINAPI *set_str)(void *, DWORD, const void *, DWORD, const void *, DWORD) = NULL;
    BOOL(WINAPI *notify)(void *, DWORD, DWORD, DWORD) = NULL;
    BOOL(WINAPI *set_open)(void *, BOOL) = NULL;
    FARPROC p;
    if (imm) {
        p = GetProcAddress(imm, "ImmGetContext"), memcpy(&get, &p, sizeof(p));
        p = GetProcAddress(imm, "ImmReleaseContext"), memcpy(&release, &p, sizeof(p));
        p = GetProcAddress(imm, "ImmSetCompositionStringW"), memcpy(&set_str, &p, sizeof(p));
        p = GetProcAddress(imm, "ImmNotifyIME"), memcpy(&notify, &p, sizeof(p));
        p = GetProcAddress(imm, "ImmSetOpenStatus"), memcpy(&set_open, &p, sizeof(p));
    }
    HWND h = FindWindowA(NULL, "goome ime");
    void *c = get && h ? get(h) : NULL;
    printf("context %s\n", c ? "yes" : "no");
    if (c) {
        static const WCHAR s[] = {0x306B, 0x307B, 0x3093, 0};
        if (set_open) set_open(c, TRUE);
        printf("set %d\n", set_str(c, 9, s, sizeof(s) - sizeof(WCHAR), NULL, 0));
        pump(5);
        printf("done %d\n", notify(c, 0x15, 1, 0));
        pump(5);
        release(h, c);
    }
    printf("ime off %d\n", goome_text_input(g, 0, NULL));
    pump(2);
    goome_close(g);
    printf("bye\n");
    return 0;
}
