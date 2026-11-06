#pragma once

#include "instruction.hpp"
#include <string>

namespace Hyrule
{
    class Translator
    {
    public:
        std::string translate(const Instruction& instruction);

    private:
        std::string translateRegister(const std::string& armRegister);
    };
}