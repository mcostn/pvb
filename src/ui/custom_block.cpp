#include "ui/custom_block.hpp"

#include <algorithm>
#include <cstring>

#include "block/converter.hpp"
#include "block/instance.hpp"
#include "codegen/frontend.hpp"
#include "util/macro.hpp"

static const char *kCallPrefix  = "custom:";
static const char *kHatPrefix   = "customdef:";
static const char *kParamPrefix = "customparam:";

std::string CustomCallOpCode(const std::string &name) { return kCallPrefix + name; }
std::string CustomHatOpCode(const std::string &name)  { return kHatPrefix + name; }

std::string CustomParamOpCode(const std::string &funcName, const std::string &paramName)
{
    return kParamPrefix + funcName + ":" + paramName;
}

bool IsCustomCall(const BlockDefinition &def)
{
    return def.OpCode.rfind(kCallPrefix, 0) == 0;
}

bool IsCustomHat(const BlockDefinition &def)
{
    return def.OpCode.rfind(kHatPrefix, 0) == 0;
}

bool IsCustomParam(const BlockDefinition &def)
{
    return def.OpCode.rfind(kParamPrefix, 0) == 0;
}

std::string CustomBlockName(const BlockDefinition &def)
{
    if (IsCustomHat(def))
        return def.OpCode.substr(strlen(kHatPrefix));
    if (IsCustomCall(def))
        return def.OpCode.substr(strlen(kCallPrefix));
    return {};
}

const BlockDefinition *FindDefinitionByOpCode(const BlockRegistry &registry, const std::string &opcode)
{
    for (const BlockDefinition &def : registry.Definitions)
        if (def.OpCode == opcode)
            return &def;
    return nullptr;
}

std::vector<const BlockDefinition *> CustomBlockParamDefs(const BlockRegistry &registry, const std::string &funcName)
{
    std::vector<const BlockDefinition *> out;
    std::string prefix = std::string(kParamPrefix) + funcName + ":";

    for (const BlockDefinition &def : registry.Definitions)
        if (def.OpCode.rfind(prefix, 0) == 0)
            out.push_back(&def);

    return out;
}

static BlockConverter::StmtBuilder MakeCustomCallStmtBuilder(CustomBlockSpec spec)
{
    return [spec](BlockConverter &conv, const BlockInstance &inst) -> std::unique_ptr<Stmt>
    {
        auto call = Call(spec.Name);

        for (const CustomBlockParam &p : spec.Params) {
            auto it = inst.Args.find(p.Name);

            std::unique_ptr<Expr> argExpr = (it != inst.Args.end())
                ? conv.ResolveArg(it->second, p.Type)
                : nullptr;

            call->Args.push_back(std::move(argExpr));
        }

        return ExprStatement(std::move(call));
    };
}

static BlockConverter::ExprBuilder MakeCustomParamExprBuilder(std::string paramName, Value paramType)
{
    return [paramName, paramType](BlockConverter&, const BlockInstance&) -> std::unique_ptr<Expr>
    {
        return Var(paramName, paramType);
    };
}

Error RegisterCustomBlock(BlockRegistry &registry, const CustomBlockSpec &spec)
{
    FAIL_COND_V_MSG(
            !registry.IsValidIdentifier(spec.Name),
            Error::CustomBlockInvalidName,
            "Block name '{}' is invalid",
            spec.Name);

    for (const CustomBlockParam &p : spec.Params) {
        FAIL_COND_V_MSG(
                !registry.IsValidIdentifier(p.Name),
                Error::CustomBlockInvalidParamName,
                "Block input name '{}' is invalid",
                p.Name);
    }

    const std::string callOp = CustomCallOpCode(spec.Name);
    const std::string hatOp  = CustomHatOpCode(spec.Name);

    FAIL_COND_V_MSG(
            FindDefinitionByOpCode(registry, callOp) || FindDefinitionByOpCode(registry, hatOp),
            Error::CustomBlockAlreadyExists,
            "A block named '{}' already exists",
            spec.Name);

    BlockDefinition call;
    call.OpCode = callOp;
    call.Category = BlockCategory::Custom;
    call.Shape = BlockShape::Chain;
    call.Description = spec.Description;

    std::string callFmt = spec.Name;
    call.Schema.push_back({ spec.Name, VAL_NONE, BlockSchemaType::Text });
    for (const CustomBlockParam &p : spec.Params) {
        call.Schema.push_back({ p.Name, p.Type, BlockSchemaType::Input });
        callFmt += " " + p.Name;
    }
    call.Fmt = callFmt;

    call.StmtBuilder = MakeCustomCallStmtBuilder(spec);
    registry.Converter.StmtBuilders[callOp] = call.StmtBuilder;

    BlockDefinition hat;
    hat.OpCode = hatOp;
    hat.Category = BlockCategory::Custom;
    hat.Shape = BlockShape::Hat;
    hat.Description = spec.Description;

    std::string defineLabel = "Define \"" + spec.Name + "\"";
    hat.Schema.push_back({ defineLabel, VAL_NONE, BlockSchemaType::Text });
    hat.Fmt = defineLabel;

    registry.Definitions.push_back(std::move(call));
    registry.Definitions.push_back(std::move(hat));

    for (const CustomBlockParam &p : spec.Params) {
        BlockDefinition param;
        param.OpCode = CustomParamOpCode(spec.Name, p.Name);
        param.Category = BlockCategory::Custom;
        param.Shape = BlockShape::Reporter;
        param.ReturnType = p.Type;
        param.Description = "Parameter of \"" + spec.Name + "\"";
        param.Schema.push_back({ p.Name, p.Type, BlockSchemaType::Text });
        param.Fmt = p.Name;

        param.ExprBuilder = MakeCustomParamExprBuilder(p.Name, p.Type);
        registry.Converter.ExprBuilders[param.OpCode] = param.ExprBuilder;

        registry.Definitions.push_back(std::move(param));
    }

    registry.CustomBlocks.push_back(spec.Name);

    return Error::Ok;
}

