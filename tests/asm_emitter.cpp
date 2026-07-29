#include "test_runner.hpp"
#include "emitter_utils.hpp"

#define TEST_PREFIX Asm
#define EMITTER AsmEmitter

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

#undef TEST_PREFIX
#undef EMITTER
