#include <sstream>
#include "test_runner.hpp"
#include "codegen/codegen.hpp"

// Utility Function
static std::string Generate(const Program& program)
{
    std::stringstream ss;
    CppEmitter emitter(&ss);
    EXPECT(emitter.Emit(program) == Error::Ok);
    return ss.str();
}

// Tests
TEST(CppIncludeIostream)
{
    auto program = MakeProgram(
        Print(Int(1))
    );

    EXPECT_EQ(
        Generate(program),
R"(#include <iostream>
using namespace std;

int main()
{
    cout << 1 << endl;
}
)");
}

TEST(CppIncludeCstdlib)
{
    auto program = MakeProgram(
        Exit(Int(0))
    );

    EXPECT_EQ(
        Generate(program),
R"(#include <cstdlib>
using namespace std;

int main()
{
    exit(0);
}
)");
}

TEST(CppIncludeCmath)
{
    auto program = MakeProgram(
        Print(Call("sqrt", Int(9)))
    );

    EXPECT_EQ(
        Generate(program),
R"(#include <cmath>
#include <iostream>
using namespace std;

int main()
{
    cout << sqrt(9) << endl;
}
)");
}

TEST(CppMultipleIncludes)
{
    auto program = MakeProgram(
        Print(Int(1)),
        Exit(Int(0)),
        Print(Call("sqrt", Int(16)))
    );

    EXPECT_EQ(
        Generate(program),
R"(#include <cmath>
#include <cstdlib>
#include <iostream>
using namespace std;

int main()
{
    cout << 1 << endl;
    exit(0);
    cout << sqrt(16) << endl;
}
)");
}

TEST(CppDuplicateIostreamInclude)
{
    auto program = MakeProgram(
        Print(Int(1)),
        Print(Int(2)),
        Print(Int(3))
    );

    EXPECT_EQ(
        Generate(program),
R"(#include <iostream>
using namespace std;

int main()
{
    cout << 1 << endl;
    cout << 2 << endl;
    cout << 3 << endl;
}
)");
}

TEST(CppDuplicateCmathInclude)
{
    auto program = MakeProgram(
        Print(Call("sqrt", Int(4))),
        Print(Call("sin", Int(0))),
        Print(Call("cos", Int(0)))
    );

    EXPECT_EQ(
        Generate(program),
R"(#include <cmath>
#include <iostream>
using namespace std;

int main()
{
    cout << sqrt(4) << endl;
    cout << sin(0) << endl;
    cout << cos(0) << endl;
}
)");
}

TEST(CppFunctionGeneratesInclude)
{
    auto program = MakeProgram(
        Function(
            VAL_NONE,
            "foo",
            {},
            Block(
                Print(Call("sqrt", Int(25)))
            )
        )
    );

    EXPECT_EQ(
        Generate(program),
R"(#include <cmath>
#include <iostream>
using namespace std;

void foo();

int main()
{
}

void foo()
{
    cout << sqrt(25) << endl;
}
)");
}

TEST(CppPrintInteger)
{
    auto program = MakeProgram(
        Print(Int(42))
    );

    EXPECT_EQ(
        Generate(program),
R"(#include <iostream>
using namespace std;

int main()
{
    cout << 42 << endl;
}
)");
}

TEST(CppPrintFloat)
{
    auto program = MakeProgram(
        Print(Float(3.5))
    );

    EXPECT_EQ(
        Generate(program),
R"(#include <iostream>
using namespace std;

int main()
{
    cout << 3.5 << endl;
}
)");
}


TEST(CppPrintBool)
{
    auto program = MakeProgram(
        Print(Bool(true))
    );

    EXPECT_EQ(
        Generate(program),
R"(#include <iostream>
using namespace std;

int main()
{
    cout << true << endl;
}
)");
}

TEST(CppPrintString)
{
    auto program = MakeProgram(
        Print(String("Hello, World!"))
    );

    EXPECT_EQ(
        Generate(program),
R"(#include <iostream>
using namespace std;

int main()
{
    cout << "Hello, World!" << endl;
}
)");
}

