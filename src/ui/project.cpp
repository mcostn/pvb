#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <algorithm>

#include "ui/project.hpp"
#include "block/registry.hpp"
#include "util/ini.hpp"

static const BlockDefinition *FindBlockDefinition(
        const BlockRegistry &registry,
        const std::string &opcode);
static const BlockSchemaItem *FindSchemaItem(
        const BlockDefinition &def,
        const std::string &name);

static Error GetString(
    const IniFile &ini,
    const std::string &section,
    const std::string &key,
    std::string &out);

static Error GetU32(
    const IniFile &ini,
    const std::string &section,
    const std::string &key,
    u32 &out);

static Error GetFloat(
    const IniFile &ini,
    const std::string &section,
    const std::string &key,
    float &out);

static Error ParseIdList(
        const std::string &s,
        std::vector<u32> &out);

static Error TypedToArg(
        const std::string &type,
        const std::string &rawValue,
        const BlockRegistry &registry,
        VisualArg &out);

static std::pair<std::string, std::string> LiteralToTyped(const LiteralValue &lit);
static void CollectBlocks(
        VisualBlock *head,
        std::vector<VisualBlock *> &out,
        std::unordered_set<VisualBlock *> &seen);
static Error ValidateGraph(
        VisualBlock *head,
        std::unordered_set<VisualBlock *> &seen);

static Error VariableTypeToString(Value type, std::string &out);
static Error VariableTypeFromString(const std::string &str, Value &out);

static Error CodeLanguageToString(CodeLanguage lang, std::string &out);
static Error CodeLanguageFromString(const std::string &str, CodeLanguage &out);

