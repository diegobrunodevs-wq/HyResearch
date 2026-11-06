
#include "parser.hpp"
#include "translator.hpp"
#include "semantic.hpp"
#include "test_support.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <random>
#include <string>

using namespace Hyrule;

int main()
{
    Parser parser;
    Translator translator;
    SemanticValidator validator;

    const auto testOperation = [&](const std::string& source,
                                   const std::array<std::uint64_t, 8>& initial)
    {
        RegisterState reference;
        RegisterState translated;

        reference.registers = initial;
        translated = reference;

        const auto instruction = parser.parse(source);
        const auto assembly = translator.translate(instruction);

        validator.executeReference(instruction, reference);
        validator.executeTranslated(assembly, translated);

        CHECK(validator.equivalent(reference, translated));
    };

    // Valores extremos de 64 bits.
    const std::uint64_t max = std::numeric_limits<std::uint64_t>::max();

    const std::array<std::array<std::uint64_t, 8>, 6> boundaryStates =
    {{
        {0, 0, 0, 0, 0, 0, 0, 0},
        {1, 1, 1, 1, 1, 1, 1, 1},
        {max, max, max, max, max, max, max, max},
        {max, 1, 2, 3, 4, 5, 6, 7},
        {0x8000000000000000ULL, 2, 3, 4, 5, 6, 7, 8},
        {0x7FFFFFFFFFFFFFFFULL, 3, 5, 7, 11, 13, 17, 19}
    }};

    // Operações básicas com estados extremos.
    const std::array<std::string, 6> instructions =
    {
        "MOV X0, X1",
        "ADD X0, X1, X2",
        "SUB X3, X4, X5",
        "MUL X6, X7, X0",
        "ADD X1, X1, X2",
        "MUL X0, X0, X1"
    };

    for (const auto& state : boundaryStates)
    {
        for (const auto& instruction : instructions)
        {
            testOperation(instruction, state);
        }
    }

    // Testes pseudoaleatórios reproduzíveis.
    std::mt19937_64 generator(0x485952554C45ULL);

    const std::array<std::string, 3> operations =
    {
        "ADD", "SUB", "MUL"
    };

    for (int i = 0; i < 1000; ++i)
    {
        std::array<std::uint64_t, 8> state;

        for (auto& value : state)
            value = generator();

        const auto destination = generator() % 8;
        const auto source1 = generator() % 8;
        const auto source2 = generator() % 8;

        const auto& operation = operations[generator() % operations.size()];

        const std::string instruction =
            operation + " X" + std::to_string(destination) +
            ", X" + std::to_string(source1) +
            ", X" + std::to_string(source2);

        testOperation(instruction, state);
    }

    // Testes de preservação dos registradores.
    // A comparação integral do estado já verifica isso,
    // pois somente o destino deve ser modificado.

    std::cout << "Testes de robustez semantica aprovados!\n";
    std::cout << "Estados extremos: "
              << boundaryStates.size() * instructions.size() << '\n';
    std::cout << "Operacoes pseudoaleatorias: 1000\n";

    return 0;
}
