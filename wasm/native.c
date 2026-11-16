#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "goome.h"
#include "gw.h"

static char *nv_read(const char *path, long *n)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    *n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *b = malloc((size_t)*n + 1);
    if (b && fread(b, 1, (size_t)*n, f) != (size_t)*n) free(b), b = NULL;
    fclose(f);
    if (b) b[*n] = 0;
    return b;
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: gw-native prog.json script.ev\n");
        return 2;
    }
    long n, en;
    char *prog = nv_read(argv[1], &n), *ev = nv_read(argv[2], &en);
    if (!prog || !ev) {
        fprintf(stderr, "cant read %s or %s\n", argv[1], argv[2]);
        return 2;
    }
    int w = 0, h = 0, frames = 0;
    double scale = 1;
    char *line = strtok(ev, "\n");
    if (!line || sscanf(line, "%d %d %lf %d", &w, &h, &scale, &frames) != 4) {
        fprintf(stderr, "bad script header\n");
        return 2;
    }
    if (gw_init(w, h, scale) || gw_load(prog, (int)n)) {
        fprintf(stderr, "load failed: %s\n", gw_error());
        return 1;
    }
    char *rest = strtok(NULL, "");
    for (int f = 0; f < frames; f++) {
        char *p = rest;
        while (p && *p) {
            char *nl = strchr(p, '\n');
            int fr, type, button, dy, key, mods, used = 0;
            double x, y;
            char text[16] = "";
            if (sscanf(p, "%d %d %lf %lf %d %d %d %d%n", &fr, &type, &x, &y, &button, &dy, &key, &mods, &used) == 8 && fr == f) {
                const char *t = p + used;
                while (*t == ' ') t++;
                int k = 0;
                while (t[k] && t[k] != '\n' && k < 15) text[k] = t[k], k++;
                text[k] = 0;
                gw_push(type, x, y, button, dy, key, mods, text);
            }
            p = nl ? nl + 1 : NULL;
        }
        gw_frame(f * 16.0);
    }
    printf("%08x %dx%d\n", (unsigned)gw_hash(), gw_width(), gw_height());
    if (argc > 3) {
        int pw = gw_width(), ph = gw_height();
        const uint8_t *px = gw_pixels();
        uint32_t *img = malloc((size_t)pw * ph * 4);
        for (int i = 0; img && px && i < pw * ph; i++) {
            uint32_t a = px[i * 4 + 3], bg = 0x40, o = 0xFF000000u;
            for (int k = 0; k < 3; k++) o |= (uint32_t)((px[i * 4 + k] * a + bg * (255 - a)) / 255) << (16 - 8 * k);
            img[i] = o;
        }
        if (img) goome_save_ppm(argv[3], img, pw, ph, pw);
        free(img);
    }
    const char *log = gw_log_take();
    if (*log) fputs(log, stdout);
    return 0;
}
