#include <sstream>
#include "test_runner.hpp"
#include "codegen/codegen.hpp"

// Utility Function
static std::string Generate(const Program& program)
{
    std::stringstream ss;
    LuaEmitter emitter(ss);
    EXPECT(emitter.EmitProgram(program) == Error::Ok);
    return ss.str();
}

// Tests
TEST(LuaPrintInteger)
{
    auto program = MakeProgram(
        Print(Int(42))
    );

    EXPECT_EQ(
        Generate(program),
R"(io.write(tostring(42) .. "\n")
)");
}

TEST(LuaPrintString)
{
    auto program = MakeProgram(
        Print(String("Hello"))
    );

    EXPECT_EQ(
        Generate(program),
R"(io.write(tostring("Hello") .. "\n")
)");
}

TEST(LuaPrintBool)
{
    auto program = MakeProgram(
        Print(Bool(true))
    );

    EXPECT_EQ(
        Generate(program),
R"(io.write(tostring(true) .. "\n")
)");
}

TEST(LuaPrintNoNewline)
{
    auto program = MakeProgram(
        Print(Int(1), false)
    );

    EXPECT_EQ(
        Generate(program),
R"(io.write(tostring(1))
)");
}

TEST(LuaNegate)
{
    auto program = MakeProgram(
        Print(Negate(Int(42)))
    );

    EXPECT_EQ(
        Generate(program),
R"(io.write(tostring(-(42)) .. "\n")
)");
}

TEST(LuaNot)
{
    auto program = MakeProgram(
        Print(Not(Bool(true)))
    );

    EXPECT_EQ(
        Generate(program),
R"(io.write(tostring(not (true)) .. "\n")
)");
}

TEST(LuaAddition)
{
    auto program = MakeProgram(
        Print(Add(Int(5), Int(10)))
    );

    EXPECT_EQ(
        Generate(program),
R"(io.write(tostring((5 + 10)) .. "\n")
)");
}

TEST(LuaComparison)
{
    auto program = MakeProgram(
        Print(Less(Int(5), Int(10)))
    );

    EXPECT_EQ(
        Generate(program),
R"(io.write(tostring((5 < 10)) .. "\n")
)");
}

TEST(LuaAnd)
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
R"(io.write(tostring((true and false)) .. "\n")
)");
}

TEST(LuaOr)
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
R"(io.write(tostring((true or false)) .. "\n")
)");
}

TEST(LuaVariable)
{
    auto program = MakeProgram(
        Print(Var("x", VAL_INT))
    );

    EXPECT_EQ(
        Generate(program),
R"(io.write(tostring(x) .. "\n")
)");
}

TEST(LuaAssignment)
{
    auto program = MakeProgram(
        ExprStatement(
            Assign("x", Int(5))
        )
    );

    EXPECT_EQ(
        Generate(program),
R"(x = 5
)");
}

TEST(LuaDeclVar)
{
    auto program = MakeProgram(
        DeclVar(VAL_INT, "x", Int(10))
    );

    EXPECT_EQ(
        Generate(program),
R"(local x = 10
)");
}

TEST(LuaDeclVarNoInitializer)
{
    auto program = MakeProgram(
        DeclVar(VAL_INT, "x", nullptr)
    );

    EXPECT_EQ(
        Generate(program),
R"(local x = nil
)");
}

TEST(LuaBlock)
{
    auto program = MakeProgram(
        Block(
            Print(Int(1))
        )
    );

    EXPECT_EQ(
        Generate(program),
R"(do
    io.write(tostring(1) .. "\n")
end
)");
}

TEST(LuaIf)
{
    auto program = MakeProgram(
        If(
            Bool(true),
            Print(Int(1))
        )
    );

    EXPECT_EQ(
        Generate(program),
R"(if true then
    io.write(tostring(1) .. "\n")
end
)");
}

TEST(LuaIfElse)
{
    auto program = MakeProgram(
        If(
            Bool(true),
            Print(Int(1)),
            Print(Int(2))
        )
    );

    EXPECT_EQ(
        Generate(program),
R"(if true then
    io.write(tostring(1) .. "\n")
else
    io.write(tostring(2) .. "\n")
end
)");
}

TEST(LuaWhile)
{
    auto program = MakeProgram(
        While(
            Bool(true),
            Print(Int(1))
        )
    );

    EXPECT_EQ(
        Generate(program),
R"(while true do
    io.write(tostring(1) .. "\n")
end
)");
}

TEST(LuaExit)
{
    auto program = MakeProgram(
        Exit(Int(1))
    );

    EXPECT_EQ(
        Generate(program),
R"(os.exit(1)
)");
}

TEST(LuaCallNoArgs)
{
    auto program = MakeProgram(
        ExprStatement(
            Call("foo")
        )
    );

    EXPECT_EQ(
        Generate(program),
R"(foo()
)");
}

TEST(LuaCallMultipleArgs)
{
    auto program = MakeProgram(
        ExprStatement(
            Call(
                "max",
                Int(1),
                Int(2),
                Int(3)
            )
        )
    );

    EXPECT_EQ(
        Generate(program),
R"(max(1, 2, 3)
)");
}

TEST(LuaNestedCall)
{
    auto program = MakeProgram(
        Print(
            Call(
                "foo",
                Call("bar", Int(1))
            )
        )
    );

    EXPECT_EQ(
        Generate(program),
R"(io.write(tostring(foo(bar(1))) .. "\n")
)");
}
