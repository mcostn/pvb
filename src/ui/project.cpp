#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>

#include "ui/project.hpp"
#include "block/registry.hpp"
#include "util/ini.hpp"

static const BlockDefinition *FindBlockDefinition(const BlockRegistry &registry, const std::string &opcode);

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
static void CollectBlocks(VisualBlock *head, std::vector<VisualBlock *> &out, std::unordered_set<VisualBlock *> &seen);

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
    TRY(CodeLanguageToString(settings.Language, languageStr));

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

        ini.SetValue(
                section,
                "next",
                std::to_string(b->Next ? b->Next->Id : 0));

        ini.SetValue(
                section,
                "body_owner",
                std::to_string(b->BodyOwner ? b->BodyOwner->Id : 0));

        ini.SetValue(
                section,
                "body_slot",
                IniFile::Escape(b->BodyOwner ? b->BodySlot : ""));

        ini.SetValue(
                section,
                "arg_owner",
                std::to_string(b->ArgOwner ? b->ArgOwner->Id : 0));

        ini.SetValue(
                section,
                "arg_slot",
                IniFile::Escape(b->ArgOwner ? b->ArgSlot : ""));


        // Block arguments
        for (auto &[key, arg] : b->Args) {
            if (std::holds_alternative<std::unique_ptr<VisualBlock>>(arg))
                continue;

            std::string type;
            std::string value;

            if (const auto *lit = std::get_if<LiteralValue>(&arg)) {
                auto result = LiteralToTyped(*lit);
                type = result.first;
                value = result.second;
            } else if (const auto *var = std::get_if<VariableRef>(&arg)) {
                type = "var";
                value = IniFile::Escape(var->Name);
            } else {
                continue;
            }

            ini.SetValue(section + ".args", key, type + ":" + value);
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
        "Unsupported project version '%s'",
        version.c_str());

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
        DISCARD(CodeLanguageFromString(languageStr, outSettings.Language));
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

    // Variables
    for (u32 i = 0; i < variableCount; ++i) {
        std::string section = "variable" + std::to_string(i);

        FAIL_COND_V_MSG(
                !ini.HasSection(section),
                Error::ProjectMissingSection,
                "Missing [%s]",
                section.c_str());

        std::string name, typeStr;
        TRY(GetString(ini, section, "name", name));
        TRY(GetString(ini, section, "type", typeStr));
        name = IniFile::Unescape(name);

        Value type;
        TRY(VariableTypeFromString(typeStr, type));

        if (!registry.HasVariable(name)) {
            Error err = registry.AddVariable(name, type);

            FAIL_COND_V_MSG(
                    err != Error::Ok,
                    err,
                    "Failed to recreate variable '%s'",
                    name.c_str());
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
                "Missing [%s]",
                section.c_str());

        u32 id = 0;
        TRY(GetU32(ini, section, "id", id));

        std::string opcode;
        TRY(GetString(ini, section, "opcode", opcode));

        opcode = IniFile::Unescape(opcode);
        const BlockDefinition *def = FindBlockDefinition(registry, opcode);

        FAIL_COND_V_MSG(
                !def,
                Error::BlockInvalidDefinition,
                "Unknown block opcode '%s'",
                opcode.c_str());

        auto block = std::make_unique<VisualBlock>();

        block->Id = id;
        block->Def = def;
        TRY(GetFloat(ini, section, "x", block->Pos.x));
        TRY(GetFloat(ini, section, "y", block->Pos.y));

        // Literal / variable arguments
        std::string argsSection = section + ".args";
        if (ini.HasSection(argsSection)) {
            for (auto &[key, raw] : ini.Sections[argsSection]) {
                size_t colon = raw.find(':');

                FAIL_COND_V_MSG(
                        colon == std::string::npos,
                        Error::ProjectInvalidData,
                        "Invalid argument '%s' in [%s]",
                        key.c_str(),
                        argsSection.c_str());

                std::string type = raw.substr(0, colon);
                std::string value = raw.substr(colon + 1);
                TRY(TypedToArg(type, value, registry, block->Args[key]));
            }
        }


        PendingBlock pb;
        pb.Ptr = block.get();
        pb.Owned = std::move(block);

        TRY(GetU32(ini, section, "next", pb.Next));
        TRY(GetU32(ini, section, "body_owner", pb.BodyOwner));
        TRY(GetU32(ini, section, "arg_owner", pb.ArgOwner));

        // These are optional
        DISCARD(GetString(
                    ini,
                    section,
                    "body_slot",
                    pb.BodySlot));
        DISCARD(GetString(
                    ini,
                    section,
                    "arg_slot",
                    pb.ArgSlot));

        pb.BodySlot = IniFile::Unescape(pb.BodySlot);
        pb.ArgSlot = IniFile::Unescape(pb.ArgSlot);

        FAIL_COND_V_MSG(
                idMap.contains(id),
                Error::ProjectInvalidData,
                "Duplicate block id %u",
                id);


        idMap[id] = pb.Ptr;
        pending.push_back(std::move(pb));
    }

    // Resolve references
    for (PendingBlock &pb : pending) {
        VisualBlock *b = pb.Ptr;

        // Next pointer
        if (pb.Next != 0) {
            auto it = idMap.find(pb.Next);

            FAIL_COND_V_MSG(
                    it == idMap.end(),
                    Error::ProjectInvalidData,
                    "Block %u references missing next block %u",
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
                    "Block %u references missing body owner %u",
                    b->Id,
                    pb.BodyOwner);


            FAIL_COND_V_MSG(
                    pb.BodySlot.empty(),
                    Error::ProjectInvalidData,
                    "Block %u has body owner but no body slot",
                    b->Id);

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
                    "Block %u references missing argument owner %u",
                    b->Id,
                    pb.ArgOwner);


            FAIL_COND_V_MSG(
                    pb.ArgSlot.empty(),
                    Error::ProjectInvalidData,
                    "Block %u has argument owner but no slot",
                    b->Id);


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
                "Argument block %u has already been moved",
                pb.Ptr->Id);

        VisualBlock *owner = pb.Ptr->ArgOwner;

        FAIL_COND_V_MSG(
                !owner,
                Error::ProjectInvalidData,
                "Argument block %u has no owner",
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
                "Root list references missing block %u",
                id);

        newRoots.push_back(it->second);
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
                "Missing [%s]",
                section.c_str());

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
            str.c_str());

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
            str.c_str());

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
            "Missing key '%s' in section [%s]",
            key.c_str(),
            section.c_str());

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
                value.c_str(),
                section.c_str(),
                key.c_str());

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
                value.c_str(),
                section.c_str(),
                key.c_str());

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
                    token.c_str());

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
            type.c_str());

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
