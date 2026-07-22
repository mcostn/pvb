#pragma once

#include <sstream>
#include "codegen/codegen.hpp"
#include "util/macro.hpp"

template <typename Emitter>
std::string Generate(const Program &program)
{
    std::stringstream ss;
    Emitter emitter(&ss);
    EXPECT(emitter.Emit(program) == Error::Ok);
    return ss.str();
}

#define CODEGEN_TEST(Name, Ast, Expected) \
    TEST(CONCAT(TEST_PREFIX, Name)) \
    { \
        EXPECT_EQ( \
            Generate<EMITTER>(Ast), \
            Expected \
        ); \
    }

// Common ASTs
#define AST_PrintInteger \
    MakeProgram( \
        Print(Int(42)) \
    )

#define AST_PrintFloat \
    MakeProgram( \
        Print(Float(3.5)) \
    )

#define AST_PrintBool \
    MakeProgram( \
        Print(Bool(true)) \
    )

#define AST_PrintString \
    MakeProgram( \
        Print(String("Hello, World!")) \
    )

#define AST_PrintVariable \
    MakeProgram( \
        Print(Var("x", VAL_ANY)) \
    )

#define AST_PrintNoNewline \
    MakeProgram( \
        Print(Int(1), false), \
        Print(Int(2), false) \
    )

#define AST_ReadString \
    MakeProgram( \
        Read(Var("value", VAL_STRING)) \
    )

#define AST_ReadInt \
    MakeProgram( \
        Read(Var("value", VAL_INT)) \
    )

#define AST_ReadFloat \
    MakeProgram( \
        Read(Var("value", VAL_FLOAT)) \
    )

#define AST_ReadAny \
    MakeProgram( \
        Read(Var("value", VAL_ANY)) \
    )

#define AST_Negate \
    MakeProgram( \
        Print(Negate(Int(42))) \
    )

#define AST_Not \
    MakeProgram( \
        Print( \
            Not( \
                Bool(true) \
           ) \
        ) \
    )

#define AST_Addition \
    MakeProgram( \
        Print( \
            Add( \
                Int(5), \
                Int(10) \
           ) \
        ) \
    )

#define AST_Subtraction \
    MakeProgram( \
        Print( \
            Sub( \
                Int(5), \
                Int(10) \
           ) \
        ) \
    )

#define AST_Multiplication \
    MakeProgram( \
        Print( \
            Mul( \
                Int(5), \
                Int(10) \
           ) \
        ) \
    )

#define AST_Division \
    MakeProgram( \
        Print( \
            Div( \
                Int(5), \
                Int(10) \
           ) \
        ) \
    )

#define AST_Modulo \
    MakeProgram( \
        Print( \
            Mod( \
                Int(10), \
                Int(3) \
           ) \
        ) \
    )

#define AST_Less \
    MakeProgram( \
        Print( \
            Less( \
                Int(10), \
                Int(5) \
            ) \
        ) \
    )

#define AST_Greater \
    MakeProgram( \
        Print( \
            Greater( \
                Int(10), \
                Int(5) \
            ) \
        ) \
    )

#define AST_LessEqual \
    MakeProgram( \
        Print( \
            LessEqual( \
                Int(10), \
                Int(5) \
            ) \
        ) \
    )

#define AST_GreaterEqual \
    MakeProgram( \
        Print( \
            GreaterEqual( \
                Int(10), \
                Int(5) \
            ) \
        ) \
    )

#define AST_Equal \
    MakeProgram( \
        Print( \
            Equal( \
                Int(10), \
                Int(5) \
            ) \
        ) \
    )

#define AST_NotEqual \
    MakeProgram( \
        Print( \
            NotEqual( \
                Int(10), \
                Int(5) \
            ) \
        ) \
    )

#define AST_And \
    MakeProgram( \
        Print( \
            And( \
                Bool(true), \
                Bool(false) \
            ) \
        ) \
    )

#define AST_Or \
    MakeProgram( \
        Print( \
            Or( \
                Bool(true), \
                Bool(false) \
            ) \
        ) \
    )

#define AST_NestedExpression \
    MakeProgram( \
        Print( \
            Not( \
                Greater( \
                    Mul( \
                        Add(Int(5), Int(10)), \
                        Negate(Int(2)) \
                    ), \
                    Int(10) \
                ) \
            ) \
        ) \
    )

#define AST_IfBareBody \
    MakeProgram( \
        If(Bool(true), Print(Int(1))) \
    )

#define AST_IfNoElse \
    MakeProgram( \
        If( \
            Bool(true), \
            Block(Print(String("yes"))) \
        ) \
    )

#define AST_IfElse \
    MakeProgram( \
        If( \
            Bool(false), \
            Block(Print(String("then"))), \
            Block(Print(String("else"))) \
        ) \
    )

#define AST_NestedIf \
    MakeProgram( \
        If( \
            Bool(true), \
            Block( \
                If( \
                    Bool(false), \
                    Block(Print(String("inner"))) \
                ) \
            ) \
        ) \
    )

#define AST_Exit \
    MakeProgram( \
        Exit(Int(1)) \
    )

#define AST_WhileBareBody \
    MakeProgram( \
        While(Bool(true), Print(Int(1))) \
    )

