#include "ui/palette.hpp"
#include "ui/canvas.hpp"
#include "ui/editor.hpp"
#include "ui/custom_block.hpp"

static const char *CategoryDisplayName(BlockCategory category)
{
    switch (category) {
        case BlockCategory::Event:       return "Events";
        case BlockCategory::Console:     return "Console";
        case BlockCategory::ControlFlow: return "Control Flow";
        case BlockCategory::Math:        return "Math";
        case BlockCategory::Logic:       return "Logic";
        case BlockCategory::Variable:    return "Variables";
        case BlockCategory::Custom:      return "My Blocks";
    }

    return "Other";
}

static bool FindVariableNameForOpCode(const BlockRegistry &registry, const std::string &opcode, std::string &outName)
{
    for (const VariableInfo &v : registry.Variables) {
        if (opcode == VarGetOpCode(v.Name) || opcode == VarSetOpCode(v.Name)) {
            outName = v.Name;
            return true;
        }
    }

    return false;
}

bool BlockPalette::Matches(const BlockDefinition &def) const
{
    if (Search[0] == '\0')
        return true;

    std::string s(Search);
    return def.OpCode.find(s) != std::string::npos ||
           def.Fmt.find(s) != std::string::npos;
}

void BlockPalette::DrawBlockPreview(
    Canvas &canvas,
    const BlockDefinition &def,
    const std::vector<BlockContextAction> &contextActions)
{
    ImGui::PushID(&def);

    VisualBlock preview;
    preview.Def = &def;

    for (const auto &item : def.Schema)
        preview.Args[item.Name] = MakeDefaultArg(def, item);

    canvas.LayoutBlock(preview);

    ImVec2 start = ImGui::GetCursorScreenPos();
    ImDrawList *draw = ImGui::GetWindowDrawList();

    BlockOutline outline = BuildOutline(preview, start, 1.0f);
    outline.Fill(draw, CategoryColor(def.Category));
    outline.Stroke(draw, IM_COL32(0,0,0,160), 1.0f);

    DrawBlockLayout(
        draw,
        preview,
        start,
        1.0f,
        ImGui::GetFont(),
        ImGui::GetFontSize(),
        IM_COL32(255,255,255,255),
        false);


    ImGui::SetCursorScreenPos(start);
    ImGui::InvisibleButton("##block_drag", preview.Size);

    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        canvas.EditorRef->BeginPaletteDrag(def);
    }

    if (!contextActions.empty() && ImGui::BeginPopupContextItem("##block_ctx_menu")) {
        for (const BlockContextAction &action : contextActions) {
            if (ImGui::MenuItem(action.Label.c_str()) && action.Action)
                action.Action();
        }
        ImGui::EndPopup();
    }

    if (ImGui::IsItemHovered() && !def.Description.empty()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 24.0f);
        ImGui::TextUnformatted(def.Description.c_str());
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }

    ImGui::SetCursorScreenPos(start + ImVec2(0, preview.Size.y));
    ImGui::Dummy(ImVec2(preview.Size.x, 8));

    ImGui::PopID();
}

void BlockPalette::DrawCategorySection(Canvas &canvas, BlockRegistry &registry, BlockCategory category)
{
    bool hasVisibleBlock = false;
    for (auto &def : registry.Definitions) {
        if (def.Category != category)
            continue;
        if (!Matches(def))
            continue;
        hasVisibleBlock = true;
        break;
    }

    if (!hasVisibleBlock)
        return;

    ImGui::TextUnformatted(CategoryDisplayName(category));
    ImGui::Separator();

    for (auto &def : registry.Definitions) {
        if (def.Category != category)
            continue;
        if (!Matches(def))
            continue;

        DrawBlockPreview(canvas, def);
    }

    ImGui::Spacing();
}

void BlockPalette::DrawVariableSection(Canvas &canvas, BlockRegistry &registry)
{
    ImGui::Separator();
    ImGui::TextUnformatted("Variables");

    float fieldWidth = Width - ImGui::GetStyle().WindowPadding.x * 2.0f;

    if (ImGui::Button("Make a Variable", ImVec2(fieldWidth, 0.0f))) {
        canvas.RequestVariableCreation(nullptr, "", VAL_ANY);
    }

    for (auto &def : registry.Definitions) {
        if (def.Category != BlockCategory::Variable)
            continue;

        std::string varName;
        if (!FindVariableNameForOpCode(registry, def.OpCode, varName))
            continue;
        if (!Matches(def))
            continue;

        std::string deleteLabel = "Delete variable '" + varName + "'";
        std::string renameLabel = "Rename variable '" + varName + "'";
        DrawBlockPreview(canvas, def, {
            { renameLabel, [&canvas, varName]() { canvas.RequestVariableRename(varName); } },
            { deleteLabel, [&canvas, varName]() { canvas.DeleteVariable(varName); } },
        });
    }
}

void BlockPalette::DrawCustomSection(Canvas &canvas, BlockRegistry &registry)
{
    ImGui::Separator();
    ImGui::TextUnformatted("My Blocks");

    float fieldWidth = Width - ImGui::GetStyle().WindowPadding.x * 2.0f;

    if (ImGui::Button("Make a Block", ImVec2(fieldWidth, 0.0f))) {
        canvas.RequestCustomBlockCreation();
    }

    for (auto &def : registry.Definitions) {
        if (def.Category != BlockCategory::Custom)
            continue;
        if (!IsCustomCall(def))
            continue;

        std::string blockName = CustomBlockName(def);
        if (!IsCustomBlockRegistered(registry, blockName))
            continue;
        if (!Matches(def))
            continue;

        std::string deleteLabel = "Delete block '" + blockName + "'";
        std::string renameLabel = "Rename block '" + blockName + "'";
        DrawBlockPreview(canvas, def, {
            { renameLabel, [&canvas, blockName]() { canvas.RequestCustomBlockRename(blockName); } },
            { deleteLabel, [&canvas, blockName]() { canvas.DeleteCustomBlock(blockName); } },
        });

        std::vector<const BlockDefinition *> params =
            CustomBlockParamDefs(registry, blockName);

        if (!params.empty()) {
            ImGui::Indent(10.0f);
            ImGui::TextDisabled("arguments:");

            for (const BlockDefinition *paramDef : params)
                DrawBlockPreview(canvas, *paramDef);

            ImGui::Unindent(10.0f);
            ImGui::Spacing();
        }
    }
}

void BlockPalette::Draw(
    Canvas &canvas,
    BlockRegistry &registry,
    const char *id,
    float height)
{
    ImGui::BeginChild( id, ImVec2(Width,height), true);

    ImGui::InputText("Search", Search, sizeof(Search));
    ImGui::Separator();

    static const BlockCategory kCategoryOrder[] = {
        BlockCategory::Event,
        BlockCategory::Console,
        BlockCategory::ControlFlow,
        BlockCategory::Math,
        BlockCategory::Logic,
    };

    for (BlockCategory category : kCategoryOrder)
        DrawCategorySection(canvas, registry, category);

    DrawVariableSection(canvas, registry);
    DrawCustomSection(canvas, registry);

    ImGui::EndChild();
}
