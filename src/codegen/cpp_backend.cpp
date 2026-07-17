#include <algorithm>

#include "codegen/cpp_backend.hpp"
#include "util/logger.hpp"

static std::string_view ValueToCppType(Value val)
{
    if (val == VAL_NONE) return "void";
    if (val == VAL_ANY) return "auto";
    if (val == VAL_NUMBER) return "float";

    if (val & VAL_INT)         return "int";
    else if (val & VAL_FLOAT)  return "float";
    else if (val & VAL_BOOL)   return "bool";
    else if (val & VAL_STRING) return "string";

    return "void";
}

std::string_view CppEmitter::UnaryOperator(UnaryOp op)
{
    switch (op) {
        case UnaryOp::Negate: return "-";
        case UnaryOp::Not:    return "!";
    }

    return "";
}

std::string_view CppEmitter::BinaryOperator(BinaryOp op)
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

        case BinaryOp::And:          return "&&";
        case BinaryOp::Or:           return "||";
    }

    return "";
}

std::string_view CppEmitter::BuiltinName(Builtin b)
{
    switch (b)
    {
        case Builtin::Sqrt:  return "sqrt";
        case Builtin::Sin:   return "sin";
        case Builtin::Cos:   return "cos";
        case Builtin::Tan:   return "tan";
        case Builtin::Atan:  return "atan";

        case Builtin::Max:   return "max";
        case Builtin::Min:   return "min";
        case Builtin::Round: return "round";

        case Builtin::Abs:   return "abs";
        case Builtin::Floor: return "floor";
        case Builtin::Ceil:  return "ceil";

        default:
            return {};
    }
};

void CppEmitter::EmitBuiltinRequirements(Builtin b)
{
    switch (b)
    {
        case Builtin::Sqrt:
        case Builtin::Sin:
        case Builtin::Cos:
        case Builtin::Tan:
        case Builtin::Atan:
        case Builtin::Max:
        case Builtin::Min:
        case Builtin::Round:
            Context.Includes.insert("<cmath>");
            Context.Namespaces.insert("std");
            return;

        default:
            return;
    }
};

Error CppEmitter::Visit(const Program &program)
{
    PushOut(&Main);
    Out() << "int main()\n"
        << "{\n";
    IndentLevel++;
    for (const auto &stmt : program.Statements) {
        if (stmt->Kind == AstNodeKind::FunctionStmt) {
            TRY(Emit(*stmt));
            continue;
        }

        Indent();
        TRY(Emit(*stmt));
        Out() << "\n";
    }
    IndentLevel--;
    Out() << "}";
    PopOut();

    std::vector<std::string> sortedIncludes(
        Context.Includes.begin(),
        Context.Includes.end()
    );
    std::sort(sortedIncludes.begin(), sortedIncludes.end());
    for (auto &inc: sortedIncludes)
        Out() << "#include " << inc << "\n";

    for (auto &ns : Context.Namespaces)
        Out() << "using namespace " << ns << ";\n";
    if (!Context.Includes.empty() || !Context.Namespaces.empty())
        Out() << "\n";

    for (auto &func : Context.FunctionDeclarations)
        Out() << func << ";\n";
    if (!Context.FunctionDeclarations.empty())
        Out() << "\n";

    Out() << Main.str() << "\n";
    std::string funcImpl = Functions.str();
    if (!funcImpl.empty())
        Out() << "\n" << funcImpl;

    return Error::Ok;
}

Error CppEmitter::Visit(const PrintStmt &stmt)
{
    Context.Includes.insert("<iostream>");
    Context.Namespaces.insert("std");

    Out() << "cout << ";
    TRY(Emit(*stmt.Data));
    if (stmt.Newline)
        Out() << " << endl";
    Out() << ";";

    return Error::Ok;
}

Error CppEmitter::Visit(const ReadStmt &stmt)
{
    Context.Includes.insert("<iostream>");
    Context.Namespaces.insert("std");

    Out() << "cin >> "
        << stmt.Variable->Name
        << ";";

    return Error::Ok;
}

Error CppEmitter::Visit(const ExitStmt &stmt)
{
    Context.Includes.insert("<cstdlib>");
    Context.Namespaces.insert("std");

    Out() << "exit(";
    TRY(Emit(*stmt.Code));
    Out() << ");";

    return Error::Ok;
}

