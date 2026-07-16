#pragma once

#include <memory>
#include <string>
#include <vector>

#include "TextEditor.h"

#include "ui/imgui.hpp"
#include "ui/canvas.hpp"
#include "block/registry.hpp"
#include "codegen/backend.hpp"

BlockArg ConvertVisualArg(const VisualArg &arg);
BlockInstance ConvertVisualBlock(const VisualBlock &block);
std::vector<std::unique_ptr<BlockInstance>> ConvertVisualChain(VisualBlock *head);
VisualBlock *FindMainBlock(Canvas &canvas);

Error BuildProgramFromCanvas(Canvas &canvas, BlockRegistry &registry, Program &outProgram);

enum class CodeLanguage
{
    Cpp,
    Python,
};

class CodeView
{
public:
    CodeView();

    Error Generate(Canvas &canvas, BlockRegistry &registry, CodeLanguage language);

    void SetCode(const std::string &code);
    void SetLanguage(const TextEditor::LanguageDefinition &lang);
    const std::string GetCode() const { return Editor.GetText(); }

    void Draw(const char *strId, ImVec2 size);

    bool ReadOnly = true;
    std::unique_ptr<Emitter> CreateEmitter(CodeLanguage language, std::ostream *stream);
    TextEditor Editor;
};
