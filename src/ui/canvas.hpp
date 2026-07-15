#pragma once

#include <memory>
#include <vector>

#include "ui/imgui.hpp"
#include "ui/block.hpp"
#include "ui/block_manager.hpp"

enum class DeleteType
{
    Normal,
    Below,
    Above,
};

class Editor;

struct CanvasComment
{
    uint32_t Id;

    ImVec2 Pos;
    ImVec2 Size;

    std::string Text {};
};

class Canvas
{
public:
    void Draw(const char *strId, ImVec2 size);
    void DrawGrid(ImDrawList *drawList, ImVec2 origin, ImVec2 size);
    void DrawBlock(ImDrawList *drawList, VisualBlock &block, ImVec2 origin);
    void DrawChain(ImDrawList *drawList, VisualBlock *block, ImVec2 origin);

    void LayoutBlock(VisualBlock &block);
    void LayoutChain(VisualBlock *head);
    VisualBlock *HitChain(VisualBlock *block, ImVec2 mouse, ImVec2 origin);
    VisualBlock *HitTest(ImVec2 mouse, ImVec2 origin);
    void BringRootToFront(VisualBlock *root);

    float FindBodyTop(const VisualBlock &block, const std::string &slot);

    BlockManager Manager;
    void AddBlock(const BlockDefinition &def, ImVec2 worldPos);
    void DuplicateBlock(VisualBlock *block);
    void DeleteBlock(VisualBlock *block, DeleteType type);

    ImVec2 WorldToScreen(ImVec2 world, ImVec2 origin) const;
    ImVec2 ScreenToWorld(ImVec2 screen, ImVec2 origin) const;

    u32 SelectedId = 0;

    ImVec2 Pan = ImVec2(0.0f, 0.0f);
    bool IsPanning = false;
    ImVec2 PanMouseStart = ImVec2(0.0f, 0.0f);
    ImVec2 PanStart = ImVec2(0.0f, 0.0f);
    float Zoom = 1.0f;
    void HandlePanAndZoom(ImVec2 origin, bool hovered);

    u32 DraggingId = 0;
    ImVec2 DragBlockStartWorld = ImVec2(0.0f, 0.0f);
    ImVec2 DragMouseStartScreen = ImVec2(0.0f, 0.0f);
    void HandleBlockDrag(ImVec2 origin, bool hovered);

    SnapResult CurrentSnap {};
    void DrawSnapPreview(ImDrawList *drawList, ImVec2 origin);

    u32 ContextMenuBlockId = 0;
    bool ContextMenuOnBlock = false;
    void HandleContextMenu(ImVec2 origin, bool hovered);

    bool ShowDebugWindow = false;
    void DrawDebugWindow();

    Editor *EditorRef = nullptr;

    ImVec2 Origin;
    bool Hovered = false;

    u32 NextCommentId = 1;
    std::vector<CanvasComment> Comments;
    void DrawComments(ImVec2 origin);
};

BlockOutline BuildOutline(const VisualBlock &block, ImVec2 topLeft, float zoom);
