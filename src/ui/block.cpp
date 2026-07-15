#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <variant>

#include "ui/block.hpp"
#include "ui/const.hpp"

static std::vector<BlockRow> BuildRows(const BlockSchema &schema);
static LiteralValue &GetLiteralArg(VisualBlock &block, const BlockSchemaItem &item);
static VariableRef &GetVariableArg(VisualBlock &block, const std::string &key);
static std::string LiteralToEditString(const LiteralValue &lit);
static void ApplyEditString(LiteralValue &lit, const std::string &text);

static void DrawTextSlot(
    ImDrawList *drawList,
    ImFont *font,
    float fontSize,
    ImU32 textColor,
    const SlotLayout &slot,
    ImVec2 topLeft,
    ImVec2 size,
    float zoom);
static void DrawVarSlot(
    ImDrawList *drawList,
    VisualBlock &block,
    const SlotLayout &slot,
    size_t slotIndex,
    ImVec2 topLeft,
    ImVec2 size,
    ImFont *font,
    float fontSize,
    ImU32 textColor,
    bool interactive);
static void DrawInputSlot(
    ImDrawList *drawList,
    VisualBlock &block,
    const SlotLayout &slot,
    size_t slotIndex,
    ImVec2 topLeft,
    ImVec2 size,
    float zoom,
    bool interactive);
static void DrawBoolSlot(
    ImDrawList* drawList,
    ImVec2 topLeft,
    ImVec2 size,
    ImU32 color);

bool IsReporter(const VisualBlock *b)
{
    return b && b->Def->Shape == BlockShape::Reporter;
}

bool IsStatement(const VisualBlock *b)
{
    return b &&
        (b->Def->Shape == BlockShape::Chain ||
         b->Def->Shape == BlockShape::Hat ||
         b->Def->Shape == BlockShape::Cap);
}

ImVec2 TopSnap(const VisualBlock &b)
{
    return {
        b.Pos.x + b.Size.x * 0.5f,
        b.Pos.y
    };
}

ImVec2 BottomSnap(const VisualBlock &b)
{
    return {
        b.Pos.x + b.Size.x * 0.5f,
        b.Pos.y + b.Size.y
    };
}

BlockOutline &BlockOutline::Tab(float travel, float depth)
{
    float necking = travel * 0.25f;
    ImVec2 c = Cursor();
    To(c + ImVec2(necking, depth));
    To(c + ImVec2(travel - necking, depth));
    To(c + ImVec2(travel, 0.0f));
    return *this;
}

BlockOutline &BlockOutline::RightEdgeMouth(float xRight, float xInner, float yTop, float yBottom,
        float notchInset, float notchWidth, float notchDepth)
{
    bool canFitNotch = (xRight - xInner) > notchInset * 2.0f + notchWidth;

    To(ImVec2(xRight, yTop));

    if (canFitNotch) {
        To(ImVec2(xInner + notchInset + notchWidth, yTop));
        Tab(-notchWidth, notchDepth);
        Right(-notchInset);
    } else {
        To(ImVec2(xInner, yTop));
    }

    To(ImVec2(xInner, yBottom));

    if (canFitNotch)
        Right(notchInset).Tab(notchWidth, notchDepth).Right((xRight - xInner) - notchInset - notchWidth);
    else
        To(ImVec2(xRight, yBottom));

    return *this;
}

void BlockOutline::Fill(ImDrawList *dl, ImU32 color) const
{
    dl->AddConcavePolyFilled(Pts.data(), (int)Pts.size(), color);
}

void BlockOutline::Stroke(ImDrawList *dl, ImU32 color, float thickness) const
{
    dl->AddPolyline(Pts.data(), (int)Pts.size(), color, ImDrawFlags_Closed, thickness);
}

