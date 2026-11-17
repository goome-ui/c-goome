#include "ui.h"
#include "../bench_util.h"

static unsigned char fb[960 * 640 * 4] __attribute__((aligned(64)));

static void flush(lv_display_t *d, const lv_area_t *a, uint8_t *px)
{
    (void)a;
    (void)px;
    lv_display_flush_ready(d);
}

static uint32_t tick(void) { return (uint32_t)bench_now_ms(); }

static void frame(lv_display_t *d)
{
    lv_obj_invalidate(lv_screen_active());
    lv_timer_ready(lv_display_get_refr_timer(d));
    lv_timer_handler();
}

static void scene(lv_display_t *d, const char *name, void (*build)(void), int frames)
{
    lv_obj_clean(lv_screen_active());
    build();
    for (int i = 0; i < 100; i++) frame(d);
    double t0 = bench_now_ms();
    for (int i = 0; i < frames; i++) frame(d);
    bench_report("lvgl", name, frames, bench_now_ms() - t0);
}

int main(int argc, char **argv)
{
    int frames = argc > 1 ? atoi(argv[1]) : 2000;
    lv_init();
    lv_tick_set_cb(tick);
    lv_display_t *d = lv_display_create(960, 640);
    lv_display_set_buffers(d, fb, NULL, sizeof fb, LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(d, flush);
    scene(d, "hello", ui_hello, frames);
    scene(d, "dash", ui_dash, frames);
    return 0;
}
