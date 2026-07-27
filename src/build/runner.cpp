#include "build/runner.hpp"

#include "build/process.hpp"

#include <chrono>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

std::optional<ToolInfo> ProjectRunner::FindTool(CodeLanguage language)
{
    switch (language) {
        case CodeLanguage::Python: return Toolchain::FindPython();
        case CodeLanguage::Cpp:    return Toolchain::FindCppCompiler();
    }

    return std::nullopt;
}

std::string ProjectRunner::SanitizeProjectName(const std::string &name)
{
    std::string sanitized;
    sanitized.reserve(name.empty() ? 7 : name.size());

    for (char ch : name) {
        if (std::isalnum(static_cast<unsigned char>(ch)) || ch == '_' || ch == '-')
            sanitized += ch;
        else if (std::isspace(static_cast<unsigned char>(ch)))
            sanitized += '_';
    }

    if (sanitized.empty())
        return "program";

    return sanitized;
}

std::string ProjectRunner::CreateBuildDirectory()
{
    const auto stamp = std::chrono::system_clock::now().time_since_epoch().count();
    fs::path dir = fs::temp_directory_path() / ("pvb_build_" + std::to_string(stamp));
    fs::create_directories(dir);
    return dir.string();
}

BuildResult ProjectRunner::CompileCpp(
    const std::string &source,
    const ToolInfo &compiler,
    const std::string &projectName)
{
    BuildResult result;
    result.Tool = compiler;

    result.BuildDirectory = CreateBuildDirectory();
    const std::string baseName = SanitizeProjectName(projectName);
    const fs::path sourcePath = fs::path(result.BuildDirectory) / (baseName + ".cpp");
#ifdef _WIN32
    result.ExecutablePath = (fs::path(result.BuildDirectory) / (baseName + ".exe")).string();
#else
    result.ExecutablePath = (fs::path(result.BuildDirectory) / baseName).string();
#endif

    {
        std::ofstream file(sourcePath);
        if (!file) {
            result.Status = Error::BuildWriteFailed;
            result.Output = "Failed to write source file: " + sourcePath.string();
            return result;
        }
        file << source;
    }

    std::ostringstream command;
    switch (compiler.Compiler) {
        case CompilerKind::Msvc:
            command << "cd /d " << QuotePath(result.BuildDirectory)
                << " && " << QuotePath(compiler.Command)
                << " /nologo /EHsc /Fe:" << QuotePath(fs::path(result.ExecutablePath).filename().string())
                << " " << QuotePath(sourcePath.filename().string());
            break;

        case CompilerKind::Gcc:
        case CompilerKind::Clang:
            command << QuotePath(compiler.Command)
                << " -std=c++20 -o " << QuotePath(result.ExecutablePath)
                << " " << QuotePath(sourcePath.string());
            break;
    }

    CommandResult compile = RunCommand(command.str() + " 2>&1");
    result.ExitCode = compile.ExitCode;
    result.Output = compile.Output;

    if (compile.ExitCode != 0) {
        result.Status = Error::BuildCompileFailed;
        return result;
    }

    if (!fs::exists(result.ExecutablePath)) {
        result.Status = Error::BuildCompileFailed;
        if (!result.Output.empty())
            result.Output += "\n";
        result.Output += "Executable was not produced: " + result.ExecutablePath;
        return result;
    }

    if (!result.Output.empty())
        result.Output += "\n";
    result.Output += "Compiled successfully.";
    return result;
}

BuildResult ProjectRunner::LaunchProgramInTerminal(
    const std::string &command,
    const std::string &workingDir,
    BuildResult prior)
{
    BuildResult result = std::move(prior);

    if (!::LaunchInTerminal(command, workingDir)) {
        result.Status = Error::BuildRunFailed;
        if (!result.Output.empty())
            result.Output += "\n";
        result.Output += "Failed to launch external terminal. Set TERMINAL or install a terminal emulator.";
        return result;
    }

    result.Status = Error::Ok;
    if (!result.Output.empty())
        result.Output += "\n";
    result.Output += "Program running in a separate terminal.";
    return result;
}

BuildResult ProjectRunner::RunInTerminal(
    const std::string &source,
    CodeLanguage language,
    const std::string &projectName)
{
    BuildResult result;

    std::optional<ToolInfo> tool = FindTool(language);
    if (!tool) {
        result.Status = Error::BuildToolNotFound;
        if (language == CodeLanguage::Python)
            result.Output = "No Python 3 interpreter found. Install Python 3 and ensure it is on PATH.";
        else
            result.Output = "No C++ compiler found. Install g++, clang++, or MSVC and ensure it is on PATH.";
        return result;
    }

    if (language == CodeLanguage::Python) {
        result.BuildDirectory = CreateBuildDirectory();
        const std::string baseName = SanitizeProjectName(projectName);
        const fs::path sourcePath = fs::path(result.BuildDirectory) / (baseName + ".py");

        {
            std::ofstream file(sourcePath);
            if (!file) {
                result.Status = Error::BuildWriteFailed;
                result.Output = "Failed to write source file: " + sourcePath.string();
                return result;
            }
            file << source;
        }

        result.Tool = *tool;
        std::ostringstream command;
        command << QuotePath(tool->Command) << " " << QuotePath(sourcePath.string());
        return LaunchProgramInTerminal(command.str(), result.BuildDirectory, std::move(result));
    }

    BuildResult compile = CompileCpp(source, *tool, projectName);
    if (compile.Status != Error::Ok)
        return compile;

    std::ostringstream command;
#ifdef _WIN32
    command << QuotePath(fs::path(compile.ExecutablePath).filename().string());
#else
    command << QuotePath(compile.ExecutablePath);
#endif

    BuildResult launched = LaunchProgramInTerminal(
        command.str(),
        fs::path(compile.ExecutablePath).parent_path().string(),
        compile);

    launched.Tool = compile.Tool;
    launched.ExecutablePath = compile.ExecutablePath;
    launched.BuildDirectory = compile.BuildDirectory;
    return launched;
}

BuildResult ProjectRunner::CompileAndRunInTerminal(
    const std::string &source,
    CodeLanguage language,
    const std::string &projectName)
{
    if (language == CodeLanguage::Python)
        return RunInTerminal(source, language, projectName);

    std::optional<ToolInfo> tool = FindTool(language);
    if (!tool) {
        BuildResult result;
        result.Status = Error::BuildToolNotFound;
        result.Output = "No C++ compiler found. Install g++, clang++, or MSVC and ensure it is on PATH.";
        return result;
    }

    BuildResult compile = CompileCpp(source, *tool, projectName);
    if (compile.Status != Error::Ok)
        return compile;

    std::ostringstream command;
#ifdef _WIN32
    command << QuotePath(fs::path(compile.ExecutablePath).filename().string());
#else
    command << QuotePath(compile.ExecutablePath);
#endif

    BuildResult launched = LaunchProgramInTerminal(
        command.str(),
        fs::path(compile.ExecutablePath).parent_path().string(),
        compile);

    launched.Tool = compile.Tool;
    launched.ExecutablePath = compile.ExecutablePath;
    launched.BuildDirectory = compile.BuildDirectory;
    return launched;
}