TEST(CppPrintVariable)
{
    auto program = MakeProgram(
        Print(Var("x", VAL_ANY))
    );

    EXPECT_EQ(
        Generate(program),
R"(#include <iostream>
using namespace std;

int main()
{
    cout << x << endl;
}
)");
}

TEST(CppPrintNoNewline)
{
    auto program = MakeProgram(
        Print(Int(1), false),
        Print(Int(2), false)
    );

    EXPECT_EQ(
        Generate(program),
R"(#include <iostream>
using namespace std;

int main()
{
    cout << 1;
    cout << 2;
}
)");
}

TEST(CppNegate)
{
    auto program = MakeProgram(
        Print(Negate(Int(42)))
    );

    EXPECT_EQ(
        Generate(program),
R"(#include <iostream>
using namespace std;

int main()
{
    cout << -(42) << endl;
}
)");
}

TEST(CppNot)
{
    auto program = MakeProgram(
        Print(Not(Bool(true)))
    );

    EXPECT_EQ(
        Generate(program),
R"(#include <iostream>
using namespace std;

int main()
{
    cout << !(true) << endl;
}
)");
}

TEST(CppAddition)
{
    auto program = MakeProgram(
        Print(
            Add(
                Int(5),
                Int(10)
           )
        )
    );

    EXPECT_EQ(
        Generate(program),
R"(#include <iostream>
using namespace std;

int main()
{
    cout << (5 + 10) << endl;
}
)");
}

TEST(CppSubtraction)
{
    auto program = MakeProgram(
        Print(
            Sub(
                Int(5),
                Int(10)
           )
        )
    );

    EXPECT_EQ(
        Generate(program),
R"(#include <iostream>
using namespace std;

int main()
{
    cout << (5 - 10) << endl;
}
)");
}

TEST(CppMultiplication)
{
    auto program = MakeProgram(
        Print(
            Mul(
                Int(5),
                Int(10)
           )
        )
    );

    EXPECT_EQ(
        Generate(program),
R"(#include <iostream>
using namespace std;

int main()
{
    cout << (5 * 10) << endl;
}
)");
}

TEST(CppDivision)
{
    auto program = MakeProgram(
        Print(
            Div(
                Int(5),
                Int(10)
           )
        )
    );

    EXPECT_EQ(
        Generate(program),
R"(#include <iostream>
using namespace std;

int main()
{
    cout << (5 / 10) << endl;
}
)");
}

TEST(CppModulo)
{
    auto program = MakeProgram(
        Print(
            Mod(
                Int(10),
                Int(3)
           )
        )
    );

    EXPECT_EQ(
        Generate(program),
R"(#include <iostream>
using namespace std;

int main()
{
    cout << (10 % 3) << endl;
}
)");
}

TEST(CppLess)
{
    auto program = MakeProgram(
        Print(
            Less(
                Int(10),
                Int(5)
            )
        )
    );

    EXPECT_EQ(
        Generate(program),
R"(#include <iostream>
using namespace std;

int main()
{
    cout << (10 < 5) << endl;
}
)");
}

TEST(CppGreater)
{
    auto program = MakeProgram(
        Print(
            Greater(
                Int(10),
                Int(5)
            )
        )
    );

    EXPECT_EQ(
        Generate(program),
R"(#include <iostream>
using namespace std;

int main()
{
    cout << (10 > 5) << endl;
}
)");
}

TEST(CppLessEqual)
{
    auto program = MakeProgram(
        Print(
            LessEqual(
                Int(10),
                Int(5)
            )
        )
    );

    EXPECT_EQ(
        Generate(program),
R"(#include <iostream>
using namespace std;

int main()
{
    cout << (10 <= 5) << endl;
}
)");
}

TEST(CppGreaterEqual)
{
    auto program = MakeProgram(
        Print(
            GreaterEqual(
                Int(10),
                Int(5)
            )
        )
    );

    EXPECT_EQ(
        Generate(program),
R"(#include <iostream>
using namespace std;

int main()
{
    cout << (10 >= 5) << endl;
}
)");
}

