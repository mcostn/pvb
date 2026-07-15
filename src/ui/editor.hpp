#pragma once

#include "block/registry.hpp"
#include "ui/canvas.hpp"
#include "ui/palette.hpp"

struct PaletteDragState
{
    bool Active = false;

    const BlockDefinition *Definition = nullptr;

    std::unique_ptr<VisualBlock> Ghost;
};

class Editor
{
public:
    explicit Editor(BlockRegistry registry);

    void Draw();
    void DrawMenuBar();

    void BeginPaletteDrag(const BlockDefinition &def);
    void HandlePaletteDrag();

    Canvas CanvasView;
    BlockPalette Palette;
    PaletteDragState Drag;

    BlockRegistry Registry;

    std::string ProjectPath;
    std::string ProjectName;
    void SaveProjectAs();
    void OpenProject();
    void DrawOpenProjectDialog();
    void DrawSaveProjectDialog();
};
