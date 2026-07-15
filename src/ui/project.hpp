#pragma once

#include <string>

#include "util/error.hpp"
#include "ui/canvas.hpp"

Error SaveProject(
        const Canvas &canvas,
        const std::string &path,
        const std::string &projectName);
Error LoadProject(
        Canvas &canvas,
        const std::string &path);