BlockLayout ComputeBlockLayout(
        const BlockDefinition &def,
        VisualBlock &block,
        const LayoutMetrics &m)
{
    BlockLayout layout;
    auto rows = BuildRows(def.Schema);

    float inlineRowHeight = m.FontSize * m.LineHeightFactor + m.Padding.y * 2.0f;

    float top = m.Padding.y;

    for (const BlockRow &src : rows) {
        RowLayout row;
        row.IsBody   = src.IsBody;
        row.BodyItem = src.BodyItem;
        row.Top      = top;

        if (src.IsBody) {
            row.Height = m.BodyMinHeight;
            row.NaturalWidth = m.BodyIndent + m.BodyNotchWidth + m.Padding.x;
        } else {
            row.Height = 0.0f;

            // Measure every slot only.
            for (const BlockSchemaItem *item : src.Tokens)
            {
                SlotLayout slot;
                slot.Item = item;

                switch (item->Type)
                {
                    case BlockSchemaType::Text:
                        {
                            const char *label =
                                item->Name.empty() ? " " : item->Name.c_str();

                            slot.Size = ImGui::CalcTextSize(label);
                            break;
                        }

                    case BlockSchemaType::Var:
                        {
                            if (VisualBlock *plugged = GetPluggedArg(block, item->Name))
                            {
                                slot.Size = plugged->Size;
                            }
                            else
                            {
                                slot.Size = ImVec2(
                                        m.VarSlotWidth,
                                        inlineRowHeight);
                            }
                            break;
                        }

                    default:
                        {
                            if (VisualBlock *plugged = GetPluggedArg(block, item->Name)) {
                                slot.Size = plugged->Size;
                            } else if (item->ValueType != VAL_ANY && (item->ValueType & VAL_BOOL)) {
                                float h = inlineRowHeight;
                                slot.Size = ImVec2(h * kBoolAspectRatio, h);
                            } else {
                                auto &lit = GetLiteralArg(block, *item);

                                std::string text = LiteralToEditString(lit);
                                ImVec2 textSize = ImGui::CalcTextSize( text.empty() ? "0" : text.c_str());
                                float w = std::clamp(
                                        textSize.x + m.Padding.x * 2.0f,
                                        m.MinInputWidth,
                                        m.MaxInputWidth);

                                slot.Size = ImVec2(w, inlineRowHeight);
                            }

                            break;
                        }
                }

                row.Slots.push_back(std::move(slot));
            }

            // Compute row height.
            float tallest = 0.0f;

            for (const SlotLayout &slot : row.Slots)
                tallest = std::max(tallest, slot.Size.y);

            row.Height = tallest + m.Padding.y * 2.0f;

            // Position horizontally.
            float cursorX = m.Padding.x;

            for (SlotLayout &slot : row.Slots) {
                slot.Pos.x = cursorX;
                cursorX += slot.Size.x + m.TokenGap;
            }

            // Center vertically.
            float maxRight = 0.0f;

            for (SlotLayout &slot : row.Slots) {
                slot.Pos.y =
                    row.Top +
                    (row.Height - slot.Size.y) * 0.5f;

                maxRight = std::max(
                        maxRight,
                        slot.Pos.x + slot.Size.x);
            }

            row.NaturalWidth = maxRight + m.Padding.x;
        }

        top += row.Height;

        if (row.IsBody)
            top += m.BodyBottomBarHeight;

        layout.Rows.push_back(std::move(row));
    }

    top += m.Padding.y;

    if (def.Shape == BlockShape::Reporter && (def.ReturnType & VAL_BOOL)) {
        float inset = top * kBoolInsetRatio;

        for (RowLayout &row : layout.Rows) {
            if (row.IsBody)
                continue;

            for (SlotLayout &slot : row.Slots)
                slot.Pos.x += inset;

            row.NaturalWidth += inset * 2.0f;
        }
    }

    // Compute final block width.
    float width = m.MinBlockWidth;
    for (const RowLayout &row : layout.Rows)
        width = std::max(width, row.NaturalWidth);

    // Center every inline row.
    for (RowLayout &row : layout.Rows) {
        if (row.IsBody)
            continue;

        float offset = (width - row.NaturalWidth) * 0.5f;

        for (SlotLayout &slot : row.Slots)
            slot.Pos.x += offset;
    }

    layout.Size = ImVec2(width, top);

    return layout;
}

