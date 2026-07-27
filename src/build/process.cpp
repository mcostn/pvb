#include "build/process.hpp"

#include <array>
#include <cstdio>
#include <sstream>

#ifdef _WIN32
    #define popen  _popen
    #define pclose _pclose
    #include <windows.h>
#else
    #include <unistd.h>
#endif

#ifdef _WIN32
static bool Win32SpawnDetached(std::string commandLine, bool newConsole)
{
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};

    DWORD flags = CREATE_NEW_PROCESS_GROUP;
    if (newConsole)
        flags |= CREATE_NEW_CONSOLE;

    BOOL ok = CreateProcessA(
        nullptr,
        commandLine.data(),
        nullptr,
        nullptr,
        FALSE,
        flags,
        nullptr,
        nullptr,
        &si,
        &pi);

    if (!ok)
        return false;

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}
#endif

CommandResult RunCommand(const std::string &command)
{
    CommandResult result;

    std::FILE *pipe = popen(command.c_str(), "r");
    if (!pipe)
        return result;

    std::array<char, 4096> buffer{};
    while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr)
        result.Output += buffer.data();

    result.ExitCode = pclose(pipe);
#ifndef _WIN32
    if (result.ExitCode != -1)
        result.ExitCode = (result.ExitCode >> 8) & 0xFF;
#endif

    return result;
}

std::string QuotePath(const std::string &path)
{
    if (path.find(' ') == std::string::npos)
        return path;

#ifdef _WIN32
    return '"' + path + '"';
#else
    std::ostringstream quoted;
    quoted << '"';
    for (char ch : path) {
        if (ch == '"' || ch == '\\' || ch == '$' || ch == '`')
            quoted << '\\';
        quoted << ch;
    }
    quoted << '"';
    return quoted.str();
#endif
}

static bool CommandOnPath(const std::string &name)
{
#ifdef _WIN32
    CommandResult result = RunCommand("where " + name + " >nul 2>&1");
#else
    CommandResult result = RunCommand("command -v " + name + " >/dev/null 2>&1");
#endif
    return result.ExitCode == 0;
}

static std::string TerminalScript(
    const std::string &command,
    const std::string &workingDir)
{
    std::ostringstream script;
    if (!workingDir.empty()) {
#ifdef _WIN32
        script << "cd /d " << QuotePath(workingDir) << " && ";
#else
        script << "cd " << QuotePath(workingDir) << " && ";
#endif
    }
    script << command;
#ifdef _WIN32
    script << " & echo. & echo Press any key to close... & pause >nul";
#else
    script << "; echo; echo \"Press Enter to close...\"; read -r";
#endif
    return script.str();
}

bool LaunchDetached(const std::string &command)
{
#ifdef _WIN32
    return Win32SpawnDetached(command, /*newConsole=*/false);
#else
    pid_t pid = fork();
    if (pid < 0)
        return false;

    if (pid == 0) {
        setsid();
        execl("/bin/sh", "sh", "-c", command.c_str(), static_cast<char *>(nullptr));
        _exit(127);
    }

    return true;
#endif
}

bool LaunchInTerminal(const std::string &command, const std::string &workingDir)
{
    const std::string script = TerminalScript(command, workingDir);

#ifdef _WIN32
    std::string launch = "cmd.exe /c \"" + script + "\"";
    return Win32SpawnDetached(std::move(launch), /*newConsole=*/true);

#elif defined(__APPLE__)
    std::ostringstream launch;
    launch << "osascript -e " << QuotePath(
        "tell application \"Terminal\" to do script \"" + script + "\"");

    CommandResult result = RunCommand(launch.str());
    return result.ExitCode == 0;

#else
    struct TerminalLaunch
    {
        const char *Name;
        const char *Command;
    };

    const TerminalLaunch candidates[] = {
        { "x-terminal-emulator", "-e bash -c" },
        { "gnome-terminal", "--disable-factory -- bash -c" },
        { "konsole", "-e bash -c" },
        { "xfce4-terminal", "-e bash -c" },
        { "alacritty", "-e bash -c" },
        { "kitty", "bash -c" },
        { "wezterm", "start -- bash -c" },
        { "xterm", "-e bash -c" },
    };

    if (const char *terminalEnv = std::getenv("TERMINAL")) {
        if (terminalEnv[0] != '\0' && CommandOnPath(terminalEnv)) {
            std::ostringstream launch;
            launch << QuotePath(terminalEnv) << " -e bash -c " << QuotePath(script);
            if (LaunchDetached(launch.str()))
                return true;
        }
    }

    for (const TerminalLaunch &candidate : candidates) {
        if (!CommandOnPath(candidate.Name))
            continue;

        std::ostringstream launch;
        launch << candidate.Name << ' ' << candidate.Command << ' '
            << QuotePath(script);

        if (LaunchDetached(launch.str()))
            return true;
    }

    return false;
#endif
}
