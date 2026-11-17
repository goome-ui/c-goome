#include "imgui.h"
#include "scene.h"
#include "../bench_util.h"

static void frame(int dash, SceneState &s)
{
    ImGui::NewFrame();
    if (dash) scene_dash(s); else scene_hello(s);
    ImGui::Render();
}

static double run(int dash, int frames, SceneState &s, long &sink)
{
    for (int i = 0; i < 100; i++) frame(dash, s);
    double t0 = bench_now_ms();
    for (int i = 0; i < frames; i++) {
        frame(dash, s);
        sink += ImGui::GetDrawData()->TotalVtxCount;
    }
    return bench_now_ms() - t0;
}

int main(int argc, char **argv)
{
    int frames = argc > 1 ? atoi(argv[1]) : 2000;
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(960, 640);
    io.DeltaTime = 1.0f / 60.0f;
    unsigned char *px;
    int w, h;
    io.Fonts->GetTexDataAsRGBA32(&px, &w, &h);
    SceneState s;
    long sink = 0;
    bench_report("imgui", "hello", frames, run(0, frames, s, sink));
    ImGui::DestroyContext();
    ImGui::CreateContext();
    ImGuiIO &io2 = ImGui::GetIO();
    io2.IniFilename = nullptr;
    io2.DisplaySize = ImVec2(960, 640);
    io2.DeltaTime = 1.0f / 60.0f;
    io2.Fonts->GetTexDataAsRGBA32(&px, &w, &h);
    bench_report("imgui", "dash", frames, run(1, frames, s, sink));
    fprintf(stderr, "vtx sink %ld\n", sink);
    ImGui::DestroyContext();
    return 0;
}
