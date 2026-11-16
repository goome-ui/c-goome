#ifndef LC_TIME_H
#define LC_TIME_H
#include <sys/types.h>
struct timespec {
    time_t tv_sec;
    long tv_nsec;
};
#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 1
int clock_gettime(int clk, struct timespec *t);
int nanosleep(const struct timespec *t, struct timespec *left);
#endif
