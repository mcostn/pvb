#include "test_runner.hpp"
#include "block_converter.hpp"

// Utility Functions
inline BlockConverter MakeConverter()
{
    return GetBlockRegistry().Converter;
}

// Tests
TEST(ConverterPrintLiteral)
{
    auto conv = MakeConverter();

    auto block = Block("print");
    Literal(block, "out", std::string("Hello"));

    auto stmt = conv.ConvertStmt(block);
    ASSERT_AS(print, stmt.get(), PrintStmt, PrintStmt);

    EXPECT(!print->Newline);
    EXPECT_STRING(print->Data.get(), "Hello");
}

TEST(ConverterPrintlnLiteral)
{
    auto conv = MakeConverter();

    auto block = Block("println");
    Literal(block, "out", 123);

    auto stmt = conv.ConvertStmt(block);
    ASSERT_AS(print, stmt.get(), PrintStmt, PrintStmt);

    EXPECT(print->Newline);
    EXPECT_INT(print->Data.get(), 123);
}

TEST(ConverterExit)
{
    auto conv = MakeConverter();

    auto block = Block("exit");
    Literal(block, "code", 5);

    auto stmt = conv.ConvertStmt(block);
    ASSERT_AS(exit, stmt.get(), ExitStmt, ExitStmt);

    EXPECT_INT(exit->Code.get(), 5);
}

TEST(ConverterVariable)
{
    auto conv = MakeConverter();

    auto block = Block("print");
    Variable(block, "out", "myVar");

    auto stmt = conv.ConvertStmt(block);
    ASSERT_AS(print, stmt.get(), PrintStmt, PrintStmt);

    EXPECT_VAR(print->Data.get(), "myVar");
}

TEST(ConverterNestedExpression)
{
    auto conv = MakeConverter();

    auto add = Block("add");
    Literal(add, "lhs", 10);
    Literal(add, "rhs", 20);

    auto print = Block("print");
    Reporter(print, "out", std::move(add));

    auto stmt = conv.ConvertStmt(print);
    ASSERT_AS(printStmt, stmt.get(), PrintStmt, PrintStmt);
    ASSERT_AS(binary, printStmt->Data.get(), BinaryExpr, BinaryExpr);

    EXPECT(binary->Op == BinaryOp::Add);

    EXPECT_INT(binary->Left.get(), 10);
    EXPECT_INT(binary->Right.get(), 20);
}

TEST(ConverterAdd)
{
    auto conv = MakeConverter();

    auto block = Block("add");
    Literal(block, "lhs", 1);
    Literal(block, "rhs", 2);

    auto expr = conv.ConvertExpr(block);
    EXPECT_BINARY(add, expr.get(), Add);
    EXPECT_INT(add->Left.get(), 1);
    EXPECT_INT(add->Right.get(), 2);
}

TEST(ConverterSubtract)
{
    auto conv = MakeConverter();

    auto block = Block("sub");
    Literal(block, "lhs", 10);
    Literal(block, "rhs", 3);

    auto expr = conv.ConvertExpr(block);
    EXPECT_BINARY(sub, expr.get(), Sub);
    EXPECT_INT(sub->Left.get(), 10);
    EXPECT_INT(sub->Right.get(), 3);
}

TEST(ConverterMultiply)
{
    auto conv = MakeConverter();

    auto block = Block("mul");
    Literal(block, "lhs", 4);
    Literal(block, "rhs", 5);

    auto expr = conv.ConvertExpr(block);
    EXPECT_BINARY(mul, expr.get(), Mul);
}

TEST(ConverterDivide)
{
    auto conv = MakeConverter();

    auto block = Block("div");
    Literal(block, "lhs", 20);
    Literal(block, "rhs", 4);

    auto expr = conv.ConvertExpr(block);
    EXPECT_BINARY(div, expr.get(), Div);
}

TEST(ConverterModulo)
{
    auto conv = MakeConverter();

    auto block = Block("mod");
    Literal(block, "lhs", 11);
    Literal(block, "rhs", 3);

    auto expr = conv.ConvertExpr(block);
    EXPECT_BINARY(mod, expr.get(), Mod);
}

TEST(ConverterRound)
{
    auto conv = MakeConverter();

    auto block = Block("round");
    Literal(block, "value", 2.5f);

    auto expr = conv.ConvertExpr(block);
    ASSERT_AS(call, expr.get(), CallExpr, CallExpr);

    EXPECT_EQ(call->Function, "round");
    EXPECT_FLOAT(call->Args[0].get(), 2.5f);
}

TEST(ConverterAbs)
{
    auto conv = MakeConverter();

    auto block = Block("abs");
    Literal(block, "value", -10);

    auto expr = conv.ConvertExpr(block);
    ASSERT_AS(call, expr.get(), CallExpr, CallExpr);

    EXPECT_EQ(call->Function, "abs");
}

TEST(ConverterSqrt)
{
    auto conv = MakeConverter();

    auto block = Block("sqrt");
    Literal(block, "value", 9);

    auto expr = conv.ConvertExpr(block);
    ASSERT_AS(call, expr.get(), CallExpr, CallExpr);

    EXPECT_EQ(call->Function, "sqrt");
}

TEST(ConverterLess)
{
    auto conv = MakeConverter();

    auto block = Block("lt");

    Literal(block, "lhs", 1);
    Literal(block, "rhs", 2);

    auto expr = conv.ConvertExpr(block);

    EXPECT_BINARY(node, expr.get(), Less);
}

TEST(ConverterGreater)
{
    auto conv = MakeConverter();

    auto block = Block("gt");

    Literal(block, "lhs", 2);
    Literal(block, "rhs", 1);

    auto expr = conv.ConvertExpr(block);

    EXPECT_BINARY(node, expr.get(), Greater);
}

