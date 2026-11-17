#include "raylib.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
#include <stdio.h>

int main(void)
{
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(480, 200, "Hello");
    if (!IsWindowReady()) return 1;
    SetTargetFPS(0);
    int clicks = 0;
    int ready = 0;
    char buf[32];
#ifdef HELLO_ONESHOT
    int frames = 1;
#else
    int frames = -1;
#endif
    while (frames != 0 && !WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(RAYWHITE);
        GuiLabel((Rectangle){20, 20, 200, 24}, "hello world");
        if (GuiButton((Rectangle){20, 60, 120, 30}, "Click me")) clicks++;
        snprintf(buf, sizeof buf, "clicks: %d", clicks);
        GuiLabel((Rectangle){20, 110, 200, 24}, buf);
        EndDrawing();
        if (!ready) {
            ready = 1;
            printf("READY\n");
            fflush(stdout);
        }
        if (frames > 0) frames--;
    }
    CloseWindow();
    return 0;
}
