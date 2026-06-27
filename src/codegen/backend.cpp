#include "codegen/backend.hpp"

Error Emitter::Emit(const Program &program)
{
    TRY(EmitProgram(program));
    return Error::Ok;
}

void Emitter::Indent(std::ostream &stream)
{
    for (int i = 0; i < IndentLevel; ++i)
        stream << "    ";
}
