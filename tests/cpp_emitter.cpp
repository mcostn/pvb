#include "test_runner.hpp"
#include "emitter_utils.hpp"

#define TEST_PREFIX Cpp
#define EMITTER CppEmitter


// Tests
CODEGEN_TEST(
    IncludeIostream,
    MakeProgram(
        Print(Int(1))
    ),
    R"(#include <iostream>
using namespace std;

int main()
{
    cout << 1 << endl;
}
)")

CODEGEN_TEST(
    IncludeCstdlib,
    MakeProgram(
        Exit(Int(0))
    ),
    R"(#include <cstdlib>
using namespace std;

int main()
{
    exit(0);
}
)")

CODEGEN_TEST(
    IncludeCmath,
    MakeProgram(
        Print(Call("sqrt", Int(9)))
    ),
    R"(#include <cmath>
#include <iostream>
using namespace std;

int main()
{
    cout << sqrt(9) << endl;
}
)")

CODEGEN_TEST(
    MultipleIncludes,
    MakeProgram(
        Print(Int(1)),
        Exit(Int(0)),
        Print(Call("sqrt", Int(16)))
    ),
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
)")

CODEGEN_TEST(
    DuplicateIostreamInclude,
    MakeProgram(
            Print(Int(1)),
            Print(Int(2)),
            Print(Int(3))
    ),
    R"(#include <iostream>
using namespace std;

int main()
{
    cout << 1 << endl;
    cout << 2 << endl;
    cout << 3 << endl;
}
)")

CODEGEN_TEST(
    DuplicateCmathInclude,
    MakeProgram(
        Print(Call("sqrt", Int(4))),
        Print(Call("sin", Int(0))),
        Print(Call("cos", Int(0)))
    ),
    R"(#include <cmath>
#include <iostream>
using namespace std;

int main()
{
    cout << sqrt(4) << endl;
    cout << sin(0) << endl;
    cout << cos(0) << endl;
}
)")

CODEGEN_TEST(
    FunctionGeneratesInclude,
    MakeProgram(
        Function(
            VAL_NONE,
            "foo",
            {},
            Block(
                Print(Call("sqrt", Int(25)))
            )
        )
    ),
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
)")

CODEGEN_TEST(
    PrintInteger,
    AST_PrintInteger,
    R"(#include <iostream>
using namespace std;

int main()
{
    cout << 42 << endl;
}
)")

CODEGEN_TEST(
    PrintFloat,
    AST_PrintFloat,
    R"(#include <iostream>
using namespace std;

int main()
{
    cout << 3.5 << endl;
}
)")

CODEGEN_TEST(
    PrintBool,
    AST_PrintBool,
R"(#include <iostream>
using namespace std;

int main()
{
    cout << true << endl;
}
)")

CODEGEN_TEST(
    PrintString,
    AST_PrintString,
R"(#include <iostream>
using namespace std;

int main()
{
    cout << "Hello, World!" << endl;
}
)")

CODEGEN_TEST(
    PrintVariable,
    AST_PrintVariable,
R"(#include <iostream>
using namespace std;

int main()
{
    cout << x << endl;
}
)")

CODEGEN_TEST(
    PrintNoNewline,
    AST_PrintNoNewline,
R"(#include <iostream>
using namespace std;

int main()
{
    cout << 1;
    cout << 2;
}
)")

CODEGEN_TEST(
    Negate,
    AST_Negate,
    R"(#include <iostream>
using namespace std;

int main()
{
    cout << -(42) << endl;
}
)")

CODEGEN_TEST(
    Not,
    AST_Not,
    R"(#include <iostream>
using namespace std;

int main()
{
    cout << !(true) << endl;
}
)")

CODEGEN_TEST(
    Addition,
    AST_Addition,
    R"(#include <iostream>
using namespace std;

int main()
{
    cout << (5 + 10) << endl;
}
)")

CODEGEN_TEST(
    Subtraction,
    AST_Subtraction,
    R"(#include <iostream>
using namespace std;

int main()
{
    cout << (5 - 10) << endl;
}
)")

CODEGEN_TEST(
    Multiplication,
    AST_Multiplication,
    R"(#include <iostream>
using namespace std;

int main()
{
    cout << (5 * 10) << endl;
}
)")

CODEGEN_TEST(
    Division,
    AST_Division,
    R"(#include <iostream>
using namespace std;

int main()
{
    cout << (5 / 10) << endl;
}
)")

CODEGEN_TEST(
    Modulo,
    AST_Modulo,
    R"(#include <iostream>
using namespace std;

int main()
{
    cout << (10 % 3) << endl;
}
)")

CODEGEN_TEST(
    Less,
    AST_Less,
    R"(#include <iostream>
using namespace std;

int main()
{
    cout << (10 < 5) << endl;
}
)")

CODEGEN_TEST(
    Greater,
    AST_Greater,
    R"(#include <iostream>
using namespace std;

int main()
{
    cout << (10 > 5) << endl;
}
)")

CODEGEN_TEST(
    LessEqual,
    AST_LessEqual,
    R"(#include <iostream>
using namespace std;

int main()
{
    cout << (10 <= 5) << endl;
}
)")

CODEGEN_TEST(
    GreaterEqual,
    AST_GreaterEqual,
    R"(#include <iostream>
using namespace std;

int main()
{
    cout << (10 >= 5) << endl;
}
)")

