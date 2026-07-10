#pragma once

#include <vector>
#include <string>

#include "block/converter.hpp"
#include "util/error.hpp"

enum class BlockSchemaType
{
    Text,
    Input,
    Var,
    Body,
};

struct BlockSchemaItem
{
    std::string Name;
    Value ValueType;
    BlockSchemaType Type;
};

using BlockSchema = std::vector<BlockSchemaItem>;

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

    BlockConverter::StmtBuilder StmtBuilder = nullptr;
    BlockConverter::ExprBuilder ExprBuilder = nullptr;
};

Error GenerateBlockSchema(BlockDefinition &def);
std::string BlockSchemaToString(const BlockSchema &schema);
std::vector<std::string> BlockSchemaBodySlots(const BlockSchema &schema);
