#include "build/toolchain.hpp"

#include "build/process.hpp"

#include <cctype>
#include <sstream>

#ifdef _WIN32
    #include <windows.h>
#endif

bool Toolchain::IsPython3(const std::string &versionOutput)
{
    std::istringstream stream(versionOutput);
    std::string token;

    while (stream >> token) {
        if (token.rfind("Python", 0) == 0 && token.size() > 6) {
            char major = token[6];
            return major >= '3';
        }

        if (token.find('.') != std::string::npos) {
            char major = token[0];
            return major >= '3';
        }
    }

    return false;
}

std::optional<ToolInfo> Toolchain::TryPython(const std::string &command)
{
    CommandResult result = RunCommand(command + " --version 2>&1");
    if (result.ExitCode != 0 || result.Output.empty())
        return std::nullopt;

    if (!IsPython3(result.Output))
        return std::nullopt;

    std::string version = result.Output;
    while (!version.empty() && (version.back() == '\n' || version.back() == '\r'))
        version.pop_back();

    return ToolInfo{
        .Kind = ToolKind::Python,
        .Command = command,
        .Version = version,
    };
}

std::optional<ToolInfo> Toolchain::TryCppCompiler(
    const std::string &command,
    CompilerKind kind)
{
    std::string versionFlag = (kind == CompilerKind::Msvc) ? "" : " --version";
    CommandResult result = RunCommand(command + versionFlag + " 2>&1");
    if (result.ExitCode != 0 && kind != CompilerKind::Msvc)
        return std::nullopt;

    if (kind == CompilerKind::Msvc && result.Output.find("Microsoft") == std::string::npos)
        return std::nullopt;

    std::string version = result.Output;
    while (!version.empty() && (version.back() == '\n' || version.back() == '\r'))
        version.pop_back();

    if (version.empty())
        version = command;

    return ToolInfo{
        .Kind = ToolKind::CppCompiler,
        .Command = command,
        .Version = version,
        .Compiler = kind,
    };
}

std::vector<std::string> Toolchain::PythonCandidates()
{
#ifdef _WIN32
    return { "py -3", "python3", "python" };
#else
    return { "python3", "python" };
#endif
}

std::vector<std::pair<std::string, CompilerKind>> Toolchain::CppCandidates()
{
#ifdef _WIN32
    return {
        { "g++", CompilerKind::Gcc },
        { "clang++", CompilerKind::Clang },
        { "cl", CompilerKind::Msvc },
    };
#else
    return {
        { "g++", CompilerKind::Gcc },
        { "clang++", CompilerKind::Clang },
    };
#endif
}

std::optional<ToolInfo> Toolchain::FindPython()
{
    for (const std::string &candidate : PythonCandidates()) {
        if (auto tool = TryPython(candidate))
            return tool;
    }

    return std::nullopt;
}

std::optional<ToolInfo> Toolchain::FindCppCompiler()
{
    for (const auto &[command, kind] : CppCandidates()) {
        if (auto tool = TryCppCompiler(command, kind))
            return tool;
    }

    return std::nullopt;
}
