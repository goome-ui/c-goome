// capture only check of the mutter backend against a live gnome session, no overlay
// gnome_cap outdir name:x,y,w,h,scale [...]
// env GN_CAP=portal GN_STATE=dir GN_EXPECT=denied for the fake portal run
#include "../src/internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static int fails;

static void check(int ok, const char *what)
{
    printf("%s: %s\n", ok ? "pass" : "FAIL", what);
    if (!ok) fails++;
}

static int grab_wait(goome *g, int mon, const goome_rect *r, goome_image *img, int ms, int *waited)
{
    long t0 = goome_now_ms();
    int e;
    while ((e = goome_capture(g, mon, r, img)) == GOOME_EAGAIN && goome_now_ms() - t0 < ms) {
        struct timespec t = {0, 20000000L};
        nanosleep(&t, NULL);
    }
    if (waited) *waited = (int)(goome_now_ms() - t0);
    return e;
}

static int parse_mon(const char *s, goome_monitor *m, int id)
{
    memset(m, 0, sizeof(*m));
    const char *c = strchr(s, ':');
    if (!c) return 0;
    snprintf(m->name, sizeof(m->name), "%.*s", (int)(c - s), s);
    const char *p = c + 1;
    int v[5];
    for (int i = 0; i < 5; i++) {
        v[i] = gm_num(&p);
        if (*p == ',' || *p == '.') p++;
    }
    m->id = id;
    m->area = (goome_rect){v[0], v[1], v[2], v[3]};
    m->scale = v[4] > 0 ? v[4] : 1;
    m->pw = (int)(m->area.w * m->scale);
    m->ph = (int)(m->area.h * m->scale);
    return 1;
}

