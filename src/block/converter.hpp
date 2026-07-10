#pragma once

#include <functional>
#include <memory>
#include <unordered_map>

#include "codegen/codegen.hpp"
#include "block/instance.hpp"

class BlockConverter
{
public:
    std::unique_ptr<Stmt> ConvertStmt(const BlockInstance &block);
    std::unique_ptr<Expr> ConvertExpr(const BlockInstance &block);

    std::unique_ptr<BlockStmt> ConvertBody(const BlockInstance &block, const std::string &bodyName);
    std::unique_ptr<BlockStmt> ConvertBody(const std::vector<std::unique_ptr<BlockInstance>> &blocks);

    std::unique_ptr<Expr> ResolveArg(const BlockArg &arg, Value expectedType);

    using StmtBuilder = std::function<std::unique_ptr<Stmt>(BlockConverter&, const BlockInstance&)>;
    using ExprBuilder = std::function<std::unique_ptr<Expr>(BlockConverter&, const BlockInstance&)>;

    std::unordered_map<std::string, StmtBuilder> StmtBuilders;
    std::unordered_map<std::string, ExprBuilder> ExprBuilders;
};
