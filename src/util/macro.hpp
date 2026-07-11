#pragma once

#include "util/logger.hpp"

#define DISCARD(expr) static_cast<void>(expr)

#define FAIL_COND_V_MSG(cond, v, msg, ...)                       \
    do {                                                         \
        if ((cond)) {                                            \
            GlobalLogger.Error((msg), ##__VA_ARGS__);            \
            return (v);                                          \
        }                                                        \
    } while (0)

#define CONCAT(a, b) CONCAT_IMPL(a, b)
#define CONCAT_IMPL(a, b) a##b
