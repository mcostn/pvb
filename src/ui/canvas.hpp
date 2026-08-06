#pragma once

#include <memory>
#include <vector>

#include "util/error.hpp"
#include "ui/imgui.hpp"
#include "ui/block.hpp"
#include "ui/block_manager.hpp"

enum class DeleteType
{
    Normal,
    Below,
    Above,
    Args,
    WithoutArgs,
    Bodies,
    WithoutBodies,
};

enum class DuplicateType
{
    Normal,
    Below,
    Above,
    WithoutArgs,
    WithoutBodies,
};

class Editor;
class BlockRegistry;

struct PendingVariableCreate
{
    bool Requested = false;
    VisualBlock *TargetBlock = nullptr;
    std::string TargetKey;
    Value RequiredType = VAL_ANY;
};

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
    VisualBlock *AddBlock(const BlockDefinition &def, ImVec2 worldPos);
    void DuplicateBlock(VisualBlock *block, DuplicateType type = DuplicateType::Normal);
    void DeleteBlock(VisualBlock *block, DeleteType type);

    ImVec2 WorldToScreen(ImVec2 world, ImVec2 origin) const;
    ImVec2 ScreenToWorld(ImVec2 screen, ImVec2 origin) const;

    u32 SelectedId = 0;
    u32 HoveredBlockId = 0;
    u32 ActiveBlockId = 0;

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
    void TryBeginBlockDrag(ImVec2 origin, bool hovered);

    SnapResult CurrentSnap {};
    void DrawSnapPreview(ImDrawList *drawList, ImVec2 origin);

    u32 ContextMenuBlockId = 0;
    bool ContextMenuOnBlock = false;
    bool ContextMenuOnComment = false;
    u32 ContextMenuCommentId = 0;
    void HandleContextMenu(ImVec2 origin, bool hovered);

    bool ShowDebugWindow = false;
    void DrawDebugWindow();

    Editor *EditorRef = nullptr;
    BlockRegistry *Registry = nullptr;

    ImVec2 Origin;
    bool Hovered = false;

    u32 NextCommentId = 1;
    std::vector<CanvasComment> Comments;
    void DrawComments(ImVec2 origin);
    void DeleteComment(uint32_t id);
    CanvasComment *HitTestComment(ImVec2 mouse, ImVec2 origin);

    PendingVariableCreate VarCreateRequest;
    char NewVarNameBuf[64] = "";
    int NewVarTypeIndex = 0;
    std::string NewVarError;

    void RequestVariableCreation(VisualBlock *targetBlock, const std::string &targetKey, Value requiredType);
    void DrawCreateVariablePopup();
    void DeleteVariable(const std::string &name);

    struct PendingVariableRename
    {
        bool Requested = false;
        std::string OldName;
    };
    PendingVariableRename VarRenameRequest;
    char RenameVarNameBuf[64] = "";
    std::string RenameVarError;

    void RequestVariableRename(const std::string &name);
    void DrawRenameVariablePopup();
    Error RenameVariable(const std::string &oldName, const std::string &newName);

    struct CustomParamEdit
    {
        char NameBuf[64] = "";
        int TypeIndex = 0;
    };

    bool CustomBlockCreateRequested = false;
    char NewCustomNameBuf[64] = "";
    char NewCustomDescBuf[256] = "";
    std::vector<CustomParamEdit> NewCustomParams;
    std::string NewCustomError;

    void RequestCustomBlockCreation();
    void DrawCreateCustomBlockPopup();
    void DeleteCustomBlock(const std::string &name);

    struct PendingCustomBlockRename
    {
        bool Requested = false;
        std::string OldName;
    };
    PendingCustomBlockRename CustomRenameRequest;
    char RenameCustomNameBuf[64] = "";
    std::string RenameCustomError;

    void RequestCustomBlockRename(const std::string &name);
    void DrawRenameCustomBlockPopup();
    Error RenameCustomBlock(const std::string &oldName, const std::string &newName);
};

BlockOutline BuildOutline(const VisualBlock &block, ImVec2 topLeft, float zoom);
