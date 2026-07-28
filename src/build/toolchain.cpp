#include "build/toolchain.hpp"

#include "build/process.hpp"

#include <cctype>
#include <sstream>

#ifdef _WIN32
    #include <windows.h>
    #include <cstdlib>
    #include <filesystem>
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

std::optional<ToolInfo> Toolchain::TryPython(const std::string &command, const std::string &extraArgs)
{
    std::string invocation = QuotePath(command);
    if (!extraArgs.empty())
        invocation += " " + extraArgs;

    CommandResult result = RunCommand(invocation + " --version 2>&1");
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
        .ExtraArgs = extraArgs,
        .Version = version,
    };
}

std::optional<ToolInfo> Toolchain::TryCppCompiler(
    const std::string &command,
    CompilerKind kind)
{
    std::string versionFlag = (kind == CompilerKind::Msvc) ? "" : " --version";
    CommandResult result = RunCommand(QuotePath(command) + versionFlag + " 2>&1");
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
        .ExtraArgs = "",
        .Version = version,
        .Compiler = kind,
    };
}

std::vector<std::pair<std::string, std::string>> Toolchain::PythonCandidates()
{
#ifdef _WIN32
    return { { "py", "-3" }, { "python3", "" }, { "python", "" } };
#else
    return { { "python3", "" }, { "python", "" } };
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

#ifdef _WIN32

namespace {

std::string GetEnvVar(const char *name)
{
    const char *value = std::getenv(name);
    return value ? std::string(value) : std::string();
}

std::vector<std::filesystem::path> WellKnownCompilerBinDirs()
{
    namespace fs = std::filesystem;

    std::vector<fs::path> dirs;
    auto addIfExists = [&](const fs::path &dir) {
        std::error_code ec;
        if (fs::is_directory(dir, ec))
            dirs.push_back(dir);
    };

    const std::string programFiles = GetEnvVar("ProgramFiles");
    const std::string programFilesX86 = GetEnvVar("ProgramFiles(x86)");

    if (!programFiles.empty())
        addIfExists(fs::path(programFiles) / "CodeBlocks" / "MinGW" / "bin");
    if (!programFilesX86.empty())
        addIfExists(fs::path(programFilesX86) / "CodeBlocks" / "MinGW" / "bin");

    if (!programFilesX86.empty())
        addIfExists(fs::path(programFilesX86) / "Dev-Cpp" / "MinGW64" / "bin");
    if (!programFiles.empty())
        addIfExists(fs::path(programFiles) / "Dev-Cpp" / "MinGW64" / "bin");

    addIfExists(fs::path("C:\\mingw64\\bin"));
    addIfExists(fs::path("C:\\MinGW\\bin"));

    for (const std::string &root : { programFiles, programFilesX86 }) {
        if (root.empty())
            continue;

        std::error_code ec;
        fs::path mingwRoot = fs::path(root) / "mingw-w64";
        if (!fs::is_directory(mingwRoot, ec))
            continue;

        for (const auto &entry : fs::directory_iterator(mingwRoot, ec)) {
            if (!entry.is_directory())
                continue;

            addIfExists(entry.path() / "mingw64" / "bin");
            addIfExists(entry.path() / "mingw32" / "bin");
        }
    }

    addIfExists(fs::path("C:\\msys64\\mingw64\\bin"));
    addIfExists(fs::path("C:\\msys64\\ucrt64\\bin"));
    addIfExists(fs::path("C:\\msys64\\clang64\\bin"));

    if (!programFiles.empty())
        addIfExists(fs::path(programFiles) / "LLVM" / "bin");

    return dirs;
}

std::optional<std::string> FindMsvcClPath()
{
    namespace fs = std::filesystem;

    const std::string programFilesX86 = GetEnvVar("ProgramFiles(x86)");
    if (programFilesX86.empty())
        return std::nullopt;

    fs::path vswhere = fs::path(programFilesX86) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe";

    std::error_code ec;
    if (!fs::exists(vswhere, ec))
        return std::nullopt;

    CommandResult result = RunCommand(
        QuotePath(vswhere.string())
        + " -latest -products * "
          "-requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 "
          "-property installationPath 2>&1");

    if (result.ExitCode != 0)
        return std::nullopt;

    std::string installPath = result.Output;
    while (!installPath.empty() && (installPath.back() == '\n' || installPath.back() == '\r' || installPath.back() == ' '))
        installPath.pop_back();

    if (installPath.empty())
        return std::nullopt;

    fs::path toolsRoot = fs::path(installPath) / "VC" / "Tools" / "MSVC";
    if (!fs::is_directory(toolsRoot, ec))
        return std::nullopt;

    std::string bestVersion;
    for (const auto &entry : fs::directory_iterator(toolsRoot, ec)) {
        if (entry.is_directory() && entry.path().filename().string() > bestVersion)
            bestVersion = entry.path().filename().string();
    }

    if (bestVersion.empty())
        return std::nullopt;

    fs::path clPath = toolsRoot / bestVersion / "bin" / "Hostx64" / "x64" / "cl.exe";
    if (!fs::exists(clPath, ec))
        return std::nullopt;

    return clPath.string();
}

} // namespace

#endif // _WIN32

std::optional<ToolInfo> Toolchain::FindPython()
{
    for (const auto &[command, extraArgs] : PythonCandidates()) {
        if (auto tool = TryPython(command, extraArgs))
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

#ifdef _WIN32
    static const std::pair<const char *, CompilerKind> kFallbackExeNames[] = {
        { "g++.exe", CompilerKind::Gcc },
        { "clang++.exe", CompilerKind::Clang },
    };

    for (const std::filesystem::path &dir : WellKnownCompilerBinDirs()) {
        for (const auto &[exeName, kind] : kFallbackExeNames) {
            std::error_code ec;
            std::filesystem::path exePath = dir / exeName;
            if (!std::filesystem::exists(exePath, ec))
                continue;

            if (auto tool = TryCppCompiler(exePath.string(), kind))
                return tool;
        }
    }

    if (auto clPath = FindMsvcClPath()) {
        if (auto tool = TryCppCompiler(*clPath, CompilerKind::Msvc))
            return tool;
    }
#endif

    return std::nullopt;
}
