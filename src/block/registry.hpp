#pragma once

#include <deque>
#include <string>
#include <vector>

#include "util/error.hpp"
#include "block/definition.hpp"
#include "block/converter.hpp"

struct VariableInfo
{
    std::string Name;
    Value Type = VAL_INT;
};

class BlockRegistry
{
    public:
        Error RegisterBlock(BlockDefinition def);

        Error AddVariable(const std::string &name, Value type);
        Error RemoveVariable(const std::string &name);
        bool HasVariable(const std::string &name) const;

        std::deque<BlockDefinition> Definitions;
        std::vector<VariableInfo> Variables;
        std::vector<std::string> CustomBlocks;
        BlockConverter Converter;
};

BlockRegistry GetBlockRegistry();

std::string VarGetOpCode(const std::string &name);
std::string VarSetOpCode(const std::string &name);
