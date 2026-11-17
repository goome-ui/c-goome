#include "nk.h"

int main(void)
{
    nkb b;
    nkb_init(&b);
    int n = nkb_frame(&b, 0);
    printf("atlas %dx%d elems %d\n", b.atlas_w, b.atlas_h, n);
    nkb_free(&b);
    return 0;
}
