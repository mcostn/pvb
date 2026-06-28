#include "frontend.hpp"

std::string ValueToString(Value val)
{
    if (val == VAL_NONE) return "NONE";
    if (val == VAL_ANY) return "ANY";
    if (val == VAL_NUMBER) return "NUMBER";

    std::string out;

    if (val & VAL_INT)    out += "INT|";
    if (val & VAL_FLOAT)  out += "FLOAT|";
    if (val & VAL_BOOL)   out += "BOOL|";
    if (val & VAL_STRING) out += "STRING|";

    if (!out.empty() && out.back() == '|')
        out.pop_back();

    return out.empty() ? "UNKNOWN" : out;
}

// Literal Expressions
template<typename T>
std::unique_ptr<LiteralExpr> MakeLiteral(Value type, T value)
{
    auto expr = std::make_unique<LiteralExpr>(type);
    expr->Data = std::move(value);
    return expr;
}

std::unique_ptr<LiteralExpr> Int(int v)            { return MakeLiteral(VAL_INT, v); }
std::unique_ptr<LiteralExpr> Float(float v)        { return MakeLiteral(VAL_FLOAT, v); }
std::unique_ptr<LiteralExpr> Bool(bool v)          { return MakeLiteral(VAL_BOOL, v); }
std::unique_ptr<LiteralExpr> String(std::string v) { return MakeLiteral(VAL_STRING, std::move(v)); }

std::unique_ptr<VariableExpr> Var(std::string name, Value type)
{
    auto expr = std::make_unique<VariableExpr>(type);
    expr->Name = std::move(name);
    return expr;
}

// Unary Operators
static std::unique_ptr<UnaryExpr> MakeUnary(UnaryOp op, Value type, std::unique_ptr<Expr> expr)
{
    auto unary = std::make_unique<UnaryExpr>(type);
    unary->Op = op;
    unary->Data = std::move(expr);
    return unary;
}

#define DEFINE_UNARY_OP(Name, Op, Type, Sym)                                  \
    std::unique_ptr<UnaryExpr> Name(std::unique_ptr<Expr> expr) {             \
        return MakeUnary(UnaryOp::Op, Type, std::move(expr));                 \
    }

UNARY_OP_LIST(DEFINE_UNARY_OP);
#undef DEFINE_UNARY_OP

// Binary Operators
static std::unique_ptr<BinaryExpr> MakeBinary(BinaryOp op, Value type, std::unique_ptr<Expr> lhs, std::unique_ptr<Expr> rhs)
{
    auto expr = std::make_unique<BinaryExpr>(type);
    expr->Op = op;
    expr->Left = std::move(lhs);
    expr->Right = std::move(rhs);
    return expr;
}

#define DEFINE_BINARY_OP(Name, Op, Type, Sym)                                                         \
    std::unique_ptr<BinaryExpr> Name(std::unique_ptr<Expr> lhs, std::unique_ptr<Expr> rhs) {          \
        return MakeBinary(BinaryOp::Op, Type, std::move(lhs), std::move(rhs));                        \
    }

BINARY_OP_LIST(DEFINE_BINARY_OP);
#undef DEFINE_BINARY_OP

// Other Expressions
std::unique_ptr<AssignExpr> Assign(std::string name, std::unique_ptr<Expr> expr)
{
    auto assign = std::make_unique<AssignExpr>(expr->Type);
    assign->Name = std::move(name);
    assign->ValueExpr = std::move(expr);
    return assign;
}

// Statements
std::unique_ptr<PrintStmt> Print(std::unique_ptr<Expr> expr, bool newline)
{
    auto stmt = std::make_unique<PrintStmt>();
    stmt->Data = std::move(expr);
    stmt->Newline = newline;
    return stmt;
}

std::unique_ptr<ExitStmt> Exit(std::unique_ptr<Expr> code)
{
    auto stmt = std::make_unique<ExitStmt>();
    stmt->Code = std::move(code);
    return stmt;
}

std::unique_ptr<ExprStmt> ExprStatement(std::unique_ptr<Expr> expr)
{
    auto stmt = std::make_unique<ExprStmt>();
    stmt->Expression = std::move(expr);
    return stmt;
}

std::unique_ptr<FunctionStmt> Function(
         Value retType,
         std::string name,
         std::vector<Param> params,
         std::unique_ptr<BlockStmt> body)
{
    auto fn = std::make_unique<FunctionStmt>();
    fn->ReturnType = retType;
    fn->Name = std::move(name);
    fn->Params = std::move(params);
    fn->Body = std::move(body);
    return fn;
}

std::unique_ptr<IfStmt> If(std::unique_ptr<Expr> cond, std::unique_ptr<Stmt> thenBranch, std::unique_ptr<Stmt> elseBranch)
{
    auto stmt = std::make_unique<IfStmt>();
    stmt->Condition = std::move(cond);
    stmt->ThenBranch = std::move(thenBranch);
    stmt->ElseBranch = std::move(elseBranch);
    return stmt;
}

std::unique_ptr<WhileStmt> While(std::unique_ptr<Expr> cond, std::unique_ptr<Stmt> body)
{
    auto stmt = std::make_unique<WhileStmt>();
    stmt->Condition = std::move(cond);
    stmt->Body = std::move(body);
    return stmt;
}

std::unique_ptr<ForStmt> For(
        std::unique_ptr<Stmt> init,
        std::unique_ptr<Expr> cond,
        std::unique_ptr<Expr> update,
        std::unique_ptr<Stmt> body)
{
    auto stmt = std::make_unique<ForStmt>();
    stmt->Init = std::move(init);
    stmt->Condition = std::move(cond);
    stmt->Update = std::move(update);
    stmt->Body = std::move(body);
    return stmt;
}

std::unique_ptr<DeclVarStmt> DeclVar(
    Value type,
    std::string name,
    std::unique_ptr<Expr> init)
{
    auto stmt = std::make_unique<DeclVarStmt>(type);
    stmt->Name = std::move(name);
    stmt->Initializer = std::move(init);
    return stmt;
}
