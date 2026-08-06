#include <algorithm>

#include "ui/canvas.hpp"
#include "ui/const.hpp"

void Canvas::DrawComments(ImVec2 origin)
{
    const ImVec2 kCommentMinSize(120.0f, 80.0f);

    uint32_t pendingDeleteId = 0;

    for (CanvasComment &c : Comments) {
        ImVec2 screenPos = WorldToScreen(c.Pos, origin);

        if (!ImGui::IsMouseDragging(ImGuiMouseButton_Left))
            ImGui::SetNextWindowPos(screenPos);
        ImGui::SetNextWindowSize(c.Size * Zoom, ImGuiCond_Always);

        ImGui::PushStyleColor(ImGuiCol_WindowBg,
                kCommentBgColor);
        ImGui::PushStyleColor(ImGuiCol_TitleBg,
                kCommentTitleBgColor);
        ImGui::PushStyleColor(ImGuiCol_TitleBgActive,
                kCommentTitleBgColor);
        ImGui::PushStyleColor(ImGuiCol_Border,
                kCommentBorderColor);
        ImGui::PushStyleColor(ImGuiCol_Text,
                kCommentTextColor);

        ImGui::PushStyleColor(ImGuiCol_FrameBg,
                kCommentBgColor);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,
                kCommentBgColor);
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive,
                kCommentBgColor);

        std::string name = "##comment" + std::to_string(c.Id);

        bool open = true;

        ImGui::Begin(
                name.c_str(),
                &open,
                ImGuiWindowFlags_NoCollapse);

        if (!open)
            pendingDeleteId = c.Id;

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

    if (pendingDeleteId != 0)
        DeleteComment(pendingDeleteId);
}

void Canvas::DeleteComment(uint32_t id)
{
    Comments.erase(
            std::remove_if(Comments.begin(), Comments.end(),
                    [id](const CanvasComment &c) { return c.Id == id; }),
            Comments.end());
}

CanvasComment *Canvas::HitTestComment(ImVec2 mouse, ImVec2 origin)
{
    for (auto it = Comments.rbegin(); it != Comments.rend(); ++it) {
        ImVec2 screenPos = WorldToScreen(it->Pos, origin);
        ImVec2 screenSize = it->Size * Zoom;

        if (mouse.x >= screenPos.x && mouse.x <= screenPos.x + screenSize.x &&
            mouse.y >= screenPos.y && mouse.y <= screenPos.y + screenSize.y)
            return &(*it);
    }

    return nullptr;
}
