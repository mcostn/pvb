#pragma once

#include <string>

#define TRY(expr) \
    do { \
        auto _err = (expr); \
        if (_err != Error::Ok) { \
            return _err; \
        } \
    } while(0)

enum class [[nodiscard]] Error
{
    Ok = 0,
    Failed,
    Unreachable,

    FileNotFound,

    IniParseError,
    IniInvalidSection,
    IniInvalidKey,
    IniDuplicateKey,
    IniDuplicateSection,
    IniWriteError,

    ProjectInvalidVersion,
    ProjectMissingSection,
    ProjectInvalidData,

    BlockInvalidFmt,
    BlockInvalidDefinition,
    BlockAlreadyExists,

    VariableAlreadyExists,
    VariableNotFound,
    CustomBlockAlreadyExists,
    CustomBlockNotFound,

    BuildToolNotFound,
    BuildWriteFailed,
    BuildCompileFailed,
    BuildRunFailed,
};

struct ErrorDetail
{
    static inline thread_local std::string Message;
};

[[nodiscard]]
inline const char *to_string(Error err)
{
    switch (err)
    {
        case Error::Ok: return "Success";
        case Error::Failed: return "Operation failed";
        case Error::Unreachable: return "Unreachable code";

        case Error::FileNotFound: return "File not found";

        case Error::IniParseError: return "Invalid INI format";
        case Error::IniInvalidSection: return "Invalid INI section";
        case Error::IniInvalidKey: return "Invalid INI key";
        case Error::IniDuplicateKey: return "Duplicate INI key";
        case Error::IniDuplicateSection: return "Duplicate INI section";
        case Error::IniWriteError: return "Failed writing file";

        case Error::ProjectInvalidVersion: return "Unsupported project version";
        case Error::ProjectMissingSection: return "Missing project section";
        case Error::ProjectInvalidData: return "Invalid project data";

        case Error::BlockInvalidFmt: return "Invalid block format";
        case Error::BlockInvalidDefinition: return "Unknown block definition";
        case Error::BlockAlreadyExists: return "Block already exists";

        case Error::VariableAlreadyExists: return "Variable already exists";
        case Error::VariableNotFound: return "Variable not found";
        case Error::CustomBlockAlreadyExists: return "Custom block already exists";
        case Error::CustomBlockNotFound: return "Block not found";

        case Error::BuildToolNotFound: return "Required build tool not found";
        case Error::BuildWriteFailed: return "Failed to write build artifacts";
        case Error::BuildCompileFailed: return "Compilation failed";
        case Error::BuildRunFailed: return "Program execution failed";
    }

    return "Unknown error";
}
