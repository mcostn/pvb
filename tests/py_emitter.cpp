#include "test_runner.hpp"
#include "emitter_utils.hpp"

#define TEST_PREFIX Py
#define EMITTER PythonEmitter
// Tests
CODEGEN_TEST(
    PrintInteger,
    AST_PrintInteger,
    R"(print(42)
)")

CODEGEN_TEST(
    PrintFloat,
    AST_PrintFloat,
    R"(print(3.5)
)")

CODEGEN_TEST(
    PrintBool,
    AST_PrintBool,
    R"(print(True)
)")

CODEGEN_TEST(
    PrintString,
    AST_PrintString,
    R"(print("Hello, World!")
)")

CODEGEN_TEST(
    PrintNoNewline,
    AST_PrintNoNewline,
    R"(print(1, end='')
print(2, end='')
)")

CODEGEN_TEST(
    ReadString,
    AST_ReadString,
R"(value = input()
)")

CODEGEN_TEST(
    ReadInt,
    AST_ReadInt,
R"(value = int(input())
)")

CODEGEN_TEST(
    ReadFloat,
    AST_ReadFloat,
R"(value = float(input())
)")

CODEGEN_TEST(
    ReadAny,
    AST_ReadAny,
R"(value = input()
)")

CODEGEN_TEST(
    Negate,
    AST_Negate,
    R"(print(-(42))
)")

CODEGEN_TEST(
    Not,
    AST_Not,
    R"(print(not (True))
)")

CODEGEN_TEST(
    Addition,
    AST_Addition,
    R"(print((5 + 10))
)")

CODEGEN_TEST(
    Subtraction,
    AST_Subtraction,
    R"(print((5 - 10))
)")

CODEGEN_TEST(
    Multiplication,
    AST_Multiplication,
    R"(print((5 * 10))
)")

CODEGEN_TEST(
    Division,
    AST_Division,
    R"(print((5 / 10))
)")

CODEGEN_TEST(
    Modulo,
    AST_Modulo,
    R"(print((10 % 3))
)")

CODEGEN_TEST(
    Less,
    AST_Less,
    R"(print((10 < 5))
)")

CODEGEN_TEST(
    Greater,
    AST_Greater,
    R"(print((10 > 5))
)")

CODEGEN_TEST(
    LessEqual,
    AST_LessEqual,
    R"(print((10 <= 5))
)")

CODEGEN_TEST(
    GreaterEqual,
    AST_GreaterEqual,
    R"(print((10 >= 5))
)")

CODEGEN_TEST(
    Equal,
    AST_Equal,
    R"(print((10 == 5))
)")

CODEGEN_TEST(
    NotEqual,
    AST_NotEqual,
    R"(print((10 != 5))
)")

CODEGEN_TEST(
    And,
    AST_And,
    R"(print((True and False))
)")

CODEGEN_TEST(
    Or,
    AST_Or,
    R"(print((True or False))
)")

CODEGEN_TEST(
    NestedExpression,
    AST_NestedExpression,
    R"(print(not ((((5 + 10) * -(2)) > 10)))
)")

CODEGEN_TEST(
    IfBareBody,
    AST_IfBareBody,
    R"(if True: print(1)
)")

CODEGEN_TEST(
    IfNoElse,
    AST_IfNoElse,
    R"(if True:
    print("yes")
)")

CODEGEN_TEST(
    IfElse,
    AST_IfElse,
    R"(if False:
    print("then")
else:
    print("else")
)")

CODEGEN_TEST(
    NestedIf,
    AST_NestedIf,
    R"(if True:
    if False:
        print("inner")
)");

CODEGEN_TEST(
    Exit,
    AST_Exit,
    R"(import sys

sys.exit(1)
)")

CODEGEN_TEST(
    WhileBareBody,
    AST_WhileBareBody,
    R"(while True: print(1)
)")

CODEGEN_TEST(
    WhileBlockBody,
    AST_WhileBlockBody,
    R"(while True:
    print("running")
)")

CODEGEN_TEST(
    ForLoop,
    AST_ForLoop,
    R"(i = 0
while (i < 10):
    print(i)
    i = (i + 1)
)")

CODEGEN_TEST(
    ForInfiniteNoCondNoUpdate,
    AST_ForInfiniteNoCondNoUpdate,
    R"(while True:
    print("looping")
)")

CODEGEN_TEST(
    DeclVarNoInitializer,
    AST_DeclVarNoInitializer,
    R"(x = None
)")

CODEGEN_TEST(
    DeclVarInt,
    AST_DeclVarInt,
    R"(i = 1
)")

CODEGEN_TEST(
    DeclVarFloat,
    AST_DeclVarFloat,
    R"(f = 1.5
)")

CODEGEN_TEST(
    DeclVarBool,
    AST_DeclVarBool,
    R"(flag = True
)")

CODEGEN_TEST(
    BareAssignment,
    AST_BareAssignment,
    R"(x = 0
x = 5
)")

CODEGEN_TEST(
    CallNoArgs,
    AST_CallNoArgs,
    R"(print(foo())
)")

CODEGEN_TEST(
    CallMultipleArgs,
    AST_CallMultipleArgs,
    R"(print(func(1, 2, 3))
)")

CODEGEN_TEST(
    NestedCall,
    AST_NestedCall,
    R"(print(fun1(fun2(1)))
)")

CODEGEN_TEST(
    CallInBinaryExpression,
    AST_CallInBinaryExpression,
    R"(print((foo() + 5))
)")

CODEGEN_TEST(
    CallComplexArguments,
    AST_CallComplexArguments,
    R"(print(foo((1 + 2), (3 * 4), not (False)))
)")

CODEGEN_TEST(
    MultipleTopLevelStatements,
    AST_MultipleTopLevelStatements,
    R"(x = 1
y = 2
print((x + y))
)")

CODEGEN_TEST(
    FuncNoArgs,
    AST_FuncNoArgs,
    R"(def myFun():
    print(1)

myFun()
)")

CODEGEN_TEST(
    FuncWithParameters,
    AST_FuncWithParameters,
    R"(def printSum(a, b):
    print((a + b))

printSum(2, 3)
)")

CODEGEN_TEST(
    MultipleFunctions,
    AST_MultipleFunctions,
    R"(def foo():
    print(1)
def bar():
    print(2)

foo()
bar()
)")

CODEGEN_TEST(
    EmptyFunction,
    AST_EmptyFunction,
    R"(def foo():
    ...

)")

#undef TEST_PREFIX
#undef EMITTER
