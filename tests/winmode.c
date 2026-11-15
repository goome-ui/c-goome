#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "goome.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

static int fails;

static void say(const char *fmt, const char *a, int ok)
{
    printf(ok ? "ok " : "bad ");
    printf(fmt, a);
    printf("\n");
    fflush(stdout);
    if (!ok) fails++;
}

static int gp(goome *g, goome_event *ev, int ms)
{
    int r = goome_poll(g, ev, ms);
    if (r > 0 && ev->type == GOOME_FOCUS) printf("focus %d\n", ev->id), fflush(stdout);
    if (r > 0 && ev->type == GOOME_MONITORS) {
        goome_monitor m;
        goome_monitors(g, &m, 1);
        printf("monitors %dx%d scale %.2f\n", m.area.w, m.area.h, m.scale);
        fflush(stdout);
    }
    return r;
}

static uint32_t pm(uint32_t c)
{
    uint32_t a = c >> 24;
    uint32_t r = ((c >> 16) & 255) * a / 255, g = ((c >> 8) & 255) * a / 255, b = (c & 255) * a / 255;
    return a << 24 | r << 16 | g << 8 | b;
}

static void draw(goome *g, const char *kind, int n)
{
    goome_canvas *c = goome_begin(g, 0);
    if (!c) return;
    goome_monitor m;
    goome_monitors(g, &m, 1);
    double w = m.area.w, h = m.area.h;
    goome_rect hit = {40, 40, 120, 80};
    if (!strcmp(kind, "trans")) {
        goome_draw_rect(c, 40, 40, 120, 80, 0xFFE04060);
        goome_draw_rect(c, 200, 40, 120, 80, 0x80000000);
        goome_draw_frame(c, 0, 0, w, h, 2, 0xFF40C0FF);
        goome_end(g, c, &hit, 1);
        return;
    }
    goome_draw_rect(c, 0, 0, w, h, kind[0] == 'b' ? 0xFF305020 : 0xFF203040);
    goome_draw_rect(c, 40, 40, 120, 80, 0xFFE04060);
    goome_draw_rect(c, w - 30, h - 30, 30, 30, 0xFF80FF80);
    goome_draw_rect(c, 0, 0, 120, 30, 0xFF606870);
    char line[64];
    snprintf(line, sizeof(line), "goome window %.0fx%.0f #%d", w, h, n);
    goome_draw_text(c, 200, 60, line, 0, 0xFFFFFFFF);
    goome_end(g, c, &hit, 1);
}

static int wait_for(goome *g, int type, int ms, goome_event *out)
{
    long end = goome_now_ms() + ms;
    goome_event ev;
    while (goome_now_ms() < end)
        if (gp(g, &ev, 20) > 0 && ev.type == type) {
            if (out) *out = ev;
            return 1;
        }
    return 0;
}

static int wait_state(goome *g, int st, int ms)
{
    long end = goome_now_ms() + ms;
    goome_event ev;
    while (goome_now_ms() < end) {
        if (goome_window_get_state(g) == st) return 1;
        gp(g, &ev, 20);
    }
    return goome_window_get_state(g) == st;
}

static void pump(goome *g, int ms)
{
    long end = goome_now_ms() + ms;
    goome_event ev;
    while (goome_now_ms() < end) gp(g, &ev, 10);
}

