#include "ui/canvas.hpp"

void Canvas::HandleContextMenu(ImVec2 origin, bool hovered)
{
    if (hovered && DraggingId == 0 && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        if (ImGui::IsAnyItemHovered() || ImGui::IsAnyItemActive())
            return;

        ContextMenuOnBlock = false;
        ContextMenuBlockId = 0;

        if (VisualBlock *hit = HitTest(ImGui::GetIO().MousePos, origin)) {
            ContextMenuOnBlock = true;
            ContextMenuBlockId = hit->Id;
            SelectedId = hit->Id;
        }

        ImGui::SetNextWindowPos(ImGui::GetIO().MousePos);
        ImGui::OpenPopup("##canvas_context_menu");
    }

    if (ImGui::BeginPopup("##canvas_context_menu")) {
        if (ContextMenuOnBlock) {
            auto block = Manager.FindBlock(ContextMenuBlockId);

            if (block) {
                bool hasArgs = Manager.HasPluggedArgs(block);
                bool hasBodies = Manager.HasBodies(block);

                if (ImGui::BeginMenu("Duplicate")) {
                    if (ImGui::MenuItem("Duplicate just Block"))
                        DuplicateBlock(block, DuplicateType::Normal);

                    if (block->Next && ImGui::MenuItem("Duplicate Below"))
                        DuplicateBlock(block, DuplicateType::Below);

                    if (block->Prev && ImGui::MenuItem("Duplicate Above"))
                        DuplicateBlock(block, DuplicateType::Above);

                    if (hasArgs && ImGui::MenuItem("Duplicate Without Args"))
                        DuplicateBlock(block, DuplicateType::WithoutArgs);

                    if (hasBodies && ImGui::MenuItem("Duplicate Without Bodies"))
                        DuplicateBlock(block, DuplicateType::WithoutBodies);

                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Delete")) {
                    if (ImGui::MenuItem("Delete just Block"))
                        DeleteBlock(block, DeleteType::Normal);

                    if (block->Next && ImGui::MenuItem("Delete Below"))
                        DeleteBlock(block, DeleteType::Below);

                    if (block->Prev && ImGui::MenuItem("Delete Above"))
                        DeleteBlock(block, DeleteType::Above);

                    if (hasArgs && ImGui::MenuItem("Delete Args"))
                        DeleteBlock(block, DeleteType::Args);

                    if (hasArgs && ImGui::MenuItem("Delete Without Args"))
                        DeleteBlock(block, DeleteType::WithoutArgs);

                    if (hasBodies && ImGui::MenuItem("Delete Bodies"))
                        DeleteBlock(block, DeleteType::Bodies);

                    if (hasBodies && ImGui::MenuItem("Delete Without Bodies"))
                        DeleteBlock(block, DeleteType::WithoutBodies);

                    ImGui::EndMenu();
                }

            }
        } else {
            if (ImGui::MenuItem("Add Comment")) {
                Comments.emplace_back(
                        NextCommentId++,
                        ScreenToWorld(ImGui::GetIO().MousePos, origin),
                        ImVec2(180,120));
            }

            if (Zoom != 1.0f && ImGui::MenuItem("Reset Zoom")) {
                Zoom = 1.0f;
            }

            if (Pan.x != 0.0f && Pan.y != 0.0f && ImGui::MenuItem("Go to origin")) {
                Pan.x = 0.0f;
                Pan.y = 0.0f;
            }

            if (!Manager.Blocks.empty() && ImGui::MenuItem("Delete All")) {
                Manager.DeleteAll();
                SelectedId = 0;
                DraggingId = 0;
            }
        }

        ImGui::EndPopup();
    }
}
