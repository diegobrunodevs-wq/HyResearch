
#pragma once

#include "instruction.hpp"

#include <string>

namespace Hyrule
{
    class Parser
    {
    public:
        Instruction parse(const std::string& source);
    };
}
