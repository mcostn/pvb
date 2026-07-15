#pragma once

#include <vector>

#include "util/error.hpp"
#include "block/definition.hpp"
#include "block/converter.hpp"

class BlockRegistry
{
    public:
        Error RegisterBlock(BlockDefinition def);

        std::vector<BlockDefinition> Definitions;
        BlockConverter Converter;
};

BlockRegistry GetBlockRegistry();
