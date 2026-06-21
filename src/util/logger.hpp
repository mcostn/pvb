#pragma once

#include <iostream>
#include <format>
#include <chrono>

enum class LogLevel
{
    Debug = 0,
    Info,
    Warn,
    Error,
    Critical
};

// TODO: Improve Performance
class Logger
{
    public:
        Logger(const Logger&) = delete;
        Logger& operator=(const Logger&) = delete;

        Logger(std::ostream &out = std::cout,
               std::ostream &err = std::cerr,
               LogLevel minLevel = LogLevel::Debug)
            : OutStream(out), ErrStream(err), MinLevel(minLevel) {}

        template <typename... Args>
        void Log(LogLevel level, std::format_string<Args...> fmt, Args&&... args)
        {
            if (level < MinLevel)
                return;

            auto& stream = (level >= LogLevel::Error) ? ErrStream
                                                      : OutStream;

            try {
                // TODO: Time formatting is expensive
                auto now = std::chrono::system_clock::now();
                stream << "[" << std::format("{:%Y-%m-%d %H:%M:%S}", now) << "] "
                       << "[" << ToString(level) << "] "
                       << std::format(fmt, std::forward<Args>(args)...)
                       << '\n';
            } catch (...) {
                stream << "[LOGGER FORMAT ERROR]\n";
            }
        }

        // TODO: Create a macro that avois this repetition
        template <typename... Args>
        void Info(std::format_string<Args...> fmt, Args&&... args)
        {
            Log(LogLevel::Info, fmt, std::forward<Args>(args)...);
        }

        template <typename... Args>
        void Warn(std::format_string<Args...> fmt, Args&&... args)
        {
            Log(LogLevel::Warn, fmt, std::forward<Args>(args)...);
        }

        template <typename... Args>
        void Error(std::format_string<Args...> fmt, Args&&... args)
        {
            Log(LogLevel::Error, fmt, std::forward<Args>(args)...);
        }

        template <typename... Args>
        void Debug(std::format_string<Args...> fmt, Args&&... args)
        {
            Log(LogLevel::Debug, fmt, std::forward<Args>(args)...);
        }

        template <typename... Args>
        void Critical(std::format_string<Args...> fmt, Args&&... args)
        {
            Log(LogLevel::Critical, fmt, std::forward<Args>(args)...);
        }

        std::ostream &OutStream;
        std::ostream &ErrStream;
        LogLevel MinLevel;

    private:
        // TODO: Delete in favour of a string table
        static constexpr std::string_view ToString(LogLevel level)
        {
            switch (level)
            {
                case LogLevel::Debug:    return "DEBUG";
                case LogLevel::Info:     return "INFO";
                case LogLevel::Warn:     return "WARN";
                case LogLevel::Error:    return "ERROR";
                case LogLevel::Critical: return "CRITICAL";
            }
            return "UNKNOWN";
        }
};

inline Logger GlobalLogger;
