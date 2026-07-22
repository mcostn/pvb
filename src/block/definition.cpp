#include "block/definition.hpp"
#include "util/macro.hpp"

static Error ParseLiteral(Value type, std::string_view text, LiteralValue &out);

Error GenerateBlockSchema(BlockDefinition &def)
{
    if (def.Fmt.empty()) {
        GlobalLogger.Error("Block format is empty");
        return Error::BlockInvalidFmt;
    }

    BlockSchema &schema = def.Schema;
    const std::string &str = def.Fmt;
    const char *ch = str.data();
    const char *end = ch + str.size();

    enum class ParseState { Type, Name, Val };

    while (ch != end) {
        if (*ch == '{') {
            ParseState state = ParseState::Type;

            bool hasVariable = false;
            std::string typeStr;
            std::string nameStr;
            std::string valStr;

            ch++;
            while (ch != end && *ch != '}') {
                switch (*ch) {
                    case ':': {
                                  FAIL_COND_V_MSG(
                                          state != ParseState::Type,
                                          Error::BlockInvalidFmt,
                                          "Invalid block format: unexpected ':'");

                                  ch++;
                                  if (ch != end && *ch == '$') {
                                      hasVariable = true;
                                      ch ++;
                                  }
                                  state = ParseState::Name;

                                  continue;
                              }
                    case '=': {
                                  FAIL_COND_V_MSG(
                                          state != ParseState::Name || hasVariable,
                                          Error::BlockInvalidFmt,
                                          "Invalid block format: unexpected '='");
                                  state = ParseState::Val;
                                  ch++;
                                  continue;
                              }
                    default: break;
                }

                switch (state) {
                    case ParseState::Type:
                        typeStr += *ch;
                        break;

                    case ParseState::Name:
                        nameStr += *ch;
                        break;

                    case ParseState::Val:
                        valStr += *ch;
                        break;
                }

                ch ++;
            }

            FAIL_COND_V_MSG(
                    ch == end,
                    Error::BlockInvalidFmt,
                    "Invalid block format: '{}' doesn't have a closing '}}'",
                    def.Fmt);
            FAIL_COND_V_MSG(
                    typeStr.empty(),
                    Error::BlockInvalidFmt,
                    "Invalid block format: no type specified");
            FAIL_COND_V_MSG(
                    nameStr.empty(),
                    Error::BlockInvalidFmt,
                    "Invalid block format: no name specified");

            BlockSchemaType schemaType;
            Value valueType = VAL_NONE;

            if (typeStr == "body") {
                 FAIL_COND_V_MSG(
                        hasVariable,
                        Error::BlockInvalidFmt,
                        "Invalid block format: body slot '{}' cannot be a variable",
                        nameStr);
                FAIL_COND_V_MSG(
                        !valStr.empty(),
                        Error::BlockInvalidFmt,
                        "Invalid block format: body slot '{}' cannot have a default value",
                        nameStr);

                schemaType = BlockSchemaType::Body;
            } else {
                if (typeStr == "int")         valueType = VAL_INT;
                else if (typeStr == "float")  valueType = VAL_FLOAT;
                else if (typeStr == "bool")   valueType = VAL_BOOL;
                else if (typeStr == "string") valueType = VAL_STRING;
                else if (typeStr == "number") valueType = VAL_NUMBER;
                else if (typeStr == "any")    valueType = VAL_ANY;
                FAIL_COND_V_MSG(
                        valueType == VAL_NONE,
                        Error::BlockInvalidFmt,
                        "Invalid block format: unknown type '{}'",
                        typeStr);

                schemaType = hasVariable ? BlockSchemaType::Var : BlockSchemaType::Input;
            }

            schema.emplace_back(nameStr, valueType, schemaType);
            if (!valStr.empty()) {
                LiteralValue value;
                TRY(ParseLiteral(valueType, valStr, value));
                def.DefaultValues.emplace(nameStr, std::move(value));
            }
        } else if (*ch == '\n') {
            schema.emplace_back("", VAL_NONE, BlockSchemaType::LineBreak);
        } else {
            if (schema.empty() || schema.back().Type != BlockSchemaType::Text)
                schema.emplace_back("", VAL_NONE, BlockSchemaType::Text);

            BlockSchemaItem &last = schema.back();
            last.Name += *ch;
        }

        ch++;
    }

    return Error::Ok;
}

Error ParseLiteral(Value type, std::string_view text, LiteralValue &out)
{
    auto stripQuotes = [](std::string_view s) -> std::string_view {
        if (s.size() >= 2 &&
                ((s.front() == '\'' && s.back() == '\'') ||
                 (s.front() == '"'  && s.back() == '"'))) {
            return s.substr(1, s.size() - 2);
        }
        return s;
    };

    if (type & VAL_ANY) {
        if (text == "true") {
            out = true;
        } else if (text == "false") {
            out = false;
        } else {
            auto stripped = stripQuotes(text);

            if (stripped.size() != text.size()) {
                out = std::string(stripped);
            } else {
                try {
                    size_t pos = 0;
                    int i = std::stoi(std::string(text), &pos);

                    if (pos == text.size()) {
                        out = i;
                    } else {
                        out = static_cast<float>(std::stof(std::string(text)));
                    }
                } catch (...) {
                    out = std::string(text);
                }
            }
        }

        return Error::Ok;
    }

    if (type & VAL_INT) {
        try {
            out = std::stoi(std::string(text));
        } catch (...) {
            FAIL_COND_V_MSG(
                    true,
                    Error::BlockInvalidFmt,
                    "Invalid default integer '{}'",
                    text);
        }
        return Error::Ok;
    }

    if (type & (VAL_FLOAT | VAL_NUMBER)) {
        try {
            out = static_cast<float>(std::stof(std::string(text)));
        } catch (...) {
            FAIL_COND_V_MSG(
                    true,
                    Error::BlockInvalidFmt,
                    "Invalid default number '{}'",
                    text);
        }
        return Error::Ok;
    }

    if (type & VAL_BOOL) {
        if (text == "true") {
            out = true;
        } else if (text == "false") {
            out = false;
        } else {
            FAIL_COND_V_MSG(
                    true,
                    Error::BlockInvalidFmt,
                    "Invalid default bool '{}'",
                    text);
        }

        return Error::Ok;
    }

    if (type & VAL_STRING) {
        out = std::string(stripQuotes(text));
        return Error::Ok;
    }

    return Error::BlockInvalidFmt;
}

std::vector<std::string> BlockSchemaBodySlots(const BlockSchema &schema)
{
    std::vector<std::string> out;

    for (const auto &item : schema)
        if (item.Type == BlockSchemaType::Body)
            out.push_back(item.Name);

    return out;
}
