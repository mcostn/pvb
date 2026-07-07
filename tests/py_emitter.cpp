#include <sstream>
#include "test_runner.hpp"
#include "codegen/codegen.hpp"

// Utility Function
static std::string Generate(const Program& program)
{
    std::stringstream ss;
    PythonEmitter emitter(&ss);
    EXPECT(emitter.Emit(program) == Error::Ok);
    return ss.str();
}

// Tests
TEST(PyPrintInteger)
{
    auto program = MakeProgram(
        Print(Int(42))
    );

    EXPECT_EQ(
        Generate(program),
R"(print(42)
)"
    );
}

TEST(PyPrintBool)
{
    auto program = MakeProgram(
        Print(Bool(true))
    );

    EXPECT_EQ(
        Generate(program),
R"(print(True)
)"
    );
}

TEST(PyPrintString)
{
    auto program = MakeProgram(
        Print(String("Hello, World!"))
    );

    EXPECT_EQ(
        Generate(program),
R"(print("Hello, World!")
)"
    );
}

TEST(PyNegate)
{
    auto program = MakeProgram(
        Print(Negate(Int(42)))
    );

    EXPECT_EQ(
        Generate(program),
R"(print(-(42))
)"
    );
}

TEST(PyNot)
{
    auto program = MakeProgram(
        Print(Not(Bool(true)))
    );

    EXPECT_EQ(
        Generate(program),
R"(print(not (True))
)"
    );
}

TEST(PyAddition)
{
    auto program = MakeProgram(
        Print(Add(Int(5), Int(10)))
    );

    EXPECT_EQ(
        Generate(program),
R"(print((5 + 10))
)"
    );
}

TEST(PySubtraction)
{
    auto program = MakeProgram(
        Print(Sub(Int(5), Int(10)))
    );

    EXPECT_EQ(
        Generate(program),
R"(print((5 - 10))
)"
    );
}

TEST(PyMultiplication)
{
    auto program = MakeProgram(
        Print(Mul(Int(5), Int(10)))
    );

    EXPECT_EQ(
        Generate(program),
R"(print((5 * 10))
)"
    );
}

TEST(PyDivision)
{
    auto program = MakeProgram(
        Print(Div(Int(5), Int(10)))
    );

    EXPECT_EQ(
        Generate(program),
R"(print((5 / 10))
)"
    );
}

TEST(PyModulo)
{
    auto program = MakeProgram(
        Print(Mod(Int(10), Int(3)))
    );

    EXPECT_EQ(
        Generate(program),
R"(print((10 % 3))
)"
    );
}

TEST(PyLess)
{
    auto program = MakeProgram(
        Print(Less(Int(10), Int(5)))
    );

    EXPECT_EQ(
        Generate(program),
R"(print((10 < 5))
)"
    );
}

TEST(PyGreater)
{
    auto program = MakeProgram(
        Print(Greater(Int(10), Int(5)))
    );

    EXPECT_EQ(
        Generate(program),
R"(print((10 > 5))
)"
    );
}

TEST(PyLessEqual)
{
    auto program = MakeProgram(
        Print(LessEqual(Int(10), Int(5)))
    );

    EXPECT_EQ(
        Generate(program),
R"(print((10 <= 5))
)"
    );
}

TEST(PyGreaterEqual)
{
    auto program = MakeProgram(
        Print(GreaterEqual(Int(10), Int(5)))
    );

    EXPECT_EQ(
        Generate(program),
R"(print((10 >= 5))
)"
    );
}

TEST(PyEqual)
{
    auto program = MakeProgram(
        Print(Equal(Int(10), Int(5)))
    );

    EXPECT_EQ(
        Generate(program),
R"(print((10 == 5))
)"
    );
}

TEST(PyNotEqual)
{
    auto program = MakeProgram(
        Print(NotEqual(Int(10), Int(5)))
    );

    EXPECT_EQ(
        Generate(program),
R"(print((10 != 5))
)"
    );
}

