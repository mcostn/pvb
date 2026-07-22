#include <cmath>
#include <algorithm>
#include <functional>
#include <cstdio>

#include "ui/canvas.hpp"
#include "ui/const.hpp"
#include "ui/custom_block.hpp"
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
        DrawCreateVariablePopup(*Registry);
        DrawCreateCustomBlockPopup(*Registry);
        DrawRenameVariablePopup(*Registry);
        DrawRenameCustomBlockPopup(*Registry);
    }
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

            ImVec2 p = WorldToScreen(target.Pos + ImVec2(kBodyIndent, bodyTop), origin);
            float width = (target.Size.x - kBodyIndent) * Zoom;

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

void Canvas::HandleContextMenu(ImVec2 origin, bool hovered)
{
    if (hovered && DraggingId == 0 && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        if (ImGui::IsAnyItemHovered() || ImGui::IsAnyItemActive())
            return;

        ContextMenuOnBlock = false;
        ContextMenuBlockId = 0;

        if (VisualBlock *hit = HitTest(ImGui::GetIO().MousePos, origin)) {
            ContextMenuOnBlock = true;
            ContextMenuBlockId = hit->Id;
            SelectedId = hit->Id;
        }

        ImGui::SetNextWindowPos(ImGui::GetIO().MousePos);
        ImGui::OpenPopup("##canvas_context_menu");
    }

    if (ImGui::BeginPopup("##canvas_context_menu")) {
        if (ContextMenuOnBlock) {
            auto block = Manager.FindBlock(ContextMenuBlockId);

            if (block) {
                bool hasArgs = Manager.HasPluggedArgs(block);
                bool hasBodies = Manager.HasBodies(block);

                if (ImGui::BeginMenu("Duplicate")) {
                    if (ImGui::MenuItem("Duplicate just Block"))
                        DuplicateBlock(block, DuplicateType::Normal);

                    if (block->Next && ImGui::MenuItem("Duplicate Below"))
                        DuplicateBlock(block, DuplicateType::Below);

                    if (block->Prev && ImGui::MenuItem("Duplicate Above"))
                        DuplicateBlock(block, DuplicateType::Above);

                    if (hasArgs && ImGui::MenuItem("Duplicate Without Args"))
                        DuplicateBlock(block, DuplicateType::WithoutArgs);

                    if (hasBodies && ImGui::MenuItem("Duplicate Without Bodies"))
                        DuplicateBlock(block, DuplicateType::WithoutBodies);

                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Delete")) {
                    if (ImGui::MenuItem("Delete just Block"))
                        DeleteBlock(block, DeleteType::Normal);

                    if (block->Next && ImGui::MenuItem("Delete Below"))
                        DeleteBlock(block, DeleteType::Below);

                    if (block->Prev && ImGui::MenuItem("Delete Above"))
                        DeleteBlock(block, DeleteType::Above);

                    if (hasArgs && ImGui::MenuItem("Delete Args"))
                        DeleteBlock(block, DeleteType::Args);

                    if (hasArgs && ImGui::MenuItem("Delete Without Args"))
                        DeleteBlock(block, DeleteType::WithoutArgs);

                    if (hasBodies && ImGui::MenuItem("Delete Bodies"))
                        DeleteBlock(block, DeleteType::Bodies);

                    if (hasBodies && ImGui::MenuItem("Delete Without Bodies"))
                        DeleteBlock(block, DeleteType::WithoutBodies);

                    ImGui::EndMenu();
                }

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

static void CollectBlocksMatching(
        VisualBlock *block,
        const std::function<bool(const VisualBlock &)> &pred,
        std::vector<VisualBlock *> &out)
{
    if (!block)
        return;

    if (pred(*block)) {
        out.push_back(block);
        return;
    }

    for (auto &[key, arg] : block->Args) {
        if (auto *held = std::get_if<std::unique_ptr<VisualBlock>>(&arg))
            CollectBlocksMatching(held->get(), pred, out);
    }
}

static void ClearVariableRefs(VisualBlock *block, const std::string &name)
{
    if (!block)
        return;

    for (auto &[key, arg] : block->Args) {
        if (auto *ref = std::get_if<VariableRef>(&arg)) {
            if (ref->Name != name)
                continue;

            const BlockSchemaItem *item = nullptr;
            if (block->Def) {
                for (const BlockSchemaItem &schemaItem : block->Def->Schema) {
                    if (schemaItem.Name == key) {
                        item = &schemaItem;
                        break;
                    }
                }
            }

            arg = (item && block->Def)
                ? MakeDefaultArg(*block->Def, *item)
                : VisualArg{ LiteralValue{ std::in_place_type<int>, 0 } };

            continue;
        }

        if (auto *held = std::get_if<std::unique_ptr<VisualBlock>>(&arg))
            ClearVariableRefs(held->get(), name);
    }
}

static void RenameVariableRefs(VisualBlock *block, const std::string &oldName, const std::string &newName)
{
    if (!block)
        return;

    for (auto &[key, arg] : block->Args) {
        if (auto *ref = std::get_if<VariableRef>(&arg)) {
            if (ref->Name == oldName)
                ref->Name = newName;
            continue;
        }

        if (auto *held = std::get_if<std::unique_ptr<VisualBlock>>(&arg))
            RenameVariableRefs(held->get(), oldName, newName);
    }
}

void Canvas::DeleteVariable(const std::string &name)
{
    if (!Registry || !Registry->HasVariable(name))
        return;

    const std::string getOp = VarGetOpCode(name);
    const std::string setOp = VarSetOpCode(name);

    auto isVariableBlock = [&](const VisualBlock &b) {
        return b.Def && (b.Def->OpCode == getOp || b.Def->OpCode == setOp);
    };

    std::vector<VisualBlock *> toDelete;
    for (auto &blockPtr : Manager.Blocks)
        CollectBlocksMatching(blockPtr.get(), isVariableBlock, toDelete);

    for (auto &blockPtr : Manager.Blocks)
        ClearVariableRefs(blockPtr.get(), name);

    for (VisualBlock *block : toDelete)
        Manager.DeleteBlock(block);

    DISCARD(Registry->RemoveVariable(name));
}

Error Canvas::RenameVariable(const std::string &oldName, const std::string &newName)
{
    if (!Registry)
        return Error::Failed;

    Error err = Registry->RenameVariable(oldName, newName);
    if (err != Error::Ok)
        return err;

    for (auto &blockPtr : Manager.Blocks)
        RenameVariableRefs(blockPtr.get(), oldName, newName);

    return Error::Ok;
}

void Canvas::RequestVariableCreation(VisualBlock *targetBlock, const std::string &targetKey, Value requiredType)
{
    VarCreateRequest.Requested = true;
    VarCreateRequest.TargetBlock = targetBlock;
    VarCreateRequest.TargetKey = targetKey;
    VarCreateRequest.RequiredType = requiredType;

    static const Value kTypesInOrder[] = { VAL_INT, VAL_FLOAT, VAL_BOOL, VAL_STRING };

    NewVarTypeIndex = 0;
    for (int i = 0; i < 4; ++i) {
        if (requiredType & kTypesInOrder[i]) {
            NewVarTypeIndex = i;
            break;
        }
    }

    NewVarNameBuf[0] = '\0';
    NewVarError.clear();
}

void Canvas::DrawCreateVariablePopup(BlockRegistry &registry)
{
    static const char *kAllNames[]  = { "Int", "Float", "Bool", "String" };
    static const Value  kAllValues[] = { VAL_INT, VAL_FLOAT, VAL_BOOL, VAL_STRING };

    if (VarCreateRequest.Requested) {
        ImGui::OpenPopup("##canvas_create_variable_popup");
        VarCreateRequest.Requested = false;
    }

    if (!ImGui::BeginPopup("##canvas_create_variable_popup"))
        return;

    const char *allowedNames[4];
    Value allowedValues[4];
    int allowedCount = 0;

    for (int i = 0; i < 4; ++i) {
        if (VarCreateRequest.RequiredType & kAllValues[i]) {
            allowedNames[allowedCount] = kAllNames[i];
            allowedValues[allowedCount] = kAllValues[i];
            allowedCount++;
        }
    }

    if (allowedCount == 0) {
        allowedNames[0] = kAllNames[0];
        allowedValues[0] = kAllValues[0];
        allowedCount = 1;
    }

    NewVarTypeIndex = std::clamp(NewVarTypeIndex, 0, allowedCount - 1);

    ImGui::TextUnformatted("New Variable");
    ImGui::Separator();

    constexpr float PopupFieldWidth = 220.0f;

    ImGui::SetNextItemWidth(PopupFieldWidth);
    if (ImGui::IsWindowAppearing())
        ImGui::SetKeyboardFocusHere(0);
    bool enterPressed = ImGui::InputTextWithHint(
            "##new_var_name",
            "name",
            NewVarNameBuf,
            sizeof(NewVarNameBuf),
            ImGuiInputTextFlags_EnterReturnsTrue);

    ImGui::SetNextItemWidth(PopupFieldWidth);
    ImGui::Combo("##new_var_type", &NewVarTypeIndex, allowedNames, allowedCount);

    if (!NewVarError.empty()) {
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + PopupFieldWidth);
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", NewVarError.c_str());
        ImGui::PopTextWrapPos();
    }

    bool createClicked = ImGui::Button("Create", ImVec2(PopupFieldWidth * 0.5f - 4.0f, 0.0f));
    ImGui::SameLine();
    bool cancelClicked = ImGui::Button("Cancel", ImVec2(PopupFieldWidth * 0.5f - 4.0f, 0.0f));

    if (createClicked || enterPressed) {
        std::string name(NewVarNameBuf);
        Value type = allowedValues[NewVarTypeIndex];

        Error err = registry.AddVariable(name, type);

        if (err == Error::Ok) {
            if (VarCreateRequest.TargetBlock && !VarCreateRequest.TargetKey.empty())
                VarCreateRequest.TargetBlock->Args[VarCreateRequest.TargetKey] = VisualArg{ VariableRef{ name, type } };

            NewVarError.clear();
            NewVarNameBuf[0] = '\0';
            VarCreateRequest = PendingVariableCreate{};
            ImGui::CloseCurrentPopup();
        } else if (err == Error::BlockAlreadyExists) {
            NewVarError = "A variable named '" + name + "' already exists";
        } else {
            NewVarError = "Couldn't create variable '" + name + "'";
        }
    }

    if (cancelClicked) {
        NewVarError.clear();
        VarCreateRequest = PendingVariableCreate{};
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

void Canvas::RequestVariableRename(const std::string &name)
{
    VarRenameRequest.Requested = true;
    VarRenameRequest.OldName = name;

    std::snprintf(RenameVarNameBuf, sizeof(RenameVarNameBuf), "%s", name.c_str());
    RenameVarError.clear();
}

void Canvas::DrawRenameVariablePopup(BlockRegistry &registry)
{
    if (VarRenameRequest.Requested) {
        ImGui::OpenPopup("##canvas_rename_variable_popup");
        VarRenameRequest.Requested = false;
    }

    if (!ImGui::BeginPopup("##canvas_rename_variable_popup"))
        return;

    constexpr float PopupFieldWidth = 220.0f;

    ImGui::TextUnformatted("Rename Variable");
    ImGui::Separator();

    ImGui::SetNextItemWidth(PopupFieldWidth);
    if (ImGui::IsWindowAppearing()) {
        ImGui::SetKeyboardFocusHere(0);
    }
    bool enterPressed = ImGui::InputTextWithHint(
            "##rename_var_name",
            "name",
            RenameVarNameBuf,
            sizeof(RenameVarNameBuf),
            ImGuiInputTextFlags_EnterReturnsTrue);

    if (!RenameVarError.empty()) {
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + PopupFieldWidth);
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", RenameVarError.c_str());
        ImGui::PopTextWrapPos();
    }

    bool renameClicked = ImGui::Button("Rename", ImVec2(PopupFieldWidth * 0.5f - 4.0f, 0.0f));
    ImGui::SameLine();
    bool cancelClicked = ImGui::Button("Cancel", ImVec2(PopupFieldWidth * 0.5f - 4.0f, 0.0f));

    if (renameClicked || enterPressed) {
        std::string newName(RenameVarNameBuf);
        Error err = RenameVariable(VarRenameRequest.OldName, newName);

        if (err == Error::Ok) {
            RenameVarError.clear();
            VarRenameRequest = PendingVariableRename{};
            ImGui::CloseCurrentPopup();
        } else if (err == Error::BlockAlreadyExists) {
            RenameVarError = "A variable named '" + newName + "' already exists";
        } else {
            RenameVarError = "Couldn't rename variable to '" + newName + "'";
        }
    }

    if (cancelClicked) {
        RenameVarError.clear();
        VarRenameRequest = PendingVariableRename{};
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

void Canvas::DeleteCustomBlock(const std::string &name)
{
    if (!Registry || !IsCustomBlockRegistered(*Registry, name))
        return;

    const std::string callOp = CustomCallOpCode(name);
    const std::string hatOp  = CustomHatOpCode(name);

    std::vector<std::string> paramOps;
    for (const BlockDefinition *paramDef : CustomBlockParamDefs(*Registry, name))
        paramOps.push_back(paramDef->OpCode);

    std::vector<VisualBlock *> hatRoots;
    for (VisualBlock *root : Manager.Roots)
        if (root->Def && root->Def->OpCode == hatOp)
            hatRoots.push_back(root);

    for (VisualBlock *hatRoot : hatRoots)
        Manager.DeleteBelow(hatRoot);

    auto isCallOrParamBlock = [&](const VisualBlock &b) {
        if (!b.Def)
            return false;
        if (b.Def->OpCode == callOp)
            return true;
        return std::find(paramOps.begin(), paramOps.end(), b.Def->OpCode) != paramOps.end();
    };

    std::vector<VisualBlock *> toDelete;
    for (auto &blockPtr : Manager.Blocks)
        CollectBlocksMatching(blockPtr.get(), isCallOrParamBlock, toDelete);

    for (VisualBlock *block : toDelete)
        Manager.DeleteBlock(block);

    DISCARD(UnregisterCustomBlock(*Registry, name));
}

void Canvas::RequestCustomBlockCreation()
{
    CustomBlockCreateRequested = true;
    NewCustomNameBuf[0] = '\0';
    NewCustomDescBuf[0] = '\0';
    NewCustomParams.clear();
    NewCustomError.clear();
}

void Canvas::DrawCreateCustomBlockPopup(BlockRegistry &registry)
{
    static const char *kTypeNames[]  = { "Int", "Float", "Bool", "String" };
    static const Value  kTypeValues[] = { VAL_INT, VAL_FLOAT, VAL_BOOL, VAL_STRING };

    if (CustomBlockCreateRequested) {
        ImGui::OpenPopup("##canvas_create_custom_block_popup");
        CustomBlockCreateRequested = false;
    }

    if (!ImGui::BeginPopup("##canvas_create_custom_block_popup"))
        return;

    constexpr float PopupFieldWidth = 280.0f;

    ImGui::TextUnformatted("Create a Block");
    ImGui::Separator();

    ImGui::SetNextItemWidth(PopupFieldWidth);
    if (ImGui::IsWindowAppearing())
        ImGui::SetKeyboardFocusHere(0);
    ImGui::InputTextWithHint(
            "##custom_block_name",
            "block name",
            NewCustomNameBuf,
            sizeof(NewCustomNameBuf));

    ImGui::SetNextItemWidth(PopupFieldWidth);
    ImGui::InputTextMultiline(
            "##custom_block_desc",
            NewCustomDescBuf,
            sizeof(NewCustomDescBuf),
            ImVec2(PopupFieldWidth, 50.0f));
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Optional description");

    ImGui::Spacing();
    ImGui::TextUnformatted("Inputs");

    int removeIndex = -1;
    for (int i = 0; i < (int)NewCustomParams.size(); ++i) {
        ImGui::PushID(i);

        CustomParamEdit &p = NewCustomParams[i];

        ImGui::SetNextItemWidth(PopupFieldWidth * 0.5f - 18.0f);
        ImGui::InputTextWithHint("##param_name", "arg name", p.NameBuf, sizeof(p.NameBuf));

        ImGui::SameLine();
        ImGui::SetNextItemWidth(PopupFieldWidth * 0.35f - 18.0f);
        ImGui::Combo("##param_type", &p.TypeIndex, kTypeNames, IM_ARRAYSIZE(kTypeNames));

        ImGui::SameLine();
        if (ImGui::Button("x"))
            removeIndex = i;

        ImGui::PopID();
    }

    if (removeIndex >= 0)
        NewCustomParams.erase(NewCustomParams.begin() + removeIndex);

    if (ImGui::Button("+ Add an Input", ImVec2(PopupFieldWidth, 0.0f)))
        NewCustomParams.push_back(CustomParamEdit{});

    if (!NewCustomError.empty()) {
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + PopupFieldWidth);
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", NewCustomError.c_str());
        ImGui::PopTextWrapPos();
    }

    ImGui::Spacing();
    bool createClicked = ImGui::Button("Create Block", ImVec2(PopupFieldWidth * 0.5f - 4.0f, 0.0f));
    ImGui::SameLine();
    bool cancelClicked = ImGui::Button("Cancel", ImVec2(PopupFieldWidth * 0.5f - 4.0f, 0.0f));

    if (createClicked) {
        std::string name(NewCustomNameBuf);

        if (name.empty()) {
            NewCustomError = "Give the block a name";
        } else {
            CustomBlockSpec spec;
            spec.Name = name;
            spec.Description = NewCustomDescBuf;

            for (CustomParamEdit &p : NewCustomParams) {
                std::string pname(p.NameBuf);
                if (pname.empty())
                    continue;
                spec.Params.push_back({ pname, kTypeValues[p.TypeIndex] });
            }

            Error err = RegisterCustomBlock(registry, spec);

            if (err == Error::Ok) {
                const BlockDefinition *hatDef =
                    FindDefinitionByOpCode(registry, CustomHatOpCode(name));

                if (hatDef) {
                    ImVec2 spawnScreen = Origin + ImVec2(60.0f, 60.0f);
                    AddBlock(*hatDef, ScreenToWorld(spawnScreen, Origin));
                }

                NewCustomError.clear();
                NewCustomParams.clear();
                ImGui::CloseCurrentPopup();
            } else if (err == Error::BlockAlreadyExists) {
                NewCustomError = "A block named '" + name + "' already exists";
            } else {
                NewCustomError = "Couldn't create block '" + name + "'";
            }
        }
    }

    if (cancelClicked) {
        NewCustomError.clear();
        NewCustomParams.clear();
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

Error Canvas::RenameCustomBlock(const std::string &oldName, const std::string &newName)
{
    if (!Registry)
        return Error::Failed;

    return ::RenameCustomBlock(*Registry, oldName, newName);
}

void Canvas::RequestCustomBlockRename(const std::string &name)
{
    CustomRenameRequest.Requested = true;
    CustomRenameRequest.OldName = name;

    std::snprintf(RenameCustomNameBuf, sizeof(RenameCustomNameBuf), "%s", name.c_str());
    RenameCustomError.clear();
}

void Canvas::DrawRenameCustomBlockPopup(BlockRegistry &registry)
{
    if (CustomRenameRequest.Requested) {
        ImGui::OpenPopup("##canvas_rename_custom_block_popup");
        CustomRenameRequest.Requested = false;
    }

    if (!ImGui::BeginPopup("##canvas_rename_custom_block_popup"))
        return;

    constexpr float PopupFieldWidth = 220.0f;

    ImGui::TextUnformatted("Rename Block");
    ImGui::Separator();

    ImGui::SetNextItemWidth(PopupFieldWidth);
    if (ImGui::IsWindowAppearing()) {
        ImGui::SetKeyboardFocusHere(0);
    }
    bool enterPressed = ImGui::InputTextWithHint(
            "##rename_custom_block_name",
            "block name",
            RenameCustomNameBuf,
            sizeof(RenameCustomNameBuf),
            ImGuiInputTextFlags_EnterReturnsTrue);

    if (!RenameCustomError.empty()) {
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + PopupFieldWidth);
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", RenameCustomError.c_str());
        ImGui::PopTextWrapPos();
    }

    bool renameClicked = ImGui::Button("Rename", ImVec2(PopupFieldWidth * 0.5f - 4.0f, 0.0f));
    ImGui::SameLine();
    bool cancelClicked = ImGui::Button("Cancel", ImVec2(PopupFieldWidth * 0.5f - 4.0f, 0.0f));

    if (renameClicked || enterPressed) {
        std::string newName(RenameCustomNameBuf);
        Error err = RenameCustomBlock(CustomRenameRequest.OldName, newName);

        if (err == Error::Ok) {
            RenameCustomError.clear();
            CustomRenameRequest = PendingCustomBlockRename{};
            ImGui::CloseCurrentPopup();
        } else if (err == Error::BlockAlreadyExists) {
            RenameCustomError = "A block named '" + newName + "' already exists";
        } else {
            RenameCustomError = "Couldn't rename block to '" + newName + "'";
        }
    }

    if (cancelClicked) {
        RenameCustomError.clear();
        CustomRenameRequest = PendingCustomBlockRename{};
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
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
