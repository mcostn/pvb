#pragma once

#include <vector>
#include <string>
#include <unordered_map>

#include "block/converter.hpp"
#include "util/error.hpp"

enum class BlockSchemaType
{
    Text,
    Input,
    Var,
    Body,
    LineBreak,
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
    Event,
    Console,
    ControlFlow,
    Math,
    Logic,
    Variable,
    Custom,
};

enum class BlockShape
{
    Unknown,
    Chain,
    Hat,
    Cap,
    Reporter,
};

struct BlockDefinition
{
    std::string Fmt;
    std::string Description;
    std::string OpCode;
    BlockCategory Category;

    BlockSchema Schema {};

    BlockShape Shape = BlockShape::Unknown;
    BlockConverter::StmtBuilder StmtBuilder = nullptr;
    BlockConverter::ExprBuilder ExprBuilder = nullptr;
    Value ReturnType = VAL_NONE;

    std::unordered_map<std::string, LiteralValue> DefaultValues {};
};

Error GenerateBlockSchema(BlockDefinition &def);
std::vector<std::string> BlockSchemaBodySlots(const BlockSchema &schema);
