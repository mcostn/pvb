#include "ui/code_view.hpp"

#include <algorithm>
#include <sstream>
#include <unordered_map>

#include "util/error.hpp"
#include "util/macro.hpp"

#include "codegen/cpp_backend.hpp"
#include "codegen/py_backend.hpp"

#include "ui/custom_block.hpp"

BlockArg ConvertVisualArg(const VisualArg &arg)
{
    if (const auto *lit = std::get_if<LiteralValue>(&arg))
        return *lit;

    if (const auto *ref = std::get_if<VariableRef>(&arg))
        return *ref;

    const auto &plugged = std::get<std::unique_ptr<VisualBlock>>(arg);

    if (!plugged)
        return LiteralValue{};

    return std::make_unique<BlockInstance>(ConvertVisualBlock(*plugged));
}

BlockInstance ConvertVisualBlock(const VisualBlock &block)
{
    BlockInstance inst;
    inst.OpCode = block.Def ? block.Def->OpCode : std::string();
    inst.SourceId = block.Id;

    for (const auto &[key, arg] : block.Args) {
        inst.Args.emplace(key, ConvertVisualArg(arg));
    }

    for (const auto &[slot, head] : block.BodyRoots) {
        inst.Bodies.emplace(slot, ConvertVisualChain(head));
    }

    return inst;
}

std::vector<std::unique_ptr<BlockInstance>> ConvertVisualChain(VisualBlock *head)
{
    std::vector<std::unique_ptr<BlockInstance>> out;

    for (VisualBlock *cur = head; cur != nullptr; cur = cur->Next) {
        out.push_back(std::make_unique<BlockInstance>(ConvertVisualBlock(*cur)));
    }

    return out;
}

static std::vector<Param> CustomBlockParams(const BlockRegistry &registry, const std::string &funcName)
{
    std::vector<Param> params;

    for (const BlockDefinition *paramDef : CustomBlockParamDefs(registry, funcName)) {
        std::string name = paramDef->Schema.empty() ? std::string() : paramDef->Schema.front().Name;
        params.push_back({ paramDef->ReturnType, std::move(name) });
    }

    return params;
}

static Error BuildCustomFunction(
        VisualBlock *hatRoot,
        BlockRegistry &registry,
        std::unique_ptr<FunctionStmt> &outFn)
{
    std::vector<std::unique_ptr<BlockInstance>> statements = ConvertVisualChain(hatRoot->Next);

    std::unique_ptr<BlockStmt> body = registry.Converter.ConvertBody(statements);
    FAIL_COND_V_MSG(!body, Error::Failed, "Failed to convert custom block body to AST");

    std::string name = CustomBlockName(*hatRoot->Def);
    outFn = Function(VAL_NONE, name, CustomBlockParams(registry, name), std::move(body));

    if (hatRoot->Id != 0)
        registry.Converter.NodeSourceIds[outFn.get()] = hatRoot->Id;

    return Error::Ok;
}

VisualBlock *FindMainBlock(Canvas &canvas)
{
    for (VisualBlock *root : canvas.Manager.Roots) {
        if (root->Def && root->Def->OpCode == "main")
            return root;
    }

    return nullptr;
}

Error BuildProgramFromCanvas(
        Canvas &canvas,
        BlockRegistry &registry,
        Program &outProgram)
{
    registry.Converter.NodeSourceIds.clear();

    for (const VariableInfo &var : registry.Variables) {
        outProgram.Statements.push_back(
                DeclVar(var.Type, var.Name, nullptr, VarScope::Global));
    }

    for (VisualBlock *root : canvas.Manager.Roots) {
        if (!root->Def || !IsCustomHat(*root->Def))
            continue;

        std::unique_ptr<FunctionStmt> fn;
        Error err = BuildCustomFunction(root, registry, fn);
        FAIL_COND_V_MSG(
                err != Error::Ok,
                err,
                "Failed to build custom block '{}'",
                CustomBlockName(*root->Def));

        outProgram.Statements.push_back(std::move(fn));
    }

    VisualBlock *mainBlock = FindMainBlock(canvas);
    FAIL_COND_V_MSG(
            !mainBlock,
            Error::Failed,
            "No 'main' block found on the canvas");

    std::vector<std::unique_ptr<BlockInstance>> statements = ConvertVisualChain(mainBlock->Next);

    std::unique_ptr<BlockStmt> body = registry.Converter.ConvertBody(statements);
    FAIL_COND_V_MSG(
            !body,
            Error::Failed,
            "Failed to convert program body to AST");

    for (auto &stmt : body->Statements)
        outProgram.Statements.push_back(std::move(stmt));

    return Error::Ok;
}

