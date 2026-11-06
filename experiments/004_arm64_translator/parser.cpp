
#include "parser.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace Hyrule
{
    Instruction Parser::parse(const std::string& source)
    {
        Instruction instruction;
        instruction.original = source;

        std::string normalized = source;

        std::transform(
            normalized.begin(),
            normalized.end(),
            normalized.begin(),
            [](unsigned char c)
            {
                return static_cast<char>(std::toupper(c));
            }
        );

        std::istringstream stream(normalized);

        std::string operation;
        stream >> operation;

        if (operation == "MOV")
            instruction.operation = Operation::MOV;
        else if (operation == "ADD")
            instruction.operation = Operation::ADD;
        else if (operation == "SUB")
            instruction.operation = Operation::SUB;
        else if (operation == "MUL")
            instruction.operation = Operation::MUL;
        else if (operation == "RET")
            instruction.operation = Operation::RET;

        std::string operand;

        while (std::getline(stream, operand, ','))
        {
            if (!operand.empty())
            {
                const auto first = operand.find_first_not_of(" \t");

                if (first != std::string::npos)
                {
                    const auto last = operand.find_last_not_of(" \t");

                    instruction.operands.push_back(
                        operand.substr(first, last - first + 1)
                    );
                }
            }
        }

        return instruction;
    }
}
