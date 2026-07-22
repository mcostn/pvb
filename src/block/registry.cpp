#include "block/registry.hpp"

#include <algorithm>

std::string VarGetOpCode(const std::string &name) { return "var_get_" + name; }
std::string VarSetOpCode(const std::string &name) { return "var_set_" + name; }

Error BlockRegistry::RegisterBlock(BlockDefinition def)
{
    for (const auto &d: Definitions) {
        if (d.OpCode == def.OpCode) {
            GlobalLogger.Error("Tried to register block with opcode '{}', which already exists", def.OpCode);
            return Error::BlockAlreadyExists;
        }
    }

    FAIL_COND_V_MSG(
            def.StmtBuilder && def.ExprBuilder,
            Error::BlockInvalidDefinition,
            "Block '{}' cannot be both a statement and an expression",
            def.OpCode);
    FAIL_COND_V_MSG(
            def.StmtBuilder && def.Shape == BlockShape::Reporter,
            Error::BlockInvalidDefinition,
            "Block '{}' cannot have a reporter shape and a statement builder at the same time",
            def.OpCode);
    FAIL_COND_V_MSG(
            def.ExprBuilder &&
            (def.Shape == BlockShape::Chain ||
             def.Shape == BlockShape::Hat ||
             def.Shape == BlockShape::Cap),
            Error::BlockInvalidDefinition,
            "Block '{}' cannot have a statement shape and a expression builder at the same time",
            def.OpCode);

    TRY(GenerateBlockSchema(def));

    if (def.ExprBuilder) {
        for (const auto &item : def.Schema) {
            FAIL_COND_V_MSG(
                    item.Type == BlockSchemaType::Body,
                    Error::BlockInvalidDefinition,
                    "Block '{}' is an expression and cannot declare body slot '{}'",
                    def.OpCode,
                    item.Name);
        }
    }

    if (def.StmtBuilder)
        Converter.StmtBuilders.emplace(def.OpCode, def.StmtBuilder);
    else if (def.ExprBuilder)
        Converter.ExprBuilders.emplace(def.OpCode, def.ExprBuilder);

    if (def.Shape == BlockShape::Unknown) {
        if (def.StmtBuilder) def.Shape = BlockShape::Chain;
        else if (def.ExprBuilder) def.Shape = BlockShape::Reporter;
    }

    Definitions.push_back(def);
    GlobalLogger.Debug("Registered block '{}'", def.OpCode);

    return Error::Ok;
}

bool BlockRegistry::HasVariable(const std::string &name) const
{
    for (const auto &v : Variables)
        if (v.Name == name)
            return true;

    return false;
}

Error BlockRegistry::AddVariable(const std::string &name, Value type)
{
    FAIL_COND_V_MSG(
            name.empty(),
            Error::BlockInvalidDefinition,
            "Variable name cannot be empty");

    FAIL_COND_V_MSG(
            HasVariable(name),
            Error::BlockAlreadyExists,
            "Variable '{}' already exists",
            name);

    std::string typeStr;
    switch (type) {
        case VAL_INT:    typeStr = "int";    break;
        case VAL_FLOAT:  typeStr = "float";  break;
        case VAL_BOOL:   typeStr = "bool";   break;
        case VAL_STRING: typeStr = "string"; break;
        default:
            GlobalLogger.Error("Variable '{}' has an unsupported type", name);
            return Error::BlockInvalidDefinition;
    }

    TRY(RegisterBlock({
        .Fmt = name,
        .Description = "Gets the value of '" + name + "'",
        .OpCode = VarGetOpCode(name),
        .Category = BlockCategory::Variable,
        .ExprBuilder = [name, type](BlockConverter &c, const BlockInstance &b) {
            DISCARD(c);
            DISCARD(b);
            return Var(name, type);
        },
        .ReturnType = type,
    }));

    Error err = RegisterBlock({
        .Fmt = "Set " + name + " to {" + typeStr + ":value=}",
        .Description = "Sets the value of '" + name + "'",
        .OpCode = VarSetOpCode(name),
        .Category = BlockCategory::Variable,
        .StmtBuilder = [name, type](BlockConverter &c, const BlockInstance &b) {
            return ExprStatement(Assign(name, c.ResolveArg(b.Args.at("value"), type)));
        },
    });

    if (err != Error::Ok) {
        Definitions.pop_back();
        Converter.ExprBuilders.erase(VarGetOpCode(name));
        return err;
    }

    Variables.push_back({ name, type });

    return Error::Ok;
}

