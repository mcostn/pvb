#include <format>

#include "block/block.hpp"
#include "util/logger.hpp"
#include "util/macro.hpp"

static Error GenerateBlockSchema(BlockDefinition &def)
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

            Value valueType = VAL_NONE;
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

            BlockSchemaType schemaType = hasVariable ? BlockSchemaType::Var : BlockSchemaType::Input;

            schema.emplace_back(nameStr, valueType, schemaType);
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

std::string BlockSchemaToString(const BlockSchema &schema)
{
    std::string out = "";

    for (size_t i = 0; i < schema.size(); i++) {
        const auto &item = schema[i];

        out += "[";
        out += std::to_string(i);
        out += "] ";

        switch (item.Type) {
            case BlockSchemaType::Text:
                out += "Text    : '";
                out += item.Name;
                out += "'";
                break;

            case BlockSchemaType::Input:
                out += "Input   : name='";
                out += item.Name;
                out += "', type=";
                out += ValueToString(item.ValueType);
                break;

            case BlockSchemaType::Var:
                out += "Var     : name='";
                out += item.Name;
                out += "', type=";
                out += ValueToString(item.ValueType);
                break;
        }

        if (i < schema.size() - 1) out += "\n";
    }

    return out;
}

Error BlockRegistry::RegisterBlock(BlockDefinition def)
{
    for (const auto &d: Definitions) {
        if (d.OpCode == def.OpCode) {
            GlobalLogger.Error("Tried to register block with opcode '{}', which already exists", def.OpCode);
            return Error::BlockAlreadyExists;
        }
    }

    TRY(GenerateBlockSchema(def));
    Definitions.push_back(def);
    GlobalLogger.Debug(
            "Registered block '{}' with schema:\n{}",
            def.OpCode,
            BlockSchemaToString(def.Schema));

    return Error::Ok;
}

BlockRegistry GetBlockRegistry()
{
    BlockRegistry out;

    DISCARD(out.RegisterBlock({
        .Fmt = "Print {any:out='Hello World'}",
        .Description = "Prints to console",
        .OpCode = "print",
        .Category = BlockCategory::Console,
    }));
    DISCARD(out.RegisterBlock({
        .Fmt = "Print line {any:out='Hello World'}",
        .Description = "Prints to console and adds a new-line at the end",
        .OpCode = "println",
        .Category = BlockCategory::Console,
    }));
    DISCARD(out.RegisterBlock({
        .Fmt = "Read into {any:$var}",
        .Description = "Reads from console into variable",
        .OpCode = "read",
        .Category = BlockCategory::Console,
    }));

    DISCARD(out.RegisterBlock({
        .Fmt = "Exit {int:code=0}",
        .Description = "Exits the program with the specified exit code",
        .OpCode = "exit",
        .Category = BlockCategory::ControlFlow,
    }));

    DISCARD(out.RegisterBlock({
        .Fmt = "{number:lhs=1} + {number:rhs=1}",
        .Description = "Adds 2 numbers",
        .OpCode = "add",
        .Category = BlockCategory::Math,
    }));
    DISCARD(out.RegisterBlock({
        .Fmt = "{number:lhs=1} - {number:rhs=1}",
        .Description = "Subtracts 2 numbers",
        .OpCode = "sub",
        .Category = BlockCategory::Math,
    }));
    DISCARD(out.RegisterBlock({
        .Fmt = "{number:lhs=1} * {number:rhs=1}",
        .Description = "Multiply 2 numbers",
        .OpCode = "mul",
        .Category = BlockCategory::Math,
    }));
    DISCARD(out.RegisterBlock({
        .Fmt = "{number:lhs=1} / {number:rhs=1}",
        .Description = "Divide 2 numbers",
        .OpCode = "div",
        .Category = BlockCategory::Math,
    }));
    DISCARD(out.RegisterBlock({
        .Fmt = "{int:lhs=1} mod {int:rhs=1}",
        .Description = "Remainder of the division of 2 numbers",
        .OpCode = "mod",
        .Category = BlockCategory::Math,
    }));
    DISCARD(out.RegisterBlock({
        .Fmt = "Round {number:value=0.5}",
        .Description = "Rounds the number",
        .OpCode = "round",
        .Category = BlockCategory::Math,
    }));
    DISCARD(out.RegisterBlock({
        .Fmt = "Abs {number:value=1}",
        .Description = "Absolute of a number",
        .OpCode = "abs",
        .Category = BlockCategory::Math,
    }));
    DISCARD(out.RegisterBlock({
        .Fmt = "Sqrt {number:value=1}",
        .Description = "Square root of a number",
        .OpCode = "sqrt",
        .Category = BlockCategory::Math,
    }));

    DISCARD(out.RegisterBlock({
        .Fmt = "{number:lhs=1} < {number:rhs=2}",
        .Description = "Checks if a number is less than another number",
        .OpCode = "lt",
        .Category = BlockCategory::Logic,
    }));
    DISCARD(out.RegisterBlock({
        .Fmt = "{number:lhs=1} > {number:rhs=2}",
        .Description = "Checks if a number is greater than another number",
        .OpCode = "gt",
        .Category = BlockCategory::Logic,
    }));
    DISCARD(out.RegisterBlock({
        .Fmt = "{number:lhs=1} <= {number:rhs=2}",
        .Description = "Checks if a number is less then or equal to another number",
        .OpCode = "le",
        .Category = BlockCategory::Logic,
    }));
    DISCARD(out.RegisterBlock({
        .Fmt = "{number:lhs=1} >= {number:rhs=2}",
        .Description = "Checks if a number is greater then or equal to another number",
        .OpCode = "ge",
        .Category = BlockCategory::Logic,
    }));
    DISCARD(out.RegisterBlock({
        .Fmt = "{number:lhs=1} >= {number:rhs=2}",
        .Description = "Checks if a number is greater then or equal to another number",
        .OpCode = "ge",
        .Category = BlockCategory::Logic,
    }));
    DISCARD(out.RegisterBlock({
        .Fmt = "not {bool:value=true}",
        .Description = "Negates a condition",
        .OpCode = "not",
        .Category = BlockCategory::Logic,
    }));
    DISCARD(out.RegisterBlock({
        .Fmt = "{bool:lhs=true} and {bool:rhs=true}",
        .Description = "Ands 2 conditions",
        .OpCode = "and",
        .Category = BlockCategory::Logic,
    }));
    DISCARD(out.RegisterBlock({
        .Fmt = "{bool:lhs=true} or {bool:rhs=true}",
        .Description = "Ors 2 conditions",
        .OpCode = "or",
        .Category = BlockCategory::Logic,
    }));

    return out;
}
