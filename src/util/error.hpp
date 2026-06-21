#pragma once

#define TRY(expr)                                                \
    do {                                                         \
        auto _err = (expr);                                      \
        if (_err != Error::Ok) {                                 \
            return _err;                                         \
        }                                                        \
    } while(0)                                                   \

enum class [[nodiscard]] Error
{
    Ok = 0,
    Failed,
    Unreachable,

    FileNotFound,

    // Block Registry
    BlockInvalidFmt,
    BlockAlreadyExists,
};