static void checks(goome *g, const char *kind)
{
    char buf[128], num[64];
    goome_monitor m;
    goome_monitors(g, &m, 1);
    double s = m.scale;

    goome_image f;
    int fe = goome_frame(g, &f);
    uint32_t want = pm(0xFFE04060), got = 0;
    if (fe == GOOME_OK) got = f.px[(size_t)(int)(60 * s) * f.stride + (int)(60 * s)];
    snprintf(num, sizeof(num), "%08x want %08x", got, want);
    say("frame %s", num, fe == GOOME_OK && got == want);

    const char *text = "goome clip \xc3\xa9 1\nline 2";
    int e = goome_clipboard_set(g, text);
    int n = goome_clipboard_get(g, buf, sizeof(buf));
    say("clipboard '%s'", buf, e == GOOME_OK && n == (int)strlen(text) && !strcmp(buf, text));
    n = goome_clipboard_get(g, buf, 5);
    say("clipboard cut '%s'", buf, n == (int)strlen(text) && !strcmp(buf, "goom"));

    e = goome_cursor(g, GOOME_CURSOR_IBEAM);
    say("cursor %s", goome_strerror(e), e == GOOME_OK && goome_cursor_shape(g) == GOOME_CURSOR_IBEAM);

    say("title %s", goome_strerror(goome_window_title(g, "goome winmode")), 1);

    e = goome_window_size(g, 700, 460);
    int ok = e == GOOME_OK && wait_for(g, GOOME_MONITORS, 3000, NULL);
    goome_monitors(g, &m, 1);
    snprintf(num, sizeof(num), "%dx%d px %dx%d", m.area.w, m.area.h, m.pw, m.ph);
    say("resize %s", num, ok && m.area.w == 700 && m.area.h == 460);
    goome_canvas *c = goome_begin(g, 0);
    snprintf(num, sizeof(num), "%dx%d", c ? c->w : 0, c ? c->h : 0);
    say("canvas follows %s", num, c && c->w == m.pw && c->h == m.ph);
    if (c) goome_end(g, c, NULL, 0);
    draw(g, kind, 0);

    int x = 0, y = 0, x2 = 0, y2 = 0;
    e = goome_window_pos(g, &x, &y, 0);
    x2 = x + 30, y2 = y + 20;
    goome_window_pos(g, &x2, &y2, 1);
    pump(g, 400);
    int x3 = 0, y3 = 0;
    goome_window_pos(g, &x3, &y3, 0);
    snprintf(num, sizeof(num), "%d,%d -> %d,%d", x, y, x3, y3);
    say("pos %s", num, e == GOOME_OK && x3 == x + 30 && y3 == y + 20);
    goome_window_pos(g, &x, &y, 1);
    pump(g, 300);

    goome_window_state(g, GOOME_STATE_MINIMIZED);
    say("minimize %s", "", wait_state(g, GOOME_STATE_MINIMIZED, 3000));
    goome_window_state(g, GOOME_STATE_NORMAL);
    say("restore %s", "", wait_state(g, GOOME_STATE_NORMAL, 3000));
    goome_window_state(g, GOOME_STATE_MAXIMIZED);
    ok = wait_state(g, GOOME_STATE_MAXIMIZED, 3000);
    pump(g, 500);
    goome_monitors(g, &m, 1);
    snprintf(num, sizeof(num), "%dx%d", m.area.w, m.area.h);
    say("maximize %s", num, ok && m.area.w > 700);
    goome_window_state(g, GOOME_STATE_NORMAL);
    ok = wait_state(g, GOOME_STATE_NORMAL, 3000);
    pump(g, 500);
    goome_monitors(g, &m, 1);
    snprintf(num, sizeof(num), "%dx%d", m.area.w, m.area.h);
    say("back to normal %s", num, ok && m.area.w == 700 && m.area.h == 460);
    draw(g, kind, 0);
    pump(g, 300);
    draw(g, kind, 0);
    pump(g, 300);

    goome_image img;
    e = goome_capture(g, 0, NULL, &img);
    if (e == GOOME_OK) {
        uint32_t p = img.px[(size_t)(int)(60 * s) * img.stride + (int)(60 * s)] & 0xFFFFFF;
        snprintf(num, sizeof(num), "%dx%d center %06x", img.w, img.h, p);
        int d = 0;
        for (int k = 0; k < 24; k += 8) {
            int x = (int)((p >> k) & 255) - (int)((0xE04060u >> k) & 255);
            if (x < 0) x = -x;
            if (x > d) d = x;
        }
        say("capture %s", num, !strcmp(kind, "trans") || d <= 8);
    } else
        printf("info capture %s: %s\n", goome_strerror(e), goome_error(g));
    e = goome_hotkey(g, "alt+shift+f9", 9);
    printf("info hotkey %s\n", goome_strerror(e));
    fflush(stdout);
}

