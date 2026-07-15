#include <fstream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <algorithm>

#include "ui/project_ini.hpp"
#include "block/registry.hpp"

static const BlockRegistry &CachedRegistry()
{
    static BlockRegistry registry = GetBlockRegistry();
    return registry;
}

static const BlockDefinition *FindBlockDefinition(const std::string &opcode)
{
    for (const BlockDefinition &def : CachedRegistry().Definitions) {
        if (def.OpCode == opcode)
            return &def;
    }
    return nullptr;
}

namespace
{

std::string EscapeIni(const std::string &s)
{
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            default:   out += c;      break;
        }
    }
    return out;
}

std::string UnescapeIni(const std::string &s)
{
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            char n = s[i + 1];
            if (n == 'n')       { out += '\n'; ++i; continue; }
            if (n == 'r')       { out += '\r'; ++i; continue; }
            if (n == '\\')      { out += '\\'; ++i; continue; }
        }
        out += s[i];
    }
    return out;
}

std::string Trim(const std::string &s)
{
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos)
        return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

// section -> (key -> value)
using IniDoc = std::unordered_map<std::string, std::unordered_map<std::string, std::string>>;

IniDoc ParseIni(const std::string &text)
{
    IniDoc doc;
    std::string currentSection;

    std::istringstream stream(text);
    std::string line;

    while (std::getline(stream, line)) {
        std::string trimmed = Trim(line);

        if (trimmed.empty() || trimmed[0] == ';' || trimmed[0] == '#')
            continue;

        if (trimmed.front() == '[' && trimmed.back() == ']') {
            currentSection = trimmed.substr(1, trimmed.size() - 2);
            doc.emplace(currentSection, std::unordered_map<std::string, std::string>{});
            continue;
        }

        size_t eq = trimmed.find('=');
        if (eq == std::string::npos)
            continue;

        std::string key = Trim(trimmed.substr(0, eq));
        std::string value = trimmed.substr(eq + 1); // keep raw; unescape on use

        doc[currentSection][key] = value;
    }

    return doc;
}

const std::string &IniGet(const IniDoc &doc, const std::string &section, const std::string &key,
        const std::string &fallback)
{
    auto sec = doc.find(section);
    if (sec == doc.end())
        return fallback;

    auto it = sec->second.find(key);
    if (it == sec->second.end())
        return fallback;

    return it->second;
}

bool IniHasSection(const IniDoc &doc, const std::string &section)
{
    return doc.find(section) != doc.end();
}

u32 ToU32(const std::string &s, u32 fallback = 0)
{
    if (s.empty())
        return fallback;
    try { return static_cast<u32>(std::stoul(s)); }
    catch (...) { return fallback; }
}

float ToFloat(const std::string &s, float fallback = 0.0f)
{
    if (s.empty())
        return fallback;
    try { return std::stof(s); }
    catch (...) { return fallback; }
}

std::vector<u32> ParseIdList(const std::string &s)
{
    std::vector<u32> ids;
    std::stringstream ss(s);
    std::string token;
    while (std::getline(ss, token, ',')) {
        token = Trim(token);
        if (!token.empty())
            ids.push_back(ToU32(token));
    }
    return ids;
}

std::pair<std::string, std::string> LiteralToTyped(const LiteralValue &lit)
{
    return std::visit([](const auto &v) -> std::pair<std::string, std::string>
    {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::string>)
            return { "str", EscapeIni(v) };
        else if constexpr (std::is_same_v<T, bool>)
            return { "bool", v ? "true" : "false" };
        else if constexpr (std::is_same_v<T, int>)
            return { "int", std::to_string(v) };
        else
            return { "float", std::to_string(v) };
    }, lit);
}

VisualArg TypedToArg(const std::string &type, const std::string &rawValue)
{
    std::string value = UnescapeIni(rawValue);

    if (type == "int")
        return VisualArg{ LiteralValue{ std::in_place_type<int>, std::atoi(value.c_str()) } };
    if (type == "float")
        return VisualArg{ LiteralValue{ std::in_place_type<float>, static_cast<float>(std::atof(value.c_str())) } };
    if (type == "bool")
        return VisualArg{ LiteralValue{ std::in_place_type<bool>, (value == "true" || value == "1") } };
    if (type == "var")
        return VisualArg{ VariableRef{ value } };

    return VisualArg{ LiteralValue{ std::in_place_type<std::string>, value } };
}

void CollectBlocks(VisualBlock *head, std::vector<VisualBlock *> &out, std::unordered_set<VisualBlock *> &seen)
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

} // namespace