TEST(ConverterLessEqual)
{
    auto conv = MakeConverter();

    auto block = Block("le");
    Literal(block, "lhs", 1);
    Literal(block, "rhs", 1);

    auto expr = conv.ConvertExpr(block);
    EXPECT_BINARY(node, expr.get(), LessEqual);
}

TEST(ConverterGreaterEqual)
{
    auto conv = MakeConverter();

    auto block = Block("ge");
    Literal(block, "lhs", 2);
    Literal(block, "rhs", 2);

    auto expr = conv.ConvertExpr(block);
    EXPECT_BINARY(node, expr.get(), GreaterEqual);
}

TEST(ConverterNot)
{
    auto conv = MakeConverter();

    auto block = Block("not");
    Literal(block, "value", true);

    auto expr = conv.ConvertExpr(block);
    EXPECT_UNARY(node, expr.get(), Not);
}

TEST(ConverterEqual)
{
    auto conv = MakeConverter();

    auto block = Block("eq");
    Literal(block, "lhs", 2);
    Literal(block, "rhs", 2);

    auto expr = conv.ConvertExpr(block);
    EXPECT_BINARY(node, expr.get(), Equal);
}

TEST(ConverterNotEqual)
{
    auto conv = MakeConverter();

    auto block = Block("neq");
    Literal(block, "lhs", 2);
    Literal(block, "rhs", 2);

    auto expr = conv.ConvertExpr(block);
    EXPECT_BINARY(node, expr.get(), NotEqual);
}

TEST(ConverterAnd)
{
    auto conv = MakeConverter();

    auto block = Block("and");
    Literal(block, "lhs", true);
    Literal(block, "rhs", false);

    auto expr = conv.ConvertExpr(block);
    EXPECT_BINARY(node, expr.get(), And);
}

TEST(ConverterOr)
{
    auto conv = MakeConverter();

    auto block = Block("or");
    Literal(block, "lhs", true);
    Literal(block, "rhs", false);

    auto expr = conv.ConvertExpr(block);
    EXPECT_BINARY(node, expr.get(), Or);
}


TEST(ConverterIf)
{
    auto conv = MakeConverter();

    auto ifBlock = Block("if");
    Literal(ifBlock, "cond", true);

    auto printBlock = Block("print");
    Literal(printBlock, "out", std::string("yes"));
    SetBody(ifBlock, "then", Body(std::move(printBlock)));

    auto stmt = conv.ConvertStmt(ifBlock);
    EXPECT_IF(ifStmt, stmt.get());

    EXPECT_BOOL(ifStmt->Condition.get(), true);
    EXPECT(ifStmt->ElseBranch == nullptr);

    EXPECT_BLOCK(thenBlock, ifStmt->ThenBranch.get());
    EXPECT_EQ(thenBlock->Statements.size(), 1u);

    EXPECT_PRINT(print, thenBlock->Statements[0].get());
    EXPECT_STRING(print->Data.get(), "yes");
}

TEST(ConverterIfEmptyBody)
{
    auto conv = MakeConverter();

    auto ifBlock = Block("if");
    Literal(ifBlock, "cond", true);

    auto stmt = conv.ConvertStmt(ifBlock);
    EXPECT_IF(ifStmt, stmt.get());

    EXPECT_BLOCK(thenBlock, ifStmt->ThenBranch.get());
    EXPECT_EQ(thenBlock->Statements.size(), 0u);
}

TEST(ConverterIfMultipleStatements)
{
    auto conv = MakeConverter();

    auto ifBlock = Block("if");
    Literal(ifBlock, "cond", true);

    auto print1 = Block("print");
    Literal(print1, "out", 1);
    auto print2 = Block("print");
    Literal(print2, "out", 2);
    SetBody(ifBlock, "then", Body(std::move(print1), std::move(print2)));

    auto stmt = conv.ConvertStmt(ifBlock);
    EXPECT_IF(ifStmt, stmt.get());

    EXPECT_BLOCK(thenBlock, ifStmt->ThenBranch.get());
    EXPECT_EQ(thenBlock->Statements.size(), 2u);

    EXPECT_PRINT(p1, thenBlock->Statements[0].get());
    EXPECT_INT(p1->Data.get(), 1);

    EXPECT_PRINT(p2, thenBlock->Statements[1].get());
    EXPECT_INT(p2->Data.get(), 2);
}

TEST(ConverterIfElse)
{
    auto conv = MakeConverter();

    auto ifBlock = Block("ifelse");
    Literal(ifBlock, "cond", false);

    auto thenPrint = Block("print");
    Literal(thenPrint, "out", std::string("then"));

    auto elsePrint = Block("print");
    Literal(elsePrint, "out", std::string("else"));

    SetBody(ifBlock, "then", Body(std::move(thenPrint)));
    SetBody(ifBlock, "else", Body(std::move(elsePrint)));

    auto stmt = conv.ConvertStmt(ifBlock);
    EXPECT_IF(ifStmt, stmt.get());

    EXPECT_BOOL(ifStmt->Condition.get(), false);

    EXPECT_BLOCK(thenBlock, ifStmt->ThenBranch.get());
    EXPECT_PRINT(thenPrintStmt, thenBlock->Statements[0].get());
    EXPECT_STRING(thenPrintStmt->Data.get(), "then");

    EXPECT_BLOCK(elseBlock, ifStmt->ElseBranch.get());
    EXPECT_PRINT(elsePrintStmt, elseBlock->Statements[0].get());
    EXPECT_STRING(elsePrintStmt->Data.get(), "else");
}
