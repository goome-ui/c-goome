#include "cl.h"
#include "../bench_util.h"

static double run(clb *b, int dash, int frames, long *sink)
{
    for (int i = 0; i < 100; i++) clb_frame(b, dash);
    double t0 = bench_now_ms();
    for (int i = 0; i < frames; i++) *sink += clb_frame(b, dash);
    return bench_now_ms() - t0;
}

int main(int argc, char **argv)
{
    int frames = argc > 1 ? atoi(argv[1]) : 2000;
    long sink = 0;
    clb b;
    void *mem;
    clb_init(&b, &mem);
    bench_report("clay", "hello", frames, run(&b, 0, frames, &sink));
    bench_report("clay", "dash", frames, run(&b, 1, frames, &sink));
    fprintf(stderr, "cmd sink %ld\n", sink);
    free(mem);
    return 0;
}
