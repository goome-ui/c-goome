#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_opengl3_loader.h"
#include <GLFW/glfw3.h>
#include "scene.h"
#include "../bench_util.h"

int main()
{
    if (!glfwInit()) {
        fprintf(stderr, "glfwInit failed\n");
        return 1;
    }
#ifdef __APPLE__
    const char *glsl = "#version 120";
#else
    const char *glsl = "#version 130";
#endif
    GLFWwindow *win = glfwCreateWindow(960, 640, "Hello", nullptr, nullptr);
    if (!win) {
        const char *err = nullptr;
        glfwGetError(&err);
        fprintf(stderr, "glfwCreateWindow failed: %s\n", err ? err : "?");
        return 1;
    }
    glfwMakeContextCurrent(win);
    glfwSwapInterval(0);
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui_ImplGlfw_InitForOpenGL(win, true);
    ImGui_ImplOpenGL3_Init(glsl);
    SceneState s;
    int n = 0;
    while (!glfwWindowShouldClose(win)) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        scene_hello(s);
        ImGui::Render();
        int fw, fh;
        glfwGetFramebufferSize(win, &fw, &fh);
        glViewport(0, 0, fw, fh);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(win);
        if (n++ == 0) bench_ready();
        bench_sleep_ms(16);
    }
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
}
