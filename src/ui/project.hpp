#pragma once

#include <string>

#include "util/error.hpp"
#include "ui/canvas.hpp"
#include "ui/code_view.hpp"
#include "block/registry.hpp"

struct ProjectSettings
{
    std::string Name;
    std::string Description;
    CodeLanguage Language = CodeLanguage::Python;
};

Error SaveProject(
        const Canvas &canvas,
        const BlockRegistry &registry,
        const std::string &path,
        const ProjectSettings &settings);
Error LoadProject(
        Canvas &canvas,
        BlockRegistry &registry,
        const std::string &path,
        ProjectSettings &outSettings);
