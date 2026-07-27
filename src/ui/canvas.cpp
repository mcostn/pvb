#include <cmath>
#include <algorithm>
#include <functional>
#include <cstdio>

#include "ui/canvas.hpp"
#include "ui/const.hpp"
#include "ui/custom_block.hpp"
#include "ui/scale.hpp"
#include "block/registry.hpp"
#include "util/macro.hpp"
#include "util/error.hpp"

static BlockOutline BuildChainOutline(
        ImVec2 topLeft,
        ImVec2 size,
        float notchInset,
        float notchWidth,
        float notchDepth,
        float spineWidth,
        const std::vector<RowLayout> &rows,
        float zoom);
static BlockOutline BuildHatOutline(
        ImVec2 topLeft,
        ImVec2 size,
        float notchInset,
        float notchWidth,
        float notchDepth,
        float spineWidth,
        const std::vector<RowLayout> &rows,
        float zoom);
static BlockOutline BuildCapOutline(
        ImVec2 topLeft,
        ImVec2 size,
        float notchInset,
        float notchWidth,
        float notchDepth,
        float spineWidth,
        const std::vector<RowLayout> &rows,
        float zoom);
static BlockOutline BuildReporterOutline(
        ImVec2 topLeft,
        ImVec2 size,
        float radius);
static BlockOutline BuildBooleanOutline(
        ImVec2 topLeft,
        ImVec2 size);

static void DrawStatementSnapPreview(
    ImDrawList* drawList,
    ImVec2 pos,
    float width,
    float zoom,
    ImU32 color);

std::unique_ptr<BlockInstance> CloneInstance(const BlockInstance &src)
{
    auto copy = std::make_unique<BlockInstance>();
    copy->OpCode = src.OpCode;
    copy->SourceId = src.SourceId;

    for (const auto &[key, arg] : src.Args) {
        copy->Args.emplace(key, std::visit([](const auto &value) -> BlockArg
        {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, std::unique_ptr<BlockInstance>>)
                return value ? CloneInstance(*value) : nullptr;
            else
                return value;
        }, arg));
    }

    for (const auto &[slot, body] : src.Bodies) {
        std::vector<std::unique_ptr<BlockInstance>> clonedBody;
        clonedBody.reserve(body.size());
        for (const auto &stmt : body)
            clonedBody.push_back(stmt ? CloneInstance(*stmt) : nullptr);
        copy->Bodies.emplace(slot, std::move(clonedBody));
    }

    return copy;
}

