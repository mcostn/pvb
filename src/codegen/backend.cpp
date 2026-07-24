#include "codegen/backend.hpp"

Error Emitter::Emit(const AstNode &node)
{
    std::ostream *streamBefore = &Out();
    std::streampos startPos = streamBefore->tellp();

    NodeStack.push_back(&node);

    Error err = Error::Unreachable;
    switch (node.Kind) {
        case AstNodeKind::Program:
            err = Visit(static_cast<const Program&>(node)); break;

        case AstNodeKind::LiteralExpr:
            err = Visit(static_cast<const LiteralExpr&>(node)); break;

        case AstNodeKind::VariableExpr:
            err = Visit(static_cast<const VariableExpr&>(node)); break;

        case AstNodeKind::AssignExpr:
            err = Visit(static_cast<const AssignExpr&>(node)); break;

        case AstNodeKind::UnaryExpr:
            err = Visit(static_cast<const UnaryExpr&>(node)); break;

        case AstNodeKind::BinaryExpr:
            err = Visit(static_cast<const BinaryExpr&>(node)); break;

        case AstNodeKind::CallExpr:
            err = Visit(static_cast<const CallExpr&>(node)); break;

        case AstNodeKind::PrintStmt:
            err = Visit(static_cast<const PrintStmt&>(node)); break;

        case AstNodeKind::ReadStmt:
            err = Visit(static_cast<const ReadStmt&>(node)); break;

        case AstNodeKind::ExitStmt:
            err = Visit(static_cast<const ExitStmt&>(node)); break;

        case AstNodeKind::ExprStmt:
            err = Visit(static_cast<const ExprStmt&>(node)); break;

        case AstNodeKind::BlockStmt:
            err = Visit(static_cast<const BlockStmt&>(node)); break;

        case AstNodeKind::FunctionStmt:
            err = Visit(static_cast<const FunctionStmt&>(node)); break;

        case AstNodeKind::IfStmt:
            err = Visit(static_cast<const IfStmt&>(node)); break;

        case AstNodeKind::WhileStmt:
            err = Visit(static_cast<const WhileStmt&>(node)); break;

        case AstNodeKind::ForStmt:
            err = Visit(static_cast<const ForStmt&>(node)); break;

        case AstNodeKind::LoopStmt:
            err = Visit(static_cast<const LoopStmt&>(node)); break;

        case AstNodeKind::DeclVarStmt:
            err = Visit(static_cast<const DeclVarStmt&>(node)); break;
    }

    NodeStack.pop_back();

    std::ostream *streamAfter = &Out();
    if (OnEmitRange && streamBefore == streamAfter) {
        std::streampos endPos = streamAfter->tellp();
        if (endPos != startPos)
            OnEmitRange(node, streamAfter, (size_t)startPos, (size_t)endPos);
    }

    return err;
}

void Emitter::PushOut(std::ostream *out)
{
    StreamStack.push_back(out);

    if (!NodeStack.empty())
        PendingPushRanges.push_back({ NodeStack.back(), out, out->tellp() });
}

void Emitter::PopOut()
{
    assert(StreamStack.size() > 1);
    std::ostream *out = StreamStack.back();
    StreamStack.pop_back();

    if (!PendingPushRanges.empty() && PendingPushRanges.back().Stream == out) {
        PendingPushRange pending = PendingPushRanges.back();
        PendingPushRanges.pop_back();

        if (OnEmitRange) {
            std::streampos endPos = out->tellp();
            if (endPos != pending.Start)
                OnEmitRange(*pending.Node, out, (size_t)pending.Start, (size_t)endPos);
        }
    }
}

void Emitter::AppendStream(std::ostringstream &sub)
{
    size_t offset = (size_t)Out().tellp();
    Splices.push_back({ &Out(), offset, &sub });
    Out() << sub.str();
}

Error Emitter::EmitStatementList(const BlockStmt& block)
{
    for (size_t i = 0; i < block.Statements.size(); ++i) {
        Indent();
        TRY(Emit(*block.Statements[i]));
        if (i + 1 < block.Statements.size())
            Out() << '\n';
    }

    return Error::Ok;
}

void Emitter::Indent()
{
    for (int i = 0; i < IndentLevel; ++i)
        Out() << "    ";
}

Error Emitter::EmitExpressionList(const std::vector<std::unique_ptr<Expr>>& exprs)
{
    for (size_t i = 0; i < exprs.size(); ++i) {
        TRY(Emit(exprs[i]));
        if (i + 1 < exprs.size())
            Out() << ", ";
    }

    return Error::Ok;
}

Error Emitter::EmitBinaryOperands(const BinaryExpr& expr)
{
    TRY(Emit(expr.Left));
    Out() << " " << BinaryOperator(expr.Op) << " ";
    TRY(Emit(expr.Right));

    return Error::Ok;
}

Error Emitter::EmitUnaryOperand(const UnaryExpr& expr)
{
    Out() << UnaryOperator(expr.Op);
    Out() << "(";
    TRY(Emit(expr.Data));
    Out() << ")";
    return Error::Ok;
}
