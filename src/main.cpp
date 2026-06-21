#include <iostream>

#include "codegen/backend.hpp"
#include "block/block.hpp"
#include "util/macro.hpp"

int main()
{
    Program program;
    auto print = std::make_unique<PrintStmt>();
    print->Newline = true;
    auto lit = std::make_unique<LiteralExpr>(VAL_INT);
    lit->Data = 42;
    print->Data = std::move(lit);
    program.Statements.push_back(std::move(print));

    CppEmitter emitter(std::cout);
    DISCARD(emitter.Emit(program));
}