CodeView::CodeView()
{
    Editor.SetLanguageDefinition(TextEditor::LanguageDefinition::CPlusPlus());
    Editor.SetReadOnly(ReadOnly);
    Editor.SetShowWhitespaces(false);
}

void CodeView::SetLanguage(const TextEditor::LanguageDefinition &lang)
{
    Editor.SetLanguageDefinition(lang);
}

void CodeView::SetCode(const std::string &code)
{
    Editor.SetText(code);
}

void CodeView::Draw(const char *strId, ImVec2 size)
{
    ImGui::PushID(strId);

    Editor.SetReadOnly(ReadOnly);

    ImGui::BeginChild("##code", size);
    ImGui::SetWindowFontScale(1.5f);
    ImGui::EndChild();

    Editor.Render("##code", size);

    ImGui::PopID();
}

std::unique_ptr<Emitter> CodeView::CreateEmitter(CodeLanguage language, std::ostream *stream)
{
    switch (language) {
        case CodeLanguage::Cpp:    return std::make_unique<CppEmitter>(stream);
        case CodeLanguage::Python: return std::make_unique<PythonEmitter>(stream);
        case CodeLanguage::Asm:    return std::make_unique<AsmEmitter>(stream);
    }

    return nullptr;
}

namespace {

struct RawRange
{
    std::ostream *Stream;
    size_t Start;
    size_t End;
};

size_t ResolveAbsoluteOffset(
        std::ostream *stream,
        size_t offset,
        const std::vector<Emitter::StreamSplice> &splices,
        std::ostream *root)
{
    while (stream != root) {
        auto it = std::find_if(splices.begin(), splices.end(),
                [&](const Emitter::StreamSplice &s) { return s.From == stream; });

        if (it == splices.end())
            return offset;

        offset += it->OffsetInto;
        stream = it->Into;
    }

    return offset;
}

} // namespace

Error CodeView::Generate(Canvas &canvas, BlockRegistry &registry, CodeLanguage language)
{
    SetCode("");
    Map.Clear();
    LastHighlightedId = 0;

    Program program;
    TRY(BuildProgramFromCanvas(canvas, registry, program));

    std::ostringstream stream;
    std::unique_ptr<Emitter> emitter = CreateEmitter(language, &stream);
    FAIL_COND_V_MSG(!emitter, Error::Failed, "No emitter available for the requested language");

    std::unordered_map<const AstNode*, RawRange> nodeRanges;
    emitter->OnEmitRange = [&](const AstNode &node, std::ostream *s, size_t start, size_t end) {
        nodeRanges[&node] = { s, start, end };
    };

    Error err = emitter->Emit(program);
    FAIL_COND_V_MSG(err != Error::Ok, err, "Code generation failed");

    LastGeneratedCode = stream.str();
    SetCode(LastGeneratedCode);

    for (const auto &[node, blockId] : registry.Converter.NodeSourceIds) {
        auto it = nodeRanges.find(node);
        if (it == nodeRanges.end())
            continue; // e.g. a block whose expression got optimized away entirely

        const RawRange &raw = it->second;
        size_t absStart = ResolveAbsoluteOffset(raw.Stream, raw.Start, emitter->Splices, &stream);
        size_t absEnd   = ResolveAbsoluteOffset(raw.Stream, raw.End,   emitter->Splices, &stream);

        if (absEnd > absStart)
            Map.Add(blockId, { absStart, absEnd });
    }

    switch (language) {
        case CodeLanguage::Asm:
        case CodeLanguage::Cpp:    SetLanguage(TextEditor::LanguageDefinition::CPlusPlus()); break;
        case CodeLanguage::Python: SetLanguage(TextEditor::LanguageDefinition::Lua());     break;
    }

    return Error::Ok;
}

void CodeView::HighlightBlock(uint32_t blockId)
{
    if (blockId == LastHighlightedId)
        return;

    LastHighlightedId = blockId;

    const SourceRange *range = blockId ? Map.Find(blockId) : nullptr;
    if (!range) {
        ClearHighlight();
        return;
    }

    TextEditor::Coordinates start = SourceMap::OffsetToCoordinates(LastGeneratedCode, range->Start);
    TextEditor::Coordinates end   = SourceMap::OffsetToCoordinates(LastGeneratedCode, range->End);

    Editor.SetSelection(start, end);
}

void CodeView::ClearHighlight()
{
    TextEditor::Coordinates cursor = Editor.GetCursorPosition();
    Editor.SetSelection(cursor, cursor);
}
