
#include "native_code_generator.hpp"

#include <stdexcept>
#include <string>

namespace Hyrule
{
    std::uint8_t NativeCodeGenerator::registerOffset(
        const std::string& name
    ) const
    {
        if (name.size() != 2 ||
            name[0] != 'X' ||
            name[1] < '0' ||
            name[1] > '7')
        {
            throw std::invalid_argument(
                "Registrador ARM64 nao suportado: " + name
            );
        }

        const auto index = static_cast<std::uint8_t>(name[1] - '0');

        return static_cast<std::uint8_t>(index * 8);
    }

    NativeCode NativeCodeGenerator::generateMov(
        const Instruction& instruction
    ) const
    {
        if (instruction.operands.size() != 2)
            throw std::invalid_argument("MOV exige 2 operandos");

        const auto destination = registerOffset(instruction.operands[0]);
        const auto source = registerOffset(instruction.operands[1]);

        NativeCode code;

        // mov rax, qword ptr [rcx + source]
        code.push_back(0x48);
        code.push_back(0x8B);

        if (source == 0)
        {
            code.push_back(0x01);
        }
        else
        {
            code.push_back(0x41);
            code.push_back(source);
        }

        // mov qword ptr [rcx + destination], rax
        code.push_back(0x48);
        code.push_back(0x89);

        if (destination == 0)
        {
            code.push_back(0x01);
        }
        else
        {
            code.push_back(0x41);
            code.push_back(destination);
        }

        return code;
    }

    NativeCode NativeCodeGenerator::generate(
        const Instruction& instruction
    ) const
    {
        switch (instruction.operation)
        {
            case Operation::MOV:
                return generateMov(instruction);

            default:
                throw std::invalid_argument(
                    "Instrucao ainda nao suportada pelo gerador nativo"
                );
        }
    }
}