bool SaveProject(const Canvas &canvas, const std::string &path, const std::string &projectName)
{
    std::vector<VisualBlock *> blocks;
    std::unordered_set<VisualBlock *> seen;

    for (VisualBlock *root : canvas.Manager.Roots)
        CollectBlocks(root, blocks, seen);

    std::unordered_map<VisualBlock *, size_t> indexOf;
    for (size_t i = 0; i < blocks.size(); ++i)
        indexOf[blocks[i]] = i;

    std::ostringstream out;
    out << "[project]\n";
    out << "name=" << EscapeIni(projectName) << "\n";
    out << "pvb_version=1.0\n";
    out << "block_count=" << blocks.size() << "\n";
    out << "comment_count=" << canvas.Comments.size() << "\n";
    out << "root_count=" << canvas.Manager.Roots.size() << "\n";

    out << "roots=";
    for (size_t i = 0; i < canvas.Manager.Roots.size(); ++i) {
        if (i > 0) out << ",";
        out << canvas.Manager.Roots[i]->Id;
    }
    out << "\n\n";

    for (size_t i = 0; i < blocks.size(); ++i) {
        VisualBlock *b = blocks[i];

        std::string opcode = b->Def ? b->Def->OpCode : std::string{};

        out << "[block" << i << "]\n";
        out << "id=" << b->Id << "\n";
        out << "opcode=" << EscapeIni(opcode) << "\n";
        out << "x=" << b->Pos.x << "\n";
        out << "y=" << b->Pos.y << "\n";
        out << "next=" << (b->Next ? b->Next->Id : 0) << "\n";
        out << "body_owner=" << (b->BodyOwner ? b->BodyOwner->Id : 0) << "\n";
        out << "body_slot=" << EscapeIni(b->BodyOwner ? b->BodySlot : "") << "\n";
        out << "arg_owner=" << (b->ArgOwner ? b->ArgOwner->Id : 0) << "\n";
        out << "arg_slot=" << EscapeIni(b->ArgOwner ? b->ArgSlot : "") << "\n";
        out << "\n";

        bool hasArgs = false;
        for (auto &[key, arg] : b->Args) {
            if (!std::holds_alternative<std::unique_ptr<VisualBlock>>(arg)) {
                hasArgs = true;
                break;
            }
        }

        if (hasArgs) {
            out << "[block" << i << ".args]\n";
            for (auto &[key, arg] : b->Args) {
                if (std::holds_alternative<std::unique_ptr<VisualBlock>>(arg))
                    continue;

                if (const auto *lit = std::get_if<LiteralValue>(&arg)) {
                    auto [type, value] = LiteralToTyped(*lit);
                    out << key << "=" << type << ":" << value << "\n";
                } else if (const auto *var = std::get_if<VariableRef>(&arg)) {
                    out << key << "=var:" << EscapeIni(var->Name) << "\n";
                }
            }
            out << "\n";
        }
    }

    for (size_t i = 0; i < canvas.Comments.size(); ++i) {
        const CanvasComment &c = canvas.Comments[i];
        out << "[comment" << i << "]\n";
        out << "id=" << c.Id << "\n";
        out << "x=" << c.Pos.x << "\n";
        out << "y=" << c.Pos.y << "\n";
        out << "w=" << c.Size.x << "\n";
        out << "h=" << c.Size.y << "\n";
        out << "text=" << EscapeIni(c.Text) << "\n";
        out << "\n";
    }

    std::ofstream file(path, std::ios::out | std::ios::trunc);
    if (!file.is_open())
        return false;

    file << out.str();
    return file.good();
}

