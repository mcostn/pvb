#pragma once

#include <memory>
#include <variant>
#include <string>
#include <unordered_map>
#include <vector>

#include "codegen/frontend.hpp"

struct BlockInstance;

struct VariableRef
{
    std::string Name;
};

using BlockArg = std::variant<
    LiteralValue,
    VariableRef,
    std::unique_ptr<BlockInstance>
>;

struct BlockInstance
{
    std::string OpCode;
    std::unordered_map<std::string, BlockArg> Args;

    // Only used by control-flow blocks that contain one or more stacks of
    // statements, keyed by slot name (e.g. "then"/"else" for if-else,
    // "body" for while/for).
    std::unordered_map<std::string, std::vector<std::unique_ptr<BlockInstance>>> Bodies;
};
