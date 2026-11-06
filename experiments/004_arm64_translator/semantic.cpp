#include "semantic.hpp"

#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <cctype>

namespace Hyrule
{
    std::size_t SemanticValidator::registerIndex(
        const std::string& name
    ) const
    {
        if (name.size() < 2 || name[0] != 'X')
            throw std::invalid_argument("Registrador ARM64 invalido: " + name);

        const std::string number = name.substr(1);

        if (!std::all_of(number.begin(), number.end(),
            [](unsigned char c) { return std::isdigit(c); }))
        {
            throw std::invalid_argument("Registrador ARM64 invalido: " + name);
        }

        const auto index = std::stoul(number);

        if (index >= 8)
            throw std::invalid_argument("Registrador ARM64 nao suportado: " + name);

        return static_cast<std::size_t>(index);
    }

    std::uint64_t SemanticValidator::readRegister(
        const std::string& name,
        const RegisterState& state
    ) const
    {
        if (name == "r10")
            return state.r10;

        static const std::array<std::string, 8> names =
        {
            "rax", "rbx", "rcx", "rdx",
            "rsi", "rdi", "r8", "r9"
        };

        for (std::size_t i = 0; i < names.size(); ++i)
        {
            if (name == names[i])
                return state.registers[i];
        }

        throw std::invalid_argument("Registrador x86-64 desconhecido: " + name);
    }

    void SemanticValidator::writeRegister(
        const std::string& name,
        std::uint64_t value,
        RegisterState& state
    ) const
    {
        if (name == "r10")
        {
            state.r10 = value;
            return;
        }

        static const std::array<std::string, 8> names =
        {
            "rax", "rbx", "rcx", "rdx",
            "rsi", "rdi", "r8", "r9"
        };

        for (std::size_t i = 0; i < names.size(); ++i)
        {
            if (name == names[i])
            {
                state.registers[i] = value;
                return;
            }
        }

        throw std::invalid_argument("Registrador x86-64 desconhecido: " + name);
    }

    void SemanticValidator::executeReference(
        const Instruction& instruction,
        RegisterState& state
    )
    {
        const auto& operands = instruction.operands;

        if (instruction.operation == Operation::RET)
            throw std::invalid_argument("RET ainda nao faz parte da validacao semantica");

        if (instruction.operation == Operation::MOV)
        {
            if (operands.size() != 2)
                throw std::invalid_argument("MOV exige 2 operandos");

            const auto source = state.registers[registerIndex(operands[1])];
            state.registers[registerIndex(operands[0])] = source;
            return;
        }

        if (operands.size() != 3)
            throw std::invalid_argument("Operacao aritmetica exige 3 operandos");

        const auto destination = registerIndex(operands[0]);
        const auto left = state.registers[registerIndex(operands[1])];
        const auto right = state.registers[registerIndex(operands[2])];

        switch (instruction.operation)
        {
            case Operation::ADD:
                state.registers[destination] = left + right;
                break;

            case Operation::SUB:
                state.registers[destination] = left - right;
                break;

            case Operation::MUL:
                state.registers[destination] = left * right;
                break;

            default:
                throw std::invalid_argument("Operacao nao suportada");
        }
    }

    void SemanticValidator::executeTranslated(
        const std::string& assembly,
        RegisterState& state
    )
    {
        std::istringstream input(assembly);
        std::string line;

        while (std::getline(input, line))
        {
            std::istringstream instructionStream(line);

            std::string operation;
            std::string destination;
            std::string source;

            instructionStream >> operation;

            if (operation.empty())
                continue;

            std::getline(instructionStream, destination, ',');
            std::getline(instructionStream, source);

            auto trim = [](std::string& value)
            {
                const auto first = value.find_first_not_of(" \t");
                if (first == std::string::npos)
                {
                    value.clear();
                    return;
                }

                const auto last = value.find_last_not_of(" \t");
                value = value.substr(first, last - first + 1);
            };

            trim(destination);
            trim(source);

            if (destination.empty() || source.empty())
                throw std::invalid_argument("Instrucao x86-64 malformada: " + line);

            const auto left = readRegister(destination, state);
            const auto right = readRegister(source, state);

            if (operation == "mov")
                writeRegister(destination, right, state);
            else if (operation == "add")
                writeRegister(destination, left + right, state);
            else if (operation == "sub")
                writeRegister(destination, left - right, state);
            else if (operation == "imul")
                writeRegister(destination, left * right, state);
            else
                throw std::invalid_argument("Instrucao x86-64 nao suportada: " + operation);
        }
    }

    bool SemanticValidator::equivalent(
        const RegisterState& reference,
        const RegisterState& translated
    ) const
    {
        return reference.registers == translated.registers;
    }
}