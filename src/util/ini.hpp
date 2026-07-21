#pragma once

#include <string>
#include <vector>

#include "util/error.hpp"

struct IniValue
{
    std::string Key;
    std::string Value;
};

struct IniSection
{
    std::string Name;
    std::vector<IniValue> Values;

    bool HasValue(const std::string &Key) const;
    const std::string *GetValue(const std::string &Key) const;
    void SetValue(const std::string &Key, const std::string &Value);
};

class IniFile
{
public:
    std::vector<IniSection> Sections;

    static Error Load(const std::string &Path, IniFile &Out);
    static Error Parse(const std::string &Text, IniFile &Out);

    Error Save(const std::string &Path) const;
    std::string Serialize() const;

    bool HasSection(const std::string &Name) const;

    IniSection *FindSection(const std::string &Name);
    const IniSection *FindSection(const std::string &Name) const;

    IniSection &GetOrCreateSection(const std::string &Name);

    bool HasValue(const std::string &Section, const std::string &Key) const;

    const std::string *GetValue(
            const std::string &Section,
            const std::string &Key) const;

    void SetValue(
            const std::string &Section,
            const std::string &Key,
            const std::string &Value);

    static std::string Trim(const std::string &Text);
    static std::string Escape(const std::string &Text);
    static std::string Unescape(const std::string &Text);
};
