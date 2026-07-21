#include "util/ini.hpp"

#include <fstream>
#include <sstream>

#include "util/macro.hpp"

bool IniSection::HasValue(const std::string &Key) const
{
    return GetValue(Key) != nullptr;
}

const std::string *IniSection::GetValue(const std::string &Key) const
{
    for (const IniValue &V : Values) {
        if (V.Key == Key)
            return &V.Value;
    }

    return nullptr;
}

void IniSection::SetValue(const std::string &Key, const std::string &Value)
{
    for (IniValue &V : Values) {
        if (V.Key == Key) {
            V.Value = Value;
            return;
        }
    }

    Values.push_back(IniValue{ Key, Value });
}

Error IniFile::Load(const std::string &Path, IniFile &Out)
{
    std::ifstream File(Path);

    FAIL_COND_V_MSG(!File.is_open(),
        Error::FileNotFound,
        "Failed to open '{}'",
        Path);

    std::stringstream Buffer;
    Buffer << File.rdbuf();

    return Parse(Buffer.str(), Out);
}

Error IniFile::Parse(const std::string &Text, IniFile &Out)
{
    Out.Sections.clear();

    std::stringstream Stream(Text);

    std::string Line;
    IniSection *CurrentSection = nullptr;

    while (std::getline(Stream, Line)) {
        Line = Trim(Line);

        if (Line.empty())
            continue;

        if (Line[0] == ';' || Line[0] == '#')
            continue;

        if (Line.front() == '[' && Line.back() == ']') {
            std::string Name = Line.substr(1, Line.size() - 2);
            CurrentSection = &Out.GetOrCreateSection(Name);
            continue;
        }

        size_t Equal = Line.find('=');

        FAIL_COND_V_MSG(Equal == std::string::npos,
            Error::IniParseError,
            "Malformed INI line '{}'",
            Line);

        FAIL_COND_V_MSG(!CurrentSection,
            Error::IniParseError,
            "Key/value pair '{}' appears before any [section]",
            Line);

        std::string Key = Trim(Line.substr(0, Equal));
        std::string Value = Unescape(Line.substr(Equal + 1));

        CurrentSection->SetValue(Key, Value);
    }

    return Error::Ok;
}

Error IniFile::Save(const std::string &Path) const
{
    std::ofstream File(Path, std::ios::trunc);

    FAIL_COND_V_MSG(!File.is_open(),
        Error::Failed,
        "Failed to write '{}'",
        Path);

    File << Serialize();

    FAIL_COND_V_MSG(!File.good(),
        Error::Failed,
        "Failed while writing '{}'",
        Path);

    return Error::Ok;
}

std::string IniFile::Serialize() const
{
    std::string Result;

    for (const IniSection &Section : Sections) {
        Result += "[";
        Result += Section.Name;
        Result += "]\n";

        for (const IniValue &Value : Section.Values) {
            Result += Value.Key;
            Result += "=";
            Result += Escape(Value.Value);
            Result += "\n";
        }

        Result += "\n";
    }

    return Result;
}

bool IniFile::HasSection(const std::string &Name) const
{
    return FindSection(Name) != nullptr;
}

IniSection *IniFile::FindSection(const std::string &Name)
{
    for (IniSection &Section : Sections) {
        if (Section.Name == Name)
            return &Section;
    }

    return nullptr;
}

const IniSection *IniFile::FindSection(const std::string &Name) const
{
    for (const IniSection &Section : Sections) {
        if (Section.Name == Name)
            return &Section;
    }

    return nullptr;
}

IniSection &IniFile::GetOrCreateSection(const std::string &Name)
{
    if (IniSection *Existing = FindSection(Name))
        return *Existing;

    Sections.push_back(IniSection{ Name, {} });
    return Sections.back();
}

bool IniFile::HasValue(const std::string &SectionName,
                       const std::string &Key) const
{
    const IniSection *Section = FindSection(SectionName);
    return Section && Section->HasValue(Key);
}

const std::string *IniFile::GetValue(const std::string &SectionName,
                                     const std::string &Key) const
{
    const IniSection *Section = FindSection(SectionName);
    if (!Section)
        return nullptr;

    return Section->GetValue(Key);
}

void IniFile::SetValue(const std::string &SectionName,
                       const std::string &Key,
                       const std::string &Value)
{
    GetOrCreateSection(SectionName).SetValue(Key, Value);
}

std::string IniFile::Trim(const std::string &Text)
{
    size_t Start = Text.find_first_not_of(" \t\r\n");
    if (Start == std::string::npos)
        return "";

    size_t End = Text.find_last_not_of(" \t\r\n");

    return Text.substr(Start, End - Start + 1);
}

std::string IniFile::Escape(const std::string &Text)
{
    std::string Result;
    Result.reserve(Text.size());

    for (char C : Text) {
        switch (C) {
            case '\\':
                Result += "\\\\";
                break;

            case '\n':
                Result += "\\n";
                break;

            case '\r':
                Result += "\\r";
                break;

            default:
                Result += C;
                break;
        }
    }

    return Result;
}

std::string IniFile::Unescape(const std::string &Text)
{
    std::string Result;
    Result.reserve(Text.size());

    for (size_t I = 0; I < Text.size(); ++I) {
        if (Text[I] == '\\' && I + 1 < Text.size()) {
            char Next = Text[I + 1];

            switch (Next) {
                case '\\':
                    Result += '\\';
                    ++I;
                    continue;

                case 'n':
                    Result += '\n';
                    ++I;
                    continue;

                case 'r':
                    Result += '\r';
                    ++I;
                    continue;
            }
        }

        Result += Text[I];
    }

    return Result;
}
