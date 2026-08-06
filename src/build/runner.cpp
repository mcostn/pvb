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
        case CodeLanguage::Asm:    return Toolchain::FindNasm();
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
                << " -std=c++11 -o " << QuotePath(result.ExecutablePath)
                << " " << QuotePath(sourcePath.string());
            break;
    }

    CommandResult compile = RunCommand(command.str() + " 2>&1");
    result.CompileCommand = command.str();
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

#ifdef _WIN32
#include <algorithm>
#include <cctype>

static std::string ToWslPath(std::string path)
{
    std::replace(path.begin(), path.end(), '\\', '/');

    if (path.size() >= 2 && path[1] == ':') {
        char drive = static_cast<char>(std::tolower(path[0]));
        return "/mnt/" + std::string(1, drive) + path.substr(2);
    }

    return path;
}

static std::string WslShellQuote(const std::string &s)
{
    std::string out = "'";
    for (char c : s) {
        if (c == '\'')
            out += "'\"'\"'";
        else
            out += c;
    }
    out += "'";
    return out;
}

static std::string BuildWslCommand(const std::string &bashCommand)
{
    return "wsl.exe -e bash -lc \"" + bashCommand + "\"";
}
#endif

BuildResult ProjectRunner::CompileAsm(
    const std::string &source,
    const ToolInfo &assembler,
    const ToolInfo &linker,
    const std::string &projectName)
{
    BuildResult result;
    result.Tool = assembler;
    result.LinkerTool = linker;

    result.BuildDirectory = CreateBuildDirectory();
    const std::string baseName = SanitizeProjectName(projectName);
    const fs::path sourcePath = fs::path(result.BuildDirectory) / (baseName + ".asm");

#ifdef _WIN32
    if (assembler.ViaWsl) {
        const fs::path objPath = fs::path(result.BuildDirectory) / (baseName + ".o");
        result.ExecutablePath = (fs::path(result.BuildDirectory) / baseName).string();

        {
            std::ofstream file(sourcePath);
            if (!file) {
                result.Status = Error::BuildWriteFailed;
                result.Output = "Failed to write source file: " + sourcePath.string();
                return result;
            }
            file << source;
        }

        const std::string wslDir = WslShellQuote(ToWslPath(result.BuildDirectory));

        std::ostringstream assembleCommand;
        assembleCommand << "cd " << wslDir
            << " && " << assembler.Command
            << " -f elf64 -o " << (baseName + ".o")
            << " " << (baseName + ".asm");

        std::string assembleFull = BuildWslCommand(assembleCommand.str());
        CommandResult assemble = RunCommand(assembleFull + " 2>&1");
        result.CompileCommand = assembleFull;
        result.ExitCode = assemble.ExitCode;
        result.Output = assemble.Output;

        if (assemble.ExitCode != 0) {
            result.Status = Error::BuildCompileFailed;
            return result;
        }

        std::ostringstream linkCommand;
        linkCommand << "cd " << wslDir
            << " && " << linker.Command
            << " -no-pie -o " << baseName
            << " " << (baseName + ".o")
            << " -lm";

        std::string linkFull = BuildWslCommand(linkCommand.str());
        CommandResult link = RunCommand(linkFull + " 2>&1");
        result.ExitCode = link.ExitCode;
        if (!result.Output.empty())
            result.Output += "\n";
        result.Output += link.Output;

        if (link.ExitCode != 0) {
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
        result.Output += "Assembled and linked successfully (via WSL).";
        return result;
    }
#endif

#if defined(__APPLE__)
    const char *objFormat = "macho64";
    const char *linkerExtraFlags = "";
#else
    const char *objFormat = "elf64";
    const char *linkerExtraFlags = "-no-pie";
#endif
    const fs::path objPath = fs::path(result.BuildDirectory) / (baseName + ".o");
    result.ExecutablePath = (fs::path(result.BuildDirectory) / baseName).string();

    {
        std::ofstream file(sourcePath);
        if (!file) {
            result.Status = Error::BuildWriteFailed;
            result.Output = "Failed to write source file: " + sourcePath.string();
            return result;
        }
        file << source;
    }

    std::ostringstream assembleCommand;
    assembleCommand << QuotePath(assembler.Command)
        << " -f " << objFormat
        << " -o " << QuotePath(objPath.string())
        << " " << QuotePath(sourcePath.string());

    CommandResult assemble = RunCommand(assembleCommand.str() + " 2>&1");
    result.CompileCommand = assembleCommand.str();
    result.ExitCode = assemble.ExitCode;
    result.Output = assemble.Output;

    if (assemble.ExitCode != 0) {
        result.Status = Error::BuildCompileFailed;
        return result;
    }

    if (!fs::exists(objPath)) {
        result.Status = Error::BuildCompileFailed;
        if (!result.Output.empty())
            result.Output += "\n";
        result.Output += "Object file was not produced: " + objPath.string();
        return result;
    }

    std::ostringstream linkCommand;
    switch (linker.Compiler) {
        case CompilerKind::Msvc:
            linkCommand << "cd /d " << QuotePath(result.BuildDirectory)
                << " && " << QuotePath(linker.Command)
                << " /nologo /Fe:" << QuotePath(fs::path(result.ExecutablePath).filename().string())
                << " " << QuotePath(objPath.filename().string());
            break;

        case CompilerKind::Gcc:
        case CompilerKind::Clang:
            linkCommand << QuotePath(linker.Command)
                << " -o " << QuotePath(result.ExecutablePath)
                << " " << QuotePath(objPath.string())
                << " -lm";
            if (*linkerExtraFlags)
                linkCommand << " " << linkerExtraFlags;
            break;
    }

    CommandResult link = RunCommand(linkCommand.str() + " 2>&1");
    result.ExitCode = link.ExitCode;
    if (!result.Output.empty())
        result.Output += "\n";
    result.Output += link.Output;

    if (link.ExitCode != 0) {
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
    result.Output += "Assembled and linked successfully.";
    return result;
}

BuildResult ProjectRunner::LaunchProgramInTerminal(
    const std::string &command,
    const std::string &workingDir,
    BuildResult prior)
{
    BuildResult result = std::move(prior);
    result.RunCommand = command;

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

BuildResult ProjectRunner::LaunchCompiledProgram(BuildResult compile)
{
    std::ostringstream command;
    const std::optional<ToolInfo> &linkedWith = compile.LinkerTool ? compile.LinkerTool : compile.Tool;

#ifdef _WIN32
    if (linkedWith && linkedWith->ViaWsl) {
        const std::string baseName = fs::path(compile.ExecutablePath).filename().string();
        const std::string wslDir = WslShellQuote(ToWslPath(compile.BuildDirectory));

        std::ostringstream runCmd;
        runCmd << "cd " << wslDir
            << " && chmod +x " << baseName
            << " && ./" << baseName;
        command << BuildWslCommand(runCmd.str());
    } else {
        if (linkedWith && linkedWith->Compiler != CompilerKind::Msvc) {
            fs::path compilerDir = fs::path(linkedWith->Command).parent_path();
            if (!compilerDir.empty())
                command << "set \"PATH=" << compilerDir.string() << ";%PATH%\" && ";
        }

        command << QuotePath(fs::path(compile.ExecutablePath).filename().string());
    }
#else
    command << QuotePath(compile.ExecutablePath);
#endif

    const std::string runCommand = command.str();

    BuildResult launched = LaunchProgramInTerminal(
        runCommand,
        fs::path(compile.ExecutablePath).parent_path().string(),
        compile);

    launched.Tool = compile.Tool;
    launched.LinkerTool = compile.LinkerTool;
    launched.ExecutablePath = compile.ExecutablePath;
    launched.BuildDirectory = compile.BuildDirectory;
    launched.RunCommand = runCommand;
    return launched;
}

static std::optional<ToolInfo> FindAsmLinker(const ToolInfo &assembler)
{
#ifdef _WIN32
    if (assembler.ViaWsl)
        return Toolchain::FindCppCompilerInWsl();
#endif
    (void)assembler;
    return Toolchain::FindCppCompiler();
}

static std::string AsmNotFoundMessage()
{
#ifdef _WIN32
    if (!Toolchain::HasWsl()) {
        return "No NASM assembler found. Building .asm on Windows requires WSL -- "
               "install it (`wsl --install`), then install nasm inside your Linux "
               "distro (e.g. `sudo apt install nasm`).";
    }
    return "No NASM assembler found inside WSL. Install it in your WSL distro "
           "(e.g. `sudo apt install nasm`).";
#else
    return "No NASM assembler found. Install NASM and ensure it is on PATH.";
#endif
}

static std::string AsmLinkerNotFoundMessage(const ToolInfo &assembler)
{
#ifdef _WIN32
    if (assembler.ViaWsl) {
        return "No C compiler found inside WSL to link the assembled program. "
               "Install one in your WSL distro (e.g. `sudo apt install build-essential`).";
    }
#else
    (void)assembler;
#endif
    return "No C/C++ compiler found to link the assembled program. Install g++, clang++, or MSVC and ensure it is on PATH.";
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
        else if (language == CodeLanguage::Asm)
            result.Output = AsmNotFoundMessage();
        else
            result.Output = "No C++ compiler found. Install g++, clang++, or MSVC and ensure it is on PATH.";
        return result;
    }

    if (language == CodeLanguage::Asm) {
        std::optional<ToolInfo> linker = FindAsmLinker(*tool);
        if (!linker) {
            result.Status = Error::BuildToolNotFound;
            result.Output = AsmLinkerNotFoundMessage(*tool);
            return result;
        }

        BuildResult compile = CompileAsm(source, *tool, *linker, projectName);
        if (compile.Status != Error::Ok)
            return compile;

        return LaunchCompiledProgram(std::move(compile));
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
        command << QuotePath(tool->Command);
        if (!tool->ExtraArgs.empty())
            command << " " << tool->ExtraArgs;
        command << " " << QuotePath(sourcePath.string());
        result.RunCommand = command.str();
        return LaunchProgramInTerminal(command.str(), result.BuildDirectory, std::move(result));
    }

    BuildResult compile = CompileCpp(source, *tool, projectName);
    if (compile.Status != Error::Ok)
        return compile;

    return LaunchCompiledProgram(std::move(compile));
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
        if (language == CodeLanguage::Asm)
            result.Output = AsmNotFoundMessage();
        else
            result.Output = "No C++ compiler found. Install g++, clang++, or MSVC and ensure it is on PATH.";
        return result;
    }

    if (language == CodeLanguage::Asm) {
        std::optional<ToolInfo> linker = FindAsmLinker(*tool);
        if (!linker) {
            BuildResult result;
            result.Status = Error::BuildToolNotFound;
            result.Output = AsmLinkerNotFoundMessage(*tool);
            return result;
        }

        BuildResult compile = CompileAsm(source, *tool, *linker, projectName);
        if (compile.Status != Error::Ok)
            return compile;

        return LaunchCompiledProgram(std::move(compile));
    }

    BuildResult compile = CompileCpp(source, *tool, projectName);
    if (compile.Status != Error::Ok)
        return compile;

    return LaunchCompiledProgram(std::move(compile));
}
