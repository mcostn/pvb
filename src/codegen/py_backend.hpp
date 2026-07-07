#pragma once

#include <sstream>
#include <string>
#include "codegen/backend.hpp"
#include "util/logger.hpp"

class PythonEmitter : public Emitter
{
public:
    using Emitter::Emitter;

    Error EmitProgram(const Program& program) override;

    Error EmitPrint(const PrintStmt &stmt) override;
    Error EmitExit(const ExitStmt &stmt) override;
    Error EmitExprStmt(const ExprStmt& stmt) override;
    Error EmitBlock(const BlockStmt &stmt) override;
    Error EmitFunction(const FunctionStmt &stmt) override;
    Error EmitIf(const IfStmt &stmt) override;
    Error EmitWhile(const WhileStmt &stmt) override;
    Error EmitFor(const ForStmt &stmt) override;
    Error EmitDeclVar(const DeclVarStmt &stmt) override;

    Error EmitLiteral(const LiteralExpr &expr) override;
    Error EmitVariable(const VariableExpr &expr) override;
    Error EmitAssign(const AssignExpr &expr) override;
    Error EmitUnary(const UnaryExpr &expr) override;
    Error EmitBinary(const BinaryExpr &expr) override;
    Error EmitCall(const CallExpr &expr) override;
};