Error SaveProject(
        const Canvas &canvas,
        const BlockRegistry &registry,
        const std::string &path,
        const ProjectSettings &settings)
{
    std::vector<VisualBlock*> blocks;
    std::unordered_set<VisualBlock*> seen;

    for (VisualBlock *root : canvas.Manager.Roots)
        CollectBlocks(root, blocks, seen);

    IniFile ini;

    // Project metadata
    ini.SetValue(
            "project",
            "name",
            IniFile::Escape(settings.Name));

    ini.SetValue(
            "project",
            "description",
            IniFile::Escape(settings.Description));

    std::string languageStr;
    DISCARD(CodeLanguageToString(settings.Language, languageStr));

    ini.SetValue(
            "project",
            "language",
            languageStr);

    ini.SetValue(
            "project",
            "pvb_version",
            "1.0");

    ini.SetValue(
            "project",
            "block_count",
            std::to_string(blocks.size()));

    ini.SetValue(
            "project",
            "comment_count",
            std::to_string(canvas.Comments.size()));

    ini.SetValue(
            "project",
            "root_count",
            std::to_string(canvas.Manager.Roots.size()));

    ini.SetValue(
            "project",
            "variable_count",
            std::to_string(registry.Variables.size()));


    std::ostringstream roots;
    for (size_t i = 0; i < canvas.Manager.Roots.size(); ++i) {
        if (i != 0)
            roots << ",";

        roots << canvas.Manager.Roots[i]->Id;
    }

    ini.SetValue(
            "project",
            "roots",
            roots.str());


    // Variables
    for (size_t i = 0; i < registry.Variables.size(); ++i) {
        const VariableInfo &v = registry.Variables[i];
        std::string section = "variable" + std::to_string(i);

        ini.SetValue(section, "name", IniFile::Escape(v.Name));

        std::string typeStr;
        TRY(VariableTypeToString(v.Type, typeStr));
        ini.SetValue(section, "type", typeStr);
    }


    // Blocks
    for (size_t i = 0; i < blocks.size(); ++i) {
        VisualBlock *b = blocks[i];

        std::string section = "block" + std::to_string(i);
        std::string opcode = b->Def ? b->Def->OpCode : std::string{};

        ini.SetValue(
                section,
                "id",
                std::to_string(b->Id));

        ini.SetValue(
                section,
                "opcode",
                IniFile::Escape(opcode));

        ini.SetValue(
                section,
                "x",
                std::to_string(b->Pos.x));

        ini.SetValue(
                section,
                "y",
                std::to_string(b->Pos.y));

        if (b->Next) {
            ini.SetValue(
                    section,
                    "next",
                    std::to_string(b->Next->Id));
        }

        if (b->BodyOwner) {
            ini.SetValue(
                    section,
                    "body_owner",
                    std::to_string(b->BodyOwner->Id));

            ini.SetValue(
                    section,
                    "body_slot",
                    IniFile::Escape(b->BodySlot));
        }

        if (b->ArgOwner) {
            ini.SetValue(
                    section,
                    "arg_owner",
                    std::to_string(b->ArgOwner->Id));

            ini.SetValue(
                    section,
                    "arg_slot",
                    IniFile::Escape(b->ArgSlot));
        }


        // Block arguments
        if (!b->Def)
            continue;

        for (const BlockSchemaItem &item : b->Def->Schema) {
            if (item.Type != BlockSchemaType::Input && item.Type != BlockSchemaType::Var)
                continue;

            auto argIt = b->Args.find(item.Name);
            if (argIt == b->Args.end())
                continue;

            const VisualArg &arg = argIt->second;

            if (std::holds_alternative<std::unique_ptr<VisualBlock>>(arg))
                continue;

            std::string type;
            std::string value;

            if (const auto *lit = std::get_if<LiteralValue>(&arg)) {
                auto defaultIt = b->Def->DefaultValues.find(item.Name);
                if (defaultIt != b->Def->DefaultValues.end() && *lit == defaultIt->second)
                    continue;

                auto typed = LiteralToTyped(*lit);
                type = typed.first;
                value = typed.second;
            } else if (const auto *var = std::get_if<VariableRef>(&arg)) {
                if (var->Name.empty())
                    continue;

                type = "var";
                value = IniFile::Escape(var->Name);
            } else {
                continue;
            }

            ini.SetValue(section + ".args", item.Name, type + ":" + value);
        }
    }


    // Comments
    for (size_t i = 0; i < canvas.Comments.size(); ++i) {
        const CanvasComment &c = canvas.Comments[i];
        std::string section = "comment" + std::to_string(i);

        ini.SetValue(
                section,
                "id",
                std::to_string(c.Id));

        ini.SetValue(
                section,
                "x",
                std::to_string(c.Pos.x));

        ini.SetValue(
                section,
                "y",
                std::to_string(c.Pos.y));

        ini.SetValue(
                section,
                "w",
                std::to_string(c.Size.x));

        ini.SetValue(
                section,
                "h",
                std::to_string(c.Size.y));

        ini.SetValue(
                section,
                "text",
                IniFile::Escape(c.Text));
    }


    return ini.Save(path);
}