bool IsCustomBlockRegistered(const BlockRegistry &registry, const std::string &name)
{
    return std::find(registry.CustomBlocks.begin(), registry.CustomBlocks.end(), name)
        != registry.CustomBlocks.end();
}

Error UnregisterCustomBlock(BlockRegistry &registry, const std::string &name)
{
    auto it = std::find(registry.CustomBlocks.begin(), registry.CustomBlocks.end(), name);

    if (it == registry.CustomBlocks.end())
        return Error::CustomBlockNotFound;

    registry.Converter.StmtBuilders.erase(CustomCallOpCode(name));

    for (const BlockDefinition *paramDef : CustomBlockParamDefs(registry, name))
        registry.Converter.ExprBuilders.erase(paramDef->OpCode);

    registry.CustomBlocks.erase(it);

    return Error::Ok;
}

Error RenameCustomBlock(BlockRegistry &registry, const std::string &oldName, const std::string &newName)
{
    FAIL_COND_V_MSG(
            !registry.IsValidIdentifier(newName),
            Error::CustomBlockInvalidName,
            "Block name '{}' is invalid",
            newName);

    FAIL_COND_V_MSG(
            !IsCustomBlockRegistered(registry, oldName),
            Error::CustomBlockNotFound,
            "Tried to rename block '{}', which doesn't exist",
            oldName);

    if (newName == oldName)
        return Error::Ok;

    FAIL_COND_V_MSG(
            IsCustomBlockRegistered(registry, newName) ||
                FindDefinitionByOpCode(registry, CustomCallOpCode(newName)) ||
                FindDefinitionByOpCode(registry, CustomHatOpCode(newName)),
            Error::CustomBlockAlreadyExists,
            "A block named '{}' already exists",
            newName);

    const std::string oldCallOp = CustomCallOpCode(oldName);
    const std::string oldHatOp = CustomHatOpCode(oldName);
    const std::string oldParamPrefix = std::string(kParamPrefix) + oldName + ":";

    BlockDefinition *callDef = nullptr;
    BlockDefinition *hatDef = nullptr;
    std::vector<BlockDefinition *> paramDefs;

    for (BlockDefinition &def : registry.Definitions) {
        if (def.OpCode == oldCallOp)
            callDef = &def;
        else if (def.OpCode == oldHatOp)
            hatDef = &def;
        else if (def.OpCode.rfind(oldParamPrefix, 0) == 0)
            paramDefs.push_back(&def);
    }

    if (!callDef || !hatDef)
        return Error::Failed;

    const std::string newCallOp = CustomCallOpCode(newName);
    const std::string newHatOp = CustomHatOpCode(newName);

    CustomBlockSpec renamedSpec;
    renamedSpec.Name = newName;
    renamedSpec.Description = callDef->Description;

    if (!callDef->Schema.empty())
        callDef->Schema[0].Name = newName;

    std::string callFmt = newName;
    for (const BlockSchemaItem &item : callDef->Schema) {
        if (item.Type == BlockSchemaType::Input) {
            callFmt += " " + item.Name;
            renamedSpec.Params.push_back({ item.Name, item.ValueType });
        }
    }
    callDef->Fmt = callFmt;
    callDef->OpCode = newCallOp;
    callDef->StmtBuilder = MakeCustomCallStmtBuilder(renamedSpec);

    registry.Converter.StmtBuilders.erase(oldCallOp);
    registry.Converter.StmtBuilders.emplace(newCallOp, callDef->StmtBuilder);

    std::string defineLabel = "Define \"" + newName + "\"";
    if (!hatDef->Schema.empty())
        hatDef->Schema[0].Name = defineLabel;

    std::string hatFmt = defineLabel;
    for (size_t i = 1; i < hatDef->Schema.size(); ++i)
        hatFmt += " " + hatDef->Schema[i].Name;
    hatDef->Fmt = hatFmt;
    hatDef->OpCode = newHatOp;

    for (BlockDefinition *paramDef : paramDefs) {
        std::string paramName = paramDef->OpCode.substr(oldParamPrefix.size());
        std::string newParamOp = CustomParamOpCode(newName, paramName);

        paramDef->Description = "Parameter of \"" + newName + "\"";

        registry.Converter.ExprBuilders.erase(paramDef->OpCode);
        paramDef->OpCode = newParamOp;
        registry.Converter.ExprBuilders.emplace(newParamOp, paramDef->ExprBuilder);
    }

    auto nameIt = std::find(registry.CustomBlocks.begin(), registry.CustomBlocks.end(), oldName);
    if (nameIt != registry.CustomBlocks.end())
        *nameIt = newName;

    return Error::Ok;
}
