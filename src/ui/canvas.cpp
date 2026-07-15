#include <cmath>

#include "ui/canvas.hpp"
#include "ui/const.hpp"
#include "util/macro.hpp"

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

std::unique_ptr<BlockInstance> CloneInstance(const BlockInstance &src)
{
    auto copy = std::make_unique<BlockInstance>();
    copy->OpCode = src.OpCode;

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

    Hovered = ImGui::IsWindowHovered();
    HandlePanAndZoom(origin, Hovered);
    HandleBlockDrag(origin, Hovered);
    HandleContextMenu(origin, Hovered);

    DrawComments(origin);

    for (VisualBlock *root : Manager.Roots)
        DrawChain(drawList, root, origin);

    DrawSnapPreview(drawList, origin);

    ImGui::EndChild();
    ImGui::PopID();
}

void Canvas::DrawGrid(ImDrawList *drawList, ImVec2 origin, ImVec2 size)
{
    drawList->AddRectFilled(origin, origin + size, IM_COL32(30, 30, 34, 255));

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

            body->Pos = ImVec2(head->Pos.x + kBodyIndent, head->Pos.y + row.Top);

            LayoutChain(body);

            float bodyHeight = 0.0f;
            for (VisualBlock *b = body; b; b = b->Next)
                bodyHeight += b->Size.y;

            float newHeight = std::max(kBodyMinHeight, bodyHeight);
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

    LayoutMetrics m;
    m.Padding = ImGui::GetStyle().FramePadding;
    m.FontSize = ImGui::GetFontSize();
    m.LineHeightFactor = kLineHeightFactor;
    m.RowGap = kRowGap;
    m.BodyMinHeight = kBodyMinHeight;
    m.BodyIndent = kBodyIndent;
    m.BodyBottomBarHeight = kBodyBottomBarHeight;
    m.MinBlockWidth = kMinBlockWidth;

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
    DrawBlockLayout(
            drawList,
            block,
            topLeft,
            Zoom,
            font,
            fontSize,
            IM_COL32(255, 255, 255, 255),
            true);
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

void Canvas::AddBlock(const BlockDefinition &def, ImVec2 worldPos)
{
    VisualBlock *result = Manager.AddBlock(def, worldPos);
    if (result) SelectedId = result->Id;
}

void Canvas::DeleteBlock(VisualBlock *block, DeleteType type)
{
    if (block->Id == SelectedId) SelectedId = 0;
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
    }
}

