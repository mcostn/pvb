#pragma once

#include <string>
#include <functional>

#include "ui/canvas.hpp"
#include "block/registry.hpp"

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
            const char *deleteLabel = nullptr,
            std::function<void()> onDelete = {});
    void DrawCategorySection(Canvas &canvas, BlockRegistry &registry, BlockCategory category);
    void DrawVariableSection(Canvas &canvas, BlockRegistry &registry);
    void DrawCustomSection(Canvas &canvas, BlockRegistry &registry);
};