Error LoadProject(
        Canvas &canvas,
        BlockRegistry &registry,
        const std::string &path,
        ProjectSettings &outSettings)
{
    IniFile ini;
    TRY(IniFile::Load(path, ini));

    // Project header
    FAIL_COND_V_MSG(
        !ini.HasSection("project"),
        Error::ProjectMissingSection,
        "Missing [project] section");

    std::string version;
    TRY(GetString(
        ini,
        "project",
        "pvb_version",
        version));

    FAIL_COND_V_MSG(
        version != "1.0",
        Error::ProjectInvalidVersion,
        "Unsupported project version '{}'",
        version);

    std::string name;
    TRY(GetString(ini, "project", "name", name));
    outSettings.Name = IniFile::Unescape(name);

    std::string description;
    if (GetString(ini, "project", "description", description) == Error::Ok)
        outSettings.Description = IniFile::Unescape(description);
    else
        outSettings.Description.clear();

    std::string languageStr;
    if (GetString(ini, "project", "language", languageStr) == Error::Ok)
        TRY(CodeLanguageFromString(languageStr, outSettings.Language));
    else
        outSettings.Language = CodeLanguage::Python;

    u32 blockCount = 0;
    u32 commentCount = 0;
    u32 variableCount = 0;

    TRY(GetU32(
        ini,
        "project",
        "block_count",
        blockCount));

    TRY(GetU32(
        ini,
        "project",
        "comment_count",
        commentCount));

    DISCARD(GetU32(
        ini,
        "project",
        "variable_count",
        variableCount));

    std::string rootsRaw;
    TRY(GetString(
        ini,
        "project",
        "roots",
        rootsRaw));

    std::vector<u32> rootIds;
    TRY(ParseIdList(rootsRaw, rootIds));

    // Remove any variables left over from a previously loaded project
    {
        std::vector<std::string> existingNames;
        existingNames.reserve(registry.Variables.size());
        for (const auto &v : registry.Variables)
            existingNames.push_back(v.Name);

        for (const auto &n : existingNames)
            DISCARD(registry.RemoveVariable(n));
    }

    // Variables
    for (u32 i = 0; i < variableCount; ++i) {
        std::string section = "variable" + std::to_string(i);

        FAIL_COND_V_MSG(
                !ini.HasSection(section),
                Error::ProjectMissingSection,
                "Missing [{}]",
                section);

        std::string rawName, typeStr;
        TRY(GetString(ini, section, "name", rawName));
        TRY(GetString(ini, section, "type", typeStr));
        std::string name2 = IniFile::Unescape(rawName);

        Value type;
        TRY(VariableTypeFromString(typeStr, type));

        if (!registry.HasVariable(name2)) {
            Error err = registry.AddVariable(name2, type);

            FAIL_COND_V_MSG(
                    err != Error::Ok,
                    err,
                    "Failed to recreate variable '{}'",
                    name2);
        }
    }

    // Blocks
    struct PendingBlock
    {
        std::unique_ptr<VisualBlock> Owned;
        VisualBlock *Ptr = nullptr;

        u32 Next = 0;

        u32 BodyOwner = 0;
        std::string BodySlot;

        u32 ArgOwner = 0;
        std::string ArgSlot;
    };

    std::vector<PendingBlock> pending;
    pending.reserve(blockCount);

    std::unordered_map<u32, VisualBlock *> idMap;

    for (u32 i = 0; i < blockCount; ++i) {
        std::string section = "block" + std::to_string(i);

        FAIL_COND_V_MSG(
                !ini.HasSection(section),
                Error::ProjectMissingSection,
                "Missing [{}]",
                section);

        u32 id = 0;
        TRY(GetU32(ini, section, "id", id));

        FAIL_COND_V_MSG(
                idMap.contains(id),
                Error::ProjectInvalidData,
                "Duplicate block id {}",
                id);

        std::string opcode;
        TRY(GetString(ini, section, "opcode", opcode));

        opcode = IniFile::Unescape(opcode);
        const BlockDefinition *def = FindBlockDefinition(registry, opcode);

        FAIL_COND_V_MSG(
                !def,
                Error::BlockInvalidDefinition,
                "Unknown block opcode '{}'",
                opcode);

        auto block = std::make_unique<VisualBlock>();

        block->Id = id;
        block->Def = def;
        TRY(GetFloat(ini, section, "x", block->Pos.x));
        TRY(GetFloat(ini, section, "y", block->Pos.y));

        for (const BlockSchemaItem &item : def->Schema) {
            if (item.Type != BlockSchemaType::Input)
                continue;

            auto defaultIt = def->DefaultValues.find(item.Name);
            if (defaultIt != def->DefaultValues.end())
                block->Args[item.Name] = VisualArg{ defaultIt->second };
        }

        // Literal / variable argument overrides
        std::string argsSection = section + ".args";
        if (const IniSection *argsSec = ini.FindSection(argsSection)) {
            for (const IniValue &kv : argsSec->Values) {
                const BlockSchemaItem *schemaItem = FindSchemaItem(*def, kv.Key);

                FAIL_COND_V_MSG(
                        !schemaItem,
                        Error::ProjectInvalidData,
                        "Unknown argument '{}' for block '{}'",
                        kv.Key,
                        opcode);

                size_t colon = kv.Value.find(':');

                FAIL_COND_V_MSG(
                        colon == std::string::npos,
                        Error::ProjectInvalidData,
                        "Invalid argument '{}' in [{}]",
                        kv.Key,
                        argsSection);

                std::string type = kv.Value.substr(0, colon);
                std::string rawValue = kv.Value.substr(colon + 1);

                bool isVarSlot = schemaItem->Type == BlockSchemaType::Var;
                bool isVarValue = type == "var";

                FAIL_COND_V_MSG(
                        isVarSlot != isVarValue,
                        Error::ProjectInvalidData,
                        "Argument '{}' for block '{}' stores a {} value but the slot expects a {}",
                        kv.Key,
                        opcode,
                        isVarValue ? "variable" : "literal",
                        isVarSlot ? "variable" : "literal");

                TRY(TypedToArg(type, rawValue, registry, block->Args[kv.Key]));
            }
        }


        PendingBlock pb;
        pb.Ptr = block.get();
        pb.Owned = std::move(block);

        // a block with no explicit next/owner
        DISCARD(GetU32(ini, section, "next", pb.Next));
        DISCARD(GetU32(ini, section, "body_owner", pb.BodyOwner));
        DISCARD(GetU32(ini, section, "arg_owner", pb.ArgOwner));

        std::string bodySlotRaw, argSlotRaw;
        DISCARD(GetString(ini, section, "body_slot", bodySlotRaw));
        DISCARD(GetString(ini, section, "arg_slot", argSlotRaw));

        pb.BodySlot = IniFile::Unescape(bodySlotRaw);
        pb.ArgSlot = IniFile::Unescape(argSlotRaw);

        idMap[id] = pb.Ptr;
        pending.push_back(std::move(pb));
    }

    // Resolve references
    std::set<std::pair<VisualBlock *, std::string>> claimedBodySlots;
    std::set<std::pair<VisualBlock *, std::string>> claimedArgSlots;

    for (PendingBlock &pb : pending) {
        VisualBlock *b = pb.Ptr;

        // Next pointer
        if (pb.Next != 0) {
            auto it = idMap.find(pb.Next);

            FAIL_COND_V_MSG(
                    it == idMap.end(),
                    Error::ProjectInvalidData,
                    "Block {} references missing next block {}",
                    b->Id,
                    pb.Next);

            b->Next = it->second;
            it->second->Prev = b;
        }

        // Body ownership
        if (pb.BodyOwner != 0) {
            auto it = idMap.find(pb.BodyOwner);

            FAIL_COND_V_MSG(
                    it == idMap.end(),
                    Error::ProjectInvalidData,
                    "Block {} references missing body owner {}",
                    b->Id,
                    pb.BodyOwner);


            FAIL_COND_V_MSG(
                    pb.BodySlot.empty(),
                    Error::ProjectInvalidData,
                    "Block {} has body owner but no body slot",
                    b->Id);

            FAIL_COND_V_MSG(
                    !FindSchemaItem(*it->second->Def, pb.BodySlot) ||
                    FindSchemaItem(*it->second->Def, pb.BodySlot)->Type != BlockSchemaType::Body,
                    Error::ProjectInvalidData,
                    "Block {} claims body slot '{}' on block {}, which has no such body slot",
                    b->Id,
                    pb.BodySlot,
                    pb.BodyOwner);

            auto claim = std::make_pair(it->second, pb.BodySlot);
            FAIL_COND_V_MSG(
                    claimedBodySlots.contains(claim),
                    Error::ProjectInvalidData,
                    "Body slot '{}' of block {} is claimed by more than one block",
                    pb.BodySlot,
                    pb.BodyOwner);
            claimedBodySlots.insert(claim);

            b->BodyOwner = it->second;
            b->BodySlot = pb.BodySlot;

            it->second->BodyRoots[pb.BodySlot] = b;
        }

        // Argument block ownership
        if (pb.ArgOwner != 0) {
            auto it = idMap.find(pb.ArgOwner);

            FAIL_COND_V_MSG(
                    it == idMap.end(),
                    Error::ProjectInvalidData,
                    "Block {} references missing argument owner {}",
                    b->Id,
                    pb.ArgOwner);


            FAIL_COND_V_MSG(
                    pb.ArgSlot.empty(),
                    Error::ProjectInvalidData,
                    "Block {} has argument owner but no slot",
                    b->Id);

            const BlockSchemaItem *ownerSlot = FindSchemaItem(*it->second->Def, pb.ArgSlot);
            FAIL_COND_V_MSG(
                    !ownerSlot,
                    Error::ProjectInvalidData,
                    "Block {} claims argument slot '{}' on block {}, which has no such slot",
                    b->Id,
                    pb.ArgSlot,
                    pb.ArgOwner);

            auto claim = std::make_pair(it->second, pb.ArgSlot);
            FAIL_COND_V_MSG(
                    claimedArgSlots.contains(claim),
                    Error::ProjectInvalidData,
                    "Argument slot '{}' of block {} is claimed by more than one block",
                    pb.ArgSlot,
                    pb.ArgOwner);
            claimedArgSlots.insert(claim);

            b->ArgOwner = it->second;
            b->ArgSlot = pb.ArgSlot;
        }
    }

    // Move owned argument blocks into their owners
    for (PendingBlock &pb : pending) {
        if (pb.ArgOwner == 0)
            continue;

        FAIL_COND_V_MSG(
                !pb.Owned,
                Error::ProjectInvalidData,
                "Argument block {} has already been moved",
                pb.Ptr->Id);

        VisualBlock *owner = pb.Ptr->ArgOwner;

        FAIL_COND_V_MSG(
                !owner,
                Error::ProjectInvalidData,
                "Argument block {} has no owner",
                pb.Ptr->Id);

        owner->Args[pb.ArgSlot] = VisualArg{ std::move(pb.Owned) };
    }

    // Collect remaining blocks owned by the manager
    std::vector<std::unique_ptr<VisualBlock>> newBlocks;
    newBlocks.reserve(pending.size());

    u32 highestId = 0;

    for (PendingBlock &pb : pending) {
        highestId = std::max(highestId, pb.Ptr->Id);
        if (pb.Owned)
            newBlocks.push_back(std::move(pb.Owned));
    }

    // Restore roots
    std::vector<VisualBlock *> newRoots;
    newRoots.reserve(rootIds.size());

    for (u32 id : rootIds) {
        auto it = idMap.find(id);

        FAIL_COND_V_MSG(
                it == idMap.end(),
                Error::ProjectInvalidData,
                "Root list references missing block {}",
                id);

        newRoots.push_back(it->second);
    }

    // Validate graph integrity
    {
        std::unordered_set<VisualBlock *> reached;

        for (VisualBlock *root : newRoots)
            TRY(ValidateGraph(root, reached));

        FAIL_COND_V_MSG(
                reached.size() != pending.size(),
                Error::ProjectInvalidData,
                "Corrupted project: {} block(s) are not reachable from any root, body, or argument slot",
                pending.size() - reached.size());
    }

    // Comments
    std::vector<CanvasComment> newComments;
    newComments.reserve(commentCount);

    u32 highestCommentId = 0;

    for (u32 i = 0; i < commentCount; ++i) {
        std::string section = "comment" + std::to_string(i);

        FAIL_COND_V_MSG(
                !ini.HasSection(section),
                Error::ProjectMissingSection,
                "Missing [{}]",
                section);

        CanvasComment c;

        TRY(GetU32(ini, section, "id", c.Id));
        TRY(GetFloat(ini, section, "x", c.Pos.x));
        TRY(GetFloat(ini, section, "y", c.Pos.y));
        TRY(GetFloat(ini, section, "w", c.Size.x));
        TRY(GetFloat(ini, section, "h", c.Size.y));

        std::string text;
        TRY(GetString(ini, section, "text", text));

        c.Text = IniFile::Unescape(text);
        highestCommentId = std::max(highestCommentId, c.Id);

        newComments.push_back(std::move(c));
    }

    canvas.Manager.Blocks = std::move(newBlocks);
    canvas.Manager.Roots = std::move(newRoots);
    canvas.Manager.NextId = highestId + 1;
    canvas.Comments = std::move(newComments);
    canvas.NextCommentId = highestCommentId + 1;
    canvas.SelectedId = 0;

    return Error::Ok;
}

