#include "ui.h"
#include <stdio.h>

static unsigned char fb[960 * 640 * 4] __attribute__((aligned(64)));

static void flush(lv_display_t *d, const lv_area_t *a, uint8_t *px)
{
    (void)a;
    (void)px;
    lv_display_flush_ready(d);
}

int main(void)
{
    lv_init();
    lv_display_t *d = lv_display_create(960, 640);
    lv_display_set_buffers(d, fb, NULL, sizeof fb, LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(d, flush);
    ui_hello();
    lv_refr_now(d);
    unsigned long sum = 0;
    for (size_t i = 0; i < sizeof fb; i += 97) sum += fb[i];
    printf("ok %lu\n", sum);
    return 0;
}
