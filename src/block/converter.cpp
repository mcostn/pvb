#include "block/converter.hpp"

std::unique_ptr<Stmt> BlockConverter::ConvertStmt(const BlockInstance &block)
{
    auto it = StmtBuilders.find(block.OpCode);
    if (it == StmtBuilders.end()) {
        GlobalLogger.Error("Unknown statement block '{}'", block.OpCode);
        return nullptr;
    }

    return it->second(*this, block);
}

std::unique_ptr<Expr> BlockConverter::ConvertExpr(const BlockInstance &block)
{
    auto it = ExprBuilders.find(block.OpCode);
    if (it == ExprBuilders.end()) {
        GlobalLogger.Error("Unknown expression block '{}'", block.OpCode);
        return nullptr;
    }

    return it->second(*this, block);
}

std::unique_ptr<BlockStmt> BlockConverter::ConvertBody(const std::vector<std::unique_ptr<BlockInstance>> &blocks)
{
    auto body = std::make_unique<BlockStmt>();

    for (const auto &b : blocks) {
        auto stmt = ConvertStmt(*b);
        if (stmt)
            body->Statements.push_back(std::move(stmt));
    }

    return body;
}

std::unique_ptr<BlockStmt> BlockConverter::ConvertBody(const BlockInstance &block, const std::string &bodyName)
{
    auto it = block.Bodies.find(bodyName);
    if (it == block.Bodies.end())
        return std::make_unique<BlockStmt>();

    return ConvertBody(it->second);
}

std::unique_ptr<Expr> BlockConverter::ResolveArg(const BlockArg &arg, Value expectedType)
{
    if (auto *lit = std::get_if<LiteralValue>(&arg))
        return std::visit([&](auto&& v) -> std::unique_ptr<Expr> {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, int>)    return Int(v);
            if constexpr (std::is_same_v<T, float>)  return Float(v);
            if constexpr (std::is_same_v<T, bool>)   return Bool(v);
            if constexpr (std::is_same_v<T, std::string>) return String(v);
        }, *lit);

    if (auto *varRef = std::get_if<VariableRef>(&arg))
        return Var(varRef->Name, varRef->Type);

    if (auto *nested = std::get_if<std::unique_ptr<BlockInstance>>(&arg))
        return ConvertExpr(**nested);

    return nullptr;
}
