#include "raylib.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
#include <stdio.h>
#include <stdlib.h>

static float vals[50];
static bool checks[50];

static void hello(void)
{
    static int clicks = 0;
    char buf[32];
    GuiLabel((Rectangle){20, 20, 200, 24}, "hello world");
    if (GuiButton((Rectangle){20, 60, 120, 30}, "Click me")) clicks++;
    snprintf(buf, sizeof buf, "clicks: %d", clicks);
    GuiLabel((Rectangle){20, 110, 200, 24}, buf);
}

static void dash(void)
{
    char buf[32];
    for (int i = 0; i < 50; i++) {
        float y = 8 + i * 28;
        snprintf(buf, sizeof buf, "row %d", i);
        GuiLabel((Rectangle){10, y, 80, 24}, buf);
        GuiButton((Rectangle){100, y, 80, 24}, "Open");
        GuiCheckBox((Rectangle){200, y + 2, 20, 20}, "On", &checks[i]);
        GuiSliderBar((Rectangle){280, y, 200, 24}, "", "", &vals[i], 0, 1);
    }
}

static void frame(void (*scene)(void))
{
    BeginDrawing();
    ClearBackground(RAYWHITE);
    scene();
    EndDrawing();
}

static void measure(const char *name, void (*scene)(void), int frames)
{
    for (int i = 0; i < 100; i++) frame(scene);
    double t0 = GetTime();
    for (int i = 0; i < frames; i++) frame(scene);
    double ms = (GetTime() - t0) * 1000.0;
    printf("{\"lib\":\"raygui\",\"scene\":\"%s\",\"frames\":%d,\"ms_per_frame\":%.5f}\n", name, frames, ms / frames);
    fflush(stdout);
}

int main(int argc, char **argv)
{
    int frames = argc > 1 ? atoi(argv[1]) : 2000;
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(960, 640, "bench");
    if (!IsWindowReady()) return 1;
    SetTargetFPS(0);
    measure("hello", hello, frames);
    measure("dash", dash, frames);
    CloseWindow();
    return 0;
}
