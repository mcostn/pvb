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
    PushOut(&Main);
    Out() << "int main()\n"
        << "{\n";
    IndentLevel++;
    for (const auto &stmt : program.Statements) {
        if (stmt->Kind == AstNodeKind::FunctionStmt) {
            TRY(EmitStmt(*stmt));
            continue;
        }

        Indent();
        TRY(EmitStmt(*stmt));
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

Error CppEmitter::EmitPrint(const PrintStmt &stmt)
{
    Context.Includes.insert("<iostream>");
    Context.Namespaces.insert("std");

    Out() << "cout << ";
    TRY(EmitExpr(*stmt.Data));
    if (stmt.Newline)
        Out() << " << endl";
    Out() << ";";

    return Error::Ok;
}

Error CppEmitter::EmitExit(const ExitStmt &stmt)
{
    Context.Includes.insert("<cstdlib>");
    Context.Namespaces.insert("std");

    Out() << "exit(";
    TRY(EmitExpr(*stmt.Code));
    Out() << ");";

    return Error::Ok;
}

Error CppEmitter::EmitExprStmt(const ExprStmt &stmt)
{
    TRY(EmitExpr(*stmt.Expression));
    Out() << ";";

    return Error::Ok;
}

Error CppEmitter::EmitBlock(const BlockStmt &stmt)
{
    Out() << "{\n";
    ++IndentLevel;
    for (const auto& s : stmt.Statements) {
        Indent();
        TRY(EmitStmt(*s));
        Out() << "\n";
    }
    --IndentLevel;
    Indent();
    Out() << "}";

    return Error::Ok;
}

Error CppEmitter::EmitFunction(const FunctionStmt &stmt)
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
    TRY(EmitBlock(*stmt.Body));
    IndentLevel = prevIndentLevel;
    Out() << '\n';

    PopOut();
    return Error::Ok;
}

Error CppEmitter::EmitIf(const IfStmt &stmt)
{
    Out() << "if (";
    TRY(EmitExpr(*stmt.Condition));
    Out() << ") ";

    TRY(EmitStmt(*stmt.ThenBranch));
    if (stmt.ElseBranch) {
        Out() << " else ";
        TRY(EmitStmt(*stmt.ElseBranch));
    }

    return Error::Ok;
}

Error CppEmitter::EmitWhile(const WhileStmt &stmt)
{
    Out() << "while (";
    TRY(EmitExpr(*stmt.Condition));
    Out() << ") ";
    TRY(EmitStmt(*stmt.Body));

    return Error::Ok;
}

Error CppEmitter::EmitFor(const ForStmt &stmt)
{
    Out() << "for (";
    if (stmt.Init) {
       if (stmt.Init->Kind == AstNodeKind::ExprStmt) {
           auto &exprStmt = static_cast<const ExprStmt&>(*stmt.Init);
           TRY(EmitExpr(*exprStmt.Expression));
       } else if (stmt.Init->Kind == AstNodeKind::DeclVarStmt) {
           auto &declStmt = static_cast<const DeclVarStmt&>(*stmt.Init);
           TRY(EmitDeclVar(declStmt));
       }
       Out() << " ";
    } else {
        Out() << "; ";
    }

    if (stmt.Condition)
        TRY(EmitExpr(*stmt.Condition));
    Out() << "; ";

    if (stmt.Update)
        TRY(EmitExpr(*stmt.Update));
    Out() << ") ";

    TRY(EmitStmt(*stmt.Body));

    return Error::Ok;
}

Error CppEmitter::EmitDeclVar(const DeclVarStmt &stmt)
{
    Out() << ValueToCppType(stmt.Type) << ' ';
    Out() << stmt.Name;
    if (stmt.Initializer) {
        Out() << " = ";
        TRY(EmitExpr(*stmt.Initializer));
    }
    Out() << ";";

    return Error::Ok;
}

Error CppEmitter::EmitLiteral(const LiteralExpr &expr)
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

Error CppEmitter::EmitVariable(const VariableExpr &expr)
{
    Out() << expr.Name;
    return Error::Ok;
}

Error CppEmitter::EmitAssign(const AssignExpr &expr)
{
    Out() << expr.Name;
    Out() << " = ";
    TRY(EmitExpr(*expr.ValueExpr));
    return Error::Ok;
}

Error CppEmitter::EmitUnary(const UnaryExpr &expr)
{
    #define EMIT_UNARY_SYMBOL(Name, Op, Type, Sym) case UnaryOp::Op: Out() << Sym; break;
    switch (expr.Op) {
        UNARY_OP_LIST(EMIT_UNARY_SYMBOL)
    }
    #undef EMIT_UNARY_SYMBOL

    Out() << "(";
    TRY(EmitExpr(*expr.Data));
    Out() << ")";
    return Error::Ok;
}

Error CppEmitter::EmitBinary(const BinaryExpr &expr)
{
    std::string_view opStr;
    #define EMIT_BINARY_SYMBOL(Name, Op, Type, Sym) case BinaryOp::Op: opStr = Sym; break;
    switch(expr.Op) {
        BINARY_OP_LIST(EMIT_BINARY_SYMBOL)
    }
    #undef EMIT_BINARY_SYMBOL

    Out() << "(";
    TRY(EmitExpr(*expr.Left));
    Out() << " " << opStr << " ";
    TRY(EmitExpr(*expr.Right));
    Out() << ")";

    return Error::Ok;
}

Error CppEmitter::EmitCall(const CallExpr &expr)
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

    Out() << expr.Function;
    Out() << "(";
    for (size_t i = 0; i < expr.Args.size(); ++i) {
        TRY(EmitExpr(*expr.Args[i]));
        if (i != expr.Args.size() - 1)
            Out() << ", ";
    }
    Out() << ")";
    return Error::Ok;
}