TEST(CppEqual)
{
    auto program = MakeProgram(
        Print(
            Equal(
                Int(10),
                Int(5)
            )
        )
    );

    EXPECT_EQ(
        Generate(program),
R"(#include <iostream>
using namespace std;

int main()
{
    cout << (10 == 5) << endl;
}
)");
}

TEST(CppNotEqual)
{
    auto program = MakeProgram(
        Print(
            NotEqual(
                Int(10),
                Int(5)
            )
        )
    );

    EXPECT_EQ(
        Generate(program),
R"(#include <iostream>
using namespace std;

int main()
{
    cout << (10 != 5) << endl;
}
)");
}

TEST(CppAnd)
{
    auto program = MakeProgram(
        Print(
            And(
                Bool(true),
                Bool(false)
            )
        )
    );

    EXPECT_EQ(
        Generate(program),
R"(#include <iostream>
using namespace std;

int main()
{
    cout << (true && false) << endl;
}
)");
}

TEST(CppOr)
{
    auto program = MakeProgram(
        Print(
            Or(
                Bool(true),
                Bool(false)
            )
        )
    );

    EXPECT_EQ(
        Generate(program),
R"(#include <iostream>
using namespace std;

int main()
{
    cout << (true || false) << endl;
}
)");
}

TEST(CppNestedExpression)
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
R"(#include <iostream>
using namespace std;

int main()
{
    cout << !((((5 + 10) * -(2)) > 10)) << endl;
}
)");
}

TEST(CppIfBareBody)
{
    auto program = MakeProgram(
        If(Bool(true), Print(Int(1)))
    );

    EXPECT_EQ(
        Generate(program),
R"(#include <iostream>
using namespace std;

int main()
{
    if (true) cout << 1 << endl;
}
)");
}

TEST(CppIfNoElse)
{
    auto program = MakeProgram(
        If(
            Bool(true),
            Block(Print(String("yes")))
        )
    );

    EXPECT_EQ(
        Generate(program),
R"(#include <iostream>
using namespace std;

int main()
{
    if (true) {
        cout << "yes" << endl;
    }
}
)");
}

TEST(CppIfElse)
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
R"(#include <iostream>
using namespace std;

int main()
{
    if (false) {
        cout << "then" << endl;
    } else {
        cout << "else" << endl;
    }
}
)");
}

TEST(CppNestedIf)
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
R"(#include <iostream>
using namespace std;

int main()
{
    if (true) {
        if (false) {
            cout << "inner" << endl;
        }
    }
}
)");
}

TEST(CppExit)
{
    auto program = MakeProgram(
        Exit(Int(1))
    );

    EXPECT_EQ(
        Generate(program),
R"(#include <cstdlib>
using namespace std;

int main()
{
    exit(1);
}
)");
}

TEST(CppWhileBareBody)
{
    auto program = MakeProgram(
        While(Bool(true), Print(Int(1)))
    );

    EXPECT_EQ(Generate(program),
R"(#include <iostream>
using namespace std;

int main()
{
    while (true) cout << 1 << endl;
}
)");
}

TEST(CppWhileBlockBody)
{
    auto program = MakeProgram(
        While(
            Bool(true),
            Block(Print(String("running")))
        )
    );

    EXPECT_EQ(
        Generate(program),
R"(#include <iostream>
using namespace std;

int main()
{
    while (true) {
        cout << "running" << endl;
    }
}
)");
}

TEST(CppForLoop)
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
R"(#include <iostream>
using namespace std;

int main()
{
    for (int i = 0; (i < 10); i = (i + 1)) {
        cout << i << endl;
    }
}
)");
}

TEST(CppForInfiniteNoCondNoUpdate)
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
R"(#include <iostream>
using namespace std;

int main()
{
    for (; ; ) {
        cout << "looping" << endl;
    }
}
)");
}

TEST(CppDeclVarNoInitializer)
{
    auto program = MakeProgram(
        DeclVar(VAL_INT, "x", nullptr)
    );

    EXPECT_EQ(
        Generate(program),
R"(int main()
{
    int x;
}
)");
}

