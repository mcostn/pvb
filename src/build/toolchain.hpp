#pragma once

#include <optional>
#include <string>
#include <vector>

enum class ToolKind
{
    Python,
    CppCompiler,
    Nasm,
};

enum class CompilerKind
{
    Gcc,
    Clang,
    Msvc,
};

struct ToolInfo
{
    ToolKind Kind;
    std::string Command;
    std::string ExtraArgs;
    std::string Version;
    CompilerKind Compiler = CompilerKind::Gcc;
};

class Toolchain
{
public:
    [[nodiscard]] static std::optional<ToolInfo> FindPython();
    [[nodiscard]] static std::optional<ToolInfo> FindCppCompiler();
    [[nodiscard]] static std::optional<ToolInfo> FindNasm();

private:
    [[nodiscard]] static std::optional<ToolInfo> TryPython(const std::string &command, const std::string &extraArgs);
    [[nodiscard]] static std::optional<ToolInfo> TryCppCompiler(const std::string &command, CompilerKind kind);
    [[nodiscard]] static std::optional<ToolInfo> TryNasm(const std::string &command);
    [[nodiscard]] static std::vector<std::pair<std::string, std::string>> PythonCandidates();
    [[nodiscard]] static std::vector<std::pair<std::string, CompilerKind>> CppCandidates();
    [[nodiscard]] static std::vector<std::string> NasmCandidates();
    [[nodiscard]] static bool IsPython3(const std::string &versionOutput);
};
