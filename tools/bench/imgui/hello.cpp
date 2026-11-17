#include "imgui.h"
#include "scene.h"
#include <stdio.h>

int main()
{
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(960, 640);
    io.DeltaTime = 1.0f / 60.0f;
    unsigned char *px;
    int w, h;
    io.Fonts->GetTexDataAsRGBA32(&px, &w, &h);
    SceneState s;
    ImGui::NewFrame();
    scene_hello(s);
    ImGui::Render();
    ImDrawData *d = ImGui::GetDrawData();
    printf("atlas %dx%d vtx %d idx %d\n", w, h, d->TotalVtxCount, d->TotalIdxCount);
    ImGui::DestroyContext();
    return 0;
}