Error BlockRegistry::RemoveVariable(const std::string &name)
{
    auto it = std::find_if(
            Variables.begin(),
            Variables.end(),
            [&](const VariableInfo &v) { return v.Name == name; });

    FAIL_COND_V_MSG(
            it == Variables.end(),
            Error::Failed,
            "Tried to remove variable '{}', which doesn't exist",
            name);

    Converter.ExprBuilders.erase(VarGetOpCode(name));
    Converter.StmtBuilders.erase(VarSetOpCode(name));

    Variables.erase(it);

    return Error::Ok;
}

Error BlockRegistry::RenameVariable(const std::string &oldName, const std::string &newName)
{
    FAIL_COND_V_MSG(
            newName.empty(),
            Error::BlockInvalidDefinition,
            "Variable name cannot be empty");

    auto it = std::find_if(
            Variables.begin(),
            Variables.end(),
            [&](const VariableInfo &v) { return v.Name == oldName; });

    FAIL_COND_V_MSG(
            it == Variables.end(),
            Error::Failed,
            "Tried to rename variable '{}', which doesn't exist",
            oldName);

    if (newName == oldName)
        return Error::Ok;

    FAIL_COND_V_MSG(
            HasVariable(newName),
            Error::BlockAlreadyExists,
            "A variable named '{}' already exists",
            newName);

    Value type = it->Type;

    std::string typeStr;
    switch (type) {
        case VAL_INT:    typeStr = "int";    break;
        case VAL_FLOAT:  typeStr = "float";  break;
        case VAL_BOOL:   typeStr = "bool";   break;
        case VAL_STRING: typeStr = "string"; break;
        default:
            GlobalLogger.Error("Variable '{}' has an unsupported type", oldName);
            return Error::BlockInvalidDefinition;
    }

    const std::string oldGetOp = VarGetOpCode(oldName);
    const std::string oldSetOp = VarSetOpCode(oldName);
    const std::string newGetOp = VarGetOpCode(newName);
    const std::string newSetOp = VarSetOpCode(newName);

    BlockDefinition *getDef = nullptr;
    BlockDefinition *setDef = nullptr;

    for (BlockDefinition &def : Definitions) {
        if (def.OpCode == oldGetOp) getDef = &def;
        else if (def.OpCode == oldSetOp) setDef = &def;
    }

    FAIL_COND_V_MSG(
            !getDef || !setDef,
            Error::Failed,
            "Couldn't find block definitions for variable '{}'",
            oldName);

    getDef->OpCode = newGetOp;
    getDef->Fmt = newName;
    getDef->Description = "Gets the value of '" + newName + "'";
    getDef->ExprBuilder = [newName, type](BlockConverter &c, const BlockInstance &b) {
        DISCARD(c);
        DISCARD(b);
        return Var(newName, type);
    };

    setDef->OpCode = newSetOp;
    setDef->Fmt = "Set " + newName + " to {" + typeStr + ":value=}";
    setDef->Description = "Sets the value of '" + newName + "'";
    setDef->StmtBuilder = [newName, type](BlockConverter &c, const BlockInstance &b) {
        return ExprStatement(Assign(newName, c.ResolveArg(b.Args.at("value"), type)));
    };

    getDef->Schema.clear();
    TRY(GenerateBlockSchema(*getDef));
    setDef->Schema.clear();
    TRY(GenerateBlockSchema(*setDef));

    Converter.ExprBuilders.erase(oldGetOp);
    Converter.StmtBuilders.erase(oldSetOp);
    Converter.ExprBuilders.emplace(newGetOp, getDef->ExprBuilder);
    Converter.StmtBuilders.emplace(newSetOp, setDef->StmtBuilder);

    it->Name = newName;

    return Error::Ok;
}