TEST(CppDeclVarInt)
{
    auto program = MakeProgram(
        DeclVar(VAL_INT, "i", Int(1))
    );

    EXPECT_EQ(
        Generate(program),
R"(int main()
{
    int i = 1;
}
)");
}

TEST(CppDeclVarFloat)
{
    auto program = MakeProgram(
        DeclVar(VAL_FLOAT, "f", Float(1.5f))
    );

    EXPECT_EQ(
        Generate(program),
R"(int main()
{
    float f = 1.5;
}
)");
}

TEST(CppDeclVarBool)
{
    auto program = MakeProgram(
        DeclVar(VAL_BOOL, "flag", Bool(true))
    );

    EXPECT_EQ(
        Generate(program),
R"(int main()
{
    bool flag = true;
}
)");
}

TEST(CppBareAssignment)
{
    auto program = MakeProgram(
        DeclVar(VAL_INT, "x", Int(0)),
        ExprStatement(Assign("x", Int(5)))
    );

    EXPECT_EQ(
        Generate(program),
R"(int main()
{
    int x = 0;
    x = 5;
}
)");
}

TEST(CppAssignmentExpression)
{
    auto program = MakeProgram(
        DeclVar(VAL_INT, "x", Int(0)),
        Print(
            Assign("x", Int(5))
        )
    );

    EXPECT_EQ(
        Generate(program),
R"(#include <iostream>
using namespace std;

int main()
{
    int x = 0;
    cout << x = 5 << endl;
}
)");
}

TEST(CppCallNoArgs)
{
    auto program = MakeProgram(
        Print(Call("foo"))
    );

    EXPECT_EQ(
        Generate(program),
R"(#include <iostream>
using namespace std;

int main()
{
    cout << foo() << endl;
}
)");
}

TEST(CppCallMultipleArgs)
{
    auto program = MakeProgram(
        Print(Call("func", Int(1), Int(2), Int(3)))
    );

    EXPECT_EQ(
        Generate(program),
R"(#include <iostream>
using namespace std;

int main()
{
    cout << func(1, 2, 3) << endl;
}
)");
}

TEST(CppNestedCall)
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
R"(#include <iostream>
using namespace std;

int main()
{
    cout << fun1(fun2(1)) << endl;
}
)");
}

TEST(CppCallInBinaryExpression)
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
R"(#include <iostream>
using namespace std;

int main()
{
    cout << (foo() + 5) << endl;
}
)");
}

TEST(CppCallComplexArguments)
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
R"(#include <iostream>
using namespace std;

int main()
{
    cout << foo((1 + 2), (3 * 4), !(false)) << endl;
}
)");
}

TEST(CppMultipleTopLevelStatements)
{
    auto program = MakeProgram(
        DeclVar(VAL_INT, "x", Int(1)),
        DeclVar(VAL_INT, "y", Int(2)),
        Print(Add(Var("x", VAL_INT), Var("y", VAL_INT)))
    );

    EXPECT_EQ(
        Generate(program),
R"(#include <iostream>
using namespace std;

int main()
{
    int x = 1;
    int y = 2;
    cout << (x + y) << endl;
}
)");
}

TEST(CppFuncNoArgs)
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
R"(#include <iostream>
using namespace std;

void myFun();

int main()
{
    myFun();
}

void myFun()
{
    cout << 1 << endl;
}
)");
}

TEST(CppFuncWithParameters)
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
R"(#include <iostream>
using namespace std;

void printSum(int a, int b);

int main()
{
    printSum(2, 3);
}

void printSum(int a, int b)
{
    cout << (a + b) << endl;
}
)");
}

TEST(CppMultipleFunctions)
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
R"(#include <iostream>
using namespace std;

void foo();
void bar();

int main()
{
    foo();
    bar();
}

void foo()
{
    cout << 1 << endl;
}
void bar()
{
    cout << 2 << endl;
}
)");
}

TEST(CppEmptyFunction)
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
R"(void foo();

int main()
{
}

void foo()
{
}
)");
}
