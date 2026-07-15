#pragma once

#include <GLFW/glfw3.h>

#include "imgui.hpp"
#include "util/error.hpp"

class Window
{
public:
    Error Init(const char *title, int width, int height);
    void Shutdown();

    bool BeginFrame();
    void EndFrame();

    GLFWwindow *Handle = nullptr;
};

Error StartApp();
