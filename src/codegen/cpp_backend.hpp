#pragma once

#include <string>
#include <unordered_set>
#include <sstream>
#include "codegen/backend.hpp"

class CppContext
{
    public:
        CppContext() = default;
        std::unordered_set<std::string> Includes {};
        std::unordered_set<std::string> Namespaces {};
        std::vector<std::string> FunctionDeclarations;
};
class CppEmitter : public Emitter
{
public:
    using Emitter::Emitter;

    Error EmitProgram(const Program& program) override;
    Error EmitMain(const Program &program);

    // Statements
    Error EmitStmt(const Stmt &stmt, std::ostream &out);
    Error EmitPrint(const PrintStmt &stmt, std::ostream &out);
    Error EmitExit(const ExitStmt &stmt, std::ostream &out);
    Error EmitExprStmt(const ExprStmt& stmt, std::ostream &out);
    Error EmitBlock(const BlockStmt &stmt, std::ostream &out);
    Error EmitFunction(const FunctionStmt &stmt, std::ostream &out);
    Error EmitIf(const IfStmt &stmt, std::ostream &out);
    Error EmitWhile(const WhileStmt &stmt, std::ostream &out);
    Error EmitFor(const ForStmt &stmt, std::ostream &out);
    Error EmitDeclVar(const DeclVarStmt &stmt, std::ostream &out);

    // Expressions
    Error EmitExpr(const Expr &expr, std::ostream &out);
    Error EmitLiteral(const LiteralExpr &expr, std::ostream &out);
    Error EmitVariable(const VariableExpr &expr, std::ostream &out);
    Error EmitAssign(const AssignExpr &expr, std::ostream &out);
    Error EmitUnary(const UnaryExpr &expr, std::ostream &out);
    Error EmitBinary(const BinaryExpr &expr, std::ostream &out);
    Error EmitCall(const CallExpr &expr, std::ostream &out);

    CppContext Context;
    std::ostringstream Main;
    std::ostringstream Functions;
};
