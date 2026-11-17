#define SDL_MAIN_HANDLED
#include "ui.h"
#include "../bench_util.h"
#include <SDL.h>

static uint32_t tick(void) { return (uint32_t)bench_now_ms(); }

int main(void)
{
    SDL_SetMainReady();
    lv_init();
    lv_tick_set_cb(tick);
    lv_display_t *d = lv_sdl_window_create(960, 640);
    lv_sdl_window_set_title(d, "Hello");
    lv_sdl_mouse_create();
    ui_hello();
    lv_refr_now(d);
    bench_ready();
    for (;;) {
        lv_timer_handler();
        SDL_Delay(5);
    }
}
