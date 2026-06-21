#include "lua_backend.hpp"
#include "util/logger.hpp"

Error LuaEmitter::EmitProgram(const Program &program)
{
    for (const auto &stmt : program.Statements) {
        Indent();
        TRY(EmitStmt(*stmt));
        Out << "\n";
    }

    return Error::Ok;
}

Error LuaEmitter::EmitStmt(const Stmt &stmt)
{
    switch (stmt.Kind) {
        case AstNodeKind::PrintStmt:
            return EmitPrint(static_cast<const PrintStmt&>(stmt));

        case AstNodeKind::ExitStmt:
            return EmitExit(static_cast<const ExitStmt&>(stmt));

        case AstNodeKind::ExprStmt:
            return EmitExprStmt(static_cast<const ExprStmt&>(stmt));

        case AstNodeKind::BlockStmt:
            return EmitBlock(static_cast<const BlockStmt&>(stmt));

        case AstNodeKind::IfStmt:
            return EmitIf(static_cast<const IfStmt&>(stmt));

        case AstNodeKind::WhileStmt:
            return EmitWhile(static_cast<const WhileStmt&>(stmt));

        case AstNodeKind::ForStmt:
            return EmitFor(static_cast<const ForStmt&>(stmt));

        case AstNodeKind::DeclVarStmt:
            return EmitDeclVar(static_cast<const DeclVarStmt&>(stmt));

        default:
            GlobalLogger.Error("Unexepected statement kind");
            return Error::Failed;
    }
}

Error LuaEmitter::EmitPrint(const PrintStmt &stmt)
{
    Out << "io.write(tostring(";
    TRY(EmitExpr(*stmt.Data));
    Out << ")";
    if (stmt.Newline)
        Out << " .. \"\\n\"";
    Out << ")";

    return Error::Ok;
}

Error LuaEmitter::EmitExit(const ExitStmt &stmt)
{
    Out << "os.exit(";
    TRY(EmitExpr(*stmt.Code));
    Out << ")";

    return Error::Ok;
}

Error LuaEmitter::EmitExprStmt(const ExprStmt &stmt)
{
    TRY(EmitExpr(*stmt.Expression));

    return Error::Ok;
}

Error LuaEmitter::EmitBlock(const BlockStmt &stmt)
{
    Out << "do\n";
    ++IndentLevel;
    for (const auto& s : stmt.Statements) {
        Indent();
        TRY(EmitStmt(*s));
        Out << "\n";
    }
    --IndentLevel;
    Indent();
    Out << "end";

    return Error::Ok;
}

Error LuaEmitter::EmitIf(const IfStmt &stmt)
{
    Out << "if ";
    TRY(EmitExpr(*stmt.Condition));
    Out << " then\n";

    ++IndentLevel;
    Indent();
    TRY(EmitStmt(*stmt.ThenBranch));
    Out << "\n";
    --IndentLevel;

    if (stmt.ElseBranch) {
        Indent();
        Out << "else\n";
        ++IndentLevel;
        Indent();
        TRY(EmitStmt(*stmt.ElseBranch));
        Out << "\n";
        --IndentLevel;
    }

    Indent();
    Out << "end";

    return Error::Ok;
}

Error LuaEmitter::EmitWhile(const WhileStmt &stmt)
{
    Out << "while ";
    TRY(EmitExpr(*stmt.Condition));
    Out << " do\n";

    ++IndentLevel;
    Indent();
    TRY(EmitStmt(*stmt.Body));
    Out << "\n";
    --IndentLevel;

    Indent();
    Out << "end";

    return Error::Ok;
}

Error LuaEmitter::EmitFor(const ForStmt &stmt)
{
    // Lua has no C-style for loop, so we desugar into a while loop wrapped
    // in a do-block so the init variable stays scoped.
    Out << "do\n";
    ++IndentLevel;

    if (stmt.Init) {
        Indent();
        if (stmt.Init->Kind == AstNodeKind::ExprStmt) {
            auto &exprStmt = static_cast<const ExprStmt&>(*stmt.Init);
            TRY(EmitExpr(*exprStmt.Expression));
        } else if (stmt.Init->Kind == AstNodeKind::DeclVarStmt) {
            auto &declStmt = static_cast<const DeclVarStmt&>(*stmt.Init);
            TRY(EmitDeclVar(declStmt));
        }
        Out << "\n";
    }

    Indent();
    Out << "while ";
    if (stmt.Condition)
        TRY(EmitExpr(*stmt.Condition));
    else
        Out << "true";
    Out << " do\n";

    ++IndentLevel;
    Indent();
    TRY(EmitStmt(*stmt.Body));
    Out << "\n";

    if (stmt.Update) {
        Indent();
        TRY(EmitExpr(*stmt.Update));
        Out << "\n";
    }
    --IndentLevel;

    Indent();
    Out << "end\n";

    --IndentLevel;
    Indent();
    Out << "end";

    return Error::Ok;
}

