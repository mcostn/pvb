#pragma once

#include <cassert>
#include <functional>
#include <ostream>
#include <sstream>
#include <vector>

#include "codegen/frontend.hpp"
#include "util/error.hpp"
#include "util/macro.hpp"

class Emitter
{
public:
    explicit Emitter(std::ostream *stream) : StreamStack{stream} {}
    virtual ~Emitter() = default;

    using SourceRangeFn = std::function<void(const AstNode&, std::ostream *stream, size_t start, size_t end)>;
    SourceRangeFn OnEmitRange;

    struct StreamSplice
    {
        std::ostream *Into;
        size_t OffsetInto;
        std::ostream *From;
    };
    std::vector<StreamSplice> Splices;
    void AppendStream(std::ostringstream &sub);

    Error Emit(const Program &p) { return Visit(p); };

    Error Emit(const AstNode &node);
    Error Emit(const AstNode* node)
    {
        FAIL_COND_V(!node, Error::Failed);
        return Emit(*node);
    }

    template<typename T>
    Error Emit(const std::unique_ptr<T>& ptr)
    {
        FAIL_COND_V(!ptr, Error::Failed);
        return Emit(*ptr);
    }

    // Common Helpers
    Error EmitStatementList(const BlockStmt &block);
    Error EmitExpressionList(const std::vector<std::unique_ptr<Expr>>& exprs);
    Error EmitBinaryOperands(const BinaryExpr &expr);
    Error EmitUnaryOperand(const UnaryExpr &expr);

    // Hooks
    virtual std::string_view BinaryOperator(BinaryOp) = 0;
    virtual std::string_view UnaryOperator(UnaryOp) = 0;

    virtual std::string_view BuiltinName(Builtin) { return {}; };
    virtual void EmitBuiltinRequirements(Builtin) { };

    // Visitors
    virtual Error Visit(const Program&) = 0;

    virtual Error Visit(const LiteralExpr&) = 0;
    virtual Error Visit(const VariableExpr&) = 0;
    virtual Error Visit(const UnaryExpr&) = 0;
    virtual Error Visit(const BinaryExpr&) = 0;
    virtual Error Visit(const AssignExpr&) = 0;
    virtual Error Visit(const CallExpr&) = 0;

    virtual Error Visit(const PrintStmt&) = 0;
    virtual Error Visit(const ReadStmt&) = 0;
    virtual Error Visit(const ExitStmt&) = 0;
    virtual Error Visit(const ExprStmt&) = 0;
    virtual Error Visit(const BlockStmt&) = 0;
    virtual Error Visit(const FunctionStmt&) = 0;
    virtual Error Visit(const IfStmt&) = 0;
    virtual Error Visit(const ForStmt&) = 0;
    virtual Error Visit(const WhileStmt&) = 0;
    virtual Error Visit(const LoopStmt&) = 0;
    virtual Error Visit(const DeclVarStmt&) = 0;

    // Util
    void Indent();
    int IndentLevel = 0;

    std::vector<std::ostream*> StreamStack;
    std::ostream &Out() { return *StreamStack.back(); }
    void PushOut(std::ostream* out);
    void PopOut();

private:
    std::vector<const AstNode*> NodeStack;

    struct PendingPushRange
    {
        const AstNode *Node;
        std::ostream *Stream;
        std::streampos Start;
    };
    std::vector<PendingPushRange> PendingPushRanges;
};
