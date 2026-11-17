#ifndef BENCH_UTIL_H
#define BENCH_UTIL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
static double bench_now_ms(void)
{
    LARGE_INTEGER f, c;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart * 1000.0 / (double)f.QuadPart;
}
static void bench_sleep_ms(int ms) { Sleep(ms); }
#else
#include <time.h>
#include <unistd.h>
static double bench_now_ms(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
}
static void bench_sleep_ms(int ms) { usleep((useconds_t)ms * 1000); }
#endif

#define BENCH_ROWS 50

static void bench_ready(void)
{
    printf("READY\n");
    fflush(stdout);
}

static void bench_report(const char *lib, const char *scene, int frames, double total_ms)
{
    printf("{\"lib\":\"%s\",\"scene\":\"%s\",\"frames\":%d,\"ms_per_frame\":%.5f}\n", lib, scene, frames, total_ms / frames);
    fflush(stdout);
}

#endif
