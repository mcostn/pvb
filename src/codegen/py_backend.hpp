#pragma once

#include <sstream>
#include <string>
#include "codegen/backend.hpp"
#include "util/logger.hpp"

struct PythonContext
{
    std::unordered_set<std::string> Imports {};
};

class PythonEmitter : public Emitter
{
public:
    using Emitter::Emitter;

    // Operator lookup
    std::string_view BinaryOperator(BinaryOp op) override;
    std::string_view UnaryOperator(UnaryOp op) override;

    std::string_view BuiltinName(Builtin b) override;
    void EmitBuiltinRequirements(Builtin b) override;

    // Visitors
    Error Visit(const Program&) override;

    Error Visit(const LiteralExpr&) override;
    Error Visit(const VariableExpr&) override;
    Error Visit(const AssignExpr&) override;
    Error Visit(const UnaryExpr&) override;
    Error Visit(const BinaryExpr&) override;
    Error Visit(const CallExpr&) override;

    Error Visit(const PrintStmt&) override;
    Error Visit(const ReadStmt&) override;
    Error Visit(const ExitStmt&) override;
    Error Visit(const ExprStmt&) override;
    Error Visit(const BlockStmt&) override;
    Error Visit(const FunctionStmt&) override;
    Error Visit(const IfStmt&) override;
    Error Visit(const WhileStmt&) override;
    Error Visit(const ForStmt&) override;
    Error Visit(const LoopStmt&) override;
    Error Visit(const DeclVarStmt&) override;

    PythonContext Context;
    std::ostringstream Main;
    std::ostringstream Functions;
};