void DrawBlockLayout(
        ImDrawList *drawList,
        VisualBlock &block,
        ImVec2 topLeft,
        float zoom,
        ImFont *font,
        float fontSize,
        ImU32 textColor,
        bool interactive)
{
    for (const RowLayout &row : block.Layout.Rows) {
        if (row.IsBody) {
            continue; // reserved: BodyHeads chain renders here
        }

        for (size_t slotIndex = 0; slotIndex < row.Slots.size(); slotIndex++) {
            const SlotLayout &slot = row.Slots[slotIndex];
            ImVec2 slotTopLeft = topLeft + slot.Pos * zoom;
            ImVec2 slotSize = slot.Size * zoom;

            switch (slot.Item->Type) {
                case BlockSchemaType::Text:
                    DrawTextSlot(drawList, font, fontSize, textColor, slot, slotTopLeft, slotSize, zoom);
                    break;
                case BlockSchemaType::Var:
                    if (!GetPluggedArg(block, slot.Item->Name))
                        DrawVarSlot(drawList, block, slot, slotIndex, slotTopLeft, slotSize, font, fontSize, textColor, interactive);
                    break;
                default:
                    if (!GetPluggedArg(block, slot.Item->Name)) {
                        if (slot.Item->ValueType == VAL_ANY)
                            DrawInputSlot(
                                    drawList,
                                    block,
                                    slot,
                                    slotIndex,
                                    slotTopLeft,
                                    slotSize,
                                    zoom,
                                    interactive);
                        else if (slot.Item->ValueType & VAL_BOOL)
                            DrawBoolSlot(
                                    drawList,
                                    slotTopLeft,
                                    slotSize,
                                    Darken(CategoryColor(block.Def->Category)));
                        else
                            DrawInputSlot(
                                    drawList,
                                    block,
                                    slot,
                                    slotIndex,
                                    slotTopLeft,
                                    slotSize,
                                    zoom,
                                    interactive);
                    }
                    break;
            }
        }
    }
}

static std::vector<BlockRow> BuildRows(const BlockSchema &schema)
{
    std::vector<BlockRow> rows;
    BlockRow current;

    auto flush = [&]()
    {
        if (!current.Tokens.empty()) {
            rows.push_back(std::move(current));
            current = BlockRow{};
        }
    };

    for (const BlockSchemaItem &item : schema) {
        if (item.Type == BlockSchemaType::Body) {
            flush();
            BlockRow bodyRow;
            bodyRow.IsBody = true;
            bodyRow.BodyItem = &item;
            rows.push_back(std::move(bodyRow));
        } else {
            current.Tokens.push_back(&item);
        }
    }
    flush();

    return rows;
}

LiteralValue MakeDefaultLiteral(Value type)
{
    if (type & VAL_FLOAT) return LiteralValue{std::in_place_type<float>, 0.0f};
    if (type & VAL_INT)   return LiteralValue{std::in_place_type<int>, 0};
    if (type & VAL_BOOL)  return LiteralValue{std::in_place_type<bool>, false};
    return LiteralValue{std::in_place_type<std::string>, std::string{}};
}

LiteralValue GetSchemaDefaultLiteral(const BlockDefinition &def, const BlockSchemaItem &item)
{
    auto it = def.DefaultValues.find(item.Name);
    if (it != def.DefaultValues.end())
        return it->second;
    return MakeDefaultLiteral(item.ValueType);
}

VisualArg MakeDefaultArg(const BlockDefinition &def, const BlockSchemaItem &item)
{
    if (item.Type == BlockSchemaType::Var)
        return VisualArg{VariableRef{}};
    return VisualArg{GetSchemaDefaultLiteral(def, item)};
}

