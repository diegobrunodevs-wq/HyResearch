
#include "parser.hpp"

#include <cassert>
#include <iostream>

using namespace Hyrule;

int main()
{
    Parser parser;

    // Teste 1: ADD
    Instruction add = parser.parse("ADD X0, X1, X2");

    assert(add.operation == Operation::ADD);
    assert(add.operands.size() == 3);
    assert(add.operands[0] == "X0");
    assert(add.operands[1] == "X1");
    assert(add.operands[2] == "X2");

    // Teste 2: SUB
    Instruction sub = parser.parse("SUB X3, X4, X5");

    assert(sub.operation == Operation::SUB);
    assert(sub.operands.size() == 3);

    // Teste 3: MOV
    Instruction mov = parser.parse("MOV X0, X1");

    assert(mov.operation == Operation::MOV);
    assert(mov.operands.size() == 2);

    // Teste 4: RET
    Instruction ret = parser.parse("RET");

    assert(ret.operation == Operation::RET);

    // Teste 5: instrução desconhecida
    Instruction unknown = parser.parse("INVALID X0");

    assert(unknown.operation == Operation::UNKNOWN);

    std::cout << "Todos os testes do Parser passaram!\n";

    return 0;
}
