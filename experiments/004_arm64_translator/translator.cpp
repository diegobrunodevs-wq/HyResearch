
#include "translator.hpp"

#include <stdexcept>
#include <unordered_map>

namespace Hyrule
{
    std::string Translator::translateRegister(const std::string& armRegister)
    {
        static const std::unordered_map<std::string, std::string> registers =
        {
            {"X0", "rax"},
            {"X1", "rbx"},
            {"X2", "rcx"},
            {"X3", "rdx"},
            {"X4", "rsi"},
            {"X5", "rdi"},
            {"X6", "r8"},
            {"X7", "r9"}
        };

        const auto it = registers.find(armRegister);

        if (it == registers.end())
        {
            throw std::invalid_argument(
                "Registrador ARM64 nao suportado: " + armRegister
            );
        }

        return it->second;
    }

    std::string Translator::translate(const Instruction& instruction)
    {
        const auto& operands = instruction.operands;

        switch (instruction.operation)
        {
            case Operation::MOV:
            {
                if (operands.size() != 2)
                    throw std::invalid_argument("MOV exige 2 operandos");

                const auto destination = translateRegister(operands[0]);
                const auto source = translateRegister(operands[1]);

                return "mov " + destination + ", " + source;
            }

            case Operation::ADD:
            case Operation::SUB:
            case Operation::MUL:
            {
                if (operands.size() != 3)
                    throw std::invalid_argument(
                        "Operacao aritmetica exige 3 operandos"
                    );

                const auto destination = translateRegister(operands[0]);
                const auto source1 = translateRegister(operands[1]);
                const auto source2 = translateRegister(operands[2]);

                const std::string operation =
                    instruction.operation == Operation::ADD ? "add" :
                    instruction.operation == Operation::SUB ? "sub" :
                    "imul";

                return
                    "mov r10, " + source1 + "\n" +
                    operation + " r10, " + source2 + "\n" +
                    "mov " + destination + ", r10";
            }

            case Operation::RET:
            {
                if (!operands.empty())
                    throw std::invalid_argument(
                        "RET com registrador explicito ainda nao suportado"
                    );

                return "ret";
            }

            default:
                throw std::invalid_argument(
                    "Operacao ARM64 ainda nao suportada"
                );
        }
    }
}