ProjectLoadResult LoadProject(Canvas &canvas, const std::string &path)
{
    ProjectLoadResult result;

    std::ifstream file(path, std::ios::in);
    if (!file.is_open()) {
        result.Error = "Could not open file: " + path;
        return result;
    }

    std::ostringstream buf;
    buf << file.rdbuf();
    IniDoc doc = ParseIni(buf.str());

    if (!IniHasSection(doc, "project")) {
        result.Error = "Missing [project] section";
        return result;
    }

    std::string version = IniGet(doc, "project", "pvb_version", "");
    if (version.empty()) {
        result.Error = "Missing pvb_version";
        return result;
    }

    size_t blockCount = ToU32(IniGet(doc, "project", "block_count", "0"));
    size_t commentCount = ToU32(IniGet(doc, "project", "comment_count", "0"));
    std::vector<u32> rootIds = ParseIdList(IniGet(doc, "project", "roots", ""));

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

    for (size_t i = 0; i < blockCount; ++i) {
        std::string section = "block" + std::to_string(i);

        if (!IniHasSection(doc, section)) {
            result.Error = "Missing section [" + section + "]";
            return result;
        }

        u32 id = ToU32(IniGet(doc, section, "id", "0"));
        std::string opcode = UnescapeIni(IniGet(doc, section, "opcode", ""));

        const BlockDefinition *def = FindBlockDefinition(opcode);
        if (!def) {
            result.Error = "Unknown opcode '" + opcode + "' in [" + section + "]";
            return result;
        }

        auto block = std::make_unique<VisualBlock>();
        block->Id = id;
        block->Def = def;
        block->Pos = ImVec2(
                ToFloat(IniGet(doc, section, "x", "0")),
                ToFloat(IniGet(doc, section, "y", "0")));

        std::string argsSection = section + ".args";
        if (IniHasSection(doc, argsSection)) {
            for (auto &[key, rawValue] : doc[argsSection]) {
                size_t colon = rawValue.find(':');
                if (colon == std::string::npos)
                    continue;

                std::string type = rawValue.substr(0, colon);
                std::string value = rawValue.substr(colon + 1);
                block->Args[key] = TypedToArg(type, value);
            }
        }

        PendingBlock pb;
        pb.BodyOwner = ToU32(IniGet(doc, section, "body_owner", "0"));
        pb.BodySlot  = UnescapeIni(IniGet(doc, section, "body_slot", ""));
        pb.ArgOwner  = ToU32(IniGet(doc, section, "arg_owner", "0"));
        pb.ArgSlot   = UnescapeIni(IniGet(doc, section, "arg_slot", ""));
        pb.Next      = ToU32(IniGet(doc, section, "next", "0"));
        pb.Ptr = block.get();
        pb.Owned = std::move(block);

        idMap[id] = pb.Ptr;
        pending.push_back(std::move(pb));
    }

    for (PendingBlock &pb : pending) {
        VisualBlock *b = pb.Ptr;

        if (pb.Next != 0) {
            auto it = idMap.find(pb.Next);
            if (it == idMap.end()) {
                result.Error = "Block " + std::to_string(b->Id) + " references unknown next id " + std::to_string(pb.Next);
                return result;
            }
            b->Next = it->second;
            it->second->Prev = b;
        }

        if (pb.BodyOwner != 0) {
            auto it = idMap.find(pb.BodyOwner);
            if (it == idMap.end()) {
                result.Error = "Block " + std::to_string(b->Id) + " references unknown body_owner id " + std::to_string(pb.BodyOwner);
                return result;
            }
            b->BodyOwner = it->second;
            b->BodySlot = pb.BodySlot;
            it->second->BodyRoots[pb.BodySlot] = b;
        }

        if (pb.ArgOwner != 0) {
            auto it = idMap.find(pb.ArgOwner);
            if (it == idMap.end()) {
                result.Error = "Block " + std::to_string(b->Id) + " references unknown arg_owner id " + std::to_string(pb.ArgOwner);
                return result;
            }
            b->ArgOwner = it->second;
            b->ArgSlot = pb.ArgSlot;
        }
    }

    for (PendingBlock &pb : pending) {
        if (pb.ArgOwner == 0 || !pb.Owned)
            continue;

        VisualBlock *owner = pb.Ptr->ArgOwner;
        owner->Args[pb.ArgSlot] = VisualArg{ std::move(pb.Owned) };
    }

    u32 highestId = 0;
    std::vector<std::unique_ptr<VisualBlock>> newBlocks;
    newBlocks.reserve(pending.size());

    for (PendingBlock &pb : pending) {
        highestId = std::max(highestId, pb.Ptr->Id);
        if (pb.Owned)
            newBlocks.push_back(std::move(pb.Owned));
    }

    std::vector<VisualBlock *> newRoots;
    newRoots.reserve(rootIds.size());
    for (u32 id : rootIds) {
        auto it = idMap.find(id);
        if (it == idMap.end()) {
            result.Error = "roots list references unknown id " + std::to_string(id);
            return result;
        }
        newRoots.push_back(it->second);
    }

    std::vector<CanvasComment> newComments;
    newComments.reserve(commentCount);
    u32 highestCommentId = 0;

    for (size_t i = 0; i < commentCount; ++i) {
        std::string section = "comment" + std::to_string(i);
        if (!IniHasSection(doc, section)) {
            result.Error = "Missing section [" + section + "]";
            return result;
        }

        CanvasComment c;
        c.Id = ToU32(IniGet(doc, section, "id", "0"));
        c.Pos = ImVec2(
                ToFloat(IniGet(doc, section, "x", "0")),
                ToFloat(IniGet(doc, section, "y", "0")));
        c.Size = ImVec2(
                ToFloat(IniGet(doc, section, "w", "0")),
                ToFloat(IniGet(doc, section, "h", "0")));
        c.Text = UnescapeIni(IniGet(doc, section, "text", ""));

        highestCommentId = std::max(highestCommentId, c.Id);
        newComments.push_back(std::move(c));
    }

    canvas.Manager.Blocks = std::move(newBlocks);
    canvas.Manager.Roots = std::move(newRoots);
    canvas.Manager.NextId = highestId + 1;

    canvas.Comments = std::move(newComments);
    canvas.NextCommentId = highestCommentId + 1;

    canvas.SelectedId = 0;

    result.Success = true;
    return result;
}