VisualBlock *GetPluggedArg(VisualBlock &block, const std::string &key)
{
    auto it = block.Args.find(key);
    if (it == block.Args.end())
        return nullptr;

    if (auto *held = std::get_if<std::unique_ptr<VisualBlock>>(&it->second))
        return held->get();

    return nullptr;
}

static LiteralValue &GetLiteralArg(VisualBlock &block, const BlockSchemaItem &item)
{
    VisualArg &arg = block.Args[item.Name];
    if (!std::holds_alternative<LiteralValue>(arg))
        arg = block.Def ? GetSchemaDefaultLiteral(*block.Def, item) : MakeDefaultLiteral(item.ValueType);
    return std::get<LiteralValue>(arg);
}

static VariableRef &GetVariableArg(VisualBlock &block, const std::string &key)
{
    VisualArg &arg = block.Args[key];
    if (!std::holds_alternative<VariableRef>(arg))
        arg = VariableRef{};
    return std::get<VariableRef>(arg);
}

static std::string LiteralToEditString(const LiteralValue &lit)
{
    return std::visit([](const auto &v) -> std::string
    {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::string>) return v;
        else if constexpr (std::is_same_v<T, bool>)    return v ? "true" : "false";
        else if constexpr (std::is_same_v<T, float>)
        {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%g", v);
            return std::string(buf);
        }
        else                                           return std::to_string(v); // int
    }, lit);
}

static void ApplyEditString(LiteralValue &lit, const std::string &text)
{
    std::visit([&](auto &v)
    {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::string>)  v = text;
        else if constexpr (std::is_same_v<T, int>)     v = std::atoi(text.c_str());
        else if constexpr (std::is_same_v<T, float>)   v = static_cast<float>(std::atof(text.c_str()));
        else if constexpr (std::is_same_v<T, bool>)    v = (text == "true" || text == "1");
    }, lit);
}

static void DrawTextSlot(
    ImDrawList *drawList,
    ImFont *font,
    float fontSize,
    ImU32 textColor,
    const SlotLayout &slot,
    ImVec2 topLeft,
    ImVec2 size,
    float zoom)
{
    const char *label =
        slot.Item->Name.empty() ? " " : slot.Item->Name.c_str();

    ImVec2 textSize =
        font->CalcTextSizeA(
            fontSize,
            FLT_MAX,
            0.0f,
            label);

    constexpr float HorizontalPadding = 6.0f;

    ImVec2 textPos(
        topLeft.x + HorizontalPadding * zoom,
        topLeft.y + (size.y - textSize.y) * 0.5f);

    drawList->AddText(
        font,
        fontSize,
        textPos,
        textColor,
        label);
}

static void DrawVarSlot(
    ImDrawList *drawList,
    VisualBlock &block,
    const SlotLayout &slot,
    size_t slotIndex,
    ImVec2 topLeft,
    ImVec2 size,
    ImFont *font,
    float fontSize,
    ImU32 textColor,
    bool interactive)
{
    VariableRef &ref = GetVariableArg(block, slot.Item->Name);
    if (ref.Name.empty())
        ref.Name = "my_var";

    drawList->AddRectFilled(topLeft, topLeft + size, IM_COL32(255, 255, 255, 30));
    drawList->AddRect(topLeft, topLeft + size, IM_COL32(255, 255, 255, 90), 0.0f, 0, 1.0f);

    ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, ref.Name.c_str());
    ImVec2 textPos(topLeft.x + (size.x - textSize.x) * 0.5f, topLeft.y + (size.y - textSize.y) * 0.5f);
    drawList->AddText(font, fontSize, textPos, textColor, ref.Name.c_str());

    if (!interactive)
        return;

    ImGui::SetCursorScreenPos(topLeft);
    ImGui::PushID(block.Id);
    ImGui::PushID((int)slotIndex);
    ImGui::InvisibleButton("##var_btn", size);
    if (ImGui::IsItemClicked())
        ImGui::OpenPopup("##var_popup");
    if (ImGui::BeginPopup("##var_popup")) {
        if (ImGui::Selectable("my_var")) ref.Name = "my_var";
        if (ImGui::Selectable("score"))  ref.Name = "score";
        if (ImGui::Selectable("Create new...")) ref.Name = "new_var";
        ImGui::EndPopup();
    }
    ImGui::PopID();
    ImGui::PopID();
}

