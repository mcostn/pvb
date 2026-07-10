#pragma once

#include "block/block.hpp"

// Utility Macros
#define EXPECT_KIND(node, kind) \
    EXPECT((node) != nullptr && (node)->Kind == AstNodeKind::kind)
#define ASSERT_AS(var, node, Type, kind)              \
    EXPECT_KIND(node, kind);                          \
    if (!(node) || (node)->Kind != AstNodeKind::kind) \
        return;                                       \
    auto *var = static_cast<Type *>(node)

#define EXPECT_INT(node, value)                               \
    do {                                                      \
        ASSERT_AS(tmp, node, LiteralExpr, LiteralExpr);       \
        EXPECT_EQ(tmp->Type, VAL_INT);                        \
        EXPECT_EQ(std::get<int>(tmp->Data), value);           \
    } while (0)
#define EXPECT_FLOAT(node, value)                             \
    do {                                                      \
        ASSERT_AS(tmp, node, LiteralExpr, LiteralExpr);       \
        EXPECT_EQ(tmp->Type, VAL_FLOAT);                      \
        EXPECT_EQ(std::get<float>(tmp->Data), value);         \
    } while (0)
#define EXPECT_BOOL(node, value)                              \
    do {                                                      \
        ASSERT_AS(tmp, node, LiteralExpr, LiteralExpr);       \
        EXPECT_EQ(tmp->Type, VAL_BOOL);                       \
        EXPECT_EQ(std::get<bool>(tmp->Data), value);          \
    } while (0)
#define EXPECT_STRING(node, value)                            \
    do {                                                      \
        ASSERT_AS(tmp, node, LiteralExpr, LiteralExpr);       \
        EXPECT_EQ(tmp->Type, VAL_STRING);                     \
        EXPECT_EQ(std::get<std::string>(tmp->Data), value);   \
    } while (0)

#define EXPECT_VAR(node, name)                                \
    do {                                                      \
        ASSERT_AS(tmp,node,VariableExpr,VariableExpr);        \
        EXPECT_EQ(tmp->Name, name);                           \
    } while(0)

#define EXPECT_BINARY(var,node,op)                        \
    ASSERT_AS(var,node,BinaryExpr,BinaryExpr);            \
    EXPECT(var->Op == BinaryOp::op)
#define EXPECT_UNARY(var,node,op)                         \
    ASSERT_AS(var,node,UnaryExpr,UnaryExpr);              \
    EXPECT(var->Op == UnaryOp::op)

#define EXPECT_PRINT(var,node) \
    ASSERT_AS(var,node,PrintStmt,PrintStmt)
#define EXPECT_EXIT(var,node) \
    ASSERT_AS(var,node,ExitStmt,ExitStmt)
#define EXPECT_IF(var,node) \
    ASSERT_AS(var,node,IfStmt,IfStmt)
#define EXPECT_BLOCK(var,node) \
    ASSERT_AS(var,node,BlockStmt,BlockStmt)
#define EXPECT_WHILE(var,node) \
    ASSERT_AS(var,node,WhileStmt,WhileStmt)
#define EXPECT_FOR(var,node) \
    ASSERT_AS(var,node,ForStmt,ForStmt)
#define EXPECT_DECLVAR(var,node) \
    ASSERT_AS(var,node,DeclVarStmt,DeclVarStmt)

// Utility Functions
inline BlockInstance Block(std::string opcode)
{
    BlockInstance b;
    b.OpCode = std::move(opcode);
    return b;
}

inline void Literal(BlockInstance &b, std::string name, LiteralValue value)
{
    b.Args.emplace(std::move(name), std::move(value));
}
inline void Variable(BlockInstance &b, std::string name, std::string var)
{
    b.Args.emplace(std::move(name), VariableRef{std::move(var)});
}
inline void Reporter(BlockInstance &b, std::string name, BlockInstance &&child)
{
    b.Args.emplace(
        std::move(name),
        std::make_unique<BlockInstance>(std::move(child)));
}

inline void Literal(BlockInstance &b, std::string name, int value)
{
    b.Args.emplace(std::move(name), LiteralValue{value});
}
inline void Literal(BlockInstance &b, std::string name, float value)
{
    b.Args.emplace(std::move(name), LiteralValue{value});
}
inline void Literal(BlockInstance &b, std::string name, bool value)
{
    b.Args.emplace(std::move(name), LiteralValue{value});
}
inline void Literal(BlockInstance &b, std::string name, std::string value)
{
    b.Args.emplace(std::move(name), LiteralValue{std::move(value)});
}
inline void Literal(BlockInstance &b, std::string name, const char *value)
{
    b.Args.emplace(std::move(name), LiteralValue{std::string(value)});
}

template<typename... Ts>
inline std::vector<std::unique_ptr<BlockInstance>> Body(Ts&&... blocks)
{
    std::vector<std::unique_ptr<BlockInstance>> out;
    (out.push_back(std::make_unique<BlockInstance>(std::forward<Ts>(blocks))), ...);
    return out;
}
inline void SetBody(BlockInstance &b, std::string name, std::vector<std::unique_ptr<BlockInstance>> body)
{
    b.Bodies.emplace(std::move(name), std::move(body));
}
