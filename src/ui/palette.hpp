#pragma once

#include <string>
#include <vector>
#include <functional>

#include "ui/canvas.hpp"
#include "block/registry.hpp"

struct BlockContextAction
{
    std::string Label;
    std::function<void()> Action;
};

class BlockPalette
{
public:
    void Draw(
            Canvas &canvas,
            BlockRegistry &registry,
            const char *id,
            float height);

    float Width = 260.0f;
    char Search[128] = "";

    bool Matches(const BlockDefinition &def) const;

    void DrawBlockPreview(
            Canvas &canvas,
            const BlockDefinition &def,
            const std::vector<BlockContextAction> &contextActions = {});
    void DrawCategorySection(Canvas &canvas, BlockRegistry &registry, BlockCategory category);
    void DrawVariableSection(Canvas &canvas, BlockRegistry &registry);
    void DrawCustomSection(Canvas &canvas, BlockRegistry &registry);
};
