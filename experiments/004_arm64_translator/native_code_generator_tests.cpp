
#include "native_code_generator.hpp"
#include "parser.hpp"
#include "test_support.hpp"

#include <array>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace Hyrule;

int main()
{
    Parser parser;
    NativeCodeGenerator generator;

    // Teste de referência conhecido:
    // MOV X0, X1
    // mov rax, [rcx + 8]
    // mov [rcx], rax
    const auto mov = parser.parse("MOV X0, X1");

    const NativeCode expected =
    {
        0x48, 0x8B, 0x41, 0x08,
        0x48, 0x89, 0x01
    };

    CHECK(generator.generate(mov) == expected);

    // Validação dos 64 pares de registradores.
    for (int destination = 0; destination < 8; ++destination)
    {
        for (int source = 0; source < 8; ++source)
        {
            const std::string instruction =
                "MOV X" + std::to_string(destination) +
                ", X" + std::to_string(source);

            const auto parsed = parser.parse(instruction);
            const auto generated = generator.generate(parsed);

            NativeCode expectedBytes;

            const auto sourceOffset =
                static_cast<std::uint8_t>(source * 8);

            const auto destinationOffset =
                static_cast<std::uint8_t>(destination * 8);

            // Leitura: mov rax, [rcx + sourceOffset]
            expectedBytes.push_back(0x48);
            expectedBytes.push_back(0x8B);

            if (sourceOffset == 0)
            {
                expectedBytes.push_back(0x01);
            }
            else
            {
                expectedBytes.push_back(0x41);
                expectedBytes.push_back(sourceOffset);
            }

            // Escrita: mov [rcx + destinationOffset], rax
            expectedBytes.push_back(0x48);
            expectedBytes.push_back(0x89);

            if (destinationOffset == 0)
            {
                expectedBytes.push_back(0x01);
            }
            else
            {
                expectedBytes.push_back(0x41);
                expectedBytes.push_back(destinationOffset);
            }

            CHECK(generated == expectedBytes);
        }
    }

    // Registrador inválido.
    bool invalidRegisterDetected = false;

    try
    {
        const auto invalid = parser.parse("MOV X8, X1");
        generator.generate(invalid);
    }
    catch (const std::invalid_argument&)
    {
        invalidRegisterDetected = true;
    }

    CHECK(invalidRegisterDetected);

    // Quantidade incorreta de operandos.
    bool invalidOperandsDetected = false;

    try
    {
        const auto invalid = parser.parse("MOV X0");
        generator.generate(invalid);
    }
    catch (const std::invalid_argument&)
    {
        invalidOperandsDetected = true;
    }

    CHECK(invalidOperandsDetected);

    std::cout << "Codificacao MOV aprovada!\n";
    std::cout << "Pares de registradores validados: 64\n";

    return 0;
}