void Canvas::Draw(const char *strId, ImVec2 size)
{
    if (ImGui::IsKeyPressed(ImGuiKey_F3)) {
        ShowDebugWindow = !ShowDebugWindow;
    }

    ImGui::PushID(strId);
    ImGui::BeginChild("##canvas_region", size, true,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImVec2 origin = ImGui::GetCursorScreenPos();
    Origin = origin;

    ImVec2 regionSize = ImGui::GetContentRegionAvail();
    ImDrawList *drawList = ImGui::GetWindowDrawList();

    for (auto &block : Manager.Blocks)
        LayoutBlock(*block);

    for (VisualBlock *root : Manager.Roots)
        LayoutChain(root);

    DrawGrid(drawList, origin, regionSize);

    Hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    HandlePanAndZoom(origin, Hovered);
    HandleBlockDrag(origin, Hovered);
    HandleContextMenu(origin, Hovered);

    HoveredBlockId = 0;
    if (DraggingId == 0 && Hovered) {
        if (VisualBlock *hb = HitTest(ImGui::GetIO().MousePos, origin))
            HoveredBlockId = hb->Id;
    }

    DrawComments(origin);

    for (VisualBlock *root : Manager.Roots)
        DrawChain(drawList, root, origin);

    DrawSnapPreview(drawList, origin);

    TryBeginBlockDrag(origin, Hovered);

    ImGui::EndChild();
    ImGui::PopID();

    if (Registry) {
        DrawCreateVariablePopup();
        DrawCreateCustomBlockPopup();
        DrawRenameVariablePopup();
        DrawRenameCustomBlockPopup();
    }
}

void Canvas::DrawGrid(ImDrawList *drawList, ImVec2 origin, ImVec2 size)
{
    drawList->AddRectFilled(origin, origin + size, kCanvasBgColor);

    const float baseStep = 24.0f;
    float gridStep = baseStep * Zoom;

    while (gridStep < 8.0f)
        gridStep *= 2.0f;

    float offsetX = std::fmod(Pan.x * Zoom, gridStep);
    float offsetY = std::fmod(Pan.y * Zoom, gridStep);
    if (offsetX < 0.0f) offsetX += gridStep;
    if (offsetY < 0.0f) offsetY += gridStep;

    ImU32 dotColor = IM_COL32(255, 255, 255, 20);
    for (float x = offsetX; x < size.x; x += gridStep)
        for (float y = offsetY; y < size.y; y += gridStep)
            drawList->AddCircleFilled(origin + ImVec2(x, y), 1.5f, dotColor, 6);
}

void Canvas::LayoutChain(VisualBlock *head)
{
    if (!head)
        return;

    ImVec2 pos = head->Pos;

    while (head) {
        head->Pos = pos;

        LayoutBlock(*head);

        float shift = 0.0f;

        for (RowLayout &row : head->Layout.Rows) {
            if (shift != 0.0f) {
                row.Top += shift;
                for (SlotLayout &slot : row.Slots)
                    slot.Pos.y += shift;
            }

            if (!row.IsBody || !row.BodyItem)
                continue;

            auto it = head->BodyRoots.find(row.BodyItem->Name);
            VisualBlock *body = (it != head->BodyRoots.end()) ? it->second : nullptr;

            if (!body)
                continue;

            body->Pos = ImVec2(head->Pos.x + kBodyIndent * GetUiScale(), head->Pos.y + row.Top);

            LayoutChain(body);

            float bodyHeight = 0.0f;
            for (VisualBlock *b = body; b; b = b->Next)
                bodyHeight += b->Size.y;

            float newHeight = std::max(kBodyMinHeight * GetUiScale(), bodyHeight);
            shift += newHeight - row.Height;
            row.Height = newHeight;
        }

        head->Layout.Size.y += shift;
        head->Size = head->Layout.Size;

        pos.y += head->Size.y;
        head = head->Next;
    }
}

void Canvas::LayoutBlock(VisualBlock &block)
{
    if (!block.Def)
        return;

    for (auto &[key, arg] : block.Args) {
        if (auto *held = std::get_if<std::unique_ptr<VisualBlock>>(&arg)) {
            if (VisualBlock *child = held->get())
                LayoutBlock(*child);
        }
    }

    const float scale = GetUiScale();

    LayoutMetrics m;
    m.Padding = ImGui::GetStyle().FramePadding;
    m.FontSize = ImGui::GetFontSize();
    m.LineHeightFactor = kLineHeightFactor;
    m.RowGap = kRowGap * scale;
    m.BodyMinHeight = kBodyMinHeight * scale;
    m.BodyIndent = kBodyIndent * scale;
    m.BodyBottomBarHeight = kBodyBottomBarHeight * scale;
    m.MinBlockWidth = kMinBlockWidth * scale;
    m.BodyNotchWidth *= scale;
    m.NotchHeight *= scale;
    m.VarSlotWidth *= scale;
    m.TokenGap *= scale;
    m.MinInputWidth *= scale;
    m.MaxInputWidth *= scale;

    block.Layout = ComputeBlockLayout(*block.Def, block, m);
    block.Size = block.Layout.Size;

    for (const RowLayout &row : block.Layout.Rows) {
        if (row.IsBody)
            continue;

        for (const SlotLayout &slot : row.Slots) {
            if (VisualBlock *child = GetPluggedArg(block, slot.Item->Name))
                child->Pos = block.Pos + slot.Pos;
        }
    }
}

VisualBlock *Canvas::HitChain(VisualBlock *block, ImVec2 mouse, ImVec2 origin)
{
    while (block) {
        for (auto &[_, body] : block->BodyRoots) {
            if (body) {
                if (auto *hit = HitChain(body, mouse, origin))
                    return hit;
            }
        }

        for (auto &[_, arg] : block->Args) {
            if (auto *held = std::get_if<std::unique_ptr<VisualBlock>>(&arg)) {
                if (held->get()) {
                    if (auto *hit = HitChain(held->get(), mouse, origin))
                        return hit;
                }
            }
        }

        ImVec2 tl = WorldToScreen(block->Pos, origin);
        ImVec2 br = tl + block->Size * Zoom;

        if (mouse.x >= tl.x &&
            mouse.x <= br.x &&
            mouse.y >= tl.y &&
            mouse.y <= br.y)
            return block;

        block = block->Next;
    }

    return nullptr;
}

VisualBlock *Canvas::HitTest(ImVec2 mouse, ImVec2 origin)
{
    const auto &roots = Manager.Roots;

    for (auto it = roots.rbegin(); it != roots.rend(); ++it) {
        if (auto *hit = HitChain(*it, mouse, origin))
            return hit;
    }

    return nullptr;
}

void Canvas::DrawBlock(ImDrawList *drawList, VisualBlock &block, ImVec2 origin)
{
    if (!block.Def)
        return;

    ImVec2 topLeft = WorldToScreen(block.Pos, origin);
    ImU32 bodyColor = CategoryColor(block.Def->Category);

    ImU32 borderColor = IM_COL32(0, 0, 0, 160);
    float borderThickness = 1.0f * Zoom;

    BlockOutline outline = BuildOutline(block, topLeft, Zoom);
    outline.Fill(drawList, bodyColor);
    outline.Stroke(drawList, borderColor, borderThickness);

    ImFont *font = ImGui::GetFont();
    float fontSize = ImGui::GetFontSize() * Zoom;

    bool interactive = (block.Id == HoveredBlockId) || (block.Id == ActiveBlockId);

    VariableSlotContext varContext;
    varContext.Registry = Registry;
    varContext.RequestVariableCreation = [this](VisualBlock *target, const std::string &key, Value type)
    {
        RequestVariableCreation(target, key, type);
    };


    bool stillFocused = DrawBlockLayout(
            drawList,
            block,
            topLeft,
            Zoom,
            font,
            fontSize,
            IM_COL32(255, 255, 255, 255),
            interactive,
            varContext);

    if (stillFocused)
        ActiveBlockId = block.Id;
    else if (ActiveBlockId == block.Id)
        ActiveBlockId = 0;
}

void Canvas::DrawChain(ImDrawList *drawList, VisualBlock *block, ImVec2 origin)
{
    while (block) {
        DrawBlock(drawList, *block, origin);

        for (auto &[slot, body] : block->BodyRoots)
            DrawChain(drawList, body, origin);

        for (auto &[key, arg] : block->Args) {
            if (auto *held = std::get_if<std::unique_ptr<VisualBlock>>(&arg)) {
                if (held->get())
                    DrawChain(drawList, held->get(), origin);
            }
        }

        block = block->Next;
    }
}

void Canvas::BringRootToFront(VisualBlock *root)
{
    auto &roots = Manager.Roots;
    auto it = std::find(roots.begin(), roots.end(), root);

    if (it != roots.end())
        std::rotate(it, it + 1, roots.end());
}

float Canvas::FindBodyTop(const VisualBlock &block, const std::string &slot)
{
    for (const RowLayout &row : block.Layout.Rows) {
        if (!row.IsBody)
            continue;

        if (!row.BodyItem)
            continue;

        if (row.BodyItem->Name == slot)
            return row.Top;
    }

    return 0.0f;
}

VisualBlock *Canvas::AddBlock(const BlockDefinition &def, ImVec2 worldPos)
{
    VisualBlock *result = Manager.AddBlock(def, worldPos);
    if (result) SelectedId = result->Id;
    return result;
}

void Canvas::DeleteBlock(VisualBlock *block, DeleteType type)
{
    if (type != DeleteType::Args && type != DeleteType::Bodies && block->Id == SelectedId)
        SelectedId = 0;

    switch (type) {
        case DeleteType::Normal:
            Manager.DeleteBlock(block);
            break;
        case DeleteType::Below:
            Manager.DeleteBelow(block);
            break;
        case DeleteType::Above:
            Manager.DeleteAbove(block);
            break;
        case DeleteType::Args:
            Manager.DeleteArgs(block);
            break;
        case DeleteType::WithoutArgs:
            Manager.DeleteWithoutArgs(block);
            break;
        case DeleteType::Bodies:
            Manager.DeleteBodies(block);
            break;
        case DeleteType::WithoutBodies:
            Manager.DeleteWithoutBodies(block);
            break;
    }
}

void Canvas::DuplicateBlock(VisualBlock *block, DuplicateType type)
{
    if (!block)
        return;

    VisualBlock *result = nullptr;

    switch (type) {
        case DuplicateType::Normal:
            result = Manager.DuplicateBlock(block);
            break;
        case DuplicateType::Below:
            result = Manager.DuplicateBelow(block);
            break;
        case DuplicateType::Above:
            result = Manager.DuplicateAbove(block);
            break;
        case DuplicateType::WithoutArgs:
            result = Manager.DuplicateBlockWithoutArgs(block);
            break;
        case DuplicateType::WithoutBodies:
            result = Manager.DuplicateBlockWithoutBodies(block);
            break;
    }

    if (result) SelectedId = result->Id;
}

ImVec2 Canvas::WorldToScreen(ImVec2 world, ImVec2 origin) const
{
    return ImVec2(origin.x + (world.x + Pan.x) * Zoom,
            origin.y + (world.y + Pan.y) * Zoom);
}

ImVec2 Canvas::ScreenToWorld(ImVec2 screen, ImVec2 origin) const
{
    return ImVec2((screen.x - origin.x) / Zoom - Pan.x,
            (screen.y - origin.y) / Zoom - Pan.y);
}

void Canvas::HandleBlockDrag(ImVec2 origin, bool hovered)
{
    ImGuiIO& io = ImGui::GetIO();

    if (DraggingId) {
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            VisualBlock *dragging = Manager.FindBlock(DraggingId);

            if (dragging) {
                SnapResult snap = Manager.FindSnapTarget(dragging, Zoom);
                if (snap.Block) {
                    switch (snap.Type) {
                        case SnapType::Append:
                            Manager.AttachAfter(snap.Block, dragging);
                            break;

                        case SnapType::Prepend:
                            Manager.AttachBefore(snap.Block, dragging);
                            break;

                        case SnapType::EnterBody:
                            Manager.AttachToBody(snap.Block, snap.Slot, dragging);
                            break;

                        case SnapType::EnterArg:
                            Manager.AttachToArg(snap.Block, snap.Slot, dragging);
                            break;

                        default:
                            break;
                    }
                }
            }

            DraggingId = 0;
            CurrentSnap = SnapResult{};
            return;
        }

        VisualBlock *root = Manager.FindBlock(DraggingId);

        if (!root) {
            DraggingId = 0;
            CurrentSnap = SnapResult{};
            return;
        }

        ImVec2 delta = io.MousePos - DragMouseStartScreen;
        root->Pos = DragBlockStartWorld + delta / Zoom;

        CurrentSnap = Manager.FindSnapTarget(root, Zoom);
        return;
    }

    CurrentSnap = SnapResult{};
}

