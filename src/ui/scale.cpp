#include "ui/scale.hpp"

#include <algorithm>

#include "ui/imgui.hpp"

namespace {

constexpr float kBaseFontSize = 13.0f;

float g_UiScale = kDefaultUiScale;
ImGuiStyle g_BaseStyle;
bool g_BaseStyleCaptured = false;

} // namespace

void InitUiScale(float initialScale)
{
    ImGuiIO &io = ImGui::GetIO();

    io.Fonts->AddFontDefaultVector();

    ImGuiStyle &style = ImGui::GetStyle();
    style.FontSizeBase = kBaseFontSize;

    if (!g_BaseStyleCaptured) {
        g_BaseStyle = style;
        g_BaseStyleCaptured = true;
    }

    SetUiScale(initialScale);
}

void SetUiScale(float scale)
{
    scale = std::clamp(scale, kMinUiScale, kMaxUiScale);
    g_UiScale = scale;

    ImGuiStyle &style = ImGui::GetStyle();
    style = g_BaseStyle;
    style.ScaleAllSizes(scale);
    style.FontScaleMain = scale;
}

float GetUiScale()
{
    return g_UiScale;
}