static void DrawInputSlot(
    ImDrawList *drawList,
    VisualBlock &block,
    const SlotLayout &slot,
    size_t slotIndex,
    ImVec2 topLeft,
    ImVec2 size,
    float zoom,
    bool interactive)
{
    LiteralValue &lit = GetLiteralArg(block, *slot.Item);
    std::string text = LiteralToEditString(lit);

    ImU32 boxColor = Darken(CategoryColor(block.Def->Category));

    if (!interactive) {
        drawList->AddRectFilled(topLeft, topLeft + size, boxColor, 2.0f * zoom);
        drawList->AddRect(topLeft, topLeft + size, IM_COL32(0, 0, 0, 120), 2.0f * zoom, 0, 1.0f);

        ImFont *font = ImGui::GetFont();
        float fontSize = ImGui::GetFontSize() * zoom;
        float textHeight = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text.c_str()).y;

        drawList->PushClipRect(topLeft, topLeft + size, true);
        drawList->AddText(
                font,
                fontSize,
                topLeft + ImVec2(4.0f * zoom, (size.y - textHeight) * 0.5f),
                IM_COL32(255,255,255,255),
                text.c_str());
        drawList->PopClipRect();

        return;
    }

    drawList->AddRectFilled(topLeft, topLeft + size, boxColor, 0.0f);
    drawList->AddRect(topLeft, topLeft + size, IM_COL32(0, 0, 0, 120), 0.0f, 0, 1.0f);

    float baseFontSize = ImGui::GetFontSize();
    float scaledFontSize = baseFontSize * zoom;
    float framePaddingY = std::max(0.0f, (size.y - scaledFontSize) * 0.5f);
    float framePaddingX = ImGui::GetStyle().FramePadding.x * zoom;

    ImGui::SetWindowFontScale(zoom);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(framePaddingX, framePaddingY));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, boxColor);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, boxColor);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, boxColor);
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));

    ImGui::SetCursorScreenPos(topLeft);
    ImGui::PushID(block.Id);
    ImGui::PushID((int)slotIndex);
    ImGui::SetNextItemWidth(size.x);

    if (ImGui::InputText("##input", &text))
        ApplyEditString(lit, text);
    ImGui::PopID();
    ImGui::PopID();

    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar();
    ImGui::SetWindowFontScale(1.0f);
}

static void DrawBoolSlot(
    ImDrawList* drawList,
    ImVec2 topLeft,
    ImVec2 size,
    ImU32 color)
{
    const float inset = size.y * kBoolInsetRatio;
    const float midY  = topLeft.y + size.y * 0.5f;

    std::array<ImVec2, 6> pts = {
        ImVec2(topLeft.x + inset,          topLeft.y),           // top-left
        ImVec2(topLeft.x + size.x - inset, topLeft.y),           // top-right
        ImVec2(topLeft.x + size.x,         midY),                // right point
        ImVec2(topLeft.x + size.x - inset, topLeft.y + size.y),  // bottom-right
        ImVec2(topLeft.x + inset,          topLeft.y + size.y),  // bottom-left
        ImVec2(topLeft.x,                  midY),                // left point
    };

    drawList->AddConvexPolyFilled(
            pts.data(),
            pts.size(),
            color);

    drawList->AddPolyline(
            pts.data(),
            pts.size(),
            IM_COL32(0,0,0,120),
            ImDrawFlags_Closed,
            1.0f);
}
