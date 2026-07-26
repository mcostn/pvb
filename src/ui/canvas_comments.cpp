#include <algorithm>

#include "ui/canvas.hpp"

void Canvas::DrawComments(ImVec2 origin)
{
    const ImVec2 kCommentMinSize(120.0f, 80.0f);

    for (CanvasComment &c : Comments) {
        ImVec2 screenPos = WorldToScreen(c.Pos, origin);

        if (!ImGui::IsMouseDragging(ImGuiMouseButton_Left))
            ImGui::SetNextWindowPos(screenPos);
        ImGui::SetNextWindowSize(c.Size * Zoom, ImGuiCond_Always);

        ImGui::PushStyleColor(ImGuiCol_WindowBg,
                IM_COL32(255,243,140,255));
        ImGui::PushStyleColor(ImGuiCol_TitleBg,
                IM_COL32(255,220,80,255));
        ImGui::PushStyleColor(ImGuiCol_TitleBgActive,
                IM_COL32(255,220,80,255));
        ImGui::PushStyleColor(ImGuiCol_Border,
                IM_COL32(180,170,70,255));
        ImGui::PushStyleColor(ImGuiCol_Text,
                IM_COL32(60,50,10,255));

        ImGui::PushStyleColor(ImGuiCol_FrameBg,
                IM_COL32(255,243,140,255));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,
                IM_COL32(255,243,140,255));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive,
                IM_COL32(255,243,140,255));

        std::string name = "##comment" + std::to_string(c.Id);

        ImGui::Begin(
                name.c_str(),
                nullptr,
                ImGuiWindowFlags_NoCollapse);

        ImFont *font = ImGui::GetFont();
        float fontSize = ImGui::GetFontSize() * Zoom;
        ImGui::PushFont(font, fontSize);

        ImGui::InputTextMultiline(
                "##text",
                &c.Text,
                ImGui::GetContentRegionAvail());

        ImGui::PopFont();

        c.Pos = ScreenToWorld(
                ImGui::GetWindowPos(),
                origin);

        ImVec2 worldSize = ImGui::GetWindowSize() / Zoom;
        c.Size = ImVec2(
                std::max(worldSize.x, kCommentMinSize.x),
                std::max(worldSize.y, kCommentMinSize.y));

        ImGui::End();
        ImGui::PopStyleColor(8);
    }
}