TEST(PyAnd)
{
    auto program = MakeProgram(
        Print(And(Bool(true), Bool(false)))
    );

    EXPECT_EQ(
        Generate(program),
R"(print((True and False))
)"
    );
}

TEST(PyOr)
{
    auto program = MakeProgram(
        Print(Or(Bool(true), Bool(false)))
    );

    EXPECT_EQ(
        Generate(program),
R"(print((True or False))
)"
    );
}

TEST(PyNestedExpression)
{
    auto program = MakeProgram(
        Print(
            Not(
                Greater(
                    Mul(
                        Add(Int(5), Int(10)),
                        Negate(Int(2))
                    ),
                    Int(10)
                )
            )
        )
    );

    EXPECT_EQ(
        Generate(program),
R"(print(not ((((5 + 10) * -(2)) > 10)))
)"
    );
}

TEST(PyIfBareBody)
{
    auto program = MakeProgram(
        If(Bool(true), Print(Int(1)))
    );

    EXPECT_EQ(
        Generate(program),
R"(if True: print(1)
)");
}

TEST(PyIfNoElse)
{
    auto program = MakeProgram(
        If(
            Bool(true),
            Block(Print(String("yes")))
        )
    );

    EXPECT_EQ(
        Generate(program),
R"(if True:
    print("yes")
)");
}

TEST(PyIfElse)
{
    auto program = MakeProgram(
        If(
            Bool(false),
            Block(Print(String("then"))),
            Block(Print(String("else")))
        )
    );

    EXPECT_EQ(
        Generate(program),
R"(if False:
    print("then")
else:
    print("else")
)");
}

TEST(PyNestedIf)
{
    auto program = MakeProgram(
        If(
            Bool(true),
            Block(
                If(
                    Bool(false),
                    Block(Print(String("inner")))
                )
            )
        )
    );

    EXPECT_EQ(
        Generate(program),
R"(if True:
    if False:
        print("inner")
)");
}

TEST(PyExit)
{
    auto program = MakeProgram(
        Exit(Int(1))
    );

    EXPECT_EQ(
        Generate(program),
R"(import sys
sys.exit(1)
)");
}

TEST(PyWhileBareBody)
{
    auto program = MakeProgram(
        While(Bool(true), Print(Int(1)))
    );

    EXPECT_EQ(Generate(program),
R"(while True: print(1)
)");
}

TEST(PyWhileBlockBody)
{
    auto program = MakeProgram(
        While(
            Bool(true),
            Block(Print(String("running")))
        )
    );

    EXPECT_EQ(
        Generate(program),
R"(while True:
    print("running")
)");
}

TEST(PyForLoop)
{
    auto program = MakeProgram(
        For(
            DeclVar(VAL_INT, "i", Int(0)),
            Less(Var("i", VAL_INT), Int(10)),
            Assign("i", Add(Var("i", VAL_INT), Int(1))),
            Block(Print(Var("i", VAL_INT)))
        )
    );

    EXPECT_EQ(
        Generate(program),
R"(i = 0
while (i < 10):
    print(i)
    i = (i + 1)
)");
}

TEST(PyForInfiniteNoCondNoUpdate)
{
    auto program = MakeProgram(
        For(
            nullptr,
            nullptr,
            nullptr,
            Block(Print(String("looping")))
        )
    );

    EXPECT_EQ(
        Generate(program),
R"(while True:
    print("looping")
)");
}

TEST(PyDeclVarNoInitializer)
{
    auto program = MakeProgram(
        DeclVar(VAL_INT, "x", nullptr)
    );

    EXPECT_EQ(
        Generate(program),
R"(x = None
)");
}

TEST(PyDeclVarInt)
{
    auto program = MakeProgram(
        DeclVar(VAL_INT, "i", Int(1))
    );

    EXPECT_EQ(
        Generate(program),
R"(i = 1
)");
}

TEST(PyDeclVarFloat)
{

    auto program = MakeProgram(
        DeclVar(VAL_FLOAT, "f", Float(1.5f))
    );

    EXPECT_EQ(
        Generate(program),
R"(f = 1.5
)");
}

