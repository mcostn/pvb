#pragma once

#include <unordered_map>
#include <string>

#include "util/error.hpp"

class IniFile
{
public:
    using Section = std::unordered_map<std::string, std::string>;
    using SectionMap = std::unordered_map<std::string, Section>;

    SectionMap Sections;

    static Error Load(const std::string &Path, IniFile &Out);
    static Error Parse(const std::string &Text, IniFile &Out);

    Error Save(const std::string &Path) const;
    std::string Serialize() const;

    bool HasSection(const std::string &Name) const;
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