static const BlockDefinition *FindBlockDefinition(const BlockRegistry &registry, const std::string &opcode)
{
    for (const BlockDefinition &def : registry.Definitions) {
        if (def.OpCode == opcode)
            return &def;
    }
    return nullptr;
}

static const BlockSchemaItem *FindSchemaItem(const BlockDefinition &def, const std::string &name)
{
    for (const BlockSchemaItem &item : def.Schema) {
        if (item.Name == name &&
                (item.Type == BlockSchemaType::Input ||
                 item.Type == BlockSchemaType::Var ||
                 item.Type == BlockSchemaType::Body))
            return &item;
    }

    return nullptr;
}

static Error VariableTypeToString(Value type, std::string &out)
{
    switch (type) {
        case VAL_INT:    out = "int";    return Error::Ok;
        case VAL_FLOAT:  out = "float";  return Error::Ok;
        case VAL_BOOL:   out = "bool";   return Error::Ok;
        case VAL_STRING: out = "string"; return Error::Ok;
        default:
            GlobalLogger.Error("Cannot save variable with unsupported type");
            return Error::ProjectInvalidData;
    }
}

static Error VariableTypeFromString(const std::string &str, Value &out)
{
    if (str == "int")         { out = VAL_INT;    return Error::Ok; }
    if (str == "float")       { out = VAL_FLOAT;  return Error::Ok; }
    if (str == "bool")        { out = VAL_BOOL;   return Error::Ok; }
    if (str == "string")      { out = VAL_STRING; return Error::Ok; }

    GlobalLogger.Error(
            "Unknown variable type '{}'",
            str);

    return Error::ProjectInvalidData;
}