#ifdef _WIN32
typedef struct {
    DWORD pFiles;
    POINT pt;
    BOOL fNC, fWide;
} drop_files;

static void self_drop(void)
{
    HWND h = FindWindowA(NULL, "goome winmode");
    const WCHAR *path = L"C:\\goome\\dropped \x00e9 file.txt";
    size_t bytes = sizeof(drop_files) + (wcslen(path) + 2) * sizeof(WCHAR);
    HGLOBAL mem = GlobalAlloc(GHND, bytes);
    drop_files *d = mem ? GlobalLock(mem) : NULL;
    if (!h || !d) {
        printf("bad drop setup\n");
        return;
    }
    d->pFiles = sizeof(drop_files), d->pt.x = 50, d->pt.y = 60, d->fWide = TRUE;
    memcpy((char *)d + sizeof(drop_files), path, wcslen(path) * sizeof(WCHAR));
    GlobalUnlock(mem);
    PostMessageW(h, WM_DROPFILES, (WPARAM)mem, 0);
}
#endif

int main(int argc, char **argv)
{
    const char *kind = argc > 1 ? argv[1] : "plain";
    int secs = 0;
    for (const char *p = argc > 2 ? argv[2] : "30"; *p >= '0' && *p <= '9'; p++) secs = secs * 10 + *p - '0';
    goome_opts o;
    memset(&o, 0, sizeof(o));
    o.title = "goome winmode", o.mode = GOOME_MODE_WINDOW, o.width = 640, o.height = 400, o.min_width = 200, o.min_height = 150;
    o.window_flags = GOOME_WINDOW_RESIZABLE | GOOME_WINDOW_CENTER;
    if (!strcmp(kind, "trans")) o.window_flags |= GOOME_WINDOW_TRANSPARENT | GOOME_WINDOW_ON_TOP;
    if (!strcmp(kind, "borderless")) o.window_flags |= GOOME_WINDOW_BORDERLESS;
    int err;
    goome *g = goome_open(&o, &err);
    if (!g) {
        printf("bad open %s\n", goome_strerror(err));
        return 1;
    }
    goome_monitor m;
    goome_monitors(g, &m, 16);
    printf("backend: %s mode %d caps %x\n", goome_backend_name(g), goome_mode(g), goome_caps(g));
    printf("window %dx%d scale %.2f px %dx%d name '%s'\n", m.area.w, m.area.h, m.scale, m.pw, m.ph, m.name);
    say("open %s", "640x400", goome_mode(g) == GOOME_MODE_WINDOW && m.area.w == 640 && m.area.h == 400 && (goome_caps(g) & GOOME_CAP_WINDOW));
    draw(g, kind, 0);
    pump(g, 600);
    draw(g, kind, 0);
    if (strcmp(kind, "trans")) checks(g, kind);
    else {
        goome_window_size(g, 700, 460);
        wait_for(g, GOOME_MONITORS, 3000, NULL);
        draw(g, kind, 0);
    }

    goome *g2 = NULL;
    if (!strcmp(kind, "two")) {
        o.title = "goome winmode b", o.width = 320, o.height = 200, o.window_flags = GOOME_WINDOW_RESIZABLE;
        g2 = goome_open(&o, &err);
        goome_canvas *c = g2 ? goome_begin(g2, 0) : NULL;
        if (c) {
            goome_draw_rect(c, 0, 0, 320, 200, 0xFF20A040);
            goome_end(g2, c, NULL, 0);
        }
        goome_image a, b;
        int ok = g2 && goome_frame(g, &a) == GOOME_OK && goome_frame(g2, &b) == GOOME_OK && a.px != b.px && (b.px[0] & 0xFFFFFF) == 0x20A040 &&
                 (a.px[(size_t)(a.h / 2) * a.stride + a.w / 2] & 0xFFFFFF) == 0x203040;
        say("two windows %s", g2 ? goome_backend_name(g2) : goome_strerror(err), ok);
    }

    int x = 0, y = 0;
    goome_window_pos(g, &x, &y, 0);
    goome_monitors(g, &m, 1);
    printf("ready client %d,%d size %dx%d scale %.2f\n", x, y, m.area.w, m.area.h, m.scale);
    fflush(stdout);
#ifdef _WIN32
    if (!strcmp(kind, "plain")) self_drop();
#endif

    char clip[128] = "";
    goome_clipboard_get(g, clip, sizeof(clip));
    int closes = 0, frame = 0, running = 1;
    long end = goome_now_ms() + secs * 1000L, next_clip = 0;
    while (running && goome_now_ms() < end) {
        goome_event ev;
        while (goome_poll(g, &ev, 15) > 0) {
            switch (ev.type) {
            case GOOME_DOWN:
                printf("down %.0f,%.0f %d\n", ev.x, ev.y, ev.button);
                goome_monitors(g, &m, 1);
                if (ev.button == 1 && ev.x < 120 && ev.y < 30) {
                    int e = goome_window_drag(g, 0);
                    goome_window_pos(g, &x, &y, 0);
                    printf("dragged %s pos %d,%d\n", goome_strerror(e), x, y);
                } else if (ev.button == 1 && ev.x >= m.area.w - 30 && ev.y >= m.area.h - 30) {
                    int e = goome_window_drag(g, GOOME_EDGE_RIGHT | GOOME_EDGE_BOTTOM);
                    pump(g, 200);
                    goome_monitors(g, &m, 1);
                    printf("sized %s %dx%d\n", goome_strerror(e), m.area.w, m.area.h);
                }
                break;
            case GOOME_UP: printf("up %.0f,%.0f %d\n", ev.x, ev.y, ev.button); break;
            case GOOME_KEY: printf("key %d mods %x text '%s'\n", ev.key, ev.mods, ev.text); break;
            case GOOME_FOCUS: printf("focus %d\n", ev.id); break;
            case GOOME_HOTKEY: printf("hotkey %d\n", ev.id); break;
            case GOOME_SCROLL: printf("scroll %d\n", ev.dy); break;
            case GOOME_MONITORS:
                goome_monitors(g, &m, 1);
                printf("monitors %dx%d scale %.2f\n", m.area.w, m.area.h, m.scale);
                break;
            case GOOME_DROP: {
                const char *full = goome_drop_path(g);
                printf("drop %.0f,%.0f '%s' full '%s'\n", ev.x, ev.y, ev.msg, full ? full : "");
                break;
            }
            case GOOME_CLOSE:
                printf("close %d\n", ++closes);
                if (closes >= 2) running = 0;
                break;
            case GOOME_QUIT: running = 0; break;
            }
            fflush(stdout);
        }
        if (goome_now_ms() >= next_clip) {
            char now[128];
            next_clip = goome_now_ms() + 300;
            if (goome_clipboard_get(g, now, sizeof(now)) >= 0 && strcmp(now, clip)) {
                snprintf(clip, sizeof(clip), "%s", now);
                printf("paste '%s'\n", clip);
                fflush(stdout);
            }
        }
        if (g2) {
            goome_event e2;
            while (goome_poll(g2, &e2, 0) > 0)
                if (e2.type == GOOME_DOWN) printf("b down %.0f,%.0f\n", e2.x, e2.y), fflush(stdout);
        }
        draw(g, kind, ++frame / 30);
    }
    printf("bye fails %d\n", fails);
    fflush(stdout);
    if (g2) goome_close(g2);
    goome_close(g);
    return fails != 0;
}
