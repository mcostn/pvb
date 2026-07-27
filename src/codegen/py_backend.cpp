#include <algorithm>
#include "codegen/py_backend.hpp"

std::string_view PythonEmitter::UnaryOperator(UnaryOp op)
{
    switch (op) {
        case UnaryOp::Negate: return "-";
        case UnaryOp::Not:    return "not ";
    }

    return "";
}

std::string_view PythonEmitter::BinaryOperator(BinaryOp op)
{
    switch (op) {
        case BinaryOp::Add:          return "+";
        case BinaryOp::Sub:          return "-";
        case BinaryOp::Mul:          return "*";
        case BinaryOp::Div:          return "/";
        case BinaryOp::Mod:          return "%";

        case BinaryOp::Less:         return "<";
        case BinaryOp::Greater:      return ">";
        case BinaryOp::LessEqual:    return "<=";
        case BinaryOp::GreaterEqual: return ">=";

        case BinaryOp::Equal:        return "==";
        case BinaryOp::NotEqual:     return "!=";

        case BinaryOp::And:          return "and";
        case BinaryOp::Or:           return "or";
    }

    return "";
}

std::string_view PythonEmitter::BuiltinName(Builtin b)
{
    switch (b)
    {
        case Builtin::Sqrt:  return "math.sqrt";
        case Builtin::Sin:   return "math.sin";
        case Builtin::Cos:   return "math.cos";
        case Builtin::Tan:   return "math.tan";
        case Builtin::Atan:  return "math.atan";

        case Builtin::Max:   return "max";
        case Builtin::Min:   return "min";
        case Builtin::Round: return "round";

        case Builtin::Abs:   return "abs";
        case Builtin::Floor: return "math.floor";
        case Builtin::Ceil:  return "math.ceil";

        case Builtin::RandomRange: return "random.randint";

        default:
            return {};
    }
};

void PythonEmitter::EmitBuiltinRequirements(Builtin b)
{
    switch (b)
    {
        case Builtin::Sqrt:
        case Builtin::Sin:
        case Builtin::Cos:
        case Builtin::Tan:
        case Builtin::Atan:
        case Builtin::Floor:
        case Builtin::Ceil:
            Context.Imports.insert("math");
            return;

        case Builtin::RandomRange:
            Context.Imports.insert("random");
            return;

        default:
            return;
    }
}

Error PythonEmitter::Visit(const Program &program)
{
    PushOut(&Main);
    for (const auto& stmt : program.Statements) {
        if (stmt->Kind == AstNodeKind::FunctionStmt) {
            TRY(Emit(*stmt));
            continue;
        }

        if (stmt->Kind == AstNodeKind::DeclVarStmt) {
            auto *decl = static_cast<DeclVarStmt*>(stmt.get());
            if (decl->Scope == VarScope::Global) {
                TRY(Emit(*stmt));
                continue;
            }
        }

        Indent();
        TRY(Emit(stmt));
        Out() << "\n";
    }
    PopOut();

    std::vector<std::string> sortedImports(
        Context.Imports.begin(),
        Context.Imports.end()
    );
    std::sort(sortedImports.begin(), sortedImports.end());
    for (const auto& import : sortedImports)
        Out() << "import " << import << "\n";
    if (!sortedImports.empty())
        Out() << "\n";

    if (GlobalVars.str().size() > 0) {
        AppendStream(GlobalVars);
        Out() << "\n";
    }

    if (Functions.str().size() > 0) {
        AppendStream(Functions);
        Out() << "\n";
    }

    AppendStream(Main);

    return Error::Ok;
}

Error PythonEmitter::Visit(const PrintStmt &stmt)
{
    Out() << "print(";
    TRY(Emit(stmt.Data));
    if (!stmt.Newline)
        Out() << ", end=''";
    Out() << ")";
    return Error::Ok;
}

Error PythonEmitter::Visit(const ReadStmt &stmt)
{
    Out() << stmt.Variable->Name << " = ";

    Value type = stmt.Variable->Type;
    if (type == VAL_ANY) {
        Out() << "input()";
    } else if (type & VAL_INT) {
        Out() << "int(input())";
    } else if (type & VAL_FLOAT) {
        Out() << "float(input())";
    } else if (type & VAL_BOOL) {
        Out() << "input().lower() == \"true\"";
    } else if (type & VAL_STRING) {
        Out() << "input()";
    } else {
        return Error::Failed;
    }

    return Error::Ok;
}

Error PythonEmitter::Visit(const ExitStmt &stmt)
{
    Context.Imports.insert("sys");

    Out() << "sys.exit(";
    TRY(Emit(*stmt.Code));
    Out() << ")";
    return Error::Ok;
}

Error PythonEmitter::Visit(const ExprStmt &stmt)
{
    TRY(Emit(stmt.Expression));
    return Error::Ok;
}

Error PythonEmitter::Visit(const BlockStmt &stmt)
{
    Out() << ":\n";
    ++IndentLevel;
    if (stmt.Statements.size() == 0) {
        Indent();
        Out() << "...";
    } else {
        TRY(EmitStatementList(stmt));
    }
    --IndentLevel;

    return Error::Ok;
}

