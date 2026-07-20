#include "codegen/backend.hpp"

Error Emitter::Emit(const AstNode &node)
{
    switch (node.Kind) {
        case AstNodeKind::Program:
            return Visit(static_cast<const Program&>(node));

        case AstNodeKind::LiteralExpr:
            return Visit(static_cast<const LiteralExpr&>(node));

        case AstNodeKind::VariableExpr:
            return Visit(static_cast<const VariableExpr&>(node));

        case AstNodeKind::AssignExpr:
            return Visit(static_cast<const AssignExpr&>(node));

        case AstNodeKind::UnaryExpr:
            return Visit(static_cast<const UnaryExpr&>(node));

        case AstNodeKind::BinaryExpr:
            return Visit(static_cast<const BinaryExpr&>(node));

        case AstNodeKind::CallExpr:
            return Visit(static_cast<const CallExpr&>(node));

        case AstNodeKind::PrintStmt:
            return Visit(static_cast<const PrintStmt&>(node));

        case AstNodeKind::ReadStmt:
            return Visit(static_cast<const ReadStmt&>(node));

        case AstNodeKind::ExitStmt:
            return Visit(static_cast<const ExitStmt&>(node));

        case AstNodeKind::ExprStmt:
            return Visit(static_cast<const ExprStmt&>(node));

        case AstNodeKind::BlockStmt:
            return Visit(static_cast<const BlockStmt&>(node));

        case AstNodeKind::FunctionStmt:
            return Visit(static_cast<const FunctionStmt&>(node));

        case AstNodeKind::IfStmt:
            return Visit(static_cast<const IfStmt&>(node));

        case AstNodeKind::WhileStmt:
            return Visit(static_cast<const WhileStmt&>(node));

        case AstNodeKind::ForStmt:
            return Visit(static_cast<const ForStmt&>(node));

        case AstNodeKind::LoopStmt:
            return Visit(static_cast<const LoopStmt&>(node));

        case AstNodeKind::DeclVarStmt:
            return Visit(static_cast<const DeclVarStmt&>(node));

    }

    return Error::Unreachable;
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
