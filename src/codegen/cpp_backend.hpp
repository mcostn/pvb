#pragma once

#include "codegen/backend.hpp"

class CppEmitter : public Emitter
{
public:
    using Emitter::Emitter;

    Error EmitProgram(const Program& program) override;

    // Statements
    Error EmitStmt(const Stmt &stmt);
    Error EmitPrint(const PrintStmt &stmt);
    Error EmitExit(const ExitStmt &stmt);
    Error EmitExprStmt(const ExprStmt& stmt);
    Error EmitBlock(const BlockStmt &stmt);
    Error EmitIf(const IfStmt &stmt);
    Error EmitWhile(const WhileStmt &stmt);
    Error EmitFor(const ForStmt &stmt);
    Error EmitDeclVar(const DeclVarStmt &stmt);

    // Expressions
    Error EmitExpr(const Expr &expr);
    Error EmitLiteral(const LiteralExpr &expr);
    Error EmitVariable(const VariableExpr &expr);
    Error EmitAssign(const AssignExpr &expr);
    Error EmitUnary(const UnaryExpr &expr);
    Error EmitBinary(const BinaryExpr &expr);
    Error EmitCall(const CallExpr &expr);
};