BlockRegistry GetBlockRegistry()
{
    BlockRegistry out;

    DISCARD(out.RegisterBlock({
        .Fmt = "Main",
        .Description = "Entry point of the program",
        .OpCode = "main",
        .Category = BlockCategory::Event,
        .Shape = BlockShape::Hat,
    }));

    DISCARD(out.RegisterBlock({
        .Fmt = "Print {any:out='Hello World'}",
        .Description = "Prints to console",
        .OpCode = "print",
        .Category = BlockCategory::Console,
        .StmtBuilder = [](BlockConverter &c, const BlockInstance &b) {
            return Print(
                    c.ResolveArg(b.Args.at("out"), VAL_ANY),
                    false);
        },
    }));

    DISCARD(out.RegisterBlock({
        .Fmt = "Print line {any:out='Hello World'}",
        .Description = "Prints to console and adds a new-line at the end",
        .OpCode = "println",
        .Category = BlockCategory::Console,
        .StmtBuilder = [](BlockConverter &c, const BlockInstance &b) {
            return Print(
                    c.ResolveArg(b.Args.at("out"), VAL_ANY),
                    true);
        },
    }));

    DISCARD(out.RegisterBlock({
        .Fmt = "Read into {any:$var}",
        .Description = "Reads from console into variable",
        .OpCode = "read",
        .Category = BlockCategory::Console,
        .StmtBuilder = [](BlockConverter &c, const BlockInstance &b) {
            DISCARD(c);
            const auto *varRef = std::get_if<VariableRef>(&b.Args.at("var"));
            std::string varName = varRef ? varRef->Name : "";
            Value varType = varRef ? varRef->Type : VAL_ANY;

            auto varExpr = std::make_unique<VariableExpr>(varType);
            varExpr->Name = varName;

            return Read(std::move(varExpr));
        }
    }));

    DISCARD(out.RegisterBlock({
        .Fmt = "{number:lhs=1} + {number:rhs=1}",
        .Description = "Adds 2 numbers",
        .OpCode = "add",
        .Category = BlockCategory::Math,
        .ExprBuilder = [](BlockConverter &c, const BlockInstance &b) {
            return Add(
                c.ResolveArg(b.Args.at("lhs"), VAL_NUMBER),
                c.ResolveArg(b.Args.at("rhs"), VAL_NUMBER));
        }
    }));

    DISCARD(out.RegisterBlock({
        .Fmt = "{number:lhs=1} - {number:rhs=1}",
        .Description = "Subtracts 2 numbers",
        .OpCode = "sub",
        .Category = BlockCategory::Math,
        .ExprBuilder = [](BlockConverter &c, const BlockInstance &b) {
            return Sub(
                c.ResolveArg(b.Args.at("lhs"), VAL_NUMBER),
                c.ResolveArg(b.Args.at("rhs"), VAL_NUMBER));
        }
    }));

    DISCARD(out.RegisterBlock({
        .Fmt = "{number:lhs=1} * {number:rhs=1}",
        .Description = "Multiply 2 numbers",
        .OpCode = "mul",
        .Category = BlockCategory::Math,
        .ExprBuilder = [](BlockConverter &c, const BlockInstance &b) {
            return Mul(
                c.ResolveArg(b.Args.at("lhs"), VAL_NUMBER),
                c.ResolveArg(b.Args.at("rhs"), VAL_NUMBER));
        }
    }));

    DISCARD(out.RegisterBlock({
        .Fmt = "{number:lhs=1} / {number:rhs=1}",
        .Description = "Divide 2 numbers",
        .OpCode = "div",
        .Category = BlockCategory::Math,
        .ExprBuilder = [](BlockConverter &c, const BlockInstance &b) {
            return Div(
                c.ResolveArg(b.Args.at("lhs"), VAL_NUMBER),
                c.ResolveArg(b.Args.at("rhs"), VAL_NUMBER));
        }
    }));

    DISCARD(out.RegisterBlock({
        .Fmt = "{int:lhs=1} mod {int:rhs=1}",
        .Description = "Remainder of the division of 2 numbers",
        .OpCode = "mod",
        .Category = BlockCategory::Math,
        .ExprBuilder = [](BlockConverter &c, const BlockInstance &b) {
            return Mod(
                c.ResolveArg(b.Args.at("lhs"), VAL_INT),
                c.ResolveArg(b.Args.at("rhs"), VAL_INT));
        }
    }));

    DISCARD(out.RegisterBlock({
        .Fmt = "Sqrt {number:value=1}",
        .Description = "Square root of a number",
        .OpCode = "sqrt",
        .Category = BlockCategory::Math,
        .ExprBuilder = [](BlockConverter &c, const BlockInstance &b) {
            return Call(
                Builtin::Sqrt,
                c.ResolveArg(b.Args.at("value"), VAL_NUMBER)
            );
        }
    }));

    DISCARD(out.RegisterBlock({
        .Fmt = "Sin {number:value=1}",
        .Description = "Sin of an angle in radians",
        .OpCode = "sin",
        .Category = BlockCategory::Math,
        .ExprBuilder = [](BlockConverter &c, const BlockInstance &b) {
            return Call(
                Builtin::Sin,
                c.ResolveArg(b.Args.at("value"), VAL_NUMBER)
            );
        }
    }));

    DISCARD(out.RegisterBlock({
        .Fmt = "Cos {number:value=1}",
        .Description = "Cos of an angle in radians",
        .OpCode = "cos",
        .Category = BlockCategory::Math,
        .ExprBuilder = [](BlockConverter &c, const BlockInstance &b) {
            return Call(
                Builtin::Cos,
                c.ResolveArg(b.Args.at("value"), VAL_NUMBER)
            );
        }
    }));

    DISCARD(out.RegisterBlock({
        .Fmt = "Tan {number:value=1}",
        .Description = "Tan of an angle in radians",
        .OpCode = "tan",
        .Category = BlockCategory::Math,
        .ExprBuilder = [](BlockConverter &c, const BlockInstance &b) {
            return Call(
                Builtin::Tan,
                c.ResolveArg(b.Args.at("value"), VAL_NUMBER)
            );
        }
    }));

    DISCARD(out.RegisterBlock({
        .Fmt = "Atan {number:value=1}",
        .Description = "Atan of an angle in radians",
        .OpCode = "atan",
        .Category = BlockCategory::Math,
        .ExprBuilder = [](BlockConverter &c, const BlockInstance &b) {
            return Call(
                Builtin::Atan,
                c.ResolveArg(b.Args.at("value"), VAL_NUMBER)
            );
        }
    }));

    DISCARD(out.RegisterBlock({
        .Fmt = "Max {number:lhs=1} {number:rhs=2}",
        .Description = "Maximum between 2 numbers",
        .OpCode = "max",
        .Category = BlockCategory::Math,
        .ExprBuilder = [](BlockConverter &c, const BlockInstance &b) {
            return Call(
                Builtin::Max,
                c.ResolveArg(b.Args.at("lhs"), VAL_NUMBER),
                c.ResolveArg(b.Args.at("rhs"), VAL_NUMBER)
            );
        }
    }));

    DISCARD(out.RegisterBlock({
        .Fmt = "Min {number:lhs=1} {number:rhs=2}",
        .Description = "Minimum between 2 numbers",
        .OpCode = "min",
        .Category = BlockCategory::Math,
        .ExprBuilder = [](BlockConverter &c, const BlockInstance &b) {
            return Call(
                Builtin::Min,
                c.ResolveArg(b.Args.at("lhs"), VAL_NUMBER),
                c.ResolveArg(b.Args.at("rhs"), VAL_NUMBER)
            );
        }
    }));

    DISCARD(out.RegisterBlock({
        .Fmt = "Round {number:value=0.5}",
        .Description = "Rounds the number",
        .OpCode = "round",
        .Category = BlockCategory::Math,
        .ExprBuilder = [](BlockConverter &c, const BlockInstance &b) {
            return Call(
                Builtin::Round,
                c.ResolveArg(b.Args.at("value"), VAL_NUMBER)
            );
        }
    }));

    DISCARD(out.RegisterBlock({
        .Fmt = "Abs {number:value=1}",
        .Description = "Absolute of a number",
        .OpCode = "abs",
        .Category = BlockCategory::Math,
        .ExprBuilder = [](BlockConverter &c, const BlockInstance &b) {
            return Call(
                Builtin::Abs,
                c.ResolveArg(b.Args.at("value"), VAL_NUMBER)
            );
        }
    }));

    DISCARD(out.RegisterBlock({
        .Fmt = "Floor {number:value=1}",
        .Description = "Floor of a number",
        .OpCode = "floor",
        .Category = BlockCategory::Math,
        .ExprBuilder = [](BlockConverter &c, const BlockInstance &b) {
            return Call(
                Builtin::Floor,
                c.ResolveArg(b.Args.at("value"), VAL_NUMBER)
            );
        }
    }));

    DISCARD(out.RegisterBlock({
        .Fmt = "Ceil {number:value=1}",
        .Description = "Floor of a number",
        .OpCode = "ceil",
        .Category = BlockCategory::Math,
        .ExprBuilder = [](BlockConverter &c, const BlockInstance &b) {
            return Call(
                Builtin::Ceil,
                c.ResolveArg(b.Args.at("value"), VAL_NUMBER)
            );
        }
    }));

    DISCARD(out.RegisterBlock({
        .Fmt = "True",
        .Description = "True value",
        .OpCode = "true",
        .Category = BlockCategory::Logic,
        .ExprBuilder = [](BlockConverter &c, const BlockInstance &b) {
            DISCARD(c);
            DISCARD(b);
            return Bool(true);
        },
        .ReturnType = VAL_BOOL,
    }));

    DISCARD(out.RegisterBlock({
        .Fmt = "False",
        .Description = "False value",
        .OpCode = "false",
        .Category = BlockCategory::Logic,
        .ExprBuilder = [](BlockConverter &c, const BlockInstance &b) {
            DISCARD(c);
            DISCARD(b);
            return Bool(false);
        },
        .ReturnType = VAL_BOOL,
    }));

    DISCARD(out.RegisterBlock({
        .Fmt = "{number:lhs=1} < {number:rhs=2}",
        .Description = "Checks if a number is less than another number",
        .OpCode = "lt",
        .Category = BlockCategory::Logic,
        .ExprBuilder = [](BlockConverter &c, const BlockInstance &b) {
            return Less(
                c.ResolveArg(b.Args.at("lhs"), VAL_NUMBER),
                c.ResolveArg(b.Args.at("rhs"), VAL_NUMBER));
        },
        .ReturnType = VAL_BOOL,
    }));

    DISCARD(out.RegisterBlock({
        .Fmt = "{number:lhs=1} > {number:rhs=2}",
        .Description = "Checks if a number is greater than another number",
        .OpCode = "gt",
        .Category = BlockCategory::Logic,
        .ExprBuilder = [](BlockConverter &c, const BlockInstance &b) {
            return Greater(
                c.ResolveArg(b.Args.at("lhs"), VAL_NUMBER),
                c.ResolveArg(b.Args.at("rhs"), VAL_NUMBER));
        },
        .ReturnType = VAL_BOOL,
    }));

    DISCARD(out.RegisterBlock({
        .Fmt = "{number:lhs=1} <= {number:rhs=2}",
        .Description = "Checks if a number is less then or equal to another number",
        .OpCode = "le",
        .Category = BlockCategory::Logic,
        .ExprBuilder = [](BlockConverter &c, const BlockInstance &b) {
            return LessEqual(
                c.ResolveArg(b.Args.at("lhs"), VAL_NUMBER),
                c.ResolveArg(b.Args.at("rhs"), VAL_NUMBER));
        },
        .ReturnType = VAL_BOOL,
    }));

    DISCARD(out.RegisterBlock({
        .Fmt = "{number:lhs=1} >= {number:rhs=2}",
        .Description = "Checks if a number is greater then or equal to another number",
        .OpCode = "ge",
        .Category = BlockCategory::Logic,
        .ExprBuilder = [](BlockConverter &c, const BlockInstance &b) {
            return GreaterEqual(
                c.ResolveArg(b.Args.at("lhs"), VAL_NUMBER),
                c.ResolveArg(b.Args.at("rhs"), VAL_NUMBER));
        },
        .ReturnType = VAL_BOOL,
    }));

    DISCARD(out.RegisterBlock({
        .Fmt = "{number:lhs=1} = {number:rhs=2}",
        .Description = "Checks if a number is equal to another number",
        .OpCode = "eq",
        .Category = BlockCategory::Logic,
        .ExprBuilder = [](BlockConverter &c, const BlockInstance &b) {
            return Equal(
                c.ResolveArg(b.Args.at("lhs"), VAL_NUMBER),
                c.ResolveArg(b.Args.at("rhs"), VAL_NUMBER));
        },
        .ReturnType = VAL_BOOL,
    }));

    DISCARD(out.RegisterBlock({
        .Fmt = "{number:lhs=1} != {number:rhs=2}",
        .Description = "Checks if a number is equal to another number",
        .OpCode = "neq",
        .Category = BlockCategory::Logic,
        .ExprBuilder = [](BlockConverter &c, const BlockInstance &b) {
            return NotEqual(
                c.ResolveArg(b.Args.at("lhs"), VAL_NUMBER),
                c.ResolveArg(b.Args.at("rhs"), VAL_NUMBER));
        },
        .ReturnType = VAL_BOOL,
    }));

    DISCARD(out.RegisterBlock({
        .Fmt = "not {bool:value=true}",
        .Description = "Negates a condition",
        .OpCode = "not",
        .Category = BlockCategory::Logic,
        .ExprBuilder = [](BlockConverter &c, const BlockInstance &b) {
            return Not(c.ResolveArg(b.Args.at("value"), VAL_BOOL));
        },
        .ReturnType = VAL_BOOL,
    }));

    DISCARD(out.RegisterBlock({
        .Fmt = "{bool:lhs=true} and {bool:rhs=true}",
        .Description = "Ands 2 conditions",
        .OpCode = "and",
        .Category = BlockCategory::Logic,
        .ExprBuilder = [](BlockConverter &c, const BlockInstance &b) {
            return And(
                c.ResolveArg(b.Args.at("lhs"), VAL_BOOL),
                c.ResolveArg(b.Args.at("rhs"), VAL_BOOL));
        },
        .ReturnType = VAL_BOOL,
    }));

    DISCARD(out.RegisterBlock({
        .Fmt = "{bool:lhs=true} or {bool:rhs=true}",
        .Description = "Ors 2 conditions",
        .OpCode = "or",
        .Category = BlockCategory::Logic,
        .ExprBuilder = [](BlockConverter &c, const BlockInstance &b) {
            return Or(
                    c.ResolveArg(b.Args.at("lhs"), VAL_BOOL),
                    c.ResolveArg(b.Args.at("rhs"), VAL_BOOL));
        },
        .ReturnType = VAL_BOOL,
    }));

    DISCARD(out.RegisterBlock({
        .Fmt = "Length of {string:value='hello'}",
        .Description = "Number of characters in a string",
        .OpCode = "str_length",
        .Category = BlockCategory::Logic,
        .ExprBuilder = [](BlockConverter &c, const BlockInstance &b) {
            return Call(
                Builtin::Length,
                c.ResolveArg(b.Args.at("value"), VAL_STRING)
            );
        },
        .ReturnType = VAL_INT,
    }));

    DISCARD(out.RegisterBlock({
        .Fmt = "Letter {int:index=1} of {string:value='hello'}",
        .Description = "The letter at the given position of a string (1 is the first letter)",
        .OpCode = "str_char_at",
        .Category = BlockCategory::Logic,
        .ExprBuilder = [](BlockConverter &c, const BlockInstance &b) {
            return Call(
                Builtin::CharAt,
                c.ResolveArg(b.Args.at("value"), VAL_STRING),
                Sub(c.ResolveArg(b.Args.at("index"), VAL_INT), Int(1))
            );
        },
        .ReturnType = VAL_STRING,
    }));

    DISCARD(out.RegisterBlock({
        .Fmt = "Join {string:lhs='hello '} and {string:rhs='world'}",
        .Description = "Joins two strings together",
        .OpCode = "str_join",
        .Category = BlockCategory::Logic,
        .ExprBuilder = [](BlockConverter &c, const BlockInstance &b) {
            return Call(
                Builtin::Join,
                c.ResolveArg(b.Args.at("lhs"), VAL_STRING),
                c.ResolveArg(b.Args.at("rhs"), VAL_STRING)
            );
        },
        .ReturnType = VAL_STRING,
    }));

    DISCARD(out.RegisterBlock({
        .Fmt = "{string:value='hello'} contains {string:substr='e'}",
        .Description = "Checks if a string contains another string",
        .OpCode = "str_contains",
        .Category = BlockCategory::Logic,
        .ExprBuilder = [](BlockConverter &c, const BlockInstance &b) {
            return Call(
                Builtin::Contains,
                c.ResolveArg(b.Args.at("value"), VAL_STRING),
                c.ResolveArg(b.Args.at("substr"), VAL_STRING)
            );
        },
        .ReturnType = VAL_BOOL,
    }));


     DISCARD(out.RegisterBlock({
        .Fmt = "If {bool:cond=true} {body:then}",
        .Description = "Runs the enclosed blocks if the condition is true",
        .OpCode = "if",
        .Category = BlockCategory::ControlFlow,
        .StmtBuilder = [](BlockConverter &c, const BlockInstance &b) {
            return If(
                    c.ResolveArg(b.Args.at("cond"), VAL_BOOL),
                    c.ConvertBody(b, "then"));
        }
    }));

    DISCARD(out.RegisterBlock({
        .Fmt = "If {bool:cond=true} {body:then} else {body:else}",
        .Description = "Runs one of two branches depending on the condition",
        .OpCode = "ifelse",
        .Category = BlockCategory::ControlFlow,
        .StmtBuilder = [](BlockConverter &c, const BlockInstance &b) {
            return If(
                    c.ResolveArg(b.Args.at("cond"), VAL_BOOL),
                    c.ConvertBody(b, "then"),
                    c.ConvertBody(b, "else"));
        }
    }));

    DISCARD(out.RegisterBlock({
        .Fmt = "While {bool:cond=true} {body:body}",
        .Description = "Repeats the enclosed blocks while the condition is true",
        .OpCode = "while",
        .Category = BlockCategory::ControlFlow,
        .StmtBuilder = [](BlockConverter &c, const BlockInstance &b) {
            return While(
                    c.ResolveArg(b.Args.at("cond"), VAL_BOOL),
                    c.ConvertBody(b, "body"));
        }
    }));

    DISCARD(out.RegisterBlock({
        .Fmt = "For {any:$var} from {int:start=0} to {int:end=10} {body:body}",
        .Description = "Repeats the enclosed blocks, counting a variable from start to end",
        .OpCode = "for",
        .Category = BlockCategory::ControlFlow,
        .StmtBuilder = [](BlockConverter &c, const BlockInstance &b) {
            const auto *varRef = std::get_if<VariableRef>(&b.Args.at("var"));
            std::string varName = varRef ? varRef->Name : "i";

            auto init = DeclVar(VAL_INT, varName, c.ResolveArg(b.Args.at("start"), VAL_INT));
            auto cond = Less(Var(varName, VAL_INT), c.ResolveArg(b.Args.at("end"), VAL_INT));
            auto update = Assign(varName, Add(Var(varName, VAL_INT), Int(1)));

            return For(
                    std::move(init),
                    std::move(cond),
                    std::move(update),
                    c.ConvertBody(b, "body"));
        }
    }));

    DISCARD(out.RegisterBlock({
        .Fmt = "Continue",
        .Description = "Goes to the next iteration of a loop",
        .OpCode = "continue",
        .Category = BlockCategory::ControlFlow,
        .Shape = BlockShape::Cap,
        .StmtBuilder = [](BlockConverter &c, const BlockInstance &b) {
            DISCARD(c);
            DISCARD(b);
            return Continue();
        }
    }));

    DISCARD(out.RegisterBlock({
        .Fmt = "Break",
        .Description = "Ends the loop",
        .OpCode = "break",
        .Category = BlockCategory::ControlFlow,
        .Shape = BlockShape::Cap,
        .StmtBuilder = [](BlockConverter &c, const BlockInstance &b) {
            DISCARD(c);
            DISCARD(b);
            return Break();
        }
    }));

    DISCARD(out.RegisterBlock({
        .Fmt = "Exit {int:code=0}",
        .Description = "Exits the program with the specified exit code",
        .OpCode = "exit",
        .Category = BlockCategory::ControlFlow,
        .Shape = BlockShape::Cap,
        .StmtBuilder = [](BlockConverter &c, const BlockInstance &b) {
            return Exit(c.ResolveArg(b.Args.at("code"), VAL_INT));
        }
    }));

    return out;
}
