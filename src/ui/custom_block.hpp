#pragma once

#include <string>
#include <vector>

#include "block/definition.hpp"
#include "block/registry.hpp"
#include "util/error.hpp"

struct CustomBlockParam
{
    std::string Name;
    Value Type = VAL_ANY;
};

struct CustomBlockSpec
{
    std::string Name;
    std::string Description;
    std::vector<CustomBlockParam> Params;
};

std::string CustomCallOpCode(const std::string &name);
std::string CustomHatOpCode(const std::string &name);

bool IsCustomCall(const BlockDefinition &def);
bool IsCustomHat(const BlockDefinition &def);

std::string CustomBlockName(const BlockDefinition &def);

const BlockDefinition *FindDefinitionByOpCode(const BlockRegistry &registry, const std::string &opcode);

Error RegisterCustomBlock(BlockRegistry &registry, const CustomBlockSpec &spec);
Error UnregisterCustomBlock(BlockRegistry &registry, const std::string &name);
bool IsCustomBlockRegistered(const BlockRegistry &registry, const std::string &name);
std::vector<const BlockDefinition *> CustomBlockParamDefs(const BlockRegistry &registry, const std::string &funcName);
