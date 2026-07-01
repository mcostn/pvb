#include "codegen/backend.hpp"

void Emitter::Indent()
{
    for (int i = 0; i < IndentLevel; ++i)
        Out() << "    ";
}

Error Emitter::EmitStmt(const Stmt &stmt)
{
    switch (stmt.Kind) {
        case AstNodeKind::PrintStmt:
            return EmitPrint(static_cast<const PrintStmt&>(stmt));

        case AstNodeKind::ExitStmt:
            return EmitExit(static_cast<const ExitStmt&>(stmt));

        case AstNodeKind::ExprStmt:
            return EmitExprStmt(static_cast<const ExprStmt&>(stmt));

        case AstNodeKind::BlockStmt:
            return EmitBlock(static_cast<const BlockStmt&>(stmt));

        case AstNodeKind::FunctionStmt:
            return EmitFunction(static_cast<const FunctionStmt&>(stmt));

        case AstNodeKind::IfStmt:
            return EmitIf(static_cast<const IfStmt&>(stmt));

        case AstNodeKind::WhileStmt:
            return EmitWhile(static_cast<const WhileStmt&>(stmt));

        case AstNodeKind::ForStmt:
            return EmitFor(static_cast<const ForStmt&>(stmt));

        case AstNodeKind::DeclVarStmt:
            return EmitDeclVar(static_cast<const DeclVarStmt&>(stmt));

        default:
            GlobalLogger.Error("Unexepected statement kind");
            return Error::Failed;
    }
}

Error Emitter::EmitExpr(const Expr &expr)
{
    switch (expr.Kind) {
        case AstNodeKind::LiteralExpr:
            return EmitLiteral(static_cast<const LiteralExpr&>(expr));

        case AstNodeKind::VariableExpr:
            return EmitVariable(static_cast<const VariableExpr&>(expr));

        case AstNodeKind::AssignExpr:
            return EmitAssign(static_cast<const AssignExpr&>(expr));

        case AstNodeKind::UnaryExpr:
            return EmitUnary(static_cast<const UnaryExpr&>(expr));

        case AstNodeKind::BinaryExpr:
            return EmitBinary(static_cast<const BinaryExpr&>(expr));

        case AstNodeKind::CallExpr:
            return EmitCall(static_cast<const CallExpr&>(expr));

        default:
            GlobalLogger.Error("Unexepected expression kind");
            return Error::Failed;
    }
}