CODEGEN_TEST(
    Equal,
    AST_Equal,
    R"(#include <iostream>
using namespace std;

int main()
{
    cout << (10 == 5) << endl;
}
)")

CODEGEN_TEST(
    NotEqual,
    AST_NotEqual,
    R"(#include <iostream>
using namespace std;

int main()
{
    cout << (10 != 5) << endl;
}
)")

CODEGEN_TEST(
    And,
    AST_And,
    R"(#include <iostream>
using namespace std;

int main()
{
    cout << (true && false) << endl;
}
)")

CODEGEN_TEST(
    Or,
    AST_Or,
    R"(#include <iostream>
using namespace std;

int main()
{
    cout << (true || false) << endl;
}
)")

CODEGEN_TEST(
    NestedExpression,
    AST_NestedExpression,
    R"(#include <iostream>
using namespace std;

int main()
{
    cout << !((((5 + 10) * -(2)) > 10)) << endl;
}
)")

CODEGEN_TEST(
    IfBareBody,
    AST_IfBareBody,
    R"(#include <iostream>
using namespace std;

int main()
{
    if (true) cout << 1 << endl;
}
)")

CODEGEN_TEST(
    IfNoElse,
    AST_IfNoElse,
    R"(#include <iostream>
using namespace std;

int main()
{
    if (true) {
        cout << "yes" << endl;
    }
}
)")

CODEGEN_TEST(
    IfElse,
    AST_IfElse,
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
)")

CODEGEN_TEST(
    NestedIf,
    AST_NestedIf,
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
)")

CODEGEN_TEST(
    Exit,
    AST_Exit,
    R"(#include <cstdlib>
using namespace std;

int main()
{
    exit(1);
}
)")

CODEGEN_TEST(
    WhileBareBody,
    AST_WhileBareBody,
    R"(#include <iostream>
using namespace std;

int main()
{
    while (true) cout << 1 << endl;
}
)")

CODEGEN_TEST(
    WhileBlockBody,
    AST_WhileBlockBody,
    R"(#include <iostream>
using namespace std;

int main()
{
    while (true) {
        cout << "running" << endl;
    }
}
)")

CODEGEN_TEST(
    ForLoop,
    AST_ForLoop,
    R"(#include <iostream>
using namespace std;

int main()
{
    for (int i = 0; (i < 10); i = (i + 1)) {
        cout << i << endl;
    }
}
)")

CODEGEN_TEST(
    ForInfiniteNoCondNoUpdate,
    AST_ForInfiniteNoCondNoUpdate,
    R"(#include <iostream>
using namespace std;

int main()
{
    for (; ; ) {
        cout << "looping" << endl;
    }
}
)")

CODEGEN_TEST(
    DeclVarNoInitializer,
    AST_DeclVarNoInitializer,
    R"(int main()
{
    int x;
}
)")

CODEGEN_TEST(
    DeclVarInt,
    AST_DeclVarInt,
    R"(int main()
{
    int i = 1;
}
)")

CODEGEN_TEST(
    DeclVarFloat,
    AST_DeclVarFloat,
    R"(int main()
{
    float f = 1.5;
}
)")

CODEGEN_TEST(
    DeclVarBool,
    AST_DeclVarBool,
    R"(int main()
{
    bool flag = true;
}
)")

CODEGEN_TEST(
    BareAssignment,
    AST_BareAssignment,
    R"(int main()
{
    int x = 0;
    x = 5;
}
)")

CODEGEN_TEST(
    AssignmentExpression,
    AST_AssignmentExpression,
    R"(#include <iostream>
using namespace std;

int main()
{
    int x = 0;
    cout << x = 5 << endl;
}
)")

CODEGEN_TEST(
    CallNoArgs,
    AST_CallNoArgs,
    R"(#include <iostream>
using namespace std;

int main()
{
    cout << foo() << endl;
}
)")

CODEGEN_TEST(
    CallMultipleArgs,
    AST_CallMultipleArgs,
    R"(#include <iostream>
using namespace std;

int main()
{
    cout << func(1, 2, 3) << endl;
}
)")

CODEGEN_TEST(
    NestedCall,
    AST_NestedCall,
    R"(#include <iostream>
using namespace std;

int main()
{
    cout << fun1(fun2(1)) << endl;
}
)")

CODEGEN_TEST(
    CallInBinaryExpression,
    AST_CallInBinaryExpression,
    R"(#include <iostream>
using namespace std;

int main()
{
    cout << (foo() + 5) << endl;
}
)")

CODEGEN_TEST(
    CallComplexArguments,
    AST_CallComplexArguments,
    R"(#include <iostream>
using namespace std;

int main()
{
    cout << foo((1 + 2), (3 * 4), !(false)) << endl;
}
)")

CODEGEN_TEST(
    MultipleTopLevelStatements,
    AST_MultipleTopLevelStatements,
    R"(#include <iostream>
using namespace std;

int main()
{
    int x = 1;
    int y = 2;
    cout << (x + y) << endl;
}
)")

CODEGEN_TEST(
    FuncNoArgs,
    AST_FuncNoArgs,
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
)")

CODEGEN_TEST(
    FuncWithParameters,
    AST_FuncWithParameters,
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
)")

CODEGEN_TEST(
    MultipleFunctions,
    AST_MultipleFunctions,
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
)")

CODEGEN_TEST(
    EmptyFunction,
    AST_EmptyFunction,
    R"(void foo();

int main()
{
}

void foo()
{
}
)")


#undef TEST_PREFIX
#undef EMITTER
