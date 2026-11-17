#include "mu.h"

int main(void)
{
    static mub b;
    mub_init(&b);
    int n = mub_frame(&b, 0);
    printf("commands %d\n", n);
    return 0;
}
