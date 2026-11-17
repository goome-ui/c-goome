#include "mu.h"
#include "../bench_util.h"

static double run(mub *b, int dash, int frames, long *sink)
{
    for (int i = 0; i < 100; i++) mub_frame(b, dash);
    double t0 = bench_now_ms();
    for (int i = 0; i < frames; i++) *sink += mub_frame(b, dash);
    return bench_now_ms() - t0;
}

int main(int argc, char **argv)
{
    int frames = argc > 1 ? atoi(argv[1]) : 2000;
    long sink = 0;
    static mub b;
    mub_init(&b);
    bench_report("microui", "hello", frames, run(&b, 0, frames, &sink));
    mub_init(&b);
    bench_report("microui", "dash", frames, run(&b, 1, frames, &sink));
    fprintf(stderr, "cmd sink %ld\n", sink);
    return 0;
}
