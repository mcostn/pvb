#pragma once

#include <string>

#include "codegen/language.hpp"
#include "build/toolchain.hpp"
#include "util/error.hpp"

struct BuildResult
{
    Error Status = Error::Ok;
    std::string Output;
    int ExitCode = 0;
    std::optional<ToolInfo> Tool;       // primary tool (compiler/interpreter/assembler)
    std::optional<ToolInfo> LinkerTool; // set alongside Tool when a separate link step ran (e.g. asm)
    std::string ExecutablePath;
    std::string BuildDirectory;
    std::string CompileCommand;
    std::string RunCommand;
};

class ProjectRunner
{
public:
    static BuildResult RunInTerminal(
        const std::string &source,
        CodeLanguage language,
        const std::string &projectName = "program");

    static BuildResult CompileAndRunInTerminal(
        const std::string &source,
        CodeLanguage language,
        const std::string &projectName = "program");

private:
    [[nodiscard]] static std::optional<ToolInfo> FindTool(CodeLanguage language);
    static BuildResult CompileCpp(
        const std::string &source,
        const ToolInfo &compiler,
        const std::string &projectName);
    static BuildResult CompileAsm(
        const std::string &source,
        const ToolInfo &assembler,
        const ToolInfo &linker,
        const std::string &projectName);
    static BuildResult LaunchProgramInTerminal(
        const std::string &command,
        const std::string &workingDir,
        BuildResult prior = {});
    static BuildResult LaunchCompiledProgram(BuildResult compile);
    [[nodiscard]] static std::string SanitizeProjectName(const std::string &name);
    [[nodiscard]] static std::string CreateBuildDirectory();
};
