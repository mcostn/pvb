#pragma once

#include "codegen/frontend.hpp"
#include "util/error.hpp"

class Emitter
{
public:
    explicit Emitter(std::ostream &out) : Out(out) {}
    virtual ~Emitter() = default;

    Error Emit(const Program &program);
    virtual Error EmitProgram(const Program &program) = 0;

    void Indent(std::ostream &stream);
    void Indent() { Indent(Out); };

    std::ostream &Out;
    int IndentLevel = 0;
};
