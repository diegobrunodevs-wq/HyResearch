#pragma once

#include "instruction.hpp"

#include <array>
#include <cstdint>
#include <string>

namespace Hyrule
{
    struct RegisterState
    {
        std::array<std::uint64_t, 8> registers{};
        std::uint64_t r10 = 0;
    };

    class SemanticValidator
    {
    public:
        void executeReference(
            const Instruction& instruction,
            RegisterState& state
        );

        void executeTranslated(
            const std::string& assembly,
            RegisterState& state
        );

        bool equivalent(
            const RegisterState& reference,
            const RegisterState& translated
        ) const;

    private:
        std::size_t registerIndex(const std::string& name) const;
        std::uint64_t readRegister(
            const std::string& name,
            const RegisterState& state
        ) const;

        void writeRegister(
            const std::string& name,
            std::uint64_t value,
            RegisterState& state
        ) const;
    };
}