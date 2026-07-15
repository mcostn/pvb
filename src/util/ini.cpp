#include "util/ini.hpp"

#include <fstream>

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
    std::string CurrentSection;

    while (std::getline(Stream, Line)) {
        Line = Trim(Line);

        if (Line.empty())
            continue;

        if (Line[0] == ';' || Line[0] == '#')
            continue;

        if (Line.front() == '[' && Line.back() == ']') {
            CurrentSection = Line.substr(1, Line.size() - 2);
            Out.Sections.emplace(CurrentSection, Section{});
            continue;
        }

        size_t Equal = Line.find('=');

        FAIL_COND_V_MSG(Equal == std::string::npos,
            Error::IniParseError,
            "Malformed INI line '{}'",
            Line);

        std::string Key = Trim(Line.substr(0, Equal));
        std::string Value = Unescape(Line.substr(Equal + 1));

        Out.Sections[CurrentSection][Key] = Value;
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

    for (const auto &SectionPair : Sections) {
        Result += "[";
        Result += SectionPair.first;
        Result += "]\n";

        for (const auto &ValuePair : SectionPair.second) {
            Result += ValuePair.first;
            Result += "=";
            Result += Escape(ValuePair.second);
            Result += "\n";
        }

        Result += "\n";
    }

    return Result;
}

bool IniFile::HasSection(const std::string &Name) const
{
    return Sections.find(Name) != Sections.end();
}

bool IniFile::HasValue(const std::string &SectionName,
                       const std::string &Key) const
{
    auto SectionIt = Sections.find(SectionName);
    if (SectionIt == Sections.end())
        return false;

    return SectionIt->second.find(Key) != SectionIt->second.end();
}

const std::string *IniFile::GetValue(const std::string &SectionName,
                                     const std::string &Key) const
{
    auto SectionIt = Sections.find(SectionName);
    if (SectionIt == Sections.end())
        return nullptr;

    auto ValueIt = SectionIt->second.find(Key);
    if (ValueIt == SectionIt->second.end())
        return nullptr;

    return &ValueIt->second;
}

void IniFile::SetValue(const std::string &SectionName,
                       const std::string &Key,
                       const std::string &Value)
{
    Sections[SectionName][Key] = Value;
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
