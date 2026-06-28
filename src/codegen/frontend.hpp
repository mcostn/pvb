#pragma once

#include <vector>
#include <memory>
#include <string>
#include <variant>
#include <cstdint>
#include <concepts>

enum Value : uint32_t
{
    VAL_NONE = 0,

    VAL_INT    = 1 << 0,
    VAL_FLOAT  = 1 << 1,
    VAL_BOOL   = 1 << 2,
    VAL_STRING = 1 << 3,

    VAL_NUMBER = VAL_INT | VAL_FLOAT,
    VAL_ANY    = VAL_INT | VAL_FLOAT | VAL_BOOL | VAL_STRING,
};
std::string ValueToString(Value val);

enum class AstNodeKind
{
    Program,

    LiteralExpr,
    VariableExpr,
    UnaryExpr,
    BinaryExpr,
    AssignExpr,
    CallExpr,

    PrintStmt,
    ReadStmt,
    ExitStmt,
    ExprStmt,
    FunctionStmt,

    BlockStmt,
    IfStmt,
    WhileStmt,
    ForStmt,

    DeclVarStmt,
};
struct AstNode
{
    AstNodeKind Kind;
    explicit AstNode(AstNodeKind kind) : Kind(kind) { }
    virtual ~AstNode() = default;
};

struct Expr : AstNode
{
    Value Type;
    Expr(AstNodeKind kind, Value type) : AstNode(kind), Type(type) {}
};

using LiteralValue = std::variant<int, float, bool, std::string>;
struct LiteralExpr : Expr
{
    LiteralValue Data;
    LiteralExpr(Value type) : Expr(AstNodeKind::LiteralExpr, type) {}
};

struct VariableExpr : Expr
{
    std::string Name;
    VariableExpr(Value type) : Expr(AstNodeKind::VariableExpr, type) {}
};

enum class UnaryOp
{
    Negate,
    Not,
};
struct UnaryExpr : Expr
{
    UnaryOp Op;
    std::unique_ptr<Expr> Data;
    UnaryExpr(Value type) : Expr(AstNodeKind::UnaryExpr, type) {}
};

enum class BinaryOp
{
    Add,
    Sub,
    Mul,
    Div,
    Mod,

    Less,
    Greater,
    LessEqual,
    GreaterEqual,

    Equal,
    NotEqual,

    And,
    Or,
};
struct BinaryExpr : Expr
{
    BinaryOp Op;
    std::unique_ptr<Expr> Left;
    std::unique_ptr<Expr> Right;
    BinaryExpr(Value type) : Expr(AstNodeKind::BinaryExpr, type) {}
};

struct AssignExpr : Expr
{
    std::string Name;
    std::unique_ptr<Expr> ValueExpr;
    AssignExpr(Value type) : Expr(AstNodeKind::AssignExpr, type) {}
};

struct CallExpr : Expr
{
    std::string Function;
    std::vector<std::unique_ptr<Expr>> Args;
    CallExpr(Value type) : Expr(AstNodeKind::CallExpr, type) {}
};

struct Stmt : AstNode
{
    explicit Stmt(AstNodeKind kind) : AstNode(kind) {}
};

struct PrintStmt : Stmt
{
    std::unique_ptr<Expr> Data;
    bool Newline;
    PrintStmt() : Stmt(AstNodeKind::PrintStmt) {}
};

struct ReadStmt : Stmt
{
    std::unique_ptr<VariableExpr> Variable;
    ReadStmt() : Stmt(AstNodeKind::ReadStmt) {}
};

struct ExitStmt : Stmt
{
    std::unique_ptr<Expr> Code;
    ExitStmt() : Stmt(AstNodeKind::ExitStmt) {}
};

struct ExprStmt : Stmt
{
    std::unique_ptr<Expr> Expression;
    ExprStmt() : Stmt(AstNodeKind::ExprStmt) {}
};

struct BlockStmt : Stmt
{
    std::vector<std::unique_ptr<Stmt>> Statements;
    BlockStmt() : Stmt(AstNodeKind::BlockStmt) {}
};

struct Param
{
    Value Type;
    std::string Name;
};
struct FunctionStmt : Stmt
{
    Value ReturnType;
    std::string Name;
    std::vector<Param> Params;
    std::unique_ptr<BlockStmt> Body;
    FunctionStmt() : Stmt(AstNodeKind::FunctionStmt) {}
};

struct IfStmt : Stmt
{
    std::unique_ptr<Expr> Condition;
    std::unique_ptr<Stmt> ThenBranch;
    std::unique_ptr<Stmt> ElseBranch;
    IfStmt() : Stmt(AstNodeKind::IfStmt) {}
};

struct WhileStmt : Stmt
{
    std::unique_ptr<Expr> Condition;
    std::unique_ptr<Stmt> Body;
    WhileStmt() : Stmt(AstNodeKind::WhileStmt) {}
};

struct ForStmt : Stmt
{
    std::unique_ptr<Stmt> Init;
    std::unique_ptr<Expr> Condition;
    std::unique_ptr<Expr> Update;
    std::unique_ptr<Stmt> Body;
    ForStmt() : Stmt(AstNodeKind::ForStmt) {}
};

