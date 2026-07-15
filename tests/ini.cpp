#include "test_runner.hpp"
#include "util/ini.hpp"

TEST(IniParseEmpty)
{
    IniFile Ini;

    EXPECT(IniFile::Parse("", Ini) == Error::Ok);
    EXPECT(Ini.Sections.empty());
}

TEST(IniParseSingleSection)
{
    IniFile Ini;

    EXPECT(IniFile::Parse("[Project]\n", Ini) == Error::Ok);
    EXPECT(Ini.HasSection("Project"));
}

TEST(IniParseSingleValue)
{
    IniFile Ini;

    EXPECT(
        IniFile::Parse(
            "[Project]\n"
            "Name=Example\n",
            Ini) == Error::Ok);
    EXPECT(Ini.HasValue("Project", "Name"));

    const std::string *Value = Ini.GetValue("Project", "Name");
    EXPECT(Value != nullptr);

    if (Value)
        EXPECT_EQ(*Value, "Example");
}

TEST(IniParseMultipleSections)
{
    IniFile Ini;

    EXPECT(
        IniFile::Parse(
            "[A]\n"
            "One=1\n"
            "\n"
            "[B]\n"
            "Two=2\n",
            Ini) == Error::Ok);

    EXPECT(Ini.HasSection("A"));
    EXPECT(Ini.HasSection("B"));

    EXPECT_EQ(*Ini.GetValue("A", "One"), "1");
    EXPECT_EQ(*Ini.GetValue("B", "Two"), "2");
}

TEST(IniParseComments)
{
    IniFile Ini;

    EXPECT(
        IniFile::Parse(
            "; comment\n"
            "# another\n"
            "[Section]\n"
            "Key=Value\n",
            Ini) == Error::Ok);
    EXPECT_EQ(*Ini.GetValue("Section", "Key"), "Value");
}

TEST(IniTrimWhitespace)
{
    IniFile Ini;

    EXPECT(
        IniFile::Parse(
            "   [Section]   \n"
            "  Key   =Value\n",
            Ini) == Error::Ok);
    EXPECT_EQ(*Ini.GetValue("Section", "Key"), "Value");
}

TEST(IniMissingValue)
{
    IniFile Ini;

    EXPECT(IniFile::Parse("[Section]\n", Ini) == Error::Ok);
    EXPECT(Ini.GetValue("Section", "Missing") == nullptr);
}

TEST(IniMissingSection)
{
    IniFile Ini;

    EXPECT(IniFile::Parse("[Section]\n", Ini) == Error::Ok);
    EXPECT(Ini.GetValue("Missing", "Key") == nullptr);
}

TEST(IniEscapeNewline)
{
    IniFile Ini;

    EXPECT(
        IniFile::Parse(
            "[Section]\n"
            "Text=Hello\\nWorld\n",
            Ini) == Error::Ok);
    EXPECT_EQ(*Ini.GetValue("Section", "Text"), "Hello\nWorld");
}

TEST(IniEscapeBackslash)
{
    IniFile Ini;

    EXPECT(
        IniFile::Parse(
            "[Section]\n"
            "Path=C:\\\\Folder\n",
            Ini) == Error::Ok);
    EXPECT_EQ(*Ini.GetValue("Section", "Path"), "C:\\Folder");
}

TEST(IniMalformedLine)
{
    IniFile Ini;

    EXPECT(
        IniFile::Parse(
            "[Section]\n"
            "ThisIsNotValid\n",
            Ini) == Error::IniParseError);
}

TEST(IniSerializeEmpty)
{
    IniFile Ini;

    EXPECT_EQ(Ini.Serialize(), "");
}

TEST(IniSerializeSingleSection)
{
    IniFile Ini;

    Ini.SetValue("Project", "Name", "Example");

    EXPECT_EQ(
        Ini.Serialize(),
        "[Project]\n"
        "Name=Example\n"
        "\n");
}

TEST(IniSerializeMultipleValues)
{
    IniFile Ini;

    Ini.SetValue("Project", "Name", "Example");
    Ini.SetValue("Project", "Version", "1.0");

    std::string Text = Ini.Serialize();

    EXPECT(Text.find("[Project]") != std::string::npos);
    EXPECT(Text.find("Name=Example") != std::string::npos);
    EXPECT(Text.find("Version=1.0") != std::string::npos);
}

TEST(IniSerializeMultipleSections)
{
    IniFile Ini;

    Ini.SetValue("Project", "Name", "Example");
    Ini.SetValue("Block", "Opcode", "print");

    std::string Text = Ini.Serialize();

    EXPECT(Text.find("[Project]") != std::string::npos);
    EXPECT(Text.find("Name=Example") != std::string::npos);

    EXPECT(Text.find("[Block]") != std::string::npos);
    EXPECT(Text.find("Opcode=print") != std::string::npos);
}

TEST(IniSerializeEscapeNewline)
{
    IniFile Ini;

    Ini.SetValue("Section", "Text", "Hello\nWorld");

    std::string Text = Ini.Serialize();

    EXPECT(Text.find("Text=Hello\\nWorld") != std::string::npos);
}

TEST(IniSerializeEscapeBackslash)
{
    IniFile Ini;

    Ini.SetValue("Section", "Path", "C:\\Folder");

    std::string Text = Ini.Serialize();

    EXPECT(Text.find("Path=C:\\\\Folder") != std::string::npos);
}

TEST(IniSerializeEscapeCarriageReturn)
{
    IniFile Ini;

    Ini.SetValue("Section", "Text", "A\rB");

    std::string Text = Ini.Serialize();

    EXPECT(Text.find("Text=A\\rB") != std::string::npos);
}

TEST(IniRealWorld)
{
    IniFile Original;

    Original.SetValue("Project", "Name", "My Project");
    Original.SetValue("Project", "Version", "1.0");

    Original.SetValue("Block0", "Opcode", "print");
    Original.SetValue("Block0", "X", "150");
    Original.SetValue("Block0", "Y", "200");

    Original.SetValue("Block1", "Opcode", "repeat");

    std::string Serialized = Original.Serialize();

    IniFile Loaded;

    EXPECT(IniFile::Parse(Serialized, Loaded) == Error::Ok);

    EXPECT_EQ(*Loaded.GetValue("Project", "Name"), "My Project");
    EXPECT_EQ(*Loaded.GetValue("Project", "Version"), "1.0");

    EXPECT_EQ(*Loaded.GetValue("Block0", "Opcode"), "print");
    EXPECT_EQ(*Loaded.GetValue("Block0", "X"), "150");
    EXPECT_EQ(*Loaded.GetValue("Block0", "Y"), "200");

    EXPECT_EQ(*Loaded.GetValue("Block1", "Opcode"), "repeat");
}