static Error CodeLanguageToString(CodeLanguage lang, std::string &out)
{
    switch (lang) {
        case CodeLanguage::Cpp:    out = "cpp";    return Error::Ok;
        case CodeLanguage::Python: out = "python"; return Error::Ok;
    }

    GlobalLogger.Error("Cannot save project with unsupported language");
    return Error::ProjectInvalidData;
}

static Error CodeLanguageFromString(const std::string &str, CodeLanguage &out)
{
    if (str == "cpp")    { out = CodeLanguage::Cpp;    return Error::Ok; }
    if (str == "python") { out = CodeLanguage::Python; return Error::Ok; }

    GlobalLogger.Error(
            "Unknown project language '{}'",
            str);

    return Error::ProjectInvalidData;
}

static Error GetString(
    const IniFile &ini,
    const std::string &section,
    const std::string &key,
    std::string &out)
{
    const std::string *value = ini.GetValue(section, key);

    FAIL_COND_V_MSG(
            !value,
            Error::ProjectInvalidData,
            "Missing key '{}' in section [{}]",
            key,
            section);

    out = *value;

    return Error::Ok;
}

static Error GetU32(
    const IniFile &ini,
    const std::string &section,
    const std::string &key,
    u32 &out)
{
    std::string value;
    TRY(GetString(ini, section, key, value));

    try {
        out = static_cast<u32>(std::stoul(value));
    } catch (...) {
        GlobalLogger.Error(
                "Invalid integer '{}' for {}.{}",
                value,
                section,
                key);

        return Error::ProjectInvalidData;
    }

    return Error::Ok;
}