void Canvas::DuplicateBlock(VisualBlock *block)
{
    VisualBlock *result = Manager.DuplicateBlock(block);
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

    if (!hovered)
        return;
    if (!ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        return;
    if (ImGui::IsAnyItemHovered() || ImGui::IsAnyItemActive())
        return;

    VisualBlock *clicked = HitTest(io.MousePos, origin);

    if (!clicked)
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
    if (!DraggingId || !CurrentSnap.Block)
        return;

    constexpr ImU32 kSnapColor = IM_COL32(245, 245, 245, 220);
    const float barThickness = 4.0f * Zoom;

    VisualBlock &target = *CurrentSnap.Block;

    switch (CurrentSnap.Type) {
        case SnapType::Append: {
            ImVec2 bl = WorldToScreen(target.Pos + ImVec2(0.0f, target.Size.y), origin);
            ImVec2 br = WorldToScreen(target.Pos + ImVec2(target.Size.x, target.Size.y), origin);

            drawList->AddRectFilled(
                    ImVec2(bl.x, bl.y - barThickness * 0.5f),
                    ImVec2(br.x, br.y + barThickness * 0.5f),
                    kSnapColor,
                    barThickness * 0.5f);
            break;
        }

        case SnapType::Prepend: {
            ImVec2 tl = WorldToScreen(target.Pos, origin);
            ImVec2 tr = WorldToScreen(target.Pos + ImVec2(target.Size.x, 0.0f), origin);

            drawList->AddRectFilled(
                    ImVec2(tl.x, tl.y - barThickness * 0.5f),
                    ImVec2(tr.x, tr.y + barThickness * 0.5f),
                    kSnapColor,
                    barThickness * 0.5f);
            break;
        }

        case SnapType::EnterBody: {
            float bodyTop = FindBodyTop(target, CurrentSnap.Slot);

            ImVec2 tl = WorldToScreen(target.Pos + ImVec2(kBodyIndent, bodyTop), origin);
            ImVec2 tr = WorldToScreen(target.Pos + ImVec2(target.Size.x - kBodyIndent * 0.5f, bodyTop), origin);

            drawList->AddRectFilled(
                    ImVec2(tl.x, tl.y - barThickness * 0.5f),
                    ImVec2(tr.x, tr.y + barThickness * 0.5f),
                    kSnapColor,
                    barThickness * 0.5f);
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

void Canvas::HandleContextMenu(ImVec2 origin, bool hovered)
{
    if (hovered && DraggingId == 0 && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        if (ImGui::IsAnyItemHovered() || ImGui::IsAnyItemActive())
            return;

        ContextMenuOnBlock = false;
        ContextMenuBlockId = 0;

        for (auto it = Manager.Blocks.rbegin(); it != Manager.Blocks.rend(); ++it) {
            VisualBlock &block = **it;
            ImVec2 topLeft = WorldToScreen(block.Pos, origin);
            ImVec2 bottomRight = WorldToScreen(block.Pos + block.Size, origin);
            ImVec2 mouse = ImGui::GetIO().MousePos;

            bool inside = mouse.x >= topLeft.x && mouse.x <= bottomRight.x &&
                          mouse.y >= topLeft.y && mouse.y <= bottomRight.y;
            if (!inside)
                continue;

            ContextMenuOnBlock = true;
            ContextMenuBlockId = block.Id;
            SelectedId = block.Id;
            break;
        }

        ImGui::OpenPopup("##canvas_context_menu");
    }

    if (ImGui::BeginPopup("##canvas_context_menu")) {
        if (ContextMenuOnBlock) {
            auto block = Manager.FindBlock(ContextMenuBlockId);

            if (block) {
                if (ImGui::MenuItem("Duplicate"))
                    DuplicateBlock(block);

                if (ImGui::MenuItem("Delete"))
                    DeleteBlock(block, DeleteType::Normal);

                if (block->Next && ImGui::MenuItem("Delete Below"))
                    DeleteBlock(block, DeleteType::Below);

                if (block->Prev && ImGui::MenuItem("Delete Above"))
                    DeleteBlock(block, DeleteType::Above);
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

static std::string DebugArgValue(const VisualArg &arg)
{
    if (auto *lit = std::get_if<LiteralValue>(&arg)) {
        return std::visit([](const auto &v) -> std::string
        {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, std::string>) return "\"" + v + "\"";
            else if constexpr (std::is_same_v<T, bool>)    return v ? "true" : "false";
            else                                           return std::to_string(v);
        }, *lit);
    }

    if (auto *ref = std::get_if<VariableRef>(&arg))
        return "var: " + (ref->Name.empty() ? std::string("(unset)") : ref->Name);

    if (auto *held = std::get_if<std::unique_ptr<VisualBlock>>(&arg)) {
        VisualBlock *plugged = held->get();
        if (!plugged)
            return "(empty)";
        return "expr: " + (plugged->Def ? plugged->Def->OpCode : std::string("unknown"))
             + " (#" + std::to_string(plugged->Id) + ")";
    }

    return "?";
}

static void DrawChainDebug(VisualBlock *chain)
{
    int index = 0;

    for (VisualBlock *block = chain; block; block = block->Next) {
        ImGui::BulletText(
                "[%d] ID %u - %s",
                index++,
                block->Id,
                block->Def
                ? block->Def->OpCode.c_str()
                : "unknown");

        if (block->BodyRoots.empty() && block->Args.empty())
            continue;

        ImGui::Indent();

        for (auto &[slot, body] : block->BodyRoots) {
            ImGui::TextDisabled("Body \"%s\":", slot.c_str());

            ImGui::Indent();
            if (body)
                DrawChainDebug(body);
            else
                ImGui::TextDisabled("(empty)");
            ImGui::Unindent();
        }

        for (auto &[key, arg] : block->Args) {
            if (auto *held = std::get_if<std::unique_ptr<VisualBlock>>(&arg); held && held->get()) {
                ImGui::TextDisabled("Arg \"%s\":", key.c_str());
                ImGui::Indent();
                DrawChainDebug(held->get());
                ImGui::Unindent();
            } else {
                ImGui::BulletText("Arg \"%s\": %s", key.c_str(), DebugArgValue(arg).c_str());
            }
        }

        ImGui::Unindent();
    }
}

void Canvas::DrawDebugWindow()
{
    ImGui::Begin("Block Debug", &ShowDebugWindow);

    const auto &blocks = Manager.Blocks;
    const auto &roots = Manager.Roots;

    ImGui::Text("Total Blocks: %zu", blocks.size());
    ImGui::Text("Root Blocks: %zu", roots.size());
    ImGui::Separator();

    if (SelectedId == 0) {
        ImGui::Text("Selected: none");
        ImGui::End();
        return;
    }

    VisualBlock *selected = Manager.FindBlock(SelectedId);
    if (!selected) {
        ImGui::Text("Selected block missing");
        ImGui::End();
        return;
    }

    ImGui::Text("Selected ID: %u", selected->Id);
    ImGui::Text( "Position: %.1f, %.1f", selected->Pos.x, selected->Pos.y);

    ImGui::Separator();

    if (selected->Def) {
        ImGui::Text("Opcode: %s", selected->Def->OpCode.c_str());
        ImGui::Text("Category: %d", (int)selected->Def->Category);
        ImGui::Text("Statement: %s", selected->Def->StmtBuilder ? "yes" : "no");

        ImGui::Text("Expression: %s", selected->Def->ExprBuilder ? "yes" : "no");
    }

    ImGui::Separator();

    ImGui::Text("Prev: %s", selected->Prev ? std::to_string(selected->Prev->Id).c_str() : "none");
    ImGui::Text( "Next: %s", selected->Next ? std::to_string(selected->Next->Id).c_str() : "none");

    if (selected->BodyOwner) {
        ImGui::Text("Body Owner: %u (slot \"%s\")", selected->BodyOwner->Id, selected->BodySlot.c_str());
    }

    ImGui::Separator();

    ImGui::Text("Bodies: %zu", selected->BodyRoots.size());

    for (auto &[slot, body] : selected->BodyRoots) {
        ImGui::BulletText("\"%s\": %s", slot.c_str(),
                body ? std::to_string(body->Id).c_str() : "empty");
    }

    ImGui::Separator();

    if (selected->ArgOwner) {
        ImGui::Text("Arg Owner: %u (slot \"%s\")", selected->ArgOwner->Id, selected->ArgSlot.c_str());
        ImGui::Separator();
    }

    ImGui::Text("Args: %zu", selected->Args.size());

    for (auto &[key, arg] : selected->Args) {
        ImGui::BulletText("\"%s\": %s", key.c_str(), DebugArgValue(arg).c_str());
    }

    ImGui::Separator();

    ImGui::Text("Chain:");

    VisualBlock *root = selected;
    while (root->Prev)
        root = root->Prev;

    DrawChainDebug(root);

    ImGui::End();

    if (ImGui::IsWindowAppearing())
        ImGui::SetWindowFocus("Block Debug");
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

    float notchInset = 14.0f * zoom;
    float notchWidth = 22.0f * zoom;
    float notchDepth = 6.0f * zoom;
    float bodyIndent = kBodyIndent * zoom;

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

void Canvas::DrawComments(ImVec2 origin)
{
    const ImVec2 kCommentMinSize(120.0f, 80.0f);

    for (CanvasComment &c : Comments) {
        ImVec2 screenPos = WorldToScreen(c.Pos, origin);

        if (!ImGui::IsMouseDragging(ImGuiMouseButton_Left))
            ImGui::SetNextWindowPos(screenPos);
        ImGui::SetNextWindowSize(c.Size * Zoom, ImGuiCond_Always);

        // Paper/ink palette for the note itself...
        ImGui::PushStyleColor(ImGuiCol_WindowBg,
                IM_COL32(255,243,140,255));
        ImGui::PushStyleColor(ImGuiCol_TitleBg,
                IM_COL32(255,220,80,255));
        ImGui::PushStyleColor(ImGuiCol_TitleBgActive,
                IM_COL32(255,220,80,255));
        ImGui::PushStyleColor(ImGuiCol_Border,
                IM_COL32(180,170,70,255));
        ImGui::PushStyleColor(ImGuiCol_Text,
                IM_COL32(60,50,10,255));

        ImGui::PushStyleColor(ImGuiCol_FrameBg,
                IM_COL32(255,243,140,255));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,
                IM_COL32(255,243,140,255));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive,
                IM_COL32(255,243,140,255));

        std::string name = "##comment" + std::to_string(c.Id);

        ImGui::Begin(
                name.c_str(),
                nullptr,
                ImGuiWindowFlags_NoCollapse);

        ImFont *font = ImGui::GetFont();
        float fontSize = ImGui::GetFontSize() * Zoom;
        ImGui::PushFont(font, fontSize);

        ImGui::InputTextMultiline(
                "##text",
                &c.Text,
                ImGui::GetContentRegionAvail());

        ImGui::PopFont();

        c.Pos = ScreenToWorld(
                ImGui::GetWindowPos(),
                origin);

        ImVec2 worldSize = ImGui::GetWindowSize() / Zoom;
        c.Size = ImVec2(
                std::max(worldSize.x, kCommentMinSize.x),
                std::max(worldSize.y, kCommentMinSize.y));

        ImGui::End();
        ImGui::PopStyleColor(8);
    }
}
