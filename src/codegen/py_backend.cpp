#include "codegen/py_backend.hpp"

Error PythonEmitter::EmitProgram(const Program &program)
{
    for (const auto &stmt : program.Statements) {
        Indent();
        TRY(EmitStmt(*stmt));
        Out() << "\n";
    }

    return Error::Ok;
}

Error PythonEmitter::EmitPrint(const PrintStmt &stmt)
{
    Out() << "print(";
    TRY(EmitExpr(*stmt.Data));
    Out() << ")";
    return Error::Ok;
}

Error PythonEmitter::EmitExit(const ExitStmt &stmt)
{
    // TODO: Dynamic imports
    Out() << "import sys\n";
    Out() << "sys.exit(";
    TRY(EmitExpr(*stmt.Code));
    Out() << ")";
    return Error::Ok;
}

Error PythonEmitter::EmitExprStmt(const ExprStmt &stmt)
{
    Indent();
    TRY(EmitExpr(*stmt.Expression));
    return Error::Ok;
}

Error PythonEmitter::EmitBlock(const BlockStmt &stmt)
{
    Out() << ":\n";
    ++IndentLevel;
    if (stmt.Statements.size() == 0) {
        Indent();
        Out() << "...";
    } else {
        for (const auto &s : stmt.Statements) {
            Indent();
            TRY(EmitStmt(*s));
        }
    }
    --IndentLevel;

    return Error::Ok;
}

Error PythonEmitter::EmitFunction(const FunctionStmt &stmt)
{
    Indent();
    Out() << "def " << stmt.Name << "(";

    for (size_t i = 0; i < stmt.Params.size(); ++i) {
        Out() << stmt.Params[i].Name;
        if (i + 1 < stmt.Params.size())
            Out() << ", ";
    }

    Out() << ")";

    TRY(EmitBlock(*stmt.Body));
    Out() << "\n";

    return Error::Ok;
}

Error PythonEmitter::EmitIf(const IfStmt &stmt)
{
    Out() << "if ";
    TRY(EmitExpr(*stmt.Condition));

    if (stmt.ThenBranch->Kind != AstNodeKind::BlockStmt)
        Out() << ": ";
    TRY(EmitStmt(*stmt.ThenBranch));

    if (stmt.ElseBranch) {
        Out() << "\n";
        Out() << "else";
        TRY(EmitStmt(*stmt.ElseBranch));
    }

    return Error::Ok;
}

Error PythonEmitter::EmitWhile(const WhileStmt &stmt)
{
    Out() << "while ";
    TRY(EmitExpr(*stmt.Condition));
    if (stmt.Body->Kind != AstNodeKind::BlockStmt)
        Out() << ": ";

    TRY(EmitStmt(*stmt.Body));
    return Error::Ok;
}

Error PythonEmitter::EmitFor(const ForStmt &stmt)
{
    // init
    if (stmt.Init) {
        TRY(EmitStmt(*stmt.Init));
        Out() << "\n";
    }

    Indent();
    Out() << "while ";

    if (stmt.Condition) TRY(EmitExpr(*stmt.Condition));
    else Out() << "True";

    if (stmt.Body) {
        if (stmt.Body->Kind != AstNodeKind::BlockStmt) {
            ++IndentLevel;
            Out() << ": ";
        }

        TRY(EmitStmt(*stmt.Body));
        if (stmt.Body->Kind != AstNodeKind::BlockStmt)
            Out() << "\n";

        if (stmt.Update) {
            Out() << "\n";
            ++IndentLevel;
            Indent();
            TRY(EmitExpr(*stmt.Update));
            --IndentLevel;
        }

        if (stmt.Body->Kind != AstNodeKind::BlockStmt) {
            --IndentLevel;
        }
    }

    return Error::Ok;
}

Error PythonEmitter::EmitDeclVar(const DeclVarStmt &stmt)
{
    Indent();
    Out() << stmt.Name;

    if (stmt.Initializer) {
        Out() << " = ";
        TRY(EmitExpr(*stmt.Initializer));
    } else {
        Out() << " = None";
    }

    return Error::Ok;
}

Error PythonEmitter::EmitLiteral(const LiteralExpr &expr)
{
    std::visit([this](auto&& value) {
        using T = std::decay_t<decltype(value)>;

        if constexpr (std::is_same_v<T, bool>) {
            Out() << (value ? "True" : "False");
        } else if constexpr (std::is_same_v<T, std::string>) {
            Out() << '"' << value << '"';
        } else {
            Out() << value;
        }
    }, expr.Data);

    return Error::Ok;
}

Error PythonEmitter::EmitVariable(const VariableExpr &expr)
{
    Out() << expr.Name;
    return Error::Ok;
}

Error PythonEmitter::EmitAssign(const AssignExpr &expr)
{
    Out() << expr.Name;
    Out() << " = ";
    TRY(EmitExpr(*expr.ValueExpr));
    return Error::Ok;
}

Error PythonEmitter::EmitUnary(const UnaryExpr &expr)
{
    switch (expr.Op) {
        case UnaryOp::Negate: Out() << "-"; break;
        case UnaryOp::Not:    Out() << "not "; break;
    }

    Out() << "(";
    TRY(EmitExpr(*expr.Data));
    Out() << ")";
    return Error::Ok;
}

Error PythonEmitter::EmitBinary(const BinaryExpr &expr)
{
    Out() << "(";
    TRY(EmitExpr(*expr.Left));

    Out() << ' ';
    switch (expr.Op) {
        case BinaryOp::Add: Out() << "+"; break;
        case BinaryOp::Sub: Out() << "-"; break;
        case BinaryOp::Mul: Out() << "*"; break;
        case BinaryOp::Div: Out() << "/"; break;
        case BinaryOp::Mod: Out() << "%"; break;

        case BinaryOp::Less: Out() << "<"; break;
        case BinaryOp::Greater: Out() << ">"; break;
        case BinaryOp::LessEqual: Out() << "<="; break;
        case BinaryOp::GreaterEqual: Out() << ">="; break;

        case BinaryOp::Equal: Out() << "=="; break;
        case BinaryOp::NotEqual: Out() << "!="; break;

        case BinaryOp::And: Out() << "and"; break;
        case BinaryOp::Or:  Out() << "or"; break;
    }
    Out() << ' ';

    TRY(EmitExpr(*expr.Right));
    Out() << ")";

    return Error::Ok;
}

Error PythonEmitter::EmitCall(const CallExpr &expr)
{
    Out() << expr.Function << "(";

    for (size_t i = 0; i < expr.Args.size(); ++i) {
        TRY(EmitExpr(*expr.Args[i]));
        if (i + 1 < expr.Args.size())
            Out() << ", ";
    }

    Out() << ")";
    return Error::Ok;
}
