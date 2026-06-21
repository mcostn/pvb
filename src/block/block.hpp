#pragma once

#include <string>
#include <vector>
#include <cstdint>

#include "codegen/codegen.hpp"
#include "util/error.hpp"

enum class BlockSchemaType
{
    Text,
    Input,
    Var,
};
struct BlockSchemaItem
{
    std::string Name;
    Value ValueType;
    BlockSchemaType Type;
};
using BlockSchema = std::vector<BlockSchemaItem>;
std::string BlockSchemaToString(const BlockSchema &schema);

enum class BlockCategory
{
    Console,
    ControlFlow,
    Math,
    Logic,
};
struct BlockDefinition
{
    std::string Fmt;
    std::string Description;
    std::string OpCode;
    BlockCategory Category;

    BlockSchema Schema {};
};

class BlockRegistry
{
    public:
        Error RegisterBlock(BlockDefinition def);

        std::vector<BlockDefinition> Definitions;
};

BlockRegistry GetBlockRegistry();
