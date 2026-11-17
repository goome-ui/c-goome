#ifndef IMGUI_SCENE_H
#define IMGUI_SCENE_H

#include "imgui.h"
#include <stdio.h>

#define BENCH_ROWS 50

struct SceneState {
    int clicks = 0;
    bool checks[BENCH_ROWS] = {};
    float vals[BENCH_ROWS];
    SceneState()
    {
        for (int i = 0; i < BENCH_ROWS; i++) vals[i] = i / (float)BENCH_ROWS;
    }
};

static void scene_hello(SceneState &s)
{
    ImGui::Begin("Hello", nullptr, ImGuiWindowFlags_NoSavedSettings);
    ImGui::Text("hello world");
    if (ImGui::Button("Click me")) s.clicks++;
    ImGui::Text("clicks: %d", s.clicks);
    ImGui::End();
}

static void scene_dash(SceneState &s)
{
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(960, 640));
    ImGui::Begin("bench", nullptr, ImGuiWindowFlags_NoSavedSettings);
    for (int i = 0; i < BENCH_ROWS; i++) {
        ImGui::PushID(i);
        ImGui::Text("row %d", i);
        ImGui::SameLine();
        ImGui::Button("Open");
        ImGui::SameLine();
        ImGui::Checkbox("On", &s.checks[i]);
        ImGui::SameLine();
        ImGui::SliderFloat("Value", &s.vals[i], 0.0f, 1.0f);
        ImGui::PopID();
    }
    ImGui::End();
}

#endif
