#pragma once

#include <functional>
#include <memory>
#include <vector>

#include "ui/imgui.hpp"
#include "block/definition.hpp"
#include "block/instance.hpp"
#include "block/registry.hpp"
#include "util/types.hpp"

struct BlockRow
{
    bool IsBody = false;
    const BlockSchemaItem *BodyItem = nullptr;
    std::vector<const BlockSchemaItem *> Tokens;
};

struct LayoutMetrics
{
    ImVec2 Padding = ImVec2(0.0f, 0.0f);
    float FontSize = 0.0f;
    float LineHeightFactor = 1.3f;
    float RowGap = 3.0f;
    float BodyMinHeight = 28.0f;
    float BodyIndent = 18.0f;
    float BodyBottomBarHeight = 14.0f;
    float BodyNotchWidth = 14.0f;
    float NotchHeight = 8.0f;
    float MinBlockWidth = 60.0f;
    float VarSlotWidth = 70.0f;
    float TokenGap = 4.0f;
    float MinInputWidth = 40.0f;
    float MaxInputWidth = 140.0f;
};

struct SlotLayout
{
    const BlockSchemaItem *Item = nullptr;
    ImVec2 Pos = ImVec2(0.0f, 0.0f);
    ImVec2 Size = ImVec2(0.0f, 0.0f);
};

struct RowLayout
{
    bool IsBody = false;
    bool IsSeparator = false;
    const BlockSchemaItem *BodyItem = nullptr;
    float Top = 0.0f;
    float Height = 0.0f;
    float NaturalWidth = 0.0f;
    std::vector<SlotLayout> Slots;
};

struct BlockLayout
{
    ImVec2 Size = ImVec2(0.0f, 0.0f);
    std::vector<RowLayout> Rows;
};

struct VisualBlock;
using VisualArg = std::variant<
    LiteralValue,
    VariableRef,
    std::unique_ptr<VisualBlock>
>;

struct VisualBlock
{
    u32 Id = 0;

    const BlockDefinition *Def = nullptr;

    std::unordered_map<std::string, VisualArg> Args;
    std::unordered_map<std::string, VisualBlock*> BodyRoots;

    VisualBlock *Prev = nullptr;
    VisualBlock *Next = nullptr;

    VisualBlock *BodyOwner = nullptr;
    std::string BodySlot;

    VisualBlock *ArgOwner = nullptr;
    std::string ArgSlot;

    ImVec2 Pos = ImVec2(0.0f, 0.0f);
    ImVec2 Size = ImVec2(0.0f, 0.0f);
    BlockLayout Layout;
};

struct VariableSlotContext
{
    const BlockRegistry *Registry = nullptr;
    std::function<void(
            VisualBlock *target,
            const std::string &slotKey,
            Value requiredType)> RequestVariableCreation;
};

bool IsReporter(const VisualBlock *b);
bool IsStatement(const VisualBlock *b);

ImVec2 TopSnap(const VisualBlock &b);
ImVec2 BottomSnap(const VisualBlock &b);

inline ImU32 CategoryColor(BlockCategory category)
{
    switch (category) {
        case BlockCategory::Event:       return IM_COL32(64, 64, 72, 255);
        case BlockCategory::Console:     return IM_COL32(38, 143, 130, 255);
        case BlockCategory::ControlFlow: return IM_COL32(191, 121, 36, 255);
        case BlockCategory::Math:        return IM_COL32(53, 105, 173, 255);
        case BlockCategory::Logic:       return IM_COL32(122, 77, 178, 255);
        case BlockCategory::Variable:    return IM_COL32(176, 77, 178, 255);
        case BlockCategory::Custom:      return IM_COL32(255, 140, 66, 255);
    }

    return IM_COL32(90, 90, 90, 255);
}

class BlockOutline
{
public:
    explicit BlockOutline(ImVec2 start) { Pts.push_back(start); }

    ImVec2 Cursor() const { return Pts.back(); }

    BlockOutline &To(ImVec2 p)    { Pts.push_back(p); return *this; }
    BlockOutline &Right(float dx) { return To(Cursor() + ImVec2(dx, 0.0f)); }

    BlockOutline &Tab(float travel, float depth);
    BlockOutline &RightEdgeMouth(
            float xRight,
            float xInner,
            float yTop,
            float yBottom,
            float notchInset,
            float notchWidth,
            float notchDepth);

    void Fill(ImDrawList *dl, ImU32 color) const;
    void Stroke(ImDrawList *dl, ImU32 color, float thickness) const;
    void StrokeOpen(ImDrawList *dl, ImU32 color, float thickness) const;

    std::vector<ImVec2> Pts;
};

BlockLayout ComputeBlockLayout(
        const BlockDefinition &def,
        VisualBlock &block,
        const LayoutMetrics &m);

bool DrawBlockLayout(
        ImDrawList *drawList,
        VisualBlock &block,
        ImVec2 topLeft,
        float zoom,
        ImFont *font,
        float fontSize,
        ImU32 textColor,
        bool interactive,
        const VariableSlotContext &varContext = {});

VisualBlock *GetPluggedArg(VisualBlock &block, const std::string &key);

LiteralValue MakeDefaultLiteral(Value type);
VisualArg MakeDefaultArg(const BlockDefinition &def, const BlockSchemaItem &item);

LiteralValue GetSchemaDefaultLiteral(const BlockDefinition &def, const BlockSchemaItem &item);
