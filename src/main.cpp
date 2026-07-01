#include <iostream>

#include "codegen/codegen.hpp"
#include "util/macro.hpp"

int main()
{
    auto program = MakeProgram(
        ExprStatement(Call("fun1")),
        Function(
            VAL_NONE,
            "fun1",
            { },
            Block()
        ),
        ExprStatement(Int(1)),
        ExprStatement(Call("fun1"))
    );

    CppEmitter emitter(&std::cout);
    DISCARD(emitter.Emit(program));
}
