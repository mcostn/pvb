#pragma once

#include <cassert>
#include "codegen/frontend.hpp"
#include "util/error.hpp"

class Emitter
{
public:
    explicit Emitter(std::ostream *stream) : StreamStack{stream} {}
    virtual ~Emitter() = default;

    Error Emit(const Program &program) { return EmitProgram(program); };
    virtual Error EmitProgram(const Program &program) = 0;

    // Statements
    Error EmitStmt(const Stmt &stmt);
    virtual Error EmitPrint(const PrintStmt &stmt) = 0;
    virtual Error EmitExit(const ExitStmt &stmt) = 0;
    virtual Error EmitExprStmt(const ExprStmt& stmt) = 0;
    virtual Error EmitBlock(const BlockStmt &stmt) = 0;
    virtual Error EmitFunction(const FunctionStmt &stmt) = 0;
    virtual Error EmitIf(const IfStmt &stmt) = 0;
    virtual Error EmitWhile(const WhileStmt &stmt) = 0;
    virtual Error EmitFor(const ForStmt &stmt) = 0;
    virtual Error EmitDeclVar(const DeclVarStmt &stmt) = 0;

    // Expressions
    Error EmitExpr(const Expr &expr);
    virtual Error EmitLiteral(const LiteralExpr &expr) = 0;
    virtual Error EmitVariable(const VariableExpr &expr) = 0;
    virtual Error EmitAssign(const AssignExpr &expr) = 0;
    virtual Error EmitUnary(const UnaryExpr &expr) = 0;
    virtual Error EmitBinary(const BinaryExpr &expr) = 0;
    virtual Error EmitCall(const CallExpr &expr) = 0;

    void Indent();
    int IndentLevel = 0;

    std::vector<std::ostream*> StreamStack;
    std::ostream &Out() { return *StreamStack.back(); }
    void PushOut(std::ostream* out) { StreamStack.push_back(out); }
    void PopOut() { assert(StreamStack.size() > 1); StreamStack.pop_back(); }
};
