#include "ui/palette.hpp"
#include "ui/canvas.hpp"
#include "ui/editor.hpp"
#include "ui/custom_block.hpp"

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
    const BlockDefinition &def)
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

    ImGui::SetCursorScreenPos(start + ImVec2(0, preview.Size.y));
    ImGui::Dummy(ImVec2(preview.Size.x, 8));

    ImGui::PopID();
}

void BlockPalette::DrawVariableSection(Canvas &canvas, BlockRegistry &registry)
{
    ImGui::TextUnformatted("Variables");

    float fieldWidth = Width - ImGui::GetStyle().WindowPadding.x * 2.0f;

    if (ImGui::Button("Make a Variable", ImVec2(fieldWidth, 0.0f))) {
        canvas.RequestVariableCreation(nullptr, "", VAL_ANY);
    }

    ImGui::Separator();

    for (auto &def : registry.Definitions) {
        if (def.Category != BlockCategory::Variable)
            continue;
        if (!Matches(def))
            continue;

        DrawBlockPreview(canvas, def);
    }
}

void BlockPalette::DrawCustomSection(Canvas &canvas, BlockRegistry &registry)
{
    ImGui::TextUnformatted("My Blocks");

    float fieldWidth = Width - ImGui::GetStyle().WindowPadding.x * 2.0f;

    if (ImGui::Button("Create a Block", ImVec2(fieldWidth, 0.0f))) {
        canvas.RequestCustomBlockCreation();
    }

    ImGui::Separator();

    for (auto &def : registry.Definitions) {
        if (def.Category != BlockCategory::Custom)
            continue;
        if (!IsCustomCall(def))
            continue;
        if (!Matches(def))
            continue;

        DrawBlockPreview(canvas, def);

        std::vector<const BlockDefinition *> params =
            CustomBlockParamDefs(registry, CustomBlockName(def));

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

    for (auto &def : registry.Definitions) {
        if (def.Category == BlockCategory::Variable)
            continue;
        if (def.Category == BlockCategory::Custom)
            continue;
        if (!Matches(def))
            continue;

        DrawBlockPreview(
            canvas,
            def);
    }

    ImGui::Separator();
    DrawVariableSection(canvas, registry);

    ImGui::Separator();
    DrawCustomSection(canvas, registry);

    ImGui::EndChild();
}
