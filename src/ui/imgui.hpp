#pragma once

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_stdlib.h>
#include <ImGuiFileDialog.h>

inline ImU32 Darken(ImU32 color)
{
    ImVec4 rgba = ImGui::ColorConvertU32ToFloat4(color);

    rgba.x *= 0.8f;
    rgba.y *= 0.8f;
    rgba.z *= 0.8f;

    return ImGui::ColorConvertFloat4ToU32(rgba);
}
