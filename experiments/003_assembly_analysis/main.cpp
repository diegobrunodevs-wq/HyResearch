#include <iostream>
#include <cstdint>
#include "calculate.hpp"

int main()
{
    const std::int64_t a = 10;
    const std::int64_t b = 20;

    const auto result = calculate(a, b);

    std::cout << "HYRULE RESEARCH - EXPERIMENTO 003\n";
    std::cout << "Entrada A: " << a << '\n';
    std::cout << "Entrada B: " << b << '\n';
    std::cout << "Resultado: " << result << '\n';

    return 0;
}
