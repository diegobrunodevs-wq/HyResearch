
#pragma once

#include "instruction.hpp"

#include <cstdint>
#include <vector>

namespace Hyrule
{
    using NativeCode = std::vector<std::uint8_t>;

    class NativeCodeGenerator
    {
    public:
        NativeCode generate(const Instruction& instruction) const;

    private:
        NativeCode generateMov(const Instruction& instruction) const;

        std::uint8_t registerOffset(const std::string& name) const;
    };
}
