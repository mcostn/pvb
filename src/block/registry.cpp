#include "block/registry.hpp"

Error BlockRegistry::RegisterBlock(BlockDefinition def)
{
    for (const auto &d: Definitions) {
        if (d.OpCode == def.OpCode) {
            GlobalLogger.Error("Tried to register block with opcode '{}', which already exists", def.OpCode);
            return Error::BlockAlreadyExists;
        }
    }

    FAIL_COND_V_MSG(
            !def.StmtBuilder && !def.ExprBuilder,
            Error::BlockInvalidDefinition,
            "Block '{}' has no builder",
            def.OpCode);
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


BlockRegistry GetBlockRegistry()
{
    BlockRegistry out;

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
        // .StmtBuilder = [](BlockConverter &, const BlockInstance &b) {
        //     return Read(b.Args.at("var"));
        // };
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
        .Fmt = "Round {number:value=0.5}",
        .Description = "Rounds the number",
        .OpCode = "round",
        .Category = BlockCategory::Math,
        .ExprBuilder = [](BlockConverter &c, const BlockInstance &b) {
            return Call(
                "round",
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
                "abs",
                c.ResolveArg(b.Args.at("value"), VAL_NUMBER)
            );
        }
    }));

    DISCARD(out.RegisterBlock({
        .Fmt = "Sqrt {number:value=1}",
        .Description = "Square root of a number",
        .OpCode = "sqrt",
        .Category = BlockCategory::Math,
        .ExprBuilder = [](BlockConverter &c, const BlockInstance &b) {
            return Call(
                "sqrt",
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
        .Fmt = "Exit {int:code=0}",
        .Description = "Exits the program with the specified exit code",
        .OpCode = "exit",
        .Category = BlockCategory::ControlFlow,
        .Shape = BlockShape::Cap,
        .StmtBuilder = [](BlockConverter &c, const BlockInstance &b) {
            return Exit(c.ResolveArg(b.Args.at("code"), VAL_INT));
        }
    }));

    DISCARD(out.RegisterBlock({
        .Fmt = "Get {any:$var}",
        .Description = "Gets the value of a variable",
        .OpCode = "get",
        .Category = BlockCategory::Variable,
        .ExprBuilder = [](BlockConverter &c, const BlockInstance &b) {
            DISCARD(c);
            const auto *varRef = std::get_if<VariableRef>(&b.Args.at("var"));
            return Var(varRef ? varRef->Name : "", VAL_ANY);
        },
        .ReturnType = VAL_ANY
    }));

    // DISCARD(out.RegisterBlock({
    //     .Fmt = "Set {any:$var} to {any:value=0}",
    //     .Description = "Sets the value of a variable",
    //     .OpCode = "set",
    //     .Category = BlockCategory::Variable,
    //     .StmtBuilder = [](BlockConverter &c, const BlockInstance &b) {
    //         const auto *varRef = std::get_if<VariableRef>(&b.Args.at("var"));
    //         return Assign(
    //                 varRef ? varRef->Name : "",
    //                 c.ResolveArg(b.Args.at("value"), VAL_ANY));
    //     }
    // }));
    //
    return out;
}
