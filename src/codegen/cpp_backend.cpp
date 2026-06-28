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

Error CppEmitter::EmitProgram(const Program &program)
{
    TRY(EmitMain(program));

    std::vector<std::string> sortedIncludes(
        Context.Includes.begin(),
        Context.Includes.end()
    );
    std::sort(sortedIncludes.begin(), sortedIncludes.end());
    for (auto &inc: sortedIncludes)
        Out << "#include " << inc << "\n";

    for (auto &ns : Context.Namespaces)
        Out << "using namespace " << ns << ";\n";
    if (!Context.Includes.empty() || !Context.Namespaces.empty())
        Out << "\n";

    for (auto &func : Context.FunctionDeclarations)
        Out << func << ";\n";
    if (!Context.FunctionDeclarations.empty())
        Out << "\n";

    Out << Main.str() << "\n";

    std::string funcImpl = Functions.str();
    if (!funcImpl.empty())
        Out << "\n" << Functions.str();

    return Error::Ok;
}

Error CppEmitter::EmitMain(const Program &program)
{
    Main << "int main()\n"
         << "{\n";
    IndentLevel++;
    for (const auto &stmt : program.Statements) {
        if (stmt->Kind == AstNodeKind::FunctionStmt) {
            TRY(EmitStmt(*stmt, Main));
            continue;
        }

        Indent(Main);
        TRY(EmitStmt(*stmt, Main));
        Main << "\n";
    }
    IndentLevel--;
    Main << "}";

    return Error::Ok;
}

Error CppEmitter::EmitStmt(const Stmt &stmt, std::ostream &out)
{
    switch (stmt.Kind) {
        case AstNodeKind::PrintStmt:
            return EmitPrint(static_cast<const PrintStmt&>(stmt), out);

        case AstNodeKind::ExitStmt:
            return EmitExit(static_cast<const ExitStmt&>(stmt), out);

        case AstNodeKind::ExprStmt:
            return EmitExprStmt(static_cast<const ExprStmt&>(stmt), out);

        case AstNodeKind::BlockStmt:
            return EmitBlock(static_cast<const BlockStmt&>(stmt), out);

        case AstNodeKind::FunctionStmt:
            return EmitFunction(static_cast<const FunctionStmt&>(stmt), Functions);

        case AstNodeKind::IfStmt:
            return EmitIf(static_cast<const IfStmt&>(stmt), out);

        case AstNodeKind::WhileStmt:
            return EmitWhile(static_cast<const WhileStmt&>(stmt), out);

        case AstNodeKind::ForStmt:
            return EmitFor(static_cast<const ForStmt&>(stmt), out);

        case AstNodeKind::DeclVarStmt:
            return EmitDeclVar(static_cast<const DeclVarStmt&>(stmt), out);

        default:
            GlobalLogger.Error("Unexepected statement kind");
            return Error::Failed;
    }
}

Error CppEmitter::EmitPrint(const PrintStmt &stmt, std::ostream &out)
{
    Context.Includes.insert("<iostream>");
    Context.Namespaces.insert("std");

    out << "cout << ";
    TRY(EmitExpr(*stmt.Data, out));
    if (stmt.Newline)
        out << " << endl";
    out << ";";

    return Error::Ok;
}

Error CppEmitter::EmitExit(const ExitStmt &stmt, std::ostream &out)
{
    Context.Includes.insert("<cstdlib>");
    Context.Namespaces.insert("std");

    out << "exit(";
    TRY(EmitExpr(*stmt.Code, out));
    out << ");";

    return Error::Ok;
}

Error CppEmitter::EmitExprStmt(const ExprStmt &stmt, std::ostream &out)
{
    TRY(EmitExpr(*stmt.Expression, out));
    out << ";";

    return Error::Ok;
}

Error CppEmitter::EmitBlock(const BlockStmt &stmt, std::ostream &out)
{
    out << "{\n";
    ++IndentLevel;
    for (const auto& s : stmt.Statements) {
        Indent(out);
        TRY(EmitStmt(*s, out));
        out << "\n";
    }
    --IndentLevel;
    Indent(out);
    out << "}";

    return Error::Ok;
}

Error CppEmitter::EmitFunction(const FunctionStmt &stmt, std::ostream &out)
{
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

    out << decl.str() << "\n";
    int prevIndentLevel = IndentLevel;
    IndentLevel = 0;
    TRY(EmitBlock(*stmt.Body, out));
    IndentLevel = prevIndentLevel;
    out << '\n';

    return Error::Ok;
}

Error CppEmitter::EmitIf(const IfStmt &stmt, std::ostream &out)
{
    out << "if (";
    TRY(EmitExpr(*stmt.Condition, out));
    out << ") ";

    TRY(EmitStmt(*stmt.ThenBranch, out));
    if (stmt.ElseBranch) {
        out << " else ";
        TRY(EmitStmt(*stmt.ElseBranch, out));
    }

    return Error::Ok;
}