#define AST_WhileBlockBody \
    MakeProgram( \
        While( \
            Bool(true), \
            Block(Print(String("running"))) \
        ) \
    )

#define AST_ForLoop \
    MakeProgram( \
        For( \
            DeclVar(VAL_INT, "i", Int(0)), \
            Less(Var("i", VAL_INT), Int(10)), \
            Assign("i", Add(Var("i", VAL_INT), Int(1))), \
            Block(Print(Var("i", VAL_INT))) \
        ) \
    )

#define AST_ForInfiniteNoCondNoUpdate \
    MakeProgram( \
        For( \
            nullptr, \
            nullptr, \
            nullptr, \
            Block(Print(String("looping"))) \
        ) \
    )

#define AST_Continue \
    MakeProgram( \
        For( \
            nullptr, \
            nullptr, \
            nullptr, \
            Block(Continue()) \
        ) \
    )

#define AST_Break \
    MakeProgram( \
        For( \
            nullptr, \
            nullptr, \
            nullptr, \
            Block(Break()) \
        ) \
    )


#define AST_DeclVarNoInitializer \
    MakeProgram( \
        DeclVar(VAL_INT, "x", nullptr) \
    )

#define AST_DeclVarInt \
    MakeProgram( \
        DeclVar(VAL_INT, "i", Int(1)) \
    )

#define AST_DeclVarFloat \
    MakeProgram( \
        DeclVar(VAL_FLOAT, "f", Float(1.5f)) \
    )

#define AST_DeclVarBool \
    MakeProgram( \
        DeclVar(VAL_BOOL, "flag", Bool(true)) \
    )

#define AST_DeclGlobal \
    MakeProgram( \
        DeclVar(VAL_INT, "num1", nullptr, VarScope::Global), \
        DeclVar(VAL_INT, "num2", nullptr, VarScope::Global) \
    )

#define AST_BareAssignment \
    MakeProgram( \
        DeclVar(VAL_INT, "x", Int(0)), \
        ExprStatement(Assign("x", Int(5))) \
    )

#define AST_AssignmentExpression \
    MakeProgram( \
        DeclVar(VAL_INT, "x", Int(0)), \
        Print( \
            Assign("x", Int(5)) \
        ) \
    )

#define AST_CallNoArgs \
    MakeProgram( \
        Print(Call("foo")) \
    )

#define AST_CallMultipleArgs \
    MakeProgram( \
        Print(Call("func", Int(1), Int(2), Int(3))) \
    )

#define AST_NestedCall \
    MakeProgram( \
        Print( \
            Call( \
                "fun1", \
                Call("fun2", Int(1)) \
            ) \
        ) \
    )

#define AST_CallInBinaryExpression \
    MakeProgram( \
        Print( \
            Add( \
                Call("foo"), \
                Int(5) \
            ) \
        ) \
    )

#define AST_CallComplexArguments \
    MakeProgram( \
        Print( \
            Call( \
                "foo", \
                Add(Int(1), Int(2)), \
                Mul(Int(3), Int(4)), \
                Not(Bool(false)) \
            ) \
        ) \
    )

#define AST_MultipleTopLevelStatements \
    MakeProgram( \
        DeclVar(VAL_INT, "x", Int(1)), \
        DeclVar(VAL_INT, "y", Int(2)), \
        Print(Add(Var("x", VAL_INT), Var("y", VAL_INT))) \
    )

#define AST_FuncNoArgs \
    MakeProgram( \
        Function( \
            VAL_NONE, \
            "myFun", \
            { }, \
            Block ( \
                Print(Int(1)) \
            ) \
        ), \
        ExprStatement( \
            Call("myFun") \
        ) \
    )

#define AST_FuncWithParameters \
    MakeProgram( \
        Function( \
            VAL_NONE, \
            "printSum", \
            { Param(VAL_INT, "a"), Param(VAL_INT, "b") }, \
            Block( \
                Print(Add(Var("a", VAL_INT), Var("b", VAL_INT))) \
            ) \
        ), \
        ExprStatement( \
            Call("printSum", Int(2), Int(3)) \
        ) \
    )

#define AST_MultipleFunctions \
    MakeProgram( \
        Function( \
            VAL_NONE, \
            "foo", \
            {}, \
            Block(Print(Int(1))) \
        ), \
        Function( \
            VAL_NONE, \
            "bar", \
            {}, \
            Block(Print(Int(2))) \
        ), \
        ExprStatement(Call("foo")), \
        ExprStatement(Call("bar")) \
    )

#define AST_StrLength \
    MakeProgram( \
        Print(Call(Builtin::Length, String("hello"))) \
    )

#define AST_StrCharAt \
    MakeProgram( \
        Print(Call(Builtin::CharAt, String("hello"), Int(1))) \
    )

#define AST_StrJoin \
    MakeProgram( \
        Print(Call(Builtin::Join, String("hello "), String("world"))) \
    )

#define AST_StrContains \
    MakeProgram( \
        Print(Call(Builtin::Contains, String("hello world"), String("world"))) \
    )

#define AST_EmptyFunction \
    MakeProgram( \
        Function( \
            VAL_NONE, \
            "foo", \
            {}, \
            Block() \
        ) \
    )