Error CppEmitter::Visit(const ExprStmt &stmt)
{
    TRY(Emit(*stmt.Expression));
    Out() << ";";

    return Error::Ok;
}

Error CppEmitter::Visit(const BlockStmt &stmt)
{
    Out() << "{\n";
    ++IndentLevel;
    for (const auto& s : stmt.Statements) {
        Indent();
        TRY(Emit(*s));
        Out() << "\n";
    }
    --IndentLevel;
    Indent();
    Out() << "}";

    return Error::Ok;
}

Error CppEmitter::Visit(const FunctionStmt &stmt)
{
    PushOut(&Functions);

    std::ostringstream decl;

    decl << ValueToCppType(stmt.ReturnType) << ' '
        << stmt.Name << "(";
    for (size_t i = 0; i < stmt.Params.size(); ++i) {
        const auto &param = stmt.Params[i];
        decl << ValueToCppType(param.Type)  << ' ' << param.Name;
        if (i + 1 < stmt.Params.size())
            decl << ", ";
    }
    decl << ")";

    Context.FunctionDeclarations.push_back(decl.str());

    Out() << decl.str() << "\n";
    int prevIndentLevel = IndentLevel;
    IndentLevel = 0;
    TRY(Emit(*stmt.Body));
    IndentLevel = prevIndentLevel;
    Out() << '\n';

    PopOut();
    return Error::Ok;
}

Error CppEmitter::Visit(const IfStmt &stmt)
{
    Out() << "if (";
    TRY(Emit(*stmt.Condition));
    Out() << ") ";

    TRY(Emit(*stmt.ThenBranch));
    if (stmt.ElseBranch) {
        Out() << " else ";
        TRY(Emit(*stmt.ElseBranch));
    }

    return Error::Ok;
}

Error CppEmitter::Visit(const WhileStmt &stmt)
{
    Out() << "while (";
    TRY(Emit(*stmt.Condition));
    Out() << ") ";
    TRY(Emit(*stmt.Body));

    return Error::Ok;
}

Error CppEmitter::Visit(const ForStmt &stmt)
{
    Out() << "for (";
    if (stmt.Init) {
       if (stmt.Init->Kind == AstNodeKind::ExprStmt) {
           auto &exprStmt = static_cast<const ExprStmt&>(*stmt.Init);
           TRY(Emit(*exprStmt.Expression));
       } else if (stmt.Init->Kind == AstNodeKind::DeclVarStmt) {
           auto &declStmt = static_cast<const DeclVarStmt&>(*stmt.Init);
           TRY(Emit(declStmt));
       }
       Out() << " ";
    } else {
        Out() << "; ";
    }

    if (stmt.Condition)
        TRY(Emit(*stmt.Condition));
    Out() << "; ";

    if (stmt.Update)
        TRY(Emit(*stmt.Update));
    Out() << ") ";

    TRY(Emit(*stmt.Body));

    return Error::Ok;
}

Error CppEmitter::Visit(const DeclVarStmt &stmt)
{
    Out() << ValueToCppType(stmt.Type) << ' ';
    Out() << stmt.Name;
    if (stmt.Initializer) {
        Out() << " = ";
        TRY(Emit(*stmt.Initializer));
    }
    Out() << ";";

    return Error::Ok;
}

Error CppEmitter::Visit(const LiteralExpr &expr)
{
    std::visit([this](auto&& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, bool>) {
            Out() << (value ? "true" : "false");
        } else if constexpr (std::is_same_v<T, std::string>) {
            Out() << '"' << value << '"';
        } else {
            Out() << value;
        }
    }, expr.Data);

    return Error::Ok;
}

Error CppEmitter::Visit(const VariableExpr &expr)
{
    Out() << expr.Name;
    return Error::Ok;
}

Error CppEmitter::Visit(const AssignExpr &expr)
{
    Out() << expr.Name;
    Out() << " = ";
    TRY(Emit(*expr.ValueExpr));
    return Error::Ok;
}

Error CppEmitter::Visit(const UnaryExpr &expr)
{
    return EmitUnaryOperand(expr);
}

Error CppEmitter::Visit(const BinaryExpr &expr)
{
    Out() << "(";
    TRY(EmitBinaryOperands(expr));
    Out() << ")";
    return Error::Ok;
}

Error CppEmitter::Visit(const CallExpr &expr)
{
    if (expr.BuiltinKind != Builtin::None) {
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
