
#include "parser.hpp"
#include "translator.hpp"

#include <cassert>
#include <iostream>
#include <stdexcept>

using namespace Hyrule;

int main()
{
    Parser parser;
    Translator translator;

    // Teste 1: MOV
    auto mov = parser.parse("MOV X0, X1");

    assert(
        translator.translate(mov) == "mov rax, rbx"
    );

    // Teste 2: ADD
    auto add = parser.parse("ADD X0, X1, X2");

    assert(
        translator.translate(add) ==
        "mov r10, rbx\n"
        "add r10, rcx\n"
        "mov rax, r10"
    );

    // Teste 3: SUB
    auto sub = parser.parse("SUB X3, X4, X5");

    assert(
        translator.translate(sub) ==
        "mov r10, rsi\n"
        "sub r10, rdi\n"
        "mov rdx, r10"
    );

    // Teste 4: MUL
    auto mul = parser.parse("MUL X6, X7, X0");

    assert(
        translator.translate(mul) ==
        "mov r10, r9\n"
        "imul r10, rax\n"
        "mov r8, r10"
    );

    // Teste 5: RET
    auto ret = parser.parse("RET");

    assert(translator.translate(ret) == "ret");

    // Teste 6: registrador não suportado
    bool unsupportedRegisterDetected = false;

    try
    {
        auto invalid = parser.parse("ADD X8, X1, X2");
        translator.translate(invalid);
    }
    catch (const std::invalid_argument&)
    {
        unsupportedRegisterDetected = true;
    }

    assert(unsupportedRegisterDetected);

    // Teste 7: quantidade incorreta de operandos
    bool invalidOperandsDetected = false;

    try
    {
        auto invalid = parser.parse("ADD X0, X1");
        translator.translate(invalid);
    }
    catch (const std::invalid_argument&)
    {
        invalidOperandsDetected = true;
    }

    assert(invalidOperandsDetected);

    std::cout << "Todos os testes do Translator passaram!\n";

    return 0;
}