static Error GetFloat(
    const IniFile &ini,
    const std::string &section,
    const std::string &key,
    float &out)
{
    std::string value;
    TRY(GetString(ini, section, key, value));

    try {
        out = std::stof(value);
    } catch (...) {
        GlobalLogger.Error(
                "Invalid float '{}' for {}.{}",
                value,
                section,
                key);

        return Error::ProjectInvalidData;
    }

    return Error::Ok;
}

static Error ParseIdList(
        const std::string &s,
        std::vector<u32> &out)
{
    std::stringstream ss(s);
    std::string token;

    while (std::getline(ss, token, ',')) {
        token = IniFile::Trim(token);

        if (token.empty())
            continue;

        try {
            out.push_back(static_cast<u32>( std::stoul(token)));
        } catch (...) {
            GlobalLogger.Error(
                    "Invalid id '{}'",
                    token);

            return Error::ProjectInvalidData;
        }
    }

    return Error::Ok;
}

static Error TypedToArg(
        const std::string &type,
        const std::string &rawValue,
        const BlockRegistry &registry,
        VisualArg &out)
{
    std::string value = IniFile::Unescape(rawValue);

    if (type == "int") {
        out = VisualArg{
            LiteralValue{ std::in_place_type<int>, std::atoi(value.c_str()) }
        };

        return Error::Ok;
    }

    if (type == "float") {
        out = VisualArg{
            LiteralValue{ std::in_place_type<float>, static_cast<float>( std::atof(value.c_str())) }
        };

        return Error::Ok;
    }


    if (type == "bool") {
        out = VisualArg{
            LiteralValue{ std::in_place_type<bool>, value == "true" || value == "1" }
        };

        return Error::Ok;
    }


    if (type == "var") {
        VariableRef ref{ value };

        for (const VariableInfo &v : registry.Variables) {
            if (v.Name == value) {
                ref.Type = v.Type;
                break;
            }
        }

        out = VisualArg{ std::move(ref) };

        return Error::Ok;
    }


    if (type == "str") {
        out = VisualArg{
            LiteralValue{ std::in_place_type<std::string>, value }
        };

        return Error::Ok;
    }


    GlobalLogger.Error(
            "Unknown argument type '{}'",
            type);

    return Error::ProjectInvalidData;
}