Error LuaEmitter::EmitDeclVar(const DeclVarStmt &stmt)
{
    // Lua is dynamically typed, so the declared Value type is dropped.
    Out << "local " << stmt.Name;
    if (stmt.Initializer) {
        Out << " = ";
        TRY(EmitExpr(*stmt.Initializer));
    } else {
        Out << " = nil";
    }

    return Error::Ok;
}

Error LuaEmitter::EmitExpr(const Expr &expr)
{
    switch (expr.Kind) {
        case AstNodeKind::LiteralExpr:
            return EmitLiteral(static_cast<const LiteralExpr&>(expr));

        case AstNodeKind::VariableExpr:
            return EmitVariable(static_cast<const VariableExpr&>(expr));

        case AstNodeKind::AssignExpr:
            return EmitAssign(static_cast<const AssignExpr&>(expr));

        case AstNodeKind::UnaryExpr:
            return EmitUnary(static_cast<const UnaryExpr&>(expr));

        case AstNodeKind::BinaryExpr:
            return EmitBinary(static_cast<const BinaryExpr&>(expr));

        case AstNodeKind::CallExpr:
            return EmitCall(static_cast<const CallExpr&>(expr));

        default:
            GlobalLogger.Error("Unexepected expression kind");
            return Error::Failed;
    }
}

Error LuaEmitter::EmitLiteral(const LiteralExpr &expr)
{
    std::visit([this](auto&& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, bool>) {
            Out << (value ? "true" : "false");
        } else if constexpr (std::is_same_v<T, std::string>) {
            Out << '"' << value << '"';
        } else {
            Out << value;
        }
    }, expr.Data);

    return Error::Ok;
}

Error LuaEmitter::EmitVariable(const VariableExpr &expr)
{
    Out << expr.Name;
    return Error::Ok;
}

Error LuaEmitter::EmitAssign(const AssignExpr &expr)
{
    Out << expr.Name;
    Out << " = ";
    TRY(EmitExpr(*expr.ValueExpr));
    return Error::Ok;
}

Error LuaEmitter::EmitUnary(const UnaryExpr &expr)
{
    switch (expr.Op) {
        case UnaryOp::Negate:
            Out << "-";
            break;
        case UnaryOp::Not:
            Out << "not ";
            break;
        default:
            GlobalLogger.Error("Unsupported unary operator for Lua backend");
            return Error::Failed;
    }

    Out << "(";
    TRY(EmitExpr(*expr.Data));
    Out << ")";
    return Error::Ok;
}

Error LuaEmitter::EmitBinary(const BinaryExpr &expr)
{
    std::string_view opStr;
    switch (expr.Op) {
        case BinaryOp::Add:          opStr = "+";   break;
        case BinaryOp::Sub:          opStr = "-";   break;
        case BinaryOp::Mul:          opStr = "*";   break;
        case BinaryOp::Div:          opStr = "/";   break;
        case BinaryOp::Mod:          opStr = "%";   break;
        case BinaryOp::Less:         opStr = "<";   break;
        case BinaryOp::Greater:      opStr = ">";   break;
        case BinaryOp::LessEqual:    opStr = "<=";  break;
        case BinaryOp::GreaterEqual: opStr = ">=";  break;
        case BinaryOp::Equal:        opStr = "==";  break;
        case BinaryOp::NotEqual:     opStr = "~=";  break;
        case BinaryOp::And:          opStr = "and"; break;
        case BinaryOp::Or:           opStr = "or";  break;
        default:
            GlobalLogger.Error("Unsupported binary operator for Lua backend");
            return Error::Failed;
    }

    Out << "(";
    TRY(EmitExpr(*expr.Left));
    Out << " " << opStr << " ";
    TRY(EmitExpr(*expr.Right));
    Out << ")";

    return Error::Ok;
}

Error LuaEmitter::EmitCall(const CallExpr &expr)
{
    Out << expr.Function;
    Out << "(";
    for (size_t i = 0; i < expr.Args.size(); ++i) {
        TRY(EmitExpr(*expr.Args[i]));
        if (i != expr.Args.size() - 1)
            Out << ", ";
    }
    Out << ")";
    return Error::Ok;
}