static int same(const goome_image *a, int ax, int ay, const goome_image *b, int w, int h)
{
    long diff = 0;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            if (a->px[(size_t)(ay + y) * a->stride + ax + x] != b->px[(size_t)y * b->stride + x]) diff++;
    return diff * 100 <= (long)w * h; // screen can change between two frames, allow 1%
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: gnome_cap outdir name:x,y,w,h,scale [...]\n");
        return 2;
    }
    const char *out = argv[1];
    goome *g = calloc(1, sizeof(*g));
    g->opts.capture_fps = 5;
    g->ctl_fd = -1;
    for (int i = 2; i < argc && g->nmon < GM_MAX_MON; i++)
        if (parse_mon(argv[i], &g->mon[g->nmon], g->nmon)) g->nmon++;
    const char *which = getenv("GN_CAP"), *state = getenv("GN_STATE"), *expect = getenv("GN_EXPECT");
    int portal = which && !strcmp(which, "portal");
    if (state) snprintf(g->state_dir, sizeof(g->state_dir), "%s", state);
    const gm_cap *cap = portal ? gm_cap_portal() : gm_cap_mutter();
    g->cap = cap;
    check(cap->probe(g) == 1, portal ? "probe sees the screencast portal" : "probe sees org.gnome.Mutter.ScreenCast");
    if (expect && !strcmp(expect, "denied")) {
        goome_image im;
        int e = goome_capture(g, 0, NULL, &im);
        printf("error: %s\n", goome_error(g));
        check(e == GOOME_EDENIED, "a no in the portal gives GOOME_EDENIED");
        check(goome_capture(g, 0, NULL, &im) == GOOME_EDENIED, "and stays denied");
        free(g);
        return fails ? 1 : 0;
    }

    long t0 = goome_now_ms();
    goome_image img, full, reg;
    int waited;
    int e = grab_wait(g, 0, NULL, &full, 5000, &waited);
    printf("open + first frame took %ld ms (grab waited %d ms)\n", goome_now_ms() - t0, waited);
    check(e == GOOME_OK, e == GOOME_OK ? "full grab of monitor 0" : goome_error(g));
    if (e) return 1;
    goome_monitor *m = &g->mon[0];
    printf("monitor 0 %s area %d,%d %dx%d -> %dx%d px stride %d has_self %d has_cursor %d\n", m->name, m->area.x, m->area.y, m->area.w,
           m->area.h, full.w, full.h, full.stride, full.has_self, full.has_cursor);
    check(full.w == m->pw && full.h == m->ph, "full grab is the monitor size in real px");
    check(full.has_cursor == 0 && full.has_self == 1, "has_self 1, no cursor");
    int opaque = 1;
    long sum = 0;
    for (int i = 0; i < full.w * full.h; i++) {
        if ((full.px[i] >> 24) != 0xFF) opaque = 0;
        sum += full.px[i] & 0xFF;
    }
    check(opaque, "every pixel is 0xFF......");
    check(sum > 0, "frame is not all black");
    char path[1200];
    snprintf(path, sizeof(path), "%s/full.ppm", out);
    goome_save_ppm(path, full.px, full.w, full.h, full.stride);
    uint32_t *keep = malloc((size_t)full.w * full.h * 4);
    memcpy(keep, full.px, (size_t)full.w * full.h * 4);
    full.px = keep;

    goome_rect r = {m->area.x + 80, m->area.y + 80, 280, 120};
    e = grab_wait(g, 0, &r, &reg, 2000, NULL);
    check(e == GOOME_OK && reg.w == (int)(280 * m->scale) && reg.h == (int)(120 * m->scale) && reg.area.x == r.x,
          "region grab 80,80 280x120 has the right size");
    if (!e) {
        check(same(&full, (int)(80 * m->scale), (int)(80 * m->scale), &reg, reg.w, reg.h), "region grab matches the same spot of the full grab");
        snprintf(path, sizeof(path), "%s/region.ppm", out);
        goome_save_ppm(path, reg.px, reg.w, reg.h, reg.stride);
    }

    // xwayland with native scaling hands us areas in a 2x space, the crop has to follow
    goome_monitor saved = *m;
    m->area = (goome_rect){saved.area.x * 2, saved.area.y * 2, saved.area.w * 2, saved.area.h * 2};
    goome_rect r2 = {m->area.x + 160, m->area.y + 160, 560, 240};
    e = grab_wait(g, 0, &r2, &reg, 2000, NULL);
    check(e == GOOME_OK && reg.w == (int)(280 * saved.scale) && reg.h == (int)(120 * saved.scale), "2x overlay space maps to the same stream");
    if (!e) check(same(&full, (int)(80 * saved.scale), (int)(80 * saved.scale), &reg, reg.w, reg.h), "2x space crop hits the same pixels");
    m->area = (goome_rect){saved.area.x + 50000, 0, saved.area.w, saved.area.h};
    e = goome_capture(g, 0, NULL, &img);
    check(e == GOOME_EGONE, "a monitor no stream covers gives GOOME_EGONE");
    *m = saved;

    // GOOME_GNOME_RESTART_AT fakes a monitor change on that grab, grabs say EAGAIN for a bit then frames come back
    int again = 0;
    long rs = goome_now_ms(), back = -1, longest = 0;
    for (int i = 0; i < 400 && back < 0; i++) {
        long c0 = goome_now_ms();
        e = goome_capture(g, 0, &r, &reg);
        if (goome_now_ms() - c0 > longest) longest = goome_now_ms() - c0;
        if (e == GOOME_EAGAIN) again++;
        else if (e == GOOME_OK && again) back = goome_now_ms() - rs;
        else if (e != GOOME_OK) break;
        struct timespec t = {0, 20000000L};
        nanosleep(&t, NULL);
        if (!again && goome_now_ms() - rs > 3000) break;
    }
    printf("restart: %d EAGAIN grabs, frames back after %ld ms, slowest grab %ld ms\n", again, back, longest);
    check(again > 0 && back > 0, "restart path (GOOME_GNOME_RESTART_AT) goes EAGAIN then recovers");
    check(longest < 300, "no grab blocks long while it restarts");

    cap->close(g);
    g->cap_state = 0;
    e = grab_wait(g, 0, NULL, &img, 5000, NULL);
    check(e == GOOME_OK && img.w == full.w, "close then open again works");
    cap->close(g);
    free(keep);
    free(g);
    printf("%s\n", fails ? "SOME FAILED" : "all passed");
    return fails ? 1 : 0;
}