static std::pair<std::string, std::string> LiteralToTyped(const LiteralValue &lit)
{
    return std::visit([](const auto &v) -> std::pair<std::string, std::string>
    {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::string>)
            return { "str", IniFile::Escape(v) };
        else if constexpr (std::is_same_v<T, bool>)
            return { "bool", v ? "true" : "false" };
        else if constexpr (std::is_same_v<T, int>)
            return { "int", std::to_string(v) };
        else
            return { "float", std::to_string(v) };
    }, lit);
}

static void CollectBlocks(VisualBlock *head, std::vector<VisualBlock *> &out, std::unordered_set<VisualBlock *> &seen)
{
    VisualBlock *b = head;
    while (b) {
        if (seen.count(b))
            return;
        seen.insert(b);
        out.push_back(b);

        for (auto &[slot, body] : b->BodyRoots)
            CollectBlocks(body, out, seen);

        for (auto &[key, arg] : b->Args) {
            if (auto *held = std::get_if<std::unique_ptr<VisualBlock>>(&arg)) {
                if (held->get())
                    CollectBlocks(held->get(), out, seen);
            }
        }

        b = b->Next;
    }
}

static Error ValidateGraph(VisualBlock *head, std::unordered_set<VisualBlock *> &seen)
{
    VisualBlock *b = head;
    while (b) {
        FAIL_COND_V_MSG(
                seen.contains(b),
                Error::ProjectInvalidData,
                "Corrupted project: block {} is reachable from more than one place",
                b->Id);

        seen.insert(b);

        for (auto &[slot, body] : b->BodyRoots) {
            DISCARD(slot);
            TRY(ValidateGraph(body, seen));
        }

        for (auto &[key, arg] : b->Args) {
            DISCARD(key);
            if (auto *held = std::get_if<std::unique_ptr<VisualBlock>>(&arg)) {
                if (held->get())
                    TRY(ValidateGraph(held->get(), seen));
            }
        }

        b = b->Next;
    }

    return Error::Ok;
}
