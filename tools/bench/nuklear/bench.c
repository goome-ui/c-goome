#include "nk.h"
#include "../bench_util.h"

static double run(nkb *b, int dash, int frames, long *sink)
{
    for (int i = 0; i < 100; i++) nkb_frame(b, dash);
    double t0 = bench_now_ms();
    for (int i = 0; i < frames; i++) *sink += nkb_frame(b, dash);
    return bench_now_ms() - t0;
}

int main(int argc, char **argv)
{
    int frames = argc > 1 ? atoi(argv[1]) : 2000;
    long sink = 0;
    nkb b;
    nkb_init(&b);
    bench_report("nuklear", "hello", frames, run(&b, 0, frames, &sink));
    nkb_free(&b);
    nkb_init(&b);
    bench_report("nuklear", "dash", frames, run(&b, 1, frames, &sink));
    fprintf(stderr, "elem sink %ld\n", sink);
    nkb_free(&b);
    return 0;
}
