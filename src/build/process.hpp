#pragma once

#include <string>

struct [[nodiscard]] CommandResult
{
    int ExitCode = -1;
    std::string Output;
};

CommandResult RunCommand(const std::string &command);
[[nodiscard]] std::string QuotePath(const std::string &path);
[[nodiscard]] bool LaunchDetached(const std::string &command);
[[nodiscard]] bool LaunchInTerminal(
    const std::string &command,
    const std::string &workingDir = "");
