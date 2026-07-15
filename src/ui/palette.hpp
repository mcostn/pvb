#pragma once

#include "ui/canvas.hpp"
#include "block/registry.hpp"

class BlockPalette
{
public:
    void Draw(
            Canvas &canvas,
            const BlockRegistry &registry,
            const char *id,
            float height);

    float Width = 260.0f;
    char Search[128] = "";

    bool Matches(const BlockDefinition &def) const;

    void DrawBlockPreview(
        Canvas &canvas,
        const BlockDefinition &def);
};
