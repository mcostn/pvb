#pragma once

#pragma once

#include <string>

#include "ui/canvas.hpp"

struct ProjectLoadResult
{
    bool Success = false;
    std::string Error;
};

bool SaveProject(const Canvas &canvas, const std::string &path, const std::string &projectName);
ProjectLoadResult LoadProject(Canvas &canvas, const std::string &path);