TEST(PyDeclVarBool)
{
    auto program = MakeProgram(
        DeclVar(VAL_BOOL, "flag", Bool(true))
    );

    EXPECT_EQ(
        Generate(program),
R"(flag = True
)");
}

TEST(PyBareAssignment)
{
    auto program = MakeProgram(
        DeclVar(VAL_INT, "x", Int(0)),
        ExprStatement(Assign("x", Int(5)))
    );

    EXPECT_EQ(
        Generate(program),
R"(x = 0
x = 5
)");
}

TEST(PyCallNoArgs)
{
    auto program = MakeProgram(
        Print(Call("foo"))
    );

    EXPECT_EQ(
        Generate(program),
R"(print(foo())
)");
}

TEST(PyCallMultipleArgs)
{
    auto program = MakeProgram(
        Print(Call("func", Int(1), Int(2), Int(3)))
    );

    EXPECT_EQ(
        Generate(program),
R"(print(func(1, 2, 3))
)");
}

TEST(PyNestedCall)
{
    auto program = MakeProgram(
        Print(
            Call(
                "fun1",
                Call("fun2", Int(1))
            )
        )
    );

    EXPECT_EQ(
        Generate(program),
R"(print(fun1(fun2(1)))
)");
}

TEST(PyCallInBinaryExpression)
{
    auto program = MakeProgram(
        Print(
            Add(
                Call("foo"),
                Int(5)
            )
        )
    );

    EXPECT_EQ(
        Generate(program),
R"(print((foo() + 5))
)");
}

TEST(PyCallComplexArguments)
{
    auto program = MakeProgram(
        Print(
            Call(
                "foo",
                Add(Int(1), Int(2)),
                Mul(Int(3), Int(4)),
                Not(Bool(false))
            )
        )
    );

    EXPECT_EQ(
        Generate(program),
R"(print(foo((1 + 2), (3 * 4), not (False)))
)");
}

TEST(PyMultipleTopLevelStatements)
{
    auto program = MakeProgram(
        DeclVar(VAL_INT, "x", Int(1)),
        DeclVar(VAL_INT, "y", Int(2)),
        Print(Add(Var("x", VAL_INT), Var("y", VAL_INT)))
    );

    EXPECT_EQ(
        Generate(program),
R"(x = 1
y = 2
print((x + y))
)");
}

TEST(PyFuncNoArgs)
{
    auto program = MakeProgram(
        Function(
            VAL_NONE,
            "myFun",
            { },
            Block (
                Print(Int(1))
            )
        ),
        ExprStatement(
            Call("myFun")
        )
    );

    EXPECT_EQ(
        Generate(program),
R"(def myFun():
    print(1)

myFun()
)");
}

TEST(PyFuncWithParameters)
{
    auto program = MakeProgram(
        Function(
            VAL_NONE,
            "printSum",
            { Param(VAL_INT, "a"), Param(VAL_INT, "b") },
            Block(
                Print(Add(Var("a", VAL_INT), Var("b", VAL_INT)))
            )
        ),
        ExprStatement(
            Call("printSum", Int(2), Int(3))
        )
    );

    EXPECT_EQ(
        Generate(program),
R"(def printSum(a, b):
    print((a + b))

printSum(2, 3)
)");
}

TEST(PyMultipleFunctions)
{
    auto program = MakeProgram(
        Function(
            VAL_NONE,
            "foo",
            {},
            Block(Print(Int(1)))
        ),
        Function(
            VAL_NONE,
            "bar",
            {},
            Block(Print(Int(2)))
        ),
        ExprStatement(Call("foo")),
        ExprStatement(Call("bar"))
    );

    EXPECT_EQ(
        Generate(program),
R"(def foo():
    print(1)

def bar():
    print(2)

foo()
bar()
)");
}

TEST(PyEmptyFunction)
{
    auto program = MakeProgram(
        Function(
            VAL_NONE,
            "foo",
            {},
            Block()
        )
    );

    EXPECT_EQ(
        Generate(program),
R"(def foo():
    ...

)");
}
