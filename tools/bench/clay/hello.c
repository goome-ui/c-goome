#include "cl.h"

int main(void)
{
    clb b;
    void *mem;
    clb_init(&b, &mem);
    int n = clb_frame(&b, 0);
    printf("commands %d\n", n);
    free(mem);
    return 0;
}