struct DeclVarStmt : Stmt
{
    Value Type;
    std::string Name;
    std::unique_ptr<Expr> Initializer;
    DeclVarStmt(Value type) : Stmt(AstNodeKind::DeclVarStmt), Type(type) {}
};

struct Program : AstNode
{
    std::vector<std::unique_ptr<Stmt>> Statements;
    Program() : AstNode(AstNodeKind::Program) {}
};

// Concepts to constraint the AST utilities
template<typename T>
concept StmtPtr = std::convertible_to<T, std::unique_ptr<Stmt>>;

template<typename T>
concept ExprPtr = std::convertible_to<T, std::unique_ptr<Expr>>;

// Useful utilities for building the AST
std::unique_ptr<LiteralExpr> Int(int value);
std::unique_ptr<LiteralExpr> Float(float value);
std::unique_ptr<LiteralExpr> Bool(bool value);
std::unique_ptr<LiteralExpr> String(std::string value);
std::unique_ptr<VariableExpr> Var(std::string name, Value type);

#define UNARY_OP_LIST(Fn)                         \
    Fn(Negate, Negate, VAL_INT, "-")              \
    Fn(Not,    Not,    VAL_BOOL, "!")

#define DECLARE_UNARY_OP(Name, Op, Type, Sym) \
    std::unique_ptr<UnaryExpr> Name(std::unique_ptr<Expr> expr);

UNARY_OP_LIST(DECLARE_UNARY_OP)
#undef DECLARE_UNARY_OP

#define BINARY_OP_LIST(Fn)                                               \
    Fn(Add,          Add,          VAL_INT, "+")                         \
    Fn(Sub,          Sub,          VAL_INT, "-")                         \
    Fn(Mul,          Mul,          VAL_INT, "*")                         \
    Fn(Div,          Div,          VAL_INT, "/")                         \
    Fn(Mod,          Mod,          VAL_INT, "%")                         \
    Fn(Less,         Less,         VAL_BOOL, "<")                        \
    Fn(Greater,      Greater,      VAL_BOOL, ">")                        \
    Fn(LessEqual,    LessEqual,    VAL_BOOL, "<=")                       \
    Fn(GreaterEqual, GreaterEqual, VAL_BOOL, ">=")                       \
    Fn(Equal,        Equal,        VAL_BOOL, "==")                       \
    Fn(NotEqual,     NotEqual,     VAL_BOOL, "!=")                       \
    Fn(And,          And,          VAL_BOOL, "&&")                       \
    Fn(Or,           Or,           VAL_BOOL, "||")

#define DECLARE_BINARY_OP(Name, Op, Type, Sym) \
    std::unique_ptr<BinaryExpr> Name(std::unique_ptr<Expr> lhs, std::unique_ptr<Expr> rhs);

BINARY_OP_LIST(DECLARE_BINARY_OP);
#undef DECLARE_BINARY_OP

std::unique_ptr<AssignExpr> Assign(std::string name, std::unique_ptr<Expr> expr);

template<StmtPtr... Ts>
std::unique_ptr<BlockStmt> Block(Ts&&... stmts)
{
    auto stmt = std::make_unique<BlockStmt>();
    (stmt->Statements.push_back(std::forward<Ts>(stmts)), ...);
    return stmt;
}
std::unique_ptr<PrintStmt> Print(std::unique_ptr<Expr> expr, bool newline = true);
std::unique_ptr<ExitStmt> Exit(std::unique_ptr<Expr> code);
std::unique_ptr<ExprStmt> ExprStatement(std::unique_ptr<Expr> expr);
std::unique_ptr<FunctionStmt> Function(Value retType, std::string name, std::vector<Param> params, std::unique_ptr<BlockStmt> body);
std::unique_ptr<WhileStmt> While(std::unique_ptr<Expr> cond, std::unique_ptr<Stmt> body);
std::unique_ptr<IfStmt> If(std::unique_ptr<Expr> cond, std::unique_ptr<Stmt> thenBranch, std::unique_ptr<Stmt> elseBranch = nullptr);
std::unique_ptr<ForStmt> For(std::unique_ptr<Stmt> init, std::unique_ptr<Expr> cond, std::unique_ptr<Expr> update, std::unique_ptr<Stmt> body);
std::unique_ptr<DeclVarStmt> DeclVar(Value type, std::string name, std::unique_ptr<Expr> init);

template<ExprPtr... Ts>
std::unique_ptr<CallExpr> Call(std::string name, Ts&&... args)
{
    auto stmt = std::make_unique<CallExpr>(VAL_NONE);
    stmt->Function = std::move(name);
    (stmt->Args.push_back(std::forward<Ts>(args)), ...);
    return stmt;
}

template<StmtPtr... Ts>
Program MakeProgram(Ts&&... stmts)
{
    Program p;
    (p.Statements.push_back(std::forward<Ts>(stmts)), ...);
    return p;
}
