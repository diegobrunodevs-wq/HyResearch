#include <iostream>
#include <cstdint>
#include <bit>
#include <string_view>

std::string_view getArchitecture()
{
#if defined(_M_X64) || defined(__x86_64__)
    return "x86-64";
#elif defined(_M_ARM64) || defined(__aarch64__)
    return "ARM64";
#elif defined(_M_IX86) || defined(__i386__)
    return "x86";
#else
    return "Unknown";
#endif
}

int main()
{
    std::cout << "================================\n";
    std::cout << " HYRULE RESEARCH - EXPERIMENTO 002\n";
    std::cout << "================================\n\n";

    std::cout << "Arquitetura: " << getArchitecture() << '\n';
    std::cout << "Ponteiro: " << sizeof(void*) * 8 << " bits\n";
    std::cout << "Tamanho de int: " << sizeof(int) << " bytes\n";
    std::cout << "Tamanho de long: " << sizeof(long) << " bytes\n";
    std::cout << "Tamanho de long long: " << sizeof(long long) << " bytes\n";

    if constexpr (std::endian::native == std::endian::little)
        std::cout << "Endian: Little Endian\n";
    else if constexpr (std::endian::native == std::endian::big)
        std::cout << "Endian: Big Endian\n";
    else
        std::cout << "Endian: Mixed/Unknown\n";

    std::cout << "\nExperimento concluido.\n";

    return 0;
}