void Canvas::TryBeginBlockDrag(ImVec2 origin, bool hovered)
{
    if (DraggingId)
        return;

    if (!hovered)
        return;
    if (!ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        return;

    ImGuiIO &io = ImGui::GetIO();
    VisualBlock *clicked = HitTest(io.MousePos, origin);

    if (!clicked)
        return;

    if (clicked->Id == ActiveBlockId)
        return;

    DraggingId = clicked->Id;
    SelectedId = clicked->Id;

    DragBlockStartWorld = clicked->Pos;
    DragMouseStartScreen = io.MousePos;

    Manager.Detach(clicked);
    BringRootToFront(clicked);
}

void Canvas::DrawSnapPreview(ImDrawList *drawList, ImVec2 origin)
{
    if (!CurrentSnap.Block)
        return;

    constexpr ImU32 kSnapColor = IM_COL32(245, 245, 245, 220);
    const float barThickness = 4.0f * Zoom;

    VisualBlock &target = *CurrentSnap.Block;

    switch (CurrentSnap.Type) {
        case SnapType::Append: {
           ImVec2 p = WorldToScreen(
                   target.Pos + ImVec2(0.0f, target.Size.y),
                   origin);

           DrawStatementSnapPreview(
                   drawList,
                   p,
                   target.Size.x * Zoom,
                   Zoom,
                   kSnapColor);
           break;
        }

        case SnapType::Prepend: {
           ImVec2 p = WorldToScreen(
                   target.Pos,
                   origin);

           DrawStatementSnapPreview(
                   drawList,
                   p,
                   target.Size.x * Zoom,
                   Zoom,
                   kSnapColor);
           break;
        }

        case SnapType::EnterBody: {
            float bodyTop = FindBodyTop(target, CurrentSnap.Slot);

            float bodyIndent = kBodyIndent * GetUiScale();
            ImVec2 p = WorldToScreen(target.Pos + ImVec2(bodyIndent, bodyTop), origin);
            float width = (target.Size.x - bodyIndent) * Zoom;

            DrawStatementSnapPreview(drawList, p, width, Zoom, kSnapColor);
            break;
        }

        case SnapType::EnterArg: {
            for (const RowLayout &row : target.Layout.Rows) {
                if (row.IsBody)
                    continue;

                for (const SlotLayout &slot : row.Slots) {
                    if (!slot.Item || slot.Item->Name != CurrentSnap.Slot)
                        continue;

                    ImVec2 slotTopLeft = WorldToScreen(target.Pos + slot.Pos, origin);
                    ImVec2 slotBottomRight = WorldToScreen(target.Pos + slot.Pos + slot.Size, origin);

                    ImVec2 outset(2.0f * Zoom, 2.0f * Zoom);

                    drawList->AddRect(
                            slotTopLeft - outset,
                            slotBottomRight + outset,
                            kSnapColor,
                            3.0f * Zoom,
                            0,
                            barThickness * 0.5f);
                    return;
                }
            }
            break;
        }

        default:
            break;
    }
}

void Canvas::HandlePanAndZoom(ImVec2 origin, bool hovered)
{
    ImGuiIO &io = ImGui::GetIO();

    if (hovered && io.MouseWheel != 0.0f && DraggingId == 0) {
        ImVec2 worldBefore = ScreenToWorld(io.MousePos, origin);

        float zoomFactor = std::pow(1.1f, io.MouseWheel);
        Zoom = std::clamp(Zoom * zoomFactor, kMinZoom, kMaxZoom);

        ImVec2 worldAfter = ScreenToWorld(io.MousePos, origin);
        Pan.x += worldAfter.x - worldBefore.x;
        Pan.y += worldAfter.y - worldBefore.y;
    }

    bool panButtonDown = ImGui::IsMouseDown(ImGuiMouseButton_Middle);

    if (!IsPanning && hovered && DraggingId == 0 &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
        IsPanning = true;
        PanMouseStart = io.MousePos;
        PanStart = Pan;
    }

    if (IsPanning) {
        if (!panButtonDown) {
            IsPanning = false;
        } else {
            ImVec2 deltaScreen = io.MousePos - PanMouseStart;
            Pan = PanStart + deltaScreen / Zoom;
        }
    }
}

static BlockOutline BuildChainOutline(
        ImVec2 topLeft,
        ImVec2 size,
        float notchInset,
        float notchWidth,
        float notchDepth,
        float spineWidth,
        const std::vector<RowLayout> &rows,
        float zoom)
{
    BlockOutline path(topLeft);

    path.Right(notchInset)
        .Tab(notchWidth, notchDepth)
        .Right(size.x - notchInset - notchWidth);

    for (const RowLayout& row : rows) {
        if (!row.IsBody)
            continue;

        path.RightEdgeMouth(
                topLeft.x + size.x,
                topLeft.x + spineWidth,
                topLeft.y + row.Top * zoom,
                topLeft.y + (row.Top + row.Height) * zoom,
                notchInset,
                notchWidth,
                notchDepth);
    }

    path.To(topLeft + ImVec2(size.x, size.y));

    path.Right(-(size.x - notchInset - notchWidth))
        .Tab(-notchWidth, notchDepth)
        .Right(-notchInset);

    return path;
}

static BlockOutline BuildHatOutline(
    ImVec2 topLeft,
    ImVec2 size,
    float notchInset,
    float notchWidth,
    float notchDepth,
    float spineWidth,
    const std::vector<RowLayout>& rows,
    float zoom)
{
    BlockOutline path(topLeft + ImVec2(0.0f, 12.0f * zoom));

    path.To(topLeft + ImVec2(size.x * 0.15f, 0.0f));
    path.To(topLeft + ImVec2(size.x * 0.85f, 0.0f));
    path.To(topLeft + ImVec2(size.x, 12.0f * zoom));

    for (const RowLayout& row : rows) {
        if (!row.IsBody)
            continue;

        path.RightEdgeMouth(
            topLeft.x + size.x,
            topLeft.x + spineWidth,
            topLeft.y + row.Top * zoom,
            topLeft.y + (row.Top + row.Height) * zoom,
            notchInset,
            notchWidth,
            notchDepth);
    }

    path.To(topLeft + ImVec2(size.x, size.y));

    path.Right(-(size.x - notchInset - notchWidth))
        .Tab(-notchWidth, notchDepth)
        .Right(-notchInset);

    return path;
}

static BlockOutline BuildCapOutline(
        ImVec2 topLeft,
        ImVec2 size,
        float notchInset,
        float notchWidth,
        float notchDepth,
        float spineWidth,
        const std::vector<RowLayout> &rows,
        float zoom)
{
    BlockOutline path(topLeft);

    path.Right(notchInset)
        .Tab(notchWidth, notchDepth)
        .Right(size.x - notchInset - notchWidth);

    for (const RowLayout& row : rows) {
        if (!row.IsBody)
            continue;

        path.RightEdgeMouth(
                topLeft.x + size.x,
                topLeft.x + spineWidth,
                topLeft.y + row.Top * zoom,
                topLeft.y + (row.Top + row.Height) * zoom,
                notchInset,
                notchWidth,
                notchDepth);
    }

    path.To(topLeft + ImVec2(size.x, size.y));
    path.To(topLeft + ImVec2(0.0f, size.y));

    return path;
}

static BlockOutline BuildReporterOutline(
    ImVec2 topLeft,
    ImVec2 size,
    float radius)
{
    DISCARD(radius);
    BlockOutline path(topLeft);

    path.To(topLeft + ImVec2(size.x, 0.0f));
    path.To(topLeft + ImVec2(size.x, size.y));
    path.To(topLeft + ImVec2(0.0f, size.y));

    return path;
}

static BlockOutline BuildBooleanOutline(
    ImVec2 topLeft,
    ImVec2 size)
{
    float inset = size.y * kBoolInsetRatio;

    BlockOutline path(topLeft + ImVec2(inset, 0));

    path.To(topLeft + ImVec2(size.x - inset, 0));
    path.To(topLeft + ImVec2(size.x, size.y * 0.5f));
    path.To(topLeft + ImVec2(size.x - inset, size.y));
    path.To(topLeft + ImVec2(inset, size.y));
    path.To(topLeft + ImVec2(0, size.y * 0.5f));

    return path;
}

BlockOutline BuildOutline(
    const VisualBlock &block,
    ImVec2 topLeft,
    float zoom)
{
    ImVec2 screenSize = block.Size * zoom;

    const float effectiveZoom = zoom * GetUiScale();

    float notchInset = 14.0f * effectiveZoom;
    float notchWidth = 22.0f * effectiveZoom;
    float notchDepth = 6.0f * effectiveZoom;
    float bodyIndent = kBodyIndent * effectiveZoom;

    switch (block.Def->Shape)
    {
        case BlockShape::Chain:
            return BuildChainOutline(
                    topLeft,
                    screenSize,
                    notchInset,
                    notchWidth,
                    notchDepth,
                    bodyIndent,
                    block.Layout.Rows,
                    zoom);

        case BlockShape::Hat:
            return BuildHatOutline(
                    topLeft,
                    screenSize,
                    notchInset,
                    notchWidth,
                    notchDepth,
                    bodyIndent,
                    block.Layout.Rows,
                    zoom);

        case BlockShape::Cap:
            return BuildCapOutline(
                    topLeft,
                    screenSize,
                    notchInset,
                    notchWidth,
                    notchDepth,
                    bodyIndent,
                    block.Layout.Rows,
                    zoom);

        case BlockShape::Reporter: {
            if (block.Def->ReturnType == VAL_ANY ||
                !(block.Def->ReturnType & VAL_BOOL))
                return BuildReporterOutline(
                        topLeft,
                        screenSize,
                        screenSize.y * 0.5f);

            return BuildBooleanOutline(
                topLeft,
                screenSize);
       }

        default:
            return BuildReporterOutline(
                    topLeft,
                    screenSize,
                    screenSize.y * 0.5f);
    }
}

static void DrawStatementSnapPreview(
    ImDrawList* drawList,
    ImVec2 pos,
    float width,
    float zoom,
    ImU32 color)
{
    const float notchInset = 14.0f * zoom;
    const float notchWidth = 22.0f * zoom;
    const float notchDepth = 6.0f * zoom;

    BlockOutline outline(pos + ImVec2(width, 0));

    outline
        .Right(-(width - notchInset - notchWidth))
        .Tab(-notchWidth, notchDepth)
        .Right(-notchInset);

    outline.StrokeOpen(drawList, color, 3.0f * zoom);
}
