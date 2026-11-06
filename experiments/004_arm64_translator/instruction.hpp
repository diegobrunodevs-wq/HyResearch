#pragma once

#include <string>
#include <vector>

namespace Hyrule
{
    enum class Operation
    {
        MOV,
        ADD,
        SUB,
        MUL,
        RET,
        UNKNOWN
    };

    struct Instruction
    {
        Operation operation = Operation::UNKNOWN;

        std::vector<std::string> operands;

        std::string original;
    };
}