Error PythonEmitter::Visit(const FunctionStmt &stmt)
{
    PushOut(&Functions);

    Indent();
    Out() << "def " << stmt.Name << "(";

    for (size_t i = 0; i < stmt.Params.size(); ++i) {
        Out() << stmt.Params[i].Name;
        if (i + 1 < stmt.Params.size())
            Out() << ", ";
    }

    Out() << ")";

    TRY(Emit(stmt.Body));
    Out() << "\n";

    PopOut();

    return Error::Ok;
}

Error PythonEmitter::Visit(const IfStmt &stmt)
{
    Out() << "if ";
    TRY(Emit(*stmt.Condition));

    if (stmt.ThenBranch->Kind != AstNodeKind::BlockStmt)
        Out() << ": ";
    TRY(Emit(stmt.ThenBranch));

    if (stmt.ElseBranch) {
        Out() << "\n";
        Indent();
        Out() << "else";
        TRY(Emit(stmt.ElseBranch));
    }

    return Error::Ok;
}

Error PythonEmitter::Visit(const WhileStmt &stmt)
{
    Out() << "while ";
    TRY(Emit(*stmt.Condition));
    if (stmt.Body->Kind != AstNodeKind::BlockStmt)
        Out() << ": ";

    TRY(Emit(*stmt.Body));
    return Error::Ok;
}

Error PythonEmitter::Visit(const ForStmt &stmt)
{
    if (stmt.Init) {
        Indent();
        TRY(Emit(*stmt.Init));
        Out() << "\n";
    }

    Indent();
    Out() << "while ";

    if (stmt.Condition) TRY(Emit(*stmt.Condition));
    else Out() << "True";

    if (stmt.Body) {
        if (stmt.Body->Kind != AstNodeKind::BlockStmt) {
            ++IndentLevel;
            Out() << ": ";
        }

        TRY(Emit(*stmt.Body));
        if (stmt.Body->Kind != AstNodeKind::BlockStmt) {
            Out() << "\n";
        }

        if (stmt.Update) {
            Out() << "\n";
            ++IndentLevel;
            Indent();
            TRY(Emit(*stmt.Update));
            --IndentLevel;
        }
    }

    return Error::Ok;
}

Error PythonEmitter::Visit(const LoopStmt &stmt)
{
    switch (stmt.LoopKind) {
        case LoopStmtKind::Continue:
            Out() << "continue";
            break;
        case LoopStmtKind::Break:
            Out() << "break";
            break;
    }

    return Error::Ok;
}

Error PythonEmitter::Visit(const DeclVarStmt &stmt)
{
    if (stmt.Scope == VarScope::Global) PushOut(&GlobalVars);

    Out() << stmt.Name;

    if (stmt.Initializer) {
        Out() << " = ";
        TRY(Emit(*stmt.Initializer));
    } else {
        Out() << " = None";
    }

    if (stmt.Scope == VarScope::Global) {
        Out() << "\n";
        PopOut();
    }
    return Error::Ok;
}

Error PythonEmitter::Visit(const LiteralExpr &expr)
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

Error PythonEmitter::Visit(const VariableExpr &expr)
{
    Out() << expr.Name;
    return Error::Ok;
}

Error PythonEmitter::Visit(const AssignExpr &expr)
{
    Out() << expr.Name;
    Out() << " = ";
    TRY(Emit(*expr.ValueExpr));
    return Error::Ok;
}

Error PythonEmitter::Visit(const UnaryExpr &expr)
{
    return EmitUnaryOperand(expr);
}

Error PythonEmitter::Visit(const BinaryExpr &expr)
{
    Out() << "(";
    TRY(EmitBinaryOperands(expr));
    Out() << ")";
    return Error::Ok;
}

Error PythonEmitter::Visit(const CallExpr &expr)
{
    if (expr.BuiltinKind != Builtin::None) {
        switch (expr.BuiltinKind) {
            case Builtin::Length:
                Out() << "len(";
                TRY(Emit(*expr.Args[0]));
                Out() << ")";
                return Error::Ok;

            case Builtin::CharAt:
                TRY(Emit(*expr.Args[0]));
                Out() << "[";
                TRY(Emit(*expr.Args[1]));
                Out() << "]";
                return Error::Ok;

            case Builtin::Join:
                Out() << "(";
                TRY(Emit(*expr.Args[0]));
                Out() << " + ";
                TRY(Emit(*expr.Args[1]));
                Out() << ")";
                return Error::Ok;

            case Builtin::Contains:
                Out() << "(";
                TRY(Emit(*expr.Args[1]));
                Out() << " in ";
                TRY(Emit(*expr.Args[0]));
                Out() << ")";
                return Error::Ok;

            default:
                break;
        }

        EmitBuiltinRequirements(expr.BuiltinKind);
        Out() << BuiltinName(expr.BuiltinKind);
    } else {
        Out() << expr.Function;
    }

    Out() << "(";
    TRY(EmitExpressionList(expr.Args));
    Out() << ")";

    return Error::Ok;
}
