// grab [monitor] [x y w h] out.ppm, coords are logical like everywhere in goome
#include <stdio.h>

#include "goome.h"

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: grab [monitor] [x y w h] out.ppm\n");
        return 2;
    }
    int nums[5], nn = 0;
    for (int i = 1; i < argc - 1 && nn < 5; i++) {
        const char *p = argv[i];
        int v = 0, neg = *p == '-';
        if (neg) p++;
        while (*p >= '0' && *p <= '9') v = v * 10 + *p++ - '0';
        nums[nn++] = neg ? -v : v;
    }
    int err;
    goome *g = goome_open(&(goome_opts){.title = "goome grab"}, &err);
    if (!g) {
        fprintf(stderr, "cant open: %s\n", goome_strerror(err));
        return 1;
    }
    int mon = nn == 1 || nn == 5 ? nums[0] : 0;
    goome_rect r, *rp = NULL;
    if (nn >= 4) r = (goome_rect){nums[nn - 4], nums[nn - 3], nums[nn - 2], nums[nn - 1]}, rp = &r;
    goome_image img;
    int e = GOOME_EAGAIN;
    for (int tries = 0; tries < 50 && e == GOOME_EAGAIN; tries++) { // stream backends need a moment for the first frame
        e = goome_capture(g, mon, rp, &img);
        if (e == GOOME_EAGAIN) {
            goome_event ev;
            goome_poll(g, &ev, 100);
        }
    }
    if (e != GOOME_OK) {
        fprintf(stderr, "grab failed: %s (%s)\n", goome_strerror(e), goome_error(g));
        goome_close(g);
        return 1;
    }
    e = goome_save_ppm(argv[argc - 1], img.px, img.w, img.h, img.stride);
    printf("%dx%d px of %d,%d %dx%d on monitor %d%s%s\n", img.w, img.h, img.area.x, img.area.y, img.area.w, img.area.h, mon,
           img.has_self ? ", has our overlay" : "", img.has_cursor ? ", has the cursor" : "");
    goome_close(g);
    return e == GOOME_OK ? 0 : 1;
}