Error CppEmitter::EmitWhile(const WhileStmt &stmt, std::ostream &out)
{
    out << "while (";
    TRY(EmitExpr(*stmt.Condition, out));
    out << ") ";
    TRY(EmitStmt(*stmt.Body, out));

    return Error::Ok;
}

Error CppEmitter::EmitFor(const ForStmt &stmt, std::ostream &out)
{
    out << "for (";
    if (stmt.Init) {
       if (stmt.Init->Kind == AstNodeKind::ExprStmt) {
           auto &exprStmt = static_cast<const ExprStmt&>(*stmt.Init);
           TRY(EmitExpr(*exprStmt.Expression, out));
       } else if (stmt.Init->Kind == AstNodeKind::DeclVarStmt) {
           auto &declStmt = static_cast<const DeclVarStmt&>(*stmt.Init);
           TRY(EmitDeclVar(declStmt, out));
       }
       out << " ";
    } else {
        out << "; ";
    }

    if (stmt.Condition)
        TRY(EmitExpr(*stmt.Condition, out));
    out << "; ";

    if (stmt.Update)
        TRY(EmitExpr(*stmt.Update, out));
    out << ") ";

    TRY(EmitStmt(*stmt.Body, out));

    return Error::Ok;
}

Error CppEmitter::EmitDeclVar(const DeclVarStmt &stmt, std::ostream &out)
{
    out << ValueToCppType(stmt.Type) << ' ';
    out << stmt.Name;
    if (stmt.Initializer) {
        out << " = ";
        TRY(EmitExpr(*stmt.Initializer, out));
    }
    out << ";";

    return Error::Ok;
}

Error CppEmitter::EmitExpr(const Expr &expr, std::ostream &out)
{
    switch (expr.Kind) {
        case AstNodeKind::LiteralExpr:
            return EmitLiteral(static_cast<const LiteralExpr&>(expr), out);

        case AstNodeKind::VariableExpr:
            return EmitVariable(static_cast<const VariableExpr&>(expr), out);

        case AstNodeKind::AssignExpr:
            return EmitAssign(static_cast<const AssignExpr&>(expr), out);

        case AstNodeKind::UnaryExpr:
            return EmitUnary(static_cast<const UnaryExpr&>(expr), out);

        case AstNodeKind::BinaryExpr:
            return EmitBinary(static_cast<const BinaryExpr&>(expr), out);

        case AstNodeKind::CallExpr:
            return EmitCall(static_cast<const CallExpr&>(expr), out);

        default:
            GlobalLogger.Error("Unexepected expression kind");
            return Error::Failed;
    }
}

Error CppEmitter::EmitLiteral(const LiteralExpr &expr, std::ostream &out)
{
    std::visit([this, &out](auto&& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, bool>) {
            out << (value ? "true" : "false");
        } else if constexpr (std::is_same_v<T, std::string>) {
            out << '"' << value << '"';
        } else {
            out << value;
        }
    }, expr.Data);

    return Error::Ok;
}

Error CppEmitter::EmitVariable(const VariableExpr &expr, std::ostream &out)
{
    out << expr.Name;
    return Error::Ok;
}

Error CppEmitter::EmitAssign(const AssignExpr &expr, std::ostream &out)
{
    out << expr.Name;
    out << " = ";
    TRY(EmitExpr(*expr.ValueExpr, out));
    return Error::Ok;
}

Error CppEmitter::EmitUnary(const UnaryExpr &expr, std::ostream &out)
{
    #define EMIT_UNARY_SYMBOL(Name, Op, Type, Sym) case UnaryOp::Op: out << Sym; break;
    switch (expr.Op) {
        UNARY_OP_LIST(EMIT_UNARY_SYMBOL)
    }
    #undef EMIT_UNARY_SYMBOL

    out << "(";
    TRY(EmitExpr(*expr.Data, out));
    out << ")";
    return Error::Ok;
}

Error CppEmitter::EmitBinary(const BinaryExpr &expr, std::ostream &out)
{
    std::string_view opStr;
    #define EMIT_BINARY_SYMBOL(Name, Op, Type, Sym) case BinaryOp::Op: opStr = Sym; break;
    switch(expr.Op) {
        BINARY_OP_LIST(EMIT_BINARY_SYMBOL)
    }
    #undef EMIT_BINARY_SYMBOL

    out << "(";
    TRY(EmitExpr(*expr.Left, out));
    out << " " << opStr << " ";
    TRY(EmitExpr(*expr.Right, out));
    out << ")";

    return Error::Ok;
}

Error CppEmitter::EmitCall(const CallExpr &expr, std::ostream &out)
{
    if (expr.Function == "max"
        || expr.Function == "min"
        || expr.Function == "sqrt"
        || expr.Function == "round"
        || expr.Function == "sin"
        || expr.Function == "cos"
        || expr.Function == "tan"
        || expr.Function == "atan")
        Context.Includes.insert("<cmath>");

    out << expr.Function;
    out << "(";
    for (size_t i = 0; i < expr.Args.size(); ++i) {
        TRY(EmitExpr(*expr.Args[i], out));
        if (i != expr.Args.size() - 1)
            out << ", ";
    }
    out << ")";
    return Error::Ok;
}